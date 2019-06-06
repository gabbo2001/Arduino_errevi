//Librerie
#include <SPI.h>
#include <Ethernet.h>
#include <PubSubClient.h>
#include <DS3231.h>
#include "DHT.h"

//Necessario per l'orario
DS3231 clock;
RTCDateTime dt;

//sensore di temperatura e umidita
#define DHTPIN 11
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

//led allarme, temp superiore alla soglia
#define PinLed 8

//imposta mac address
byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };

//imposta indirizzo ip
IPAddress ip(192, 168, 0, 177);

//imposta DNS
IPAddress myDns(192, 168, 0, 1);

//indirizzo del server
char server[] = "10.100.251.2";

//porta del server
int porta_server = 1883;

//id del client con il quale mi connetto
char id_client[] = "myClientId";

//topic al quale inviamo messaggi
char topic[] = "out_arduino_rv";

//topic dal quale riceviamo messaggi
char receiving_topic[] = "in_arduino_rv";

// Ethernet e MQTT
EthernetClient ethClient;
PubSubClient mqttClient(ethClient);

//soglia di temperatura
int soglia;

//potenziometro
int potPin = A0;
int val = 0;

//dichiaro variabili per temperatura e umidità
int t ,h;

//dichiaro costante soglia di umidità
const int SOGLIA_H = 10;

void sendMessage();

void setup()
{
  Serial.begin(9600);

  //CONNESSIONE
  mqttClient.setCallback(receivingMessagge);
  Serial.println("Initialize Ethernet with DHCP:");
  if(Ethernet.begin(mac) == 0)
  {
    Serial.println("Failed to configure Ethernet using DHCP");
    if (Ethernet.hardwareStatus() == EthernetNoHardware)
    {
      Serial.println("Ethernet shield was not found.  Sorry, can't run without hardware. :(");
      while (true)
      {
        delay(1);
      }
    }
    if (Ethernet.linkStatus() == LinkOFF)
    {
      Serial.println("Ethernet cable is not connected.");
    }
    //configurazione con IP statico e non DHCP
    Ethernet.begin(mac, ip, myDns);
  }
  else
  {
    Serial.print("  DHCP assigned IP ");
    Serial.println(Ethernet.localIP());
  }
  delay(1000);
  Serial.print("connecting to ");
  Serial.print(server);
  Serial.println("...");
  mqttClient.setServer(server, porta_server);   
 
  //Provo a connettermi con un id client
  if (mqttClient.connect(id_client, "admin", "admin123"))
  {
    Serial.println("Connection has been established, well done");    
  } 
  else 
  {
    Serial.println("Looks like the server connection failed...");
  }
  //Fine fase di connessione


  //Inizializzazione e setup orologio
  clock.begin();
  clock.setDateTime(__DATE__, __TIME__);

  //Setup iniziale led allarme
  pinMode(PinLed, OUTPUT);

  //Ci sottoscriviamo al topic (canale)
  mqttClient.subscribe(receiving_topic);
}

void loop()
{
  //Necessario all'inizio della fase di loop
  mqttClient.loop();

  //Funzione per inviare i messaggi sul topic inserito precedentemente
  sendingMisure();
}

boolean humidityControl(int h){
  if(h<(dht.readHumidity()-SOGLIA_H) || h>(dht.readHumidity()+SOGLIA_H))
    return true;
  else
    return false;
}

void sendingMisure(){
  if(t != dht.readTemperature() || humidityControl(h)){
    sendMessage();
  }
}
void receivingMessagge(char* local_topic, byte* payload, unsigned int length)
{
  if(String(local_topic) == String(receiving_topic))
  {    
    String sogliaString;
    for(int j = 0; j < length; j ++)
    {
      sogliaString += char (payload[j]);
    }
    soglia = sogliaString.toInt();
    sendMessage();
    Serial.print("Soglia:");
    Serial.println(soglia);
  }
}
void sendMessage(){
  //Sensore di temperatura e umidità
    t = dht.readTemperature();  //lettura temperatura
    h = dht.readHumidity();     //lettura umidità
    
    /* POTENZIOMETRO
    int t = map(analogRead(potPin), 0, 1023, 0, 40);
    int h = t;
    String payload;                 //stringa che verrà visualizzata come messaggio
    */
    
    dt = clock.getDateTime();       //creazione oggetto date_time (dt)
    
    if (isnan(h) || isnan(t))
    {
      Serial.println("Failed to read from DHT sensor!");
    }
    else
    {
      String temperature = String(t);
      String humidity = String(h);
      
      String payload = "{";
      payload += "\"t\":"; 
      payload += temperature;
      payload += ",";
      payload += "\"h\":";
      payload += humidity;
      payload += ",";
      payload += "\"sgl\":";
      payload += soglia;
      payload += ",";
      /*payload += "\"dgr\":";
      payload += "\"C°\",";*/
      //verifico che la temperatura sia maggiore della soglia
      if(t>soglia)
      {
        digitalWrite(PinLed,HIGH);  //accendo il led di allarme
      
        payload += "\"alr\":";
        payload += "\"on\", ";
      }
      else
      {
        digitalWrite(PinLed,LOW);   //spengo il led di allarme
      
        payload += "\"alr\":";
        payload += "\"off\", ";
      }
  
      //creo la variabile orario per creare data e ora in tempo reale
      String orario =
                   "\"d\":\"" +
                   String(dt.day) +
                   "/"  + String(dt.month) +
                   "/"  + String(dt.year) +
                   "\",\"hr\":\"" +
                   String(dt.hour) +
                   ":"  + String(dt.minute) +
                   ":"  + String(dt.second) +
                   "\"";
      payload += orario;
      payload += "}";
      Serial.println(payload);
      int nCaratteri = payload.length()+1;
      char buffer[nCaratteri];
      payload.toCharArray(buffer,nCaratteri);
  
      //pubblico il messaggio
      if(mqttClient.publish(topic, buffer))
      {
        Serial.println("Publish message success");
      }
      else
      {
        Serial.println("Could not send message :(");
      }
    }
}
