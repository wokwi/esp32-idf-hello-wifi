/*|------------------------------------------------------------------------|*/
/*|WiFi connection in STA mode for ESP32 under ESP-IDF - Wokwi simulator   |*/
/*|Edited by: martinius96                                                  |*/
/*|Buy me a coffee at: paypal.me/chlebovec for more examples               |*/
/*|Tested under ESP-IDF v4.4.1                                             |*/
/*|------------------------------------------------------------------------|*/

#include <string.h>
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "lwip/err.h"
#include "lwip/sys.h"

// Status LED
#define LED_RED GPIO_NUM_2

// WiFi parameters
#define WIFI_SSID CONFIG_ESP_WIFI_SSID
#define WIFI_PASS CONFIG_ESP_WIFI_PASSWORD

// Event group
static EventGroupHandle_t wifi_event_group;
const int CONNECTED_BIT = BIT0;

// WiFi event handler
static esp_err_t event_handler(void *ctx, system_event_t *event)
{
  switch (event->event_id) {

    case SYSTEM_EVENT_STA_START:
      esp_wifi_connect();
      break;

    case SYSTEM_EVENT_STA_GOT_IP:
      xEventGroupSetBits(wifi_event_group, CONNECTED_BIT);
      break;

    case SYSTEM_EVENT_STA_DISCONNECTED:
      xEventGroupClearBits(wifi_event_group, CONNECTED_BIT);
      break;

    default:
      break;
  }

  return ESP_OK;
}

void led_config()
{
    gpio_reset_pin(LED_RED);
    gpio_set_direction(LED_RED, GPIO_MODE_OUTPUT);
}

void led_task(void *pvParameter)
{
  while (1) {
    if (xEventGroupGetBits(wifi_event_group) & CONNECTED_BIT) {
      // We are connected - LED on
      gpio_set_level(LED_RED, 1);
      vTaskDelay(200 / portTICK_RATE_MS);
    } else {
      // We are connecting - blink fast
      gpio_set_level(LED_RED, 0);
      vTaskDelay(200 / portTICK_RATE_MS);
      gpio_set_level(LED_RED, 1);
      vTaskDelay(200 / portTICK_RATE_MS);
    }
  }
}

// Main task
void main_task(void *pvParameter)
{
  tcpip_adapter_ip_info_t ip_info;
  // wait for connection
  printf("Waiting for connection to the WiFi network... \n");
  xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT, false, true, portMAX_DELAY);
  printf("Connected!\n");

  // print the local IP address
  ESP_ERROR_CHECK(tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_STA, &ip_info));
  printf("IP Address:  %s\n", ip4addr_ntoa(&ip_info.ip));
  printf("Subnet mask: %s\n", ip4addr_ntoa(&ip_info.netmask));
  printf("Gateway:     %s\n", ip4addr_ntoa(&ip_info.gw));
  printf("You can connect now to any webserver online! :-)\n");

  while (1) {
    vTaskDelay(1000 / portTICK_RATE_MS);
  }
}


void app_main()
{
  led_config();

  printf("ESP-IDF version used: %s\n", IDF_VER);

  // disable the default wifi logging
  esp_log_level_set("wifi", ESP_LOG_NONE);

  // initialize NVS
  ESP_ERROR_CHECK(nvs_flash_init());

  // create the event group to handle wifi events
  wifi_event_group = xEventGroupCreate();

  // The LED task is used to show the connection status
  xTaskCreate(&led_task, "led_task", 2048, NULL, 5, NULL);

  // initialize the tcp stack
  tcpip_adapter_init();

  // initialize the wifi event handler
  ESP_ERROR_CHECK(esp_event_loop_init(event_handler, NULL));

  // initialize the wifi stack in STAtion mode with config in RAM
  wifi_init_config_t wifi_init_config = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&wifi_init_config));
  ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));

  // configure the wifi connection and start the interface
  wifi_config_t wifi_config = {
    .sta = {
      .ssid = WIFI_SSID,
      .password = WIFI_PASS,
    },
  };

  // start the main task
  xTaskCreate(&main_task, "main_task", 2048, NULL, 5, NULL);
  ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
  ESP_ERROR_CHECK(esp_wifi_start());
  printf("Connecting to %s...\n", WIFI_SSID);
}
