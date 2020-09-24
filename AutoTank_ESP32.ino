const int ID_DISPOSITIVO = 1;

//Librerie 
#include <EEPROM.h>
#include <WiFi.h>          
#include <SPI.h>
#include <WebServer.h>
#include <WiFiManager.h> 
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include "SH1106.h"      
#include "images.h"
#include "ESPAsyncWebServer.h"
#include "DFRobot_ESP_PH.h"

//Librerie Sensori
#include <analogWrite.h>
#include <OneWire.h>
#include <DallasTemperature.h>

//Dichiarazione PIN
const int LINEA_SENSORI_TEMPERATURA = 13;
const int POMPA = 25;
const int SENSORE_LIVELLO_ACQUA = 34;
const int TERMO_RISCALDATORE = 16;
const int PH = 35;
const int OLED_SDA = 21;
const int OLED_SCL = 22;

struct LED{
  int ROSSO = -1;
  int VERDE = -1;
  int BLU = -1;
};

LED STRISCIA_RGB1; 
LED STRISCIA_RGB2;
const int STRISCIA_3000k = 5;
  
//Inizializzazione Libreria OLED
SH1106 display(0x3c, OLED_SDA, OLED_SCL);

//Inizializzazione Libreria WiFiManager
WiFiManager wifiManager;

//Dichiarazion per connessione ad internet
WiFiClient client;
HTTPClient http; 
AsyncWebServer server(81);

//Inizializzazione Libreria Sensore Temperatura

OneWire oneWire(LINEA_SENSORI_TEMPERATURA);
DallasTemperature sensors(&oneWire);

//Inizializzazioni Sensore Ph
DFRobot_ESP_PH ph;
#define ESPADC 4096   //the esp Analog Digital Convertion value
#define ESPVOLTAGE 3300 //the esp voltage supply value
float voltaggio = 0;
//Dichiarazione costanti e variabili
unsigned long ultimoInvio;
const int TEMPO_MISURAZIONE = 30000; //1 ora in millisecondi

bool new_config;

struct  ValoriAcqua{
  float livello = 0;
  float temperatura = 0;
  float ph = 0;
};

ValoriAcqua valori;

enum TIPO_LUCE{Mattina, Mezzogiorno, Sera, LunaPiena, FitoStimolante, LUCE_DEFAULT};

class LUCE{
  public:
    TIPO_LUCE tipo;
    uint8_t red1;
    uint8_t green1;
    uint8_t blu1;
    
    uint8_t red2;
    uint8_t green2;
    uint8_t blu2;
    
    uint8_t white;

    LUCE(){
      tipo = LUCE_DEFAULT;
      this->red1 = 255;
      this->green1 = 255;
      this->blu1 = 255;
      this->red2 = 255;
      this->green2 = 255;
      this->blu2 = 255;
      this->white = 255;
    }
    
    LUCE(TIPO_LUCE type, uint8_t red1, uint8_t green1, uint8_t blu1, uint8_t red2, uint8_t green2, uint8_t blu2, uint8_t white){
      tipo = type;
      this->red1 = red1;
      this->green1 = green1;
      this->blu1 = blu1;
      this->red2 = red2;
      this->green2 = green2;
      this->blu2 = blu2;
      this->white = white;
    }
    
    void operator=(const LUCE luce){
      tipo = luce.tipo;
      red1 = luce.red1;
      green1 = luce.green1;
      blu1 = luce.blu1;
      red2 = luce.red2;
      green2 = luce.green2;
      blu2 = luce.blu2;
      white = luce.white;
    }
};

const LUCE mezzogiorno(Mezzogiorno, 255,255,0,255,255,0,255);
const LUCE sera(Sera,255,54,0,255,86,0,150);
const LUCE mattina(Mattina,0,255,216,234,255,0,210);
const LUCE luna_piena(LunaPiena,0,0,200,0,0,200,50);
const LUCE fito_stimolante(FitoStimolante,255,0,0,0,0,255,75);

const int CONFIGURATION_SET_FLAG_ADDR = 0; //Serve per capire se la configurazione presente è stata settata dall'utente o è quella default (per esempio al primo avvio)
const int CONFIG_ADDR = 1;


struct  Configurazione{ //Ho messo dei valori di default in modo che al primo avvio i pesci non vengano uccisi
  int livello = 0;
  float temperatura = 26;
  LUCE luce;
};

Configurazione configurazione;

//Funzioni
bool misurazione_time(){
  if(millis() - ultimoInvio >= TEMPO_MISURAZIONE )
    return true;
  else
    return false; 
}
bool invioNuoviDati(ValoriAcqua valori){
  Serial.println("Invio Dati...\n");
  String payload = "";
  payload ="ID="+ String(ID_DISPOSITIVO) + "&temp=" + String(valori.temperatura) + "&liv=" + String(valori.livello) + "&ph=" + String(valori.ph);
  http.begin("http://www.autotank.altervista.org/Autotank/connection/add.php");
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");
  if(http.POST(payload) != 200){
    http.end();
    return false;
  }
  http.end();
  return true;
}

void displayLogo(){
  display.drawXbm(28,0, Betta_Logo_Width, Betta_Logo_Height, Betta_Logo); //l'immagine si trova nella lireria OLED thingPulse in images.h
  display.drawString(42,50,"AutoTank");
  display.display();
  delay(2500);
  display.clear();
}

void screenWiFi(){
  display.drawXbm((128-WiFi_Logo_width)/2,5,WiFi_Logo_width, WiFi_Logo_height, WiFi_Logo_bits);
  display.drawString(25,45,"Waiting for WiFi...");
  display.display();
}

void displayValori(ValoriAcqua valori){
  display.clear();
  display.setFont(ArialMT_Plain_24);
  display.drawString(35,17,(String)valori.temperatura+"°");
  display.setFont(ArialMT_Plain_16);
  display.drawString(0,0,"pH: " + (String)valori.ph);
  float var = valori.livello/1000;
  String str = "livello: " + (String)var;
  display.drawString(0,45,str);
  display.display();
}


 void setRGB(LED led, unsigned char rosso, unsigned char verde, unsigned char blu){ //attiva il led con l’intensita’ definita nella variabile(varia da 0 a 255)
 analogWrite(led.ROSSO, rosso);
 analogWrite(led.BLU, blu);
 analogWrite(led.VERDE, verde); 
}

void setLUCE(LUCE luce){
  setRGB(STRISCIA_RGB1,luce.red1,luce.green1, luce.blu1);
  setRGB(STRISCIA_RGB2, luce.red2, luce.green2, luce.blu2);
  analogWrite(STRISCIA_3000k, luce.white);
}

void setup_led(int pinR, int pinG, int pinB){
  pinMode(pinR, OUTPUT);
  pinMode(pinG, OUTPUT);
  pinMode(pinB, OUTPUT);
}

void extractParameters(uint8_t *data, size_t length, String *ArrayOfParametri){
  String parametri[] = {"","",""};
  int pos = 0;
  for(int i=0;i<3;i++){
    while(data[pos] != 59){ //59 è il codice ASCII per ";"
      if(pos >= length){return;} 
      parametri[i] += (char)data[pos];
      pos++;
      ArrayOfParametri[i] = parametri[i]; 
    }
    pos++;
  }
  return;
}

void setup(){
  //Inizializzazione Porta Seriale e FLASH MEMORY per simulare la EEPROM
  Serial.begin(115200);
  EEPROM.begin(512);
  
  
  if(EEPROM.read(CONFIGURATION_SET_FLAG_ADDR) == 1)
    EEPROM.get(CONFIG_ADDR, configurazione);
  
  //Setup dispaly OLED
  display.init();
  display.clear();
  displayLogo();
  
  //Setup Sensori
  ph.begin();
  pinMode(SENSORE_LIVELLO_ACQUA, INPUT);
  analogReadResolution(12); 
  sensors.begin(); //Faccio partire libreria sensore temperatura

  //Setup Attuatori
  pinMode(POMPA, OUTPUT);
  pinMode(TERMO_RISCALDATORE,OUTPUT);
  digitalWrite(POMPA,LOW);
  digitalWrite(TERMO_RISCALDATORE,LOW);

  //Setup Led
  STRISCIA_RGB1.ROSSO = 17;
  STRISCIA_RGB1.VERDE = 19 ;
  STRISCIA_RGB1.BLU = 18;

  STRISCIA_RGB2.ROSSO = 33;
  STRISCIA_RGB2.VERDE = 32;
  STRISCIA_RGB2.BLU = 23;

  setup_led(STRISCIA_RGB1.ROSSO, STRISCIA_RGB1.VERDE,STRISCIA_RGB1.BLU);
  setup_led(STRISCIA_RGB2.ROSSO, STRISCIA_RGB2.VERDE,STRISCIA_RGB2.BLU);
  pinMode(STRISCIA_3000k, OUTPUT);
  
  setRGB(STRISCIA_RGB1,0,0,0);
  setRGB(STRISCIA_RGB2,0,0,0);
  
  //Setup WiFi
  screenWiFi();
  //wifiManager.setSTAStaticIPConfig(IPAddress(192,168,1,22), IPAddress(192,168,1,1), IPAddress(255,255,255,0));
  wifiManager.setConfigPortalTimeout(180);
  String msg = "";
  if(wifiManager.autoConnect("AutoTank","vivaipesci")){
    display.resetDisplay();
    msg = "Connessione a " + WiFi.SSID() + " riuscita!";
  }
  else{
    display.resetDisplay();
    msg = "Connessione FALLITA!";
  }
  display.drawStringMaxWidth((128-100)/2, 25, 100, msg);
  display.display();
  delay(2000);
  display.clear();

  new_config = true;

//Setup comportamento del server
//La prima lambda function è quella che risponde al richiedente la seconda funge da handler all'interno del server(la scheda)
  server.on(
    "/setConfiguration",HTTP_POST,[](AsyncWebServerRequest * request){
        AsyncWebServerResponse *response = request->beginResponse(200,"text/plain","OK");
        response->addHeader("Access-Control-Allow-Origin","*");
        response->addHeader("X-Content-Type-Options", "nosniff");
        request->send(response);
      },
      NULL,
      [](AsyncWebServerRequest * request, uint8_t *data, size_t len, size_t index, size_t total) {
         String parametri[3];
         extractParameters(data, len, parametri);

         configurazione.temperatura = parametri[0].toInt();
         configurazione.livello = parametri[1].toInt();
         Serial.println("LUCE:");
         Serial.println(parametri[2]);
         
         if(parametri[2] == "Mattina")
            configurazione.luce = mattina;
         if(parametri[2] == "Mezzogiorno")
          configurazione.luce = mezzogiorno;
         if(parametri[2] == "Sera")
          configurazione.luce = sera;
         if(parametri[2] == "Luna piena")
          configurazione.luce = luna_piena;
         if(parametri[2] == "Fitostimolante")
          configurazione.luce = fito_stimolante;
         
         EEPROM.put(CONFIG_ADDR, configurazione);
         EEPROM.write(CONFIGURATION_SET_FLAG_ADDR,1);
         EEPROM.commit();

         new_config = true;
         AsyncWebServerResponse *response = request->beginResponse(200,"text/plain","CONFIG_SET");
         response->addHeader("Access-Control-Allow-Origin","*");
         response->addHeader("X-Content-Type-Options", "nosniff");
         request->send(response);
  });
  server.on("/setConfiguration", HTTP_OPTIONS, [](AsyncWebServerRequest * request) {
          AsyncWebServerResponse *response = request->beginResponse(204);
          response->addHeader("Access-Control-Allow-Origin","*");
          response->addHeader("X-Content-Type-Options", "nosniff");
          request->send(response);
  }); 
  server.on(
      "/tankValues",HTTP_GET,[](AsyncWebServerRequest * request){
          StaticJsonDocument<JSON_OBJECT_SIZE(4)> JSONDoc; 
          JSONDoc["temperatura"] = valori.temperatura;
          JSONDoc["livello"] = valori.livello;
          JSONDoc["ph"] = valori.ph;
          JSONDoc["luce"] = configurazione.luce.tipo;
          String json;
          serializeJson(JSONDoc, json);
          AsyncWebServerResponse *response = request->beginResponse(200,"text/json",json);
          response->addHeader("Access-Control-Allow-Origin","*");
          response->addHeader("X-Content-Type-Options", "nosniff");
          request->send(response);
       });
  server.on("/tankValues", HTTP_OPTIONS, [](AsyncWebServerRequest * request) {
      AsyncWebServerResponse *response = request->beginResponse(204);
          response->addHeader("Access-Control-Allow-Origin","*");
          response->addHeader("X-Content-Type-Options", "nosniff");
          request->send(response);
  });
  server.on("/connected",HTTP_GET,[](AsyncWebServerRequest * request){
          AsyncWebServerResponse *response = request->beginResponse(200,"text/plain","CONNECTED");
          response->addHeader("Access-Control-Allow-Origin","*");
          response->addHeader("X-Content-Type-Options", "nosniff");
          request->send(response);
   });
  server.on("/connected",HTTP_OPTIONS,[](AsyncWebServerRequest * request){
          AsyncWebServerResponse *response = request->beginResponse(204);
          response->addHeader("Access-Control-Allow-Origin","*");
          response->addHeader("X-Content-Type-Options", "nosniff");
          request->send(response);
  });  
  server.onNotFound([](AsyncWebServerRequest * request){
      AsyncWebServerResponse *response = request->beginResponse(404);
          response->addHeader("Access-Control-Allow-Origin","*");
          request->send(response);
  });
  
  server.begin();
  //Assegnazione variabile per il timer di invio
  ultimoInvio =  millis();
}


void loop(){
  valori.livello = analogRead(SENSORE_LIVELLO_ACQUA);
  valori.livello -= 2000; //Scalo lo zero al centro del sensore
  sensors.requestTemperatures();
  valori.temperatura = sensors.getTempCByIndex(0);
  voltaggio = (float)analogRead(PH) * ESPVOLTAGE / (float)ESPADC ;
  Serial.println(analogRead(PH));
  Serial.println(voltaggio);
  valori.ph = ph.readPH(voltaggio, valori.temperatura);
  ph.calibration(voltaggio, valori.temperatura); 
  
  displayValori(valori);
  
  while(valori.livello < configurazione.livello){
    display.clear();
    digitalWrite(POMPA,HIGH);
    valori.livello = analogRead(SENSORE_LIVELLO_ACQUA);
    valori.livello -= 2000;
  }
  digitalWrite(POMPA,LOW);
    
  if(valori.temperatura < configurazione.temperatura)
    digitalWrite(TERMO_RISCALDATORE,HIGH);
  else
    digitalWrite(TERMO_RISCALDATORE,LOW);

  if(new_config){
    new_config = false;
    setLUCE(configurazione.luce);
  }
  
  if(misurazione_time() && WiFi.status() == WL_CONNECTED){
    invioNuoviDati(valori);
    ultimoInvio = millis();
  }
  delay(100);
}
