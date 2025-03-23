#include <FreeRTOS.h>
#include <task.h>
#include <semphr.h>
#include <queue.h>
#include "ssd1306.h"
#include "gfx.h"
#include "pico/stdlib.h"
#include <stdio.h>

const int ECHO_PIN = 16;
const int TRIGGER_PIN = 17;
const int TRIGGER_PULSE_US = 10;

QueueHandle_t xQueueTime;
QueueHandle_t xQueueDistance;
SemaphoreHandle_t xSemaphoreTrigger;

void echo_pin_callback(uint gpio, uint32_t events) {
    if (gpio == ECHO_PIN) {
        if (events & GPIO_IRQ_EDGE_RISE) {
            absolute_time_t echo_start_time = to_us_since_boot(get_absolute_time());
        } else if (events & GPIO_IRQ_EDGE_FALL) {
            absolute_time_t echo_end_time = to_us_since_boot(get_absolute_time());
            absolute_time_t echo_time = absolute_time_diff_us(echo_start_time, echo_end_time);

            xSemaphoreGiveFromISR(xSemaphoreTrigger, NULL);
            xQueueSendFromISR(xQueueTime, &echo_time, NULL);
        }
    }
}

void trigger_task(void *p) {
    while (true) {
        gpio_put(TRIGGER_PIN, 1);
        vTaskDelay(pdMS_TO_TICKS(TRIGGER_PULSE_US/1000));
        gpio_put(TRIGGER_PIN, 0);
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void echo_task(void *p) {
    absolute_time_t received_echo_time;
    float distancia;
    while (true) {
        if (xQueueReceive(xQueueTime, &received_echo_time, 0)) {
            distancia = (received_echo_time * 0.0343) / 2.0;
            xQueueSend(xQueueDistance, &distancia, 0);
        }
    }
}

void oled_task(void *p) {
    printf("Inicializando Driver\n");
    ssd1306_init();

    ssd1306_t disp;
    gfx_init(&disp, 128, 32);

    char buffer[16];
    float distancia;

    while (true) {
        if (xSemaphoreTake(xSemaphoreTrigger, pdMS_TO_TICKS(500)) == pdTRUE) {
            if (xQueueReceive(xQueueDistance, &distancia, 0)) {
                sprintf(buffer, "%.2f cm", distancia);
                gfx_clear_buffer(&disp);
                int line_size = (int) distancia*127/400;
    
                if (distancia < 2 || distancia > 400) {
                    gfx_draw_string(&disp, 0, 0, 1, "Falha");
                } else {
                    gfx_draw_string(&disp, 0, 0, 1, "Distancia:");
                    gfx_draw_string(&disp, 0, 10, 1, buffer);
                    gfx_draw_line(&disp, 0, 27, line_size, 27);
                }
    
                gfx_show(&disp);
            }
        }
    }
}

int main() {
    stdio_init_all();

    gpio_init(ECHO_PIN);
    gpio_init(TRIGGER_PIN);
    gpio_set_dir(ECHO_PIN, GPIO_IN);
    gpio_set_dir(TRIGGER_PIN, GPIO_OUT);
    gpio_put(TRIGGER_PIN, 0);

    gpio_set_irq_enabled_with_callback(ECHO_PIN, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true, &echo_pin_callback);

    xQueueTime = xQueueCreate(32, sizeof(absolute_time_t));
    xQueueDistance = xQueueCreate(32, sizeof(float));
    xSemaphoreTrigger = xSemaphoreCreateBinary();

    xTaskCreate(trigger_task, "Trigger Task", 512, NULL, 1, NULL);
    xTaskCreate(echo_task, "Echo Task", 512, NULL, 1, NULL);
    xTaskCreate(oled_task, "OLED Task", 512, NULL, 1, NULL);

    vTaskStartScheduler();

    while (true)
        ;
}


// void oled1_btn_led_init(void) {
//     gpio_init(LED_1_OLED);
//     gpio_set_dir(LED_1_OLED, GPIO_OUT);

//     gpio_init(LED_2_OLED);
//     gpio_set_dir(LED_2_OLED, GPIO_OUT);

//     gpio_init(LED_3_OLED);
//     gpio_set_dir(LED_3_OLED, GPIO_OUT);

//     gpio_init(BTN_1_OLED);
//     gpio_set_dir(BTN_1_OLED, GPIO_IN);
//     gpio_pull_up(BTN_1_OLED);

//     gpio_init(BTN_2_OLED);
//     gpio_set_dir(BTN_2_OLED, GPIO_IN);
//     gpio_pull_up(BTN_2_OLED);

//     gpio_init(BTN_3_OLED);
//     gpio_set_dir(BTN_3_OLED, GPIO_IN);
//     gpio_pull_up(BTN_3_OLED);
// }

// void oled1_demo_1(void *p) {
//     printf("Inicializando Driver\n");
//     ssd1306_init();

//     printf("Inicializando GLX\n");
//     ssd1306_t disp;
//     gfx_init(&disp, 128, 32);

//     printf("Inicializando btn and LEDs\n");
//     oled1_btn_led_init();

//     char cnt = 15;
//     while (1) {

//         if (gpio_get(BTN_1_OLED) == 0) {
//             cnt = 15;
//             gpio_put(LED_1_OLED, 0);
//             gfx_clear_buffer(&disp);
//             gfx_draw_string(&disp, 0, 0, 1, "LED 1 - ON");
//             gfx_show(&disp);
//         } else if (gpio_get(BTN_2_OLED) == 0) {
//             cnt = 15;
//             gpio_put(LED_2_OLED, 0);
//             gfx_clear_buffer(&disp);
//             gfx_draw_string(&disp, 0, 0, 1, "LED 2 - ON");
//             gfx_show(&disp);
//         } else if (gpio_get(BTN_3_OLED) == 0) {
//             cnt = 15;
//             gpio_put(LED_3_OLED, 0);
//             gfx_clear_buffer(&disp);
//             gfx_draw_string(&disp, 0, 0, 1, "LED 3 - ON");
//             gfx_show(&disp);
//         } else {

//             gpio_put(LED_1_OLED, 1);
//             gpio_put(LED_2_OLED, 1);
//             gpio_put(LED_3_OLED, 1);
//             gfx_clear_buffer(&disp);
//             gfx_draw_string(&disp, 0, 0, 1, "PRESSIONE ALGUM");
//             gfx_draw_string(&disp, 0, 10, 1, "BOTAO");
//             gfx_draw_line(&disp, 15, 27, cnt,
//                           27);
//             vTaskDelay(pdMS_TO_TICKS(50));
//             if (++cnt == 112)
//                 cnt = 15;

//             gfx_show(&disp);
//         }
//     }
// }

// void oled1_demo_2(void *p) {
//     printf("Inicializando Driver\n");
//     ssd1306_init();

//     printf("Inicializando GLX\n");
//     ssd1306_t disp;
//     gfx_init(&disp, 128, 32);

//     printf("Inicializando btn and LEDs\n");
//     oled1_btn_led_init();

//     char cnt = 15;
//     while (1) {

//         gfx_clear_buffer(&disp);
//         gfx_draw_string(&disp, 0, 0, 1, "Mandioca");
//         gfx_show(&disp);
//         vTaskDelay(pdMS_TO_TICKS(150));

//         gfx_clear_buffer(&disp);
//         gfx_draw_string(&disp, 0, 0, 2, "Batata");
//         gfx_show(&disp);
//         vTaskDelay(pdMS_TO_TICKS(150));

//         gfx_clear_buffer(&disp);
//         gfx_draw_string(&disp, 0, 0, 4, "Inhame");
//         gfx_show(&disp);
//         vTaskDelay(pdMS_TO_TICKS(150));
//     }
// }

// int main() {
//     stdio_init_all();
    
//     xTaskCreate(oled1_demo_2, "Demo 2", 4095, NULL, 1, NULL);
    
//     vTaskStartScheduler();
    
//     while (true)
//     ;
// }