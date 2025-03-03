#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <math.h>
#include <OneWire.h>
#include "esp_bt.h"
#include "esp_wifi.h"
#include <esp_system.h>


#define uS_TO_S_FACTOR 1000000ULL  /* Conversion factor for micro seconds to seconds */
#define TIME_TO_SLEEP 180        /* Time ESP32 will go to sleep (in seconds) */
#define BUTTON_PIN_BITMASK 0x2000  // bitmask del pin que despierta

//**************************************
//*********** MQTT CONFIG **************
//**************************************

//const char* mqtt_server = "181.171.18.41";
const char* mqtt_server = "181.229.219.74";
const int mqtt_port = 1883;
const char* mqtt_user = "admin"; 
const char* mqtt_pass = "Agimed123456";
const char* root_topic_subscribe = "inTopic";
const char* root_topic_publish = "outTopic";

/*
const char* mqtt_server = "192.168.0.96";
const int mqtt_port = 1883;
const char* mqtt_user = "";
const char* mqtt_pass = "";
const char* root_topic_subscribe = "inTopic";
const char* root_topic_publish = "outTopic";
*/
//**************************************
//*********** WIFICONFIG ***************
//**************************************
//const char* ssid = "WIFI Agimed";
//const char* password = "Cullen5769!";
//const char* ssid = "Red_Link_Mateo_Felipe";
//const char* password = "cc88fee64D!$#SEGURA";
const char* ssid = "MauHogar 2.4GHz";
const char* password = "DorianElGris1986";


//**************************************
//*********** GLOBALES   ***************
//**************************************
WiFiClient espClient;
PubSubClient client(espClient);
char msg[25];
long count = 0;
OneWire ourWire(26);  //Se establece el pin 26  como bus OneWire Conectar al pin físico GPIO 26
//DallasTemperature sensors(&ourWire);       //Se declara una variable u objeto para nuestro sensor
const int BUTTON_PIN = 36;  // Define el pin del pulsador de la apertura de puerta
int BUZZER = 13;            // configuracion de la salida GPIO13 que se conecta al buzz de la alarma
long lastTime = 0;

RTC_DATA_ATTR int bootCount = 0;

int LED1 = 23;
int LED2 = 15;

struct Module {
  String id;
  String status_device = "normal";  // "normal" | "low_battery"
  String statuc_dea = "normal";     // "normal" | "open_door" | "low_battery"
  String statuc_alarma = "no_detecta" ; // "no_detecta" | "alarma_detectada"
  // float temperature; // in celcius
} module;


//************************
//** F U N C I O N E S ***
//**    prototipos     ***
//************************
void callback(char* topic, byte* payload, unsigned int length);
void reconnect();
void setup_wifi();
void setup_pins();       // Nueva función para configurar los pines GPIO
void verificarestado();  // verifica si la puerta ha sido abierta
void print_wakeup_reason();


void setup() {
  Serial.begin(115200);  // configuracion de velociodad de puerto serie para configurar el equipo
  //Serial.begin(9600);  // configuracion de velociodad de puerto serie para configurar el equipo
  setup_wifi();
  esp_bt_controller_disable();  // desabilitamos el bluetooth porque no lo usamos
  setup_pins();                 // Llama a la función para configurar los pines GPIO
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
  esp_sleep_enable_ext0_wakeup(GPIO_NUM_36, 1);  //1 = High, 0 = Low

  module.id = "id-123123";
  module.status_device = "normal";
  module.statuc_dea = "normal";
  module.statuc_alarma = "no_detecta";

}

void loop() {

  Serial.print("iniciando programa ");
  delay(500);

  // revisar estado de la puerta
  Serial.print("verifico puerta ");
  delay(500);
  // revisar estado de la bateria
  Serial.print("verifico bateria ");
  delay(500);
  // revisar estado de la alarma
Serial.print("revisando alarma sonora ");
  delay(500);


  if (!client.connected()) {
    reconnect();
  }// conectarse con wifi
  client.loop(); // conectarse con el cliente mosquito
  
  //envio resultados al servidor
  Serial.println("*** resultados de los chequeos  y se va a dormir ***");

  String str = module.id;
 Serial.println(str);
  str.toCharArray(msg, 25);
  client.publish(root_topic_publish, msg);

  str = module.status_device;
  Serial.println(str);
  str.toCharArray(msg, 25);
  client.publish(root_topic_publish, msg);

  str = module.statuc_dea;
  Serial.println(str);
  str.toCharArray(msg, 25);
  client.publish(root_topic_publish, msg);

  str = module.statuc_alarma;
  Serial.println(str);
  str.toCharArray(msg, 25);
  client.publish(root_topic_publish, msg);

  delay(500);

  Serial.println("cargando el ESp32 listo para dormir por " + String(TIME_TO_SLEEP) + " Seconds");
  delay(500);
  str = "se va a dormir ";
  str.toCharArray(msg, 25);
  client.publish(root_topic_publish, msg);
  esp_light_sleep_start();
  print_wakeup_reason();
}








//*****************************
//***    CONEXION WIFI      ***
//*****************************
void setup_wifi() {
  int i = 0;
  delay(10);
  // Nos conectamos a nuestra red Wifi
  Serial.println();
  Serial.print("Conectando a ssid: ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    if (i == 10) {  // si no se conecta en 10 intentos resetea el microcontrolador
      esp_restart();
    } else {
      delay(500);
      Serial.print(i);
      i++;
    }
  }
  i = 0;
  Serial.println("");
  Serial.println("Conectado a red WiFi!");
  Serial.println("Dirección IP: ");
  Serial.println(WiFi.localIP());
}

//*****************************
//***    CONEXION MQTT      ***
//*****************************

void reconnect() {
  int i = 0;
  while (!client.connected()) {
    if (i == 10) {  // si no se conecta en 10 intentos resetea el microcontrolador
      esp_restart();
    } else {
      Serial.print("Intentando conexión Mqtt...");
      // Creamos un cliente ID
      String clientId = "IOTICOS_H_W_";
      clientId += String(random(0xffff), HEX);
      // Intentamos conectar
      if (client.connect(clientId.c_str(), mqtt_user, mqtt_pass)) {
        Serial.println("Conectado!");
        // Nos suscribimos
        if (client.subscribe(root_topic_subscribe)) {
          Serial.println("Suscripcion ok");
          i = 0;
        } else {
          Serial.println("Fallo Suscripción");
          i++;
        }
      } else {
        Serial.print("Falló :( con error -> ");
        Serial.print(client.state());
        Serial.println(" Intentamos de nuevo en 5 segundos");
        delay(5000);
        i++;
      }
    }
  }
}


void print_wakeup_reason() {
  if (!client.connected()) {
    reconnect();
  }

  esp_sleep_wakeup_cause_t wakeup_reason;

  wakeup_reason = esp_sleep_get_wakeup_cause();
  String str;
  switch (wakeup_reason) {
    case ESP_SLEEP_WAKEUP_EXT0:
      str = "se desperto por la puerta";
      Serial.println(str);
      str.toCharArray(msg, 25);
      client.publish(root_topic_publish, msg);
      delay(500);
      break;
    case ESP_SLEEP_WAKEUP_TIMER:
      str = "se desperto por el timer";
      Serial.println(str);
      str.toCharArray(msg, 25);
      client.publish(root_topic_publish, msg);
      break;
    default: Serial.printf("Wakeup was not caused by deep sleep: %d\n", wakeup_reason); break;
  }
}

//*****************************
//***       CALLBACK        ***
//*****************************

void callback(char* topic, byte* payload, unsigned int length) {
  String incoming = "";
  Serial.print("Mensaje recibido desde -> ");
  Serial.print(topic);
  Serial.println("");

  for (int i = 0; i < length; i++) {
    incoming += (char)payload[i];
  }

  incoming.trim();
  Serial.println("Mensaje -> " + incoming);

  // Verificar si la cadena es numérica
  if (isNumeric(incoming)) {
    int mensaje = incoming.toInt();
    Serial.println("Mensaje convertido a entero -> " + String(mensaje));


    switch (mensaje) {
      case 45:
        esp_restart();
        break;
      case 70:
        // con esto se resetea la alarma que se abrio la puerta

        Serial.println("Reseteo la alarma de puerta");
        if (client.connected()) {
          String str = "Reseteo la alarma de puerta:";
          str.toCharArray(msg, 30);
          client.publish(root_topic_publish, msg);
        }
        break;
      case 250:
        // GUARDADO PARA FUTURAS FUNCIONES
        break;
      case 251:
        // GUARDADO PARA FUTURAS FUNCIONES
        break;
      case 26:

        //Serial.println("temperatura es: " + leer_temperatura() );
        //if (client.connected()) {
        //String str = "temperatura es: " + leer_temperatura();
        //String str = "temperatura es: ";
        //str.toCharArray(msg, 25);
        //client.publish(root_topic_publish, msg);
        //}

        break;
      default:
        // instructions par défaut
        break;
    }


  } else {
    Serial.println("El mensaje no es numérico.");
    // Puedes manejar el caso de un mensaje no numérico aquí
  }
}

bool isNumeric(String str) {
  // Verifica si una cadena es numérica
  for (int i = 0; i < str.length(); i++) {
    if (!isdigit(str.charAt(i))) {
      return false;
    }
  }
  return true;
}

//*****************************
//***    CONFIGURAR PINS   ***
//*****************************
void setup_pins() {
  pinMode(BUTTON_PIN, INPUT_PULLUP);  // Configura el pin del botón como entrada con resistencia de pull-up

  pinMode(BUZZER, OUTPUT);  // Define el pin 13 como salida para el LED integrado en la placa (puede variar según la placa)
  pinMode(LED1, OUTPUT);
  pinMode(LED2, OUTPUT);
  digitalWrite(LED1, LOW);  // Inicialmente, apagar todos los pines
  digitalWrite(LED2, LOW);  // Inicialmente, apagar todos los pines
}

//*****************************
//***  VERIFICAR PUERTA *****
//*****************************

