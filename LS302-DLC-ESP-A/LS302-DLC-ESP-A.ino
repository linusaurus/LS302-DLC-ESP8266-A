
/*****************************************************
MQTT client LIFT_SLIDE Motor Controller
r.young 
v6-12-2018,4-17-2018,2-29-2018, 8.29.2017
LS302-DLC-ESP-A
*****************************************************/
// ------------------ INCLUDES ----------------------
//#include <DHT.h>
//#include <DHT_U.h>
#include <Atm_esp8266.h>
#include <Encoder.h>
#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <EEPROM.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
// ---------------------------------------------------

#define DEBUG 
//#define ENCODER_OUTPUT 0

Encoder encoder(4,5);
//--------------WIFI-------------------------------------
const char* ssid = "MonkeyRanch";
const char* password = "12345678";
const char* mqtt_server = "192.168.10.62";
//-------------------------------------------------------
WiFiClient espClient;
PubSubClient client(espClient);
//-------------------------------------------------------
long lastMsg = 0;
char msg[50];
int value = 0;

unsigned long CURRENT_MILLIS;
int PWMpin = 15;
int DIRpin = 12;
int LEDpin = 2;

const int STATUS_LED = 16;
const int BEACON_LED = 13;
//------------------------MOTOR OPERATION---------------------  
byte MotorState= 0;  // 0=STOP; 1=RAISE ; 2=LOWER
long HI_LIMIT = 55000;
long LO_LIMIT = 0;
//------------------------------------------------------------
//-----------------------ENCODER-----------------------------
long newPosition;
long oldPosition;
byte newDoorPosition;
byte oldDoorPosition;
//------------------------------------------------------------
const char* MQTT_CLIENT_ID = "LS302-DLC-02";
const char* outTopic = "STATUS";
const char* inTopic = "DLIN";


//-------------Diagnostic Signals ------------------+
unsigned long     BEACON_DELAY = 2000;
Atm_led led;
//--------------------------------------------------+

void callback(char* topic, byte* payload, unsigned int length) {
  #ifdef DEBUG
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] "); 
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();
  #endif
  // Switch on the LED if an 1 was received as first character
  if ((char)payload[0] == '0') {
    MotorState = 0;  // Stop Motor
    led.begin(BEACON_LED).blink(30,2000).trigger(led.EVT_START);  //Standby
    #ifdef DEBUG
    Serial.println("Motor State -> 0");
    #endif
    
  } else if ((char)payload[0] == '1') {    
    MotorState = 1;  // Raise Doors
    led.begin(BEACON_LED).blink(100,200).trigger(led.EVT_START);  //Raising Blink-----   
    #ifdef DEBUG
    Serial.println("Motor State -> 1");    
    #endif
    
  } else if ((char)payload[0] == '2') {
    MotorState = 2;  //Lower Doors
    led.begin(BEACON_LED).blink(400,400).trigger(led.EVT_START);  //Lower Blink-----
    
    #ifdef DEBUG
    Serial.print("Motor State -> 2 ");
    #endif
       
  }
}

void reconnect() {
  //Tag the MCU with the ESO8266 chip id
  // Loop until we're reconnected
  char CHIP_ID[8];
  String(ESP.getChipId()).toCharArray(CHIP_ID,8);
  
  while (!client.connected()) {
    #ifdef DEBUG
    Serial.print("Attempting MQTT connection...");
    #endif
    // Attempt to connect
    if (client.connect(CHIP_ID)) {
      #ifdef DEBUG
      Serial.println("connected");
      #endif
      // Once connected, publish an announcement...
      char tag[11] = "Connected-";
      char msg[19]="";
      strcat(tag, CHIP_ID);
      strcat(msg,tag);
      client.publish(outTopic,msg);
      
      // ... and resubscribe
      client.subscribe(inTopic);
    } else {
      #ifdef DEBUG
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      #endif
      // Wait 5 seconds before retrying
      for(int i = 0; i<5000; i++){
        
       delay(1);
      }
    }
  }
}



void OnPositionChanged(){
  // check for value change
   if(newDoorPosition != oldDoorPosition){
      if(newDoorPosition==1){
      client.publish(outTopic, "3");
      EEPROM.write(0,newDoorPosition);
      EEPROM.commit();
      led.begin(BEACON_LED).blink(20,2000).trigger(led.EVT_START);
      #ifdef DEBUG
      Serial.println("-> Send 3");
      #endif
        }
      if(newDoorPosition==2){
        client.publish(outTopic, "4");
        EEPROM.write(0,newDoorPosition);
        EEPROM.commit();
        led.begin(BEACON_LED).blink(20,2000).trigger(led.EVT_START);
        #ifdef DEBUG
        Serial.println("-> Send 4");
        #endif
        }
              
      oldDoorPosition = newDoorPosition; 
   }
}

void setup() {

  Serial.begin(115200);
  Serial.println("Booting...");
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.println("Connection Failed! Rebooting...");
    delay(5000);
    ESP.restart();
  }
//-------------------------------------------------------------------------------------
//----------------------------ArduinoOTA Functions Block-------------------------------
  ArduinoOTA.onStart([]() {
    Serial.println("Start");
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  ArduinoOTA.begin();
//------------------------------------------------------------------------------------ 
  EEPROM.begin(512);              // Begin eeprom to store on/off state

  pinMode(BEACON_LED,OUTPUT);
  pinMode(STATUS_LED,OUTPUT);
  pinMode(12,OUTPUT);
  pinMode(PWMpin,OUTPUT);
  pinMode(DIRpin,OUTPUT);
  pinMode(0,OUTPUT);
 


  digitalWrite(LEDpin,LOW);
  digitalWrite(PWMpin,LOW);
  newDoorPosition = EEPROM.read(0);
 
  delay(500);
  #ifdef DEBUG
  Serial.begin(115200);
  #endif
 
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);

  //------------Program Code Here --------------------------------------
  led.begin(BEACON_LED).blink(30,2000);  //Set beacon time interval-----


  //--------------------------------------------------------------------
  
}

void loop() {
 ArduinoOTA.handle();
  
  
   if (!client.connected()) {
    reconnect();
  }
  client.loop();
  automaton.run();

  //-----------------Encoder Loop ---------------------
  newPosition = encoder.read();
  if (newPosition != oldPosition) {
    oldPosition = newPosition;
    #ifdef ENCODER_OUTPUT
    Serial.println(newPosition);
    #endif
  }

    
/// ---------------------State Table-----------------------------------------
  if(newPosition < HI_LIMIT && MotorState==1){Raise();}
  if(newPosition >= HI_LIMIT && MotorState==1){
    newDoorPosition=1;//Door is Raised
    MotorState=0;
    }
  
  if(newPosition > LO_LIMIT && MotorState==2){Lower();}
  if(newPosition <= LO_LIMIT && MotorState==2){
    newDoorPosition=2;//Door us Lowered
    MotorState=0;   
    }
    
  if(MotorState==0){Stop();} 

  OnPositionChanged();

  
}
///----------------------END-LOOP------------------------



//------------------------------------------------
////////////////// Motor Functions /////////////////////
//------------------------------------------------

void Stop(){
  digitalWrite(DIRpin,LOW);
  digitalWrite(PWMpin, LOW);
}
void Raise(){
  digitalWrite(DIRpin,HIGH);
  digitalWrite(PWMpin, HIGH);
}
void Lower(){
  digitalWrite(DIRpin,LOW);
  digitalWrite(PWMpin, HIGH);
}

  



