/* 
 *  Technical information:
 *  Node Mcu pins:
    static const uint8_t D0   = 16;
    static const uint8_t D1   = 5;
    static const uint8_t D2   = 4;
    static const uint8_t D3   = 0;
    static const uint8_t D4   = 2;
    static const uint8_t D5   = 14;
    static const uint8_t D6   = 12;
    static const uint8_t D7   = 13;
    static const uint8_t D8   = 15;
    static const uint8_t D9   = 3;
    static const uint8_t D10  = 1;
*/

//Для работы с Wi-Fi библиотеки
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
//Для работы с датчиком температуры "DS18B20"
#include <OneWire.h>
#include <DallasTemperature.h>

// ---------- НАСТРОЙКА СИСТЕМЫ --------------
const char* ssid = "name"; //Имя Wi-Fi cети
const char* password = "password"; //Пароль для Wi-Fi сети
String id_device = "example"; //ID device, которое было указано при регистрации пользователя
int mintime = 5; //Время включения фитоламп(МСК), световой день не более 14 часов!
int maxtime = 19; //Время отключения фитоламп(МСК, световой день не более 14 часов!
int timepump = 120; //Время работы помпы в секундах
// ---------- НАСТРОЙКА СИСТЕМЫ --------------

// ---------- СИСТЕМНЫЕ НАСТРОЙКИ ------------
int pinPump = 15; //Пин с помпой
int pinRelay = 16; //Пин с реле
int pinMultA = 14; //Пин для мультиплексора A
int pinMultB = 12; //Пин для мультиплексора B
int pinMultC = 13; //Пин для мультиплексора C
int rgb_blue = 0; //Пин для управления голубым каналом RGB-ленты
int rgb_red = 4; //Пин для управления красным каналом RGB-ленты
int rgb_green = 5; //Пин для управления зелёным каналом RGB-ленты
int ds18b20 = 2; //Пин с датчиками DS18B20
byte levels = 2; //Количество уровней(этажей) на ферме
boolean work_led = false; //Состояние ламп(работают или нет).

int groundisdry = 0; //Значение с гигрометра при сухой почве(min)
int groundishum = 450; //Значение с гигрометра при влажной почве(max)

int groundishum_un = 250; //Значение с гигрометра при универсальной влажной почве(max)
int dryorhum = groundishum - groundisdry; //Для перевода значения влажности в проценты
int dryorhum_un = groundishum_un - groundisdry; //Для перевода значения влажности в проценты


boolean debug = true; //Функция отладки для разработчиков, информация выводится в сериал-порт(115200)
// ---------- СИСТЕМНЫЕ НАСТРОЙКИ ------------

// ---------- НАСТРОЙКИ ДАТЧИКОВ ------------
OneWire oneWire(ds18b20); //Пин с датчиками "DS18B20"
DallasTemperature ds(&oneWire);
// ---------- НАСТРОЙКИ ДАТЧИКОВ ------------


void setup () {
  //Инициализируем пины
  pinMode(pinPump, OUTPUT); 
  pinMode(pinRelay, OUTPUT);
  pinMode(pinMultA, OUTPUT);
  pinMode(pinMultB, OUTPUT);
  pinMode(pinMultC, OUTPUT);
  pinMode(rgb_blue, OUTPUT);
  pinMode(rgb_red, OUTPUT);
  pinMode(rgb_green, OUTPUT);
  pinMode(ds18b20, OUTPUT);

  //Инициализируем датчики температуры
  ds.begin(); 

  Serial.begin(115200); //Для отладки настраиваем сериал-порт, скорость 115200.
  
  //Отключимся от сети, которая в памяти.
  WiFi.disconnect();
  delay(10);
  //Подключаемся к сети Wi-Fi
  WiFi.begin(ssid, password);
  if(debug) Serial.println("Connecting...");
  while (WiFi.status() != WL_CONNECTED) {
    delay(5000);
    if(debug) Serial.print(".");
  }
  if(debug) Serial.println("Connected!");
  if(debug) Serial.println(WiFi.localIP());
  delay(3000);
}

//Функция запрашивает текущее время на сервере для управления фитолампами
//Если возвращается время в промежутке от 6 утра до 20 вечера, то фитолампы работают.
//Если время > 20 вечера и < 6 утра, то фитолампы отключены.
int timenow() {
  int numb = 0;
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;  
    http.begin("http://adscity.ru/scripts/time.php");
    int httpCode = http.GET();                                                                 
    if (httpCode > 0) { 
      String payload = http.getString();   
      int str_len = payload.length() + 1;
      char char_array[str_len];
      payload.toCharArray(char_array, str_len);
      numb = atoi (char_array);
      Serial.println(payload);                     
    }
    http.end();   
  }
  return numb;
}

String timepout() {
  String answer = "";
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;  
    http.begin("http://adscity.ru/scripts/timepout.php");
    int httpCode = http.GET();                                                                 
    if (httpCode > 0) { 
      String payload = http.getString();  
      int str_len = payload.length() + 1;
      char char_array[str_len];
      payload.toCharArray(char_array, str_len); 
      answer = payload;
      if(debug) {
        Serial.print("Time pount = ");
        Serial.println(payload);                 
      }
    }
    http.end();   
  }
  return answer;
}

//Функция отправляет данные на сервер
void senddata(int hums[], double temperatures[], int error) {
  //Если соединение с Wi-Fi имеется, то мы начинаем сбор данных
  /*Пример GET-запроса
    *http://adscity.ru/scripts/sendfromfarm.php?device_id="example"&temp1="22.56"
    *&temp2="23.56"&temp3="24.56"&temp4="25.56"&temp5="22.00"&hum1="97"&hum2="98"
    *&hum3="99"&hum4="100"&hum5="100"&error1=0&error2=0&error3=0
  */
  if(WiFi.status() == WL_CONNECTED) {
    String data = "http://adscity.ru/scripts/sendfromfarm.php?device_id=\"";
    data += id_device;
    data += "\"";
    for(int i = 0; i < 5; i++) {
      data += "&temp";
      data += i + 1;
      data += "=";
      data += temperatures[i];
    }
    for(int i = 0; i < 5; i++) {
      data += "&hum";
      data += i + 1;
      data += "=";
      data += hums[i];
    }
    data += "&error1=";
    data += error;
    if(debug) {
      Serial.println("DATA: ");
      Serial.println(data);
    }
    //Отправляем данные на сервер
    HTTPClient http;
    http.begin(data);
    int httpCode = http.GET();    
    //Должны получить "OK";                                                             
    if (httpCode > 0) { 
      String payload = http.getString();   
      int str_len = payload.length() + 1;
      char char_array[str_len];
      payload.toCharArray(char_array, str_len);
      Serial.println(payload);                     
    }
    http.end(); 
  }
}
 
void loop() {
  // ------- БЛОК УПРАВЛЕНИЯ ФИТОЛАМПАМИ -------
  //Запрашиваем время на сервере
  int restime = timenow();
  //Включаем(Выключаем) фитолампы в зависимости от времени
  if(restime >= mintime && restime <= maxtime) {
    if(work_led == false) {
      digitalWrite(pinRelay, HIGH);
      Serial.println("LIGHT ON!");
      work_led = true;
      int r, g, b;
      for(int i = 0; i < 139; i++) {
        analogWrite(rgb_red, i);
        delay(30);
      }
      for(int i = 0; i < 255; i++) {
        analogWrite(rgb_blue, i);
        delay(30);
      }
    }
  } else {
    if(work_led) {
      digitalWrite(pinRelay, LOW);
      Serial.println("LIGHT OFF!");
      work_led = false;
    }
    if(debug) {
      Serial.print("Time from server - ");
      Serial.println(restime);
    }
  }
  // ------- БЛОК УПРАВЛЕНИЯ ФИТОЛАМПАМИ -------

  // ------- БЛОК СБОРА И ОТПРАВКИ ДАННЫХ НА СЕРВЕР -------
  //Собираем все данные о температуре.
  double temperatures[5];
  ds.requestTemperatures(); //Обновляем все температуры
  for(int i = 0; i < 5; i++) {
    if(i < levels) {
      double temp = ds.getTempCByIndex(i);
      if(temp != -127.00) temperatures[i] = temp;
      else temperatures[i] = 0; 
    }
    else {
      temperatures[i] = 1; //Если у нас нет уровня, где находится этот датчик, то мы присваиваем это значение
    }
    
    if(debug) {
        Serial.print("Temperature ");
        Serial.print(i);
        Serial.print(" - ");
        Serial.println(temperatures[i]); 
      }
  }

  //Собираем все данные о влажности.
  int hums[5];
  int hum = 0;
  
  //Считываем с 1 датчика
  digitalWrite(pinMultA, bitRead(0b000, 0));
  digitalWrite(pinMultB, bitRead(0b000, 1));
  digitalWrite(pinMultC, bitRead(0b000, 2));
  delay(1500);
  hum = analogRead(A0);
    float humf = analogRead(A0);
    Serial.print("humf 1 analog = ");
    Serial.println(analogRead(A0));
    humf = ((humf - groundisdry) /  dryorhum ) * 100;
    humf = abs(humf);
    if(humf < 0.00) humf = 0.00;
    if(humf > 100.00) humf = 100.00;
    hum = (int) humf;
    hums[0] = hum;  
  
  if(debug) {
      Serial.print("Hum 1 = ");
      Serial.println(hums[0]);
  }

  //Считываем с 2 датчика
  digitalWrite(pinMultA, bitRead(0b010, 0));
  digitalWrite(pinMultB, bitRead(0b010, 1));
  digitalWrite(pinMultC, bitRead(0b010, 2));
  delay(1500);
  hum = analogRead(A0);
  
  humf = analogRead(A0);
  Serial.print("humf 2 analog = ");
  Serial.println(analogRead(A0));
  humf = ((humf - groundisdry) /  dryorhum_un ) * 100;
  humf = abs(humf);
  if(humf < 0.00) humf = 0.00;
  if(humf > 100.00) humf = 100.00;
  hum = (int) humf;
  hums[1] = hum;  
  
  if(debug) {
      Serial.print("Hum 2  = ");
      Serial.println(hums[1]);
  }
  
  //Если датчиков нет, то присваеваем 1
  for(int i = 2; i < 5; i++) {
    hums[i] = -1;
    if(debug) {
      Serial.print("Hum ");
      Serial.print(i);
      Serial.print(" = ");
      Serial.println(hums[i]);
    }
  }

  int errorpump = 0;
  // ----------- УПРАВЛЕНИЕ ПОМПОЙ -----------
  //Если почва сухая, то включаем насос иначе идем дальше.
  if(hums[1] <= groundisdry || hums[0] <= groundisdry && timepout() == "yes") {
  //Тестим на наличие ошибок
  int beforehumtest = 0;
  digitalWrite(pinMultA, bitRead(0b010, 0));
  digitalWrite(pinMultB, bitRead(0b010, 1));
  digitalWrite(pinMultC, bitRead(0b010, 2));
  delay(500);
  beforehumtest = analogRead(A0);
     
  digitalWrite(pinPump, HIGH);
  if(debug) Serial.println("PUMP WORK!");
  delay(timepump * 1000);
  digitalWrite(pinPump, LOW);
  if(debug) Serial.println("END THE WORK PUMP!");

  //Ждём пока вода впитается в почву
  delay(1000);
  //Считываем влагу, чтобы определить были ли политы грядки
  int afterhumtest = 0;

  digitalWrite(pinMultA, bitRead(0b010, 0));
  digitalWrite(pinMultB, bitRead(0b010, 1));
  digitalWrite(pinMultC, bitRead(0b010, 2));
  delay(500);
  afterhumtest = analogRead(A0);

  if(!(afterhumtest > beforehumtest)) errorpump = 1;
   if(debug) {
      Serial.print("Error pump - ");
      Serial.println(errorpump);
    }
  }
  // ----------- КОНЕЦ УПРАВЛЕНИЯ ПОМПОЙ -----------

  //Начинаем собирать данные для отправки на сервер.
  senddata(hums, temperatures, errorpump);
  
  // ------- БЛОК СБОРА И ОТПРАВКИ ДАННЫХ НА СЕРВЕР -------

  delay(1800000);
}
 
