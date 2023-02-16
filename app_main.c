#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "esp_wifi.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_wpa2.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"

#include "lwip/sockets.h"
#include "lwip/dns.h"
#include "lwip/netdb.h"
#include "lwip/sys.h"

#include "esp_log.h"
#include <driver/adc.h>

#define PORT_NUMBER 8888
#define BUF_SIZE (1024)

#define TCPSERVER "192.168.251.170"

char buff[50];
static const char *TAG = "Exemplo";

/*Bloco que define o status da Conexão Wifi*/
static void wifi_event_handler(void *event_handler_arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    switch (event_id)
    {
    case WIFI_EVENT_STA_START:
        printf("WiFi Conectando ... \n");
        break;
    case WIFI_EVENT_STA_CONNECTED:
        printf("WiFi Conectado ... \n");
        break;
    case WIFI_EVENT_STA_DISCONNECTED:
        printf("WiFi Conexão Perdida ... \n");
        break;
    case IP_EVENT_STA_GOT_IP:
        printf("WiFi recebendo IP ... \n\n");
        break;
    default:
        break;
    }
}

//Conexão WiFI
void wifi_connection()
{
    esp_netif_init();                    // TCP/IP Inicialização 			
    esp_event_loop_create_default();     // evento loop 			                
    esp_netif_create_default_wifi_sta(); // WiFi station 	                    
    wifi_init_config_t wifi_initiation = WIFI_INIT_CONFIG_DEFAULT(); //configuração default do wifi
    esp_wifi_init(&wifi_initiation); //Inicializa o wifi

    esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler, NULL);
    esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_event_handler, NULL);
    
    //Configuração da rede de acesso
    wifi_config_t wifi_configuration = {
        .sta = {
            .ssid = " ---",
            .password = " ---"}};
    
    esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_configuration); //set a configuração
    esp_wifi_start(); 
    esp_wifi_connect(); //Conecta
}

void tcp_client(void *pvParam){
    //LOG
    ESP_LOGI(TAG,"tcp_client task Inicializada \n");

    //Configuração do socket
    struct sockaddr_in tcpServerAddr;
    tcpServerAddr.sin_addr.s_addr = inet_addr(TCPSERVER);
    tcpServerAddr.sin_family = AF_INET;
    tcpServerAddr.sin_port = htons(8888);
    int s, r;
    char recv_buf[64];

    while(1){
        //Leitura de novos dados
        readSensor();

        //Inicialização do Socket
        s = socket(AF_INET, SOCK_STREAM, 0);
        if(s < 0) {
            ESP_LOGE(TAG, "... Falha em alocacao do socket.\n");
            vTaskDelay(1000 / portTICK_PERIOD_MS);
            continue;
        }
        ESP_LOGI(TAG, "...  socket ok!\n");
        //conecta no socket
         if(connect(s, (struct sockaddr *)&tcpServerAddr, sizeof(tcpServerAddr)) != 0) {
            ESP_LOGE(TAG, "... falha na conexao do socket, erro n=%d \n", errno);
            close(s); //fechamento do socket
            vTaskDelay(4000 / portTICK_PERIOD_MS);
            continue;
        }
        ESP_LOGI(TAG, "... Conectado \n");
        
        if( write(s , buff , strlen(buff)) < 0) //Envia o dado de Temperatura pelo socket
        {
            ESP_LOGE(TAG, "... Falha no envio \n");
            close(s);
            vTaskDelay(4000 / portTICK_PERIOD_MS);
            continue;
        }
        ESP_LOGI(TAG, "... Mensagem enviada com sucesso");
        bzero(recv_buf, sizeof(recv_buf));         //Zera o buffer
        r = read(s, recv_buf, sizeof(recv_buf)-1); //Leitura do socket
        printf("Recebido: %s", recv_buf);   

        close(s);
        ESP_LOGI(TAG, "... nova requisicao em 5 seconds");
        vTaskDelay(10000 / portTICK_PERIOD_MS);
    }
    ESP_LOGI(TAG, "...task tcp_client fechada\n");
}

void readSensor(){
    // Função de leitura do valor analogico, passando o ADC1 que é o do GPIO1
    int sensorRead = 0;
    for(int i=0; i<10; i++){
        sensorRead += adc1_get_raw(ADC1_CHANNEL_0);
    }
    float sensorValue = sensorRead/10;

    //Conversao do valor analogico para temperatura
    float tensao = (sensorValue * 3.3) / 4096;
    float temperatura = tensao / 0.010; // dividimos a tensão por 0.010 que representa os 10 mV

    sprintf(buff, "Temperatura = %f", temperatura);

    // Imprimindo valores lidos
    printf("Valor do lm35: %d\n", sensorValue);
    printf("Temperatura: %f\n", temperatura);

}
void app_main(void)
{
    nvs_flash_init(); 
    wifi_connection(); //conexao wifi

    vTaskDelay(2000 / portTICK_PERIOD_MS);
    printf("WIFI está conectado ...........\n");

    // Configurando a resolução do ADC para 12bits        
    adc1_config_width(ADC_WIDTH_BIT_12);
    // Configurando o Channel do ADC para o Channel 0
    adc1_config_channel_atten(ADC1_CHANNEL_0,ADC_ATTEN_DB_11);

    readSensor();

    //criando uma task para o socket
    xTaskCreatePinnedToCore(tcp_client,"tcp_client",8192,NULL,10,NULL,0);

}
