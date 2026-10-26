#include <DHT.h>
#include <SoftwareSerial.h>
#include <TinyGPS++.h>
#include <SPI.h>
#include <MFRC522.h>

// Définitions des broches
#define DHTPIN 2          // Broche du capteur DHT22
#define DHTTYPE DHT22     // Type de capteur DHT
#define FLAME_SENSOR A0   // Broche du capteur de flamme
#define GAS_SENSOR A1     // Broche du capteur de gaz
#define PIR_PIN 8         // Broche du capteur PIR (HC-SR501)
#define VEN 12            // Broche de la ventilation
#define SPEAKER_PIN 7     // Broche du buzzer

// Broches pour le GPS NEO
#define GPS_RX 3          // Broche RX du GPS (connectée à TX du GPS)
#define GPS_TX 4          // Broche TX du GPS (connectée à RX du GPS)

// Broches pour le module RFID
#define RFID_SS_PIN 10
#define RFID_RST_PIN 9

// Initialisation des capteurs
DHT dht(DHTPIN, DHTTYPE);

// Initialisation de SoftwareSerial pour le GPS
SoftwareSerial gpsSerial(GPS_RX, GPS_TX);

// Initialisation de TinyGPS++
TinyGPSPlus gps;

// Initialisation du module RFID
MFRC522 rfid(RFID_SS_PIN, RFID_RST_PIN);

// Variables globales
float humidity, temperature;
int valeurGaz, valeurFlamme;
float tensionGaz, Rs;
bool mouvementDetecte = false;  // Variable pour le capteur de mouvement

// Constantes pour le calcul des gaz
#define RL 1000  // Résistance de charge
#define VCC 5.0  // Tension d'alimentation
#define RO 10.0  // Résistance de référence
#define M_CH4 -0.38
#define B_CH4 1.5
#define M_LPG -0.47
#define B_LPG 1.7
#define M_H2 -0.42
#define B_H2 1.6
#define M_CO -0.35
#define B_CO 1.3

float ppm_CH4, ppm_LPG, ppm_H2, ppm_CO;
bool flammeDetectee = false;
unsigned long previousMillis = 0;

void setup() {
    // Initialisation de la communication série
    Serial.begin(9600);
    Serial.println("Initialisation du système...");

    // Initialisation du capteur DHT
    Serial.println("Initialisation du capteur DHT...");
    dht.begin();
    Serial.println("Capteur DHT initialisé.");

    // Initialisation du GPS
    gpsSerial.begin(9600);
    Serial.println("GPS initialisé.");

    // Initialisation du module RFID
    SPI.begin();          // Initialisation du bus SPI
    rfid.PCD_Init();      // Initialisation du module RFID
    Serial.println("RFID initialisé.");

    // Configuration des broches
    pinMode(PIR_PIN, INPUT);      // Broche du capteur de mouvement en entrée
    pinMode(VEN, OUTPUT);
    pinMode(FLAME_SENSOR, INPUT);
    pinMode(SPEAKER_PIN, OUTPUT);  // Configuration de la broche du buzzer en sortie
    digitalWrite(VEN, LOW);

    Serial.println("Setup terminé.");
}

void loop() {
    unsigned long currentMillis = millis();

    // Exécuter toutes les 2000 ms (2 secondes)
    if (currentMillis - previousMillis >= 2000) {
        previousMillis = currentMillis;

        // Lecture des capteurs
        gaz();
        temp();
        flamme();
        mouvement();  // Lecture du capteur de mouvement

        // Lecture des données GPS
        lireGPS();

        // Lecture du RFID
        lireRFID();

        // Envoi des données sur le port série
        ser();
    }
}

// 🔥 Détection de flamme
void flamme() {
    valeurFlamme = analogRead(FLAME_SENSOR);
    flammeDetectee = (valeurFlamme < 200);

    if (flammeDetectee) {
        digitalWrite(VEN, HIGH);
        tone(SPEAKER_PIN, 2000);
    } else {
        digitalWrite(VEN, LOW);
        noTone(SPEAKER_PIN);
    }
}

// 💨 Détection de gaz
void gaz() {
    valeurGaz = analogRead(GAS_SENSOR);
    tensionGaz = valeurGaz * (VCC / 1023.0);
    Rs = ((VCC - tensionGaz) * RL) / tensionGaz;

    ppm_CH4 = pow(10, (M_CH4 * log10(Rs / RO) + B_CH4));
    ppm_LPG = pow(10, (M_LPG * log10(Rs / RO) + B_LPG));
    ppm_H2  = pow(10, (M_H2  * log10(Rs / RO) + B_H2));
    ppm_CO  = pow(10, (M_CO  * log10(Rs / RO) + B_CO));
}

// 🌡️ Température et humidité
void temp() {
    humidity = dht.readHumidity();
    temperature = dht.readTemperature();
    if (isnan(humidity) || isnan(temperature)) {
        Serial.println("Erreur de lecture du capteur DHT !");
    }
}

// 📡 Lecture des données GPS
void lireGPS() {
    while (gpsSerial.available() > 0) {
        char c = gpsSerial.read();
        if (gps.encode(c)) {  // Décodage des données GPS
            // Les données GPS sont stockées dans l'objet gps
        }
    }
}

// 🚶 Détection de mouvement
void mouvement() {
    mouvementDetecte = digitalRead(PIR_PIN) == HIGH;  // HIGH = mouvement détecté
}

// 🏷️ Lecture du RFID
void lireRFID() {
    // Vérifie si une nouvelle carte est détectée
    if (rfid.PICC_IsNewCardPresent() && rfid.PICC_ReadCardSerial()) {
        Serial.print("Carte RFID détectée. UID : ");
        String uid = "";
        for (byte i = 0; i < rfid.uid.size; i++) {
            uid += String(rfid.uid.uidByte[i] < 0x10 ? "0" : "");
            uid += String(rfid.uid.uidByte[i], HEX);
        }
        Serial.println(uid);  // Affiche l'UID de la carte

        // Liste des UIDs autorisés
        String uidsAutorises[] = {
            "4aedf803",  // UID de la première carte
            "b442ca01"   // UID de la deuxième carte
        };
        int nombreUidsAutorises = 2;  // Nombre d'UIDs dans la liste

        // Vérifie si l'UID est autorisé
        bool accesAutorise = false;
        for (int i = 0; i < nombreUidsAutorises; i++) {
            if (uid.equalsIgnoreCase(uidsAutorises[i])) {
                accesAutorise = true;
                break;
            }
        }

        // Affiche le résultat
        if (accesAutorise) {
            Serial.println("Accès donné ✅");
        } else {
            Serial.println("Accès refusé ❌");
        }

        rfid.PICC_HaltA();  // Arrête la communication avec la carte
    }
}

// 📡 Envoi des données série
void ser() {
    Serial.print("CH4: "); Serial.print(ppm_CH4, 1); Serial.print(" ppm, ");
    Serial.print("LPG: "); Serial.print(ppm_LPG, 1); Serial.print(" ppm, ");
    Serial.print("H2: "); Serial.print(ppm_H2, 1); Serial.print(" ppm, ");
    Serial.print("CO: "); Serial.print(ppm_CO, 1); Serial.print(" ppm, ");
    Serial.print("Hum: "); Serial.print(humidity); Serial.print(" %, ");
    Serial.print("Temp: "); Serial.print(temperature); Serial.print(" °C, ");
    Serial.print("Flamme: "); Serial.print(flammeDetectee ? "OUI" : "NON"); Serial.print(", ");
    Serial.print("Mouvement: "); Serial.print(mouvementDetecte ? "Détecté 🚶" : "Aucun mouvement"); Serial.print(", ");

    // Affichage des données GPS
    if (gps.location.isValid()) {
        Serial.print("Latitude: "); Serial.print(gps.location.lat(), 6); Serial.print(", ");
        Serial.print("Longitude: "); Serial.print(gps.location.lng(), 6); Serial.print(", ");
        Serial.print("Altitude: "); Serial.print(gps.altitude.meters()); Serial.print(" m, ");
        Serial.print("Vitesse: "); Serial.print(gps.speed.kmph()); Serial.print(" km/h, ");
        Serial.print("Satellites: "); Serial.print(gps.satellites.value());
    } else {
        Serial.print("GPS: Pas de signal");
    }

    Serial.println();
}