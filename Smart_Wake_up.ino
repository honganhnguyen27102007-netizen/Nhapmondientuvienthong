//millis() : get the time in miliseconds since the start of the device
#define BLYNK_PRINT Serial
#define BLYNK_TEMPLATE_ID "TMPL6uG2NY6z1"
#define BLYNK_TEMPLATE_NAME "test1"
#define BLYNK_AUTH_TOKEN  "OxlQZS9cGrxKGJuKl5kWHptMTkxwjIvS"
#define LED_PIN LED_BUILTIN //the pin connected to the LED
#include <BlynkSimpleEsp32.h>
#include <Wire.h>
#include "MAX30105.h"
#include "heartRate.h"
MAX30105 particleSensor;             // Đối tượng cảm biến
int motorPin = 13;
long thoi_gian_dap_truoc = 0;        // Lưu mốc thời gian của nhịp đập trước
int nhip_tim_trung_binh = 0;         // Kết quả cuối cùng hiển thị lên App
byte mang_nhip_tim[4] = {0, 0, 0, 0}; // Mảng lưu 4 nhịp gần nhất để tính trung bình

// Biến phục vụ logic ngủ nông
int nhip_tim_day = 200;              // Mốc nhịp tim thấp nhất (ngủ sâu)
int nguong_danh_thuc = 0;            // Mốc 115% nhịp tim để báo ngủ nông
int dem_so_lan_cao = 0;              // Đếm số lần tim đập nhanh liên tiếp
bool is_ngu_nong = false;            // Trạng thái xác nhận ngủ nông hay chưa
//
bool alarmEnabled = false;
bool alarmTriggered = false;
bool testTriggered=false;
unsigned long now;
 long remaining;
unsigned long waketime;//time cant be negative
int averagebpm;
BlynkTimer timer; 
String sleepstate;
void myTimer() 
{
  Blynk.virtualWrite(V2, alarmEnabled);  //update the working state of the alarm
  Blynk.virtualWrite(V3, averagebpm); //send the average bpm
  Blynk.virtualWrite(V1,sleepstate); //send the sleep state : deep/ narrow
}



BLYNK_WRITE(V0){ //event based , every time the slider is touched, we assign a new wakeup time
  double hoursinput =param.asDouble();
 waketime = millis() + hoursinput*60UL*60UL*1000UL;
 alarmEnabled = true;
 alarmTriggered=false;
 testTriggered = false;
}
BLYNK_WRITE(V4) { // cancel button
  int choice = param.asInt();

  if (choice == 1) {
    alarmEnabled   = false;
    alarmTriggered = false;
    testTriggered  = false;

    // auto reset switch to mimic push button
    Blynk.virtualWrite(V4, 0);
  }
}
void setup() {
Serial.begin(115200);
Blynk.begin(BLYNK_AUTH_TOKEN,"wifiname","wifipass");
pinMode(motorPin, OUTPUT);
  // put your setup code here, to run once:
  timer.setInterval(500L, myTimer);
    Wire.begin(); 
  if (!particleSensor.begin(Wire, I2C_SPEED_FAST)){// kiểm tra xem 2 chân nối day đãdđung chưa , không tìm thấy thì chạy dòng dưới  {

    Serial.println("LOI: Khong thay MAX30102!");
    while(1);// vòng lặp chạy vô hạn nếu lỗi , dễ kiểm tra ; 
  }
  particleSensor.setup();//  dùng để các thông số có sẵn trong hàmdđêddo ; 
  particleSensor.setPulseAmplitudeRed(0x0A);// thiết lập cường độ dòng điện led đỏ hợp lý ( vừa đủ xuyên qua da đo kết quả chính xác cũng như tiết kiệm pin , 0x0A là 2mA
  particleSensor.setPulseAmplitudeGreen(0);// tắt đèn xanh đi vì không dùng tới 

  Serial.println("He thong da san sang!");
}


void loop() {
Blynk.run();
timer.run();
    // --- PHẦN XỬ LÝ NHỊP TIM ---
    long gia_tri_hong_ngoai = particleSensor.getIR(); // Đọc dữ liệu thô từ cảm biến

    // Bước 1: Kiểm tra xem có áp tay vào cảm biến không
    if (gia_tri_hong_ngoai < 50000) { 
        nhip_tim_trung_binh = 0;
        is_ngu_nong = false;
        Serial.println("Chưa đặt tay vào cảm biến!");
    } 
    else {
        // Bước 2: Bắt sườn dốc sóng để tìm nhịp đập (beat)
        if (checkForBeat(gia_tri_hong_ngoai) == true) {
            
            // Tính thời gian (mili giây) giữa 2 nhịp đập gần nhất
            long thoi_gian_giua_2_nhip = millis() - thoi_gian_dap_truoc;
            thoi_gian_dap_truoc = millis();

            // Đổi từ mili giây sang giây (chia cho 1000)
            float so_giay = thoi_gian_giua_2_nhip / 1000.0;
            
            // Công thức tính Nhịp tim (BPM)
            float bpm_tuc_thoi = 60 / so_giay;

            // Lọc nhiễu: Chỉ lấy nhịp tim từ 40 đến 150
            if (bpm_tuc_thoi > 40 && bpm_tuc_thoi < 150) {
                
                // Kỹ thuật dịch chuyển mảng để tính trung bình mượt hơn
                mang_nhip_tim[0] = mang_nhip_tim[1];
                mang_nhip_tim[1] = mang_nhip_tim[2];
                mang_nhip_tim[2] = mang_nhip_tim[3];
                mang_nhip_tim[3] = (byte)bpm_tuc_thoi; // Ép kiểu float về byte cho nhẹ máy

                // Tính tổng 4 nhịp gần nhất bằng vòng lặp
                long tong_nhip_tim = 0;
                for (int i = 0; i < 4; i++) {
                    tong_nhip_tim = tong_nhip_tim + mang_nhip_tim[i];
                }
                
                // Kết quả nhịp tim trung bình cuối cùng
                nhip_tim_trung_binh = tong_nhip_tim / 4;
                averagebpm = nhip_tim_trung_binh;
                // Tự động tìm "Đáy" nhịp tim lúc ngủ sâu nhất
                if (nhip_tim_trung_binh < nhip_tim_day) {
                    nhip_tim_day = nhip_tim_trung_binh;
                    // Ngưỡng báo thức = Nhịp tim đáy + 15% của nó
nguong_danh_thuc = nhip_tim_day + (nhip_tim_day * 0.15);
                }
            }
        }

        // Bước 3: Logic xác nhận trạng thái ngủ nông
        if (nhip_tim_trung_binh >= nguong_danh_thuc && nhip_tim_trung_binh > 40) {
            dem_so_lan_cao = dem_so_lan_cao + 1; // Tăng đếm nếu tim đập nhanh
        } else {
            dem_so_lan_cao = 0; // Reset đếm nếu tim đập bình thường
        }

        // Xác nhận ngủ nông nếu nhịp tim cao liên tục trong 5 lần đo
        if (dem_so_lan_cao >= 5) {
            is_ngu_nong = true;
            sleepstate = "Narrow";
        } else {
            is_ngu_nong = false;
            sleepstate = "Deep";
        }
}
  // put your main code here, to run repeatedly:
if(!alarmEnabled){
    digitalWrite(motorPin, LOW);
    return;}
now = millis();
remaining = long(waketime - now);
if(remaining <= 15UL*60UL*1000UL && !testTriggered){
  if(is_ngu_nong){
    
  testTriggered=true;
  alarmTriggered=true;//wake up immediately after testing
  digitalWrite(motorPin, HIGH);
}
}
if(remaining <=0 && alarmTriggered == false){
  //send code to the waking up device
  alarmTriggered = true;
  alarmEnabled = false; //turn off the device after each session  
  digitalWrite(motorPin, HIGH);
}
}