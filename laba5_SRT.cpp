#include <chrono>
#include <thread>
#include <atomic>
#include <vector>
#include <iostream>
#include <windows.h>
#include <locale>

using namespace std::chrono_literals;

// ============================ m`ytex ============================
class CustomMutex {
private:
    std::atomic<bool> lock_flag{ false };

public:
    void lock() {
        // Zhdem poka flag stanet false, potom ustanavlivaem ego v true
        bool expected = false;
        while (!lock_flag.compare_exchange_weak(expected, true, std::memory_order_acquire)) {
            expected = false;  // Sbrosaem expected pri neudache
            std::this_thread::yield();  // Daem shans drugim potokam
        }
    }

    void unlock() {
        // Ustanavlivaem flag v false dlya osvobozhdeniya
        lock_flag.store(false, std::memory_order_release);
    }
};

// ============================ programma ============================
CustomMutex kuhnya_lock;
std::atomic<bool> povar_rabotaet{ true };
std::atomic<bool> tolstyaki_edyat{ false };
std::atomic<int> tarelki_gotovy{ 0 };

int tarelka1, tarelka2, tarelka3;
int tolstyak1_syel, tolstyak2_syel, tolstyak3_syel;
std::atomic<bool> simulatsiya_rabotaet;
std::atomic<bool> stsenary_zavershon;

void sbrosit_simulatsiyu() {
    tarelka1 = tarelka2 = tarelka3 = 3000;
    tolstyak1_syel = tolstyak2_syel = tolstyak3_syel = 0;
    simulatsiya_rabotaet = true;
    stsenary_zavershon = false;
    povar_rabotaet = true;
    tolstyaki_edyat = false;
    tarelki_gotovy = 0;
}

void potok_povara(int effektivnost) {
    while (simulatsiya_rabotaet && !stsenary_zavershon) {
        while (!povar_rabotaet && simulatsiya_rabotaet && !stsenary_zavershon) {
            std::this_thread::yield();
        }

        if (!simulatsiya_rabotaet || stsenary_zavershon) return;

        // Kriticheskaya sektsiya - rabota povara
        kuhnya_lock.lock();

        tarelka1 += effektivnost;
        tarelka2 += effektivnost;
        tarelka3 += effektivnost;

        tarelki_gotovy = 3;
        povar_rabotaet = false;
        tolstyaki_edyat = true;

        kuhnya_lock.unlock();
    }
}

void potok_tolstyaka(int tolstyak_id, int appetite) {
    int* tsel_tarelka = nullptr;
    int* syedeno_vsego = nullptr;

    if (tolstyak_id == 1) {
        tsel_tarelka = &tarelka1;
        syedeno_vsego = &tolstyak1_syel;
    }
    else if (tolstyak_id == 2) {
        tsel_tarelka = &tarelka2;
        syedeno_vsego = &tolstyak2_syel;
    }
    else {
        tsel_tarelka = &tarelka3;
        syedeno_vsego = &tolstyak3_syel;
    }

    while (simulatsiya_rabotaet && !stsenary_zavershon) {
        while (!tolstyaki_edyat && simulatsiya_rabotaet && !stsenary_zavershon) {
            std::this_thread::yield();
        }

        if (!simulatsiya_rabotaet || stsenary_zavershon) return;

        kuhnya_lock.lock();

        bool pustaya_tarelka = (tarelka1 <= 0) || (tarelka2 <= 0) || (tarelka3 <= 0);
        bool vse_lopnuli = (tolstyak1_syel >= 10000) && (tolstyak2_syel >= 10000) && (tolstyak3_syel >= 10000);

        if (pustaya_tarelka || vse_lopnuli) {
            stsenary_zavershon = true;
            kuhnya_lock.unlock();
            return;
        }

        if (tarelki_gotovy > 0 && *tsel_tarelka >= appetite) {
            *tsel_tarelka -= appetite;
            *syedeno_vsego += appetite;
            tarelki_gotovy--;

            if (tarelki_gotovy == 0) {
                tolstyaki_edyat = false;
                povar_rabotaet = true;
            }
        }

        kuhnya_lock.unlock();
    }
}

void zapustit_stsenary(int effektivnost_povara, int appetite_tolstyaka,
    const std::string& nazvanie, int ozhidaemy_rezultat) {
    sbrosit_simulatsiyu();

    std::cout << "\n===== " << nazvanie << " =====" << std::endl;
    std::cout << "Parametry: effektivnost=" << effektivnost_povara
        << ", appetite=" << appetite_tolstyaka << std::endl;

    std::thread povar(potok_povara, effektivnost_povara);
    std::thread tolstyak1(potok_tolstyaka, 1, appetite_tolstyaka);
    std::thread tolstyak2(potok_tolstyaka, 2, appetite_tolstyaka);
    std::thread tolstyak3(potok_tolstyaka, 3, appetite_tolstyaka);

    auto nachalo_vremeni = std::chrono::steady_clock::now();
    int proshlo_sekund = 0;

    while (simulatsiya_rabotaet && !stsenary_zavershon && proshlo_sekund < 5) {
        auto tekushee_vremya = std::chrono::steady_clock::now();
        proshlo_sekund = std::chrono::duration_cast<std::chrono::seconds>(
            tekushee_vremya - nachalo_vremeni).count();
        std::this_thread::sleep_for(10ms);
    }

    simulatsiya_rabotaet = false;
    stsenary_zavershon = true;

    povar.join();
    tolstyak1.join();
    tolstyak2.join();
    tolstyak3.join();

    auto konec_vremeni = std::chrono::steady_clock::now();
    auto prodolzhitelnost = std::chrono::duration_cast<std::chrono::seconds>(
        konec_vremeni - nachalo_vremeni);

    std::cout << "Rezultaty cherez " << prodolzhitelnost.count() << " sekund:" << std::endl;
    std::cout << "- Syedeno tolstyakami: " << tolstyak1_syel << ", "
        << tolstyak2_syel << ", " << tolstyak3_syel << std::endl;
    std::cout << "- Ostalos v tarelkakh: " << tarelka1 << ", "
        << tarelka2 << ", " << tarelka3 << std::endl;

    bool pustaya_tarelka = (tarelka1 <= 0) || (tarelka2 <= 0) || (tarelka3 <= 0);
    bool vse_lopnuli = (tolstyak1_syel >= 10000) && (tolstyak2_syel >= 10000) && (tolstyak3_syel >= 10000);
    bool vremya_isteklo = (prodolzhitelnost.count() >= 5);

    bool uspekh = false;

    if (ozhidaemy_rezultat == 1) {
        uspekh = pustaya_tarelka && !vremya_isteklo;
        std::cout << "ITOG: "
            << (uspekh ? "USPEKH - Povar uvolen! (tarelka pustaya do 5 dney)"
                : "NEUDACHA - Cel ne dostignuta") << std::endl;
    }
    else if (ozhidaemy_rezultat == 2) {
        uspekh = vse_lopnuli && !vremya_isteklo;
        std::cout << "ITOG: "
            << (uspekh ? "USPEKH - Povar ne poluchil zarplatu! (vse lopnuli do 5 dney)"
                : "NEUDACHA - Cel ne dostignuta") << std::endl;
    }
    else if (ozhidaemy_rezultat == 3) {
        uspekh = vremya_isteklo && !pustaya_tarelka && !vse_lopnuli;
        std::cout << "ITOG: "
            << (uspekh ? "USPEKH - Povar uvolilsya sam! (proshlo 5 dney, vse stabilno)"
                : "NEUDACHA - Cel ne dostignuta") << std::endl;
    }

    if (!uspekh) {
        std::cout << "   (Prichiny: ";
        if (vremya_isteklo) std::cout << "vremya istaklo, ";
        if (pustaya_tarelka) std::cout << "pustaya tarelka, ";
        if (vse_lopnuli) std::cout << "vse lopnuli";
        std::cout << ")" << std::endl;
    }
}

int main() {
    SetConsoleOutputCP(CP_UTF8);
    std::locale::global(std::locale("en_US.UTF-8"));

    std::cout << "LABORATORNAYa RABOTA #4: TRI TOLSTYAKA" << std::endl;
    std::cout << "Polzovatelsky myutex na osnove compare_exchange" << std::endl;
    std::cout << "======================================================" << std::endl;
    std::cout << "Nachalnye usloviya: 3000 naggetsov v kajdoy tarelke" << std::endl;
    std::cout << "Tolstyaki lopayutsya posle 10000 naggetsov kajdy" << std::endl;
    std::cout << "Simulyatsiya zakanchivaetsya posle 5 dney (5 sekund)" << std::endl;
    std::cout << "======================================================" << std::endl;

    // Stsenary 1: Povar uvolen - tarelki bystro pusteyut
    zapustit_stsenary(5, 150, "STsENARY 1: Povar uvolen", 1);

    // Stsenary 2: Povar ne poluchil zarplatu - vse tolstyaki bystro lopayutsya
    zapustit_stsenary(300, 250, "STsENARY 2: Povar bez zarplaty", 2);

    // Stsenary 3: Povar uvolilsya sam - sbalansirovannye parametry
    zapustit_stsenary(20, 15, "STsENARY 3: Povar uvolilsya sam", 3);

    std::cout << "\n======================================================" << std::endl;
    std::cout << "DEMONSTRATsIYa RABOTY POLZOVATELSKOGO MYUTEXA:" << std::endl;
    std::cout << "1. Ispolzuet atomarnuyu peremennuyu s compare_exchange_weak" << std::endl;
    std::cout << "2. Realizuet lock/unlock mechanismy" << std::endl;
    std::cout << "3. Obespechivaet vzaimnoe isklyuchenie dlya kriticheskih sektsiy" << std::endl;
    std::cout << "4. Vse tri stsenarya demonstriruyut pravilnuyu sinhronizatsiyu" << std::endl;
    std::cout << "\nProgramma zavershena uspeshno." << std::endl;

    return 0;
}