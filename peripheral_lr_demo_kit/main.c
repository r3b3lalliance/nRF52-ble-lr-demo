/**
 * Copyright (c) 2016 - 2018, Nordic Semiconductor ASA
 * 
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 * 
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 * 
 * 2. Redistributions in binary form, except as embedded into a Nordic
 *    Semiconductor ASA integrated circuit in a product or a software update for
 *    such product, must reproduce the above copyright notice, this list of
 *    conditions and the following disclaimer in the documentation and/or other
 *    materials provided with the distribution.
 * 
 * 3. Neither the name of Nordic Semiconductor ASA nor the names of its
 *    contributors may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 * 
 * 4. This software, with or without modification, must only be used with a
 *    Nordic Semiconductor ASA integrated circuit.
 * 
 * 5. Any software provided in binary form under this license must not be reverse
 *    engineered, decompiled, modified and/or disassembled.
 * 
 * THIS SOFTWARE IS PROVIDED BY NORDIC SEMICONDUCTOR ASA "AS IS" AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY, NONINFRINGEMENT, AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL NORDIC SEMICONDUCTOR ASA OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * 
 */
/**@cond To Make Doxygen skip documentation generation for this file.
 * @{
 */

#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>

//#include "amt.h"
//#include "counter.h"

#include "sdk_config.h"
#include "nrf.h"
#include "ble.h"
#include "ble_gatt.h"
#include "ble_hci.h"
#include "nordic_common.h"
#include "nrf_gpio.h"
#include "bsp_btn_ble.h"
#include "ble_advdata.h"
#include "ble_srv_common.h"
#include "nrf_sdh.h"
#include "nrf_sdh_ble.h"
#include "app_timer.h"
#include "app_error.h"
#include "nrf_pwr_mgmt.h"

#include "nrf_log.h"
#include "nrf_log_ctrl.h"
#include "nrf_log_default_backends.h"

#include "ugui.h"

UG_WINDOW window_1;
char m_summary_string1[64];
char m_summary_string2[64];

extern const nrf_lcd_t nrf_lcd_ili9341;
static const nrf_lcd_t * p_lcd = &nrf_lcd_ili9341;

/* MQ  Activate PPI to count events */
#define PIN_EVENTS_CRC_OK                    31    // RADIO->EVENTS_CRCOK  
#define PIN_EVENTS_CRC_ERROR                 30    // RADIO->EVENTS_CRCERROR
#define PIN_EVENTS_END                       29    // RADIO->EVENTS_END
#define PIN_EVENTS_RXREADY                     28    // RADIO->EVENTS_READY

const uint32_t EGU_CH0 = 0;
const uint32_t EGU_CH1 = 1;
const uint32_t EGU_CH2 = 2;
const uint32_t EGU_CH3 = 3;

static volatile uint32_t crc_ok_event = 0;
static volatile uint32_t end_event = 0;
static volatile uint32_t crc_error_event = 0;
static volatile uint32_t ready_rxevent = 0;


#define CONN_INTERVAL_DEFAULT           (uint16_t)(MSEC_TO_UNITS(7.5, UNIT_1_25_MS))    /**< Default connection interval used at connection establishment by central side. */

#define CONN_INTERVAL_MIN               (uint16_t)(MSEC_TO_UNITS(7.5, UNIT_1_25_MS))    /**< Minimum acceptable connection interval, in 1.25 ms units. */
#define CONN_INTERVAL_MAX               (uint16_t)(MSEC_TO_UNITS(500, UNIT_1_25_MS))    /**< Maximum acceptable connection interval, in 1.25 ms units. */
#define CONN_SUP_TIMEOUT                (uint16_t)(MSEC_TO_UNITS(8000,  UNIT_10_MS))    /**< Connection supervisory timeout (4 seconds). */
#define SLAVE_LATENCY                   0                                               /**< Slave latency. */

#define PHY_SELECTION_LED               BSP_BOARD_LED_0																	/**< LED indicating which phy is in use. */
#define OUTPUT_POWER_SELECTION_LED      BSP_BOARD_LED_1																	/**< LED indicating at which ouput power the radio is transmitting */
#define NON_CONN_ADV_LED                BSP_BOARD_LED_2																	/**< LED indicting if the device is advertising non-connectable advertising or not. */
#define CONN_ADV_CONN_STATE_LED         BSP_BOARD_LED_3																	/**< LED indicating that if device is advertising with connectable advertising, in a connected state, or none. */

#define PHY_SELECTION_BUTTON                  BSP_BUTTON_0
#define PHY_SELECTION_BUTTON_EVENT            BSP_EVENT_KEY_0
#define OUTPUT_POWER_SELECTION_BUTTON         BSP_BUTTON_1
#define OUTPUT_POWER_SELECTION_BUTTON_EVENT   BSP_EVENT_KEY_1
#define NON_CONN_OR_CONN_ADV_BUTTON           BSP_BUTTON_2
#define NON_CONN_OR_CONN_ADV_BUTTON_EVENT     BSP_EVENT_KEY_2
//#define BUTTON_NOT_IN_USE                     BSP_BUTTON_3
//#define BUTTON_NOT_IN_USE_EVENT               BSP_EVENT_KEY_3
#define PER_RESET_BUTTON                      BSP_BUTTON_3
#define PER_RESET_BUTTON_EVENT                BSP_EVENT_KEY_3

#define FAST_BLINK_INTERVAL		APP_TIMER_TICKS(200)
APP_TIMER_DEF(m_non_conn_fast_blink_timer_id);                /**< Timer used to toggle LED indicating non-connectable advertising on the dev. kit. */
APP_TIMER_DEF(m_conn_adv_fast_blink_timer_id);                /**< Timer used to toggle LED indicating connectable advertising on the dev. kit. */


#define SLOW_BLINK_INTERVAL		APP_TIMER_TICKS(750)
APP_TIMER_DEF(m_1Mbps_led_slow_blink_timer_id);                /**< Timer used to toggle LED for phy selection indication on the dev.kit. */                        
APP_TIMER_DEF(m_8dBm_led_slow_blink_timer_id);                 /**< Timer used to toggle LED for output power selection indication on the dev.kit. */ 


#define APP_BLE_CONN_CFG_TAG            1                                               /**< A tag that refers to the BLE stack configuration. */
#define APP_BLE_OBSERVER_PRIO           3                                               /**< Application's BLE observer priority. You shouldn't need to modify this value. */

static uint8_t m_adv_handle = BLE_GAP_ADV_SET_HANDLE_NOT_SET;                           /**< Advertising handle used to identify an advertising set. */
static uint8_t m_enc_advdata[BLE_GAP_ADV_SET_DATA_SIZE_MAX];                            /**< Buffer for storing an encoded advertising set. */
static uint8_t m_enc_scandata[BLE_GAP_SCAN_BUFFER_EXTENDED_MIN];                            /**< Buffer for storing an encoded advertising set. */


// Type holding the two output power options for this application.
typedef enum
{
    SELECTION_0_dBm = 0, 
    SELECTION_8_dBm = 8
} output_power_seclection_t;


// Type holding the two advertising selection modes.
typedef enum
{
    SELECTION_CONNECTABLE = 0, 
    SELECTION_NON_CONNECTABLE
} adv_scan_type_seclection_t;


// Type holding the two possible phy options.
typedef enum
{
    SELECTION_1M_PHY = 0, 
    SELECTION_CODED_PHY
} adv_scan_phy_seclection_t;

static adv_scan_type_seclection_t   m_adv_scan_type_selected = SELECTION_CONNECTABLE;   /**< Global variable holding the current scan selection mode. */
static adv_scan_phy_seclection_t    m_adv_scan_phy_selected  = SELECTION_CODED_PHY;      /**< Global variable holding the current phy selection. */
static output_power_seclection_t    m_output_power_selected  = SELECTION_8_dBm;          /**< Global variable holding the current output power selection. */
static bool    m_app_initiated_disconnect  = false;                //The application has initiated disconnect. Used to "tell" on_ble_gap_evt_disconnected() to not start advertising.
static bool    m_waiting_for_disconnect_evt     = false;          // Disconnect is initiated. The application has to wait for BLE_GAP_EVT_DISCONNECTED before proceeding.

static void set_current_adv_params_and_start_advertising(void);

/**@brief Struct that contains pointers to the encoded advertising data. */
static ble_gap_adv_data_t m_adv_data =
{
    .adv_data =
    {
        .p_data = m_enc_advdata,
        .len    = BLE_GAP_ADV_SET_DATA_SIZE_MAX
    },
    .scan_rsp_data =
    {
        .p_data = NULL,
        .len    = 0

    }
};


static uint16_t m_conn_handle = BLE_CONN_HANDLE_INVALID;    /**< Handle of the current BLE connection .*/
static uint8_t m_gap_role     = BLE_GAP_ROLE_INVALID;       /**< BLE role for this connection, see @ref BLE_GAP_ROLES */


// Scan parameters requested for scanning and connection.
static ble_gap_scan_params_t const m_scan_param =
{
    .active        = 0x00,
    .interval      = SCAN_INTERVAL,
    .window        = SCAN_WINDOW,
    .timeout       = 0x0000, // No timeout.
    .scan_phys     = BLE_GAP_PHY_1MBPS,
    .filter_policy = BLE_GAP_SCAN_FP_ACCEPT_ALL,
};

// Connection parameters requested for connection.
static ble_gap_conn_params_t m_conn_param =
{
    .min_conn_interval = CONN_INTERVAL_MIN,   // Minimum connection interval.
    .max_conn_interval = CONN_INTERVAL_MAX,   // Maximum connection interval.
    .slave_latency     = SLAVE_LATENCY,       // Slave latency.
    .conn_sup_timeout  = CONN_SUP_TIMEOUT     // Supervisory timeout.
};


/* MQ  Lifted from central's code. */
/**@brief Function for enabling reception of RSSI values when in a connection. 
 */
static void rssi_measurements_start(void)
{
    uint8_t threshold    = 0;
    uint8_t skip_count   = 0;
    ret_code_t err_code = sd_ble_gap_rssi_start(m_conn_handle, threshold, skip_count);
    APP_ERROR_CHECK(err_code);
}

static inline float calculate_per(void)
{   
    float per_value = 0;
    
    if (!ready_rxevent) //prevent division by zero
      return ((float)0.0);
    /*  Packet error rate: PER = (RADIO EVENTS_RXREADY � CRC ok) / RADIO EVENTS_RXREADY */
    per_value = ((((float)ready_rxevent - (float)crc_ok_event) / (float)ready_rxevent) * 100);

    return per_value;
}

/**@brief Function for manually resetting the PER calculation.
 */
static void on_per_reset_selection(void)
{
  //Reset the PER calculation variables (counters) to 0 to reset calculation to 0.
  
  //Disable interrupts using SD since SD is running..
  sd_nvic_ClearPendingIRQ(SWI3_EGU3_IRQn);
  sd_nvic_DisableIRQ(SWI3_EGU3_IRQn);
  
  //Reset variables
  crc_ok_event = 0;
  end_event = 0;
  crc_error_event = 0;
  ready_rxevent = 0;
  
  //Enable interrupts using SD since SD is running..
  sd_nvic_ClearPendingIRQ(SWI3_EGU3_IRQn);
  sd_nvic_SetPriority(SWI3_EGU3_IRQn, APP_IRQ_PRIORITY_LOW);
  sd_nvic_EnableIRQ(SWI3_EGU3_IRQn);

  UG_TextboxSetText(&window_1 , TXB_ID_6 , "");
}


static void instructions_print(void)
{
    NRF_LOG_INFO("Press the buttons to set up the peripheral in wanted mode:");
    NRF_LOG_INFO("Button 1: switch between coded phy and 1Mbps");
    NRF_LOG_INFO("Button 2: switch between 0 dbm and 8 dBm output power.");
    NRF_LOG_INFO("Button 3: switch between non-connectable and connectable advertising.");
}

static void non_conn_adv_fast_blink_timeout_handler(void * p_context)
{
  UNUSED_PARAMETER(p_context); 
  bsp_board_led_invert(NON_CONN_ADV_LED);
}

static void conn_adv_fast_blink_timeout_handler(void * p_context)
{
  UNUSED_PARAMETER(p_context);  
  bsp_board_led_invert(CONN_ADV_CONN_STATE_LED);
}


static void led_1Mbps_slow_blink_timeout_handler(void * p_context)
{
  UNUSED_PARAMETER(p_context);    
  bsp_board_led_invert(PHY_SELECTION_LED);    
}                     


static void led_8dBm_slow_blink_timeout_handler(void * p_context)
{
  UNUSED_PARAMETER(p_context);
  bsp_board_led_invert(OUTPUT_POWER_SELECTION_LED);
}



/**@brief Function for handling BLE_GAP_EVT_CONNECTED events.
 * Save the connection handle and GAP role.
 * Turn on the "connected state" LED.
 */
static void on_ble_gap_evt_connected(ble_gap_evt_t const * p_gap_evt)
{
    ret_code_t err_code;
    
    m_conn_handle = p_gap_evt->conn_handle;
    m_gap_role    = p_gap_evt->params.connected.role;

    err_code = sd_ble_gap_tx_power_set(BLE_GAP_TX_POWER_ROLE_CONN, m_conn_handle, m_output_power_selected);
    APP_ERROR_CHECK(err_code);
    
    if (m_gap_role == BLE_GAP_ROLE_PERIPH)
    {
        NRF_LOG_INFO("Connected as a peripheral.");
    }
    else if (m_gap_role == BLE_GAP_ROLE_CENTRAL)
    {
        NRF_LOG_INFO("Connected as a central. Should not happen.");
    }
    
    //If advertising, stop advertising.
    (void) sd_ble_gap_adv_stop(m_adv_handle);

    // Stop advertising LED timer, turn on "connected state" LED
    err_code = app_timer_stop(m_conn_adv_fast_blink_timer_id);
    APP_ERROR_CHECK(err_code);
    bsp_board_led_on(CONN_ADV_CONN_STATE_LED);
    UG_TextboxSetBackColor (&window_1 , TXB_ID_9 , C_DARK_GREEN );   
  
    /* MQ  Start our own RSSI measurements. */
    rssi_measurements_start();
}


/**@brief Function for handling BLE_GAP_EVT_DISCONNECTED events.
 * Unset the connection handle and restart advertising if needed. 
 */
static void on_ble_gap_evt_disconnected(ble_gap_evt_t const * p_gap_evt)
{
    m_conn_handle = BLE_CONN_HANDLE_INVALID;
    m_waiting_for_disconnect_evt = false;

    NRF_LOG_DEBUG("Disconnected: reason 0x%x.", p_gap_evt->params.disconnected.reason);

    if (m_app_initiated_disconnect == false) // (If a the app itself (button push) has initiated a disconnect, bsp_evt_handler will start the advertising.)
    {
      // Start advertising with the current setup.
      bsp_board_leds_off();
      set_current_adv_params_and_start_advertising();

    }
    UG_TextboxSetBackColor (&window_1 , TXB_ID_9 , C_DARK_RED );  
    on_per_reset_selection();
}

#define NRF_LOG_FLOAT_MARKER8 "%10d.%08d"

#define NRF_LOG_FLOAT8(val) (int32_t)(val),                                      \
                       (int32_t)(((val > 0) ? (val) - (int32_t)(val)            \
                                            : (int32_t)(val) - (val))*100000000)


/**@brief Function for handling scan request report.
 * Print the RSSI and address of the initiator if the RSSI has changed.
 */
static void on_scan_req_report(ble_gap_evt_scan_req_report_t const * p_scan_req_report)
{
    static int8_t         rssi_value = 0;
    float per_value;

   
     if(rssi_value != p_scan_req_report->rssi)
     {
       rssi_value = p_scan_req_report->rssi;
       per_value = calculate_per();
       NRF_LOG_INFO("Received scan request with RSSI %d", rssi_value);
       NRF_LOG_INFO("addr %02x:%02x:%02x:%02x:%02x:%02x",
               p_scan_req_report->peer_addr.addr[0],
               p_scan_req_report->peer_addr.addr[1],
               p_scan_req_report->peer_addr.addr[2],
               p_scan_req_report->peer_addr.addr[3],
               p_scan_req_report->peer_addr.addr[4],
               p_scan_req_report->peer_addr.addr[5]);
       //NRF_LOG_INFO("PER: " NRF_LOG_FLOAT_MARKER8 "\r\n", NRF_LOG_FLOAT8(calculate_per()));
       NRF_LOG_INFO("PER: " NRF_LOG_FLOAT_MARKER8 "\r\n", NRF_LOG_FLOAT8(per_value));
    }
    sprintf(&m_summary_string1[0], "%d", rssi_value);
    sprintf(&m_summary_string2[0], "%0.8f", per_value);
    UG_TextboxSetText(&window_1 , TXB_ID_5 , m_summary_string1);
    UG_TextboxSetText(&window_1 , TXB_ID_6 , m_summary_string2);
}


/**@brief Function for handling BLE Stack events.
 *
 * @param[in] p_ble_evt  Bluetooth stack event.
 */
static void ble_evt_handler(ble_evt_t const * p_ble_evt, void * p_context)
{
    
    uint32_t              err_code;
    ble_gap_evt_t const * p_gap_evt = &p_ble_evt->evt.gap_evt;

    switch (p_ble_evt->header.evt_id)
    {
       
        case BLE_GAP_EVT_CONNECTED:
            on_ble_gap_evt_connected(p_gap_evt);
            break;
        
        case BLE_GAP_EVT_DISCONNECTED:
            on_ble_gap_evt_disconnected(p_gap_evt);
            break;
        
        case BLE_GAP_EVT_CONN_PARAM_UPDATE:
        {
            NRF_LOG_INFO("Connection interval updated: 0x%x, 0x%x.",
                p_gap_evt->params.conn_param_update.conn_params.min_conn_interval,
                p_gap_evt->params.conn_param_update.conn_params.max_conn_interval);
        } break;
        
       case BLE_GAP_EVT_CONN_PARAM_UPDATE_REQUEST:
       {
           // Accept parameters requested by the peer.
           ble_gap_conn_params_t params;
           params = p_gap_evt->params.conn_param_update_request.conn_params;
           err_code = sd_ble_gap_conn_param_update(p_gap_evt->conn_handle, &params);
           APP_ERROR_CHECK(err_code);
       
           NRF_LOG_INFO("Connection interval updated (upon request): 0x%x, 0x%x.",
               p_gap_evt->params.conn_param_update_request.conn_params.min_conn_interval,
               p_gap_evt->params.conn_param_update_request.conn_params.max_conn_interval);
       } break;
       
       case BLE_GATTS_EVT_SYS_ATTR_MISSING:
       {
           NRF_LOG_DEBUG("BLE_GATTS_EVT_SYS_ATTR_MISSING");
           err_code = sd_ble_gatts_sys_attr_set(p_gap_evt->conn_handle, NULL, 0, 0);
           APP_ERROR_CHECK(err_code);
       } break;
       
       case BLE_GATTC_EVT_TIMEOUT: // Fallthrough.
       case BLE_GATTS_EVT_TIMEOUT:
       {
           NRF_LOG_DEBUG("GATT timeout, disconnecting.");
           err_code = sd_ble_gap_disconnect(m_conn_handle,
                                            BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
           APP_ERROR_CHECK(err_code);
       } break;
       
       case BLE_GAP_EVT_PHY_UPDATE:
       {
            NRF_LOG_DEBUG("BLE_GAP_EVT_PHY_UPDATE: not implemented.");
           
       } break;
       
       case BLE_GAP_EVT_PHY_UPDATE_REQUEST:
       { 
           NRF_LOG_DEBUG("BLE_GAP_EVT_PHY_UPDATE_REQUEST: not implemented.");
       } break;

       case BLE_GAP_EVT_SCAN_REQ_REPORT:
       {
          on_scan_req_report(&p_gap_evt->params.scan_req_report);
       } break;


        //MQ
        case BLE_GAP_EVT_RSSI_CHANGED:
        {
            int8_t rssi_value;  
            uint8_t rssi_channel_index;
            float per_value;
            static float per_value_prev = 0.0;
            static int8_t rssi_value_prev = 0;

            err_code = sd_ble_gap_rssi_get(p_gap_evt->conn_handle, &rssi_value, &rssi_channel_index);
            APP_ERROR_CHECK(err_code);
            per_value = calculate_per();
            //NRF_LOG_INFO("RSSI in connected mode %d", rssi_value);
            //NRF_LOG_INFO("PER: " NRF_LOG_FLOAT_MARKER8 "\r\n", NRF_LOG_FLOAT8(per_value));
            if (rssi_value_prev != rssi_value) {
              sprintf(&m_summary_string1[0], "%d", rssi_value);
              UG_TextboxSetText(&window_1 , TXB_ID_5 , m_summary_string1);
              rssi_value_prev = rssi_value;
            }
            if (per_value_prev != per_value) {
              sprintf(&m_summary_string2[0], "%0.6f", per_value);
              UG_TextboxSetText(&window_1 , TXB_ID_6 , m_summary_string2);
              per_value_prev = per_value;
            }                                                
        } break;

        default:
        {
          NRF_LOG_DEBUG("Received an unimplemented BLE event.");
            // No implementation needed.
        } break;
    }
}

/**@brief This function will disconnect if connected, and stop advertising if advertising. */
static void disconnect_stop_adv(void)
{
    ret_code_t err_code;
    // If advertising, stop advertising.
    (void) sd_ble_gap_adv_stop(m_adv_handle);
    
    // If connected, disconnect.
    if(m_conn_handle != BLE_CONN_HANDLE_INVALID)
    {
      NRF_LOG_INFO("Disconnecting...");
      
      m_app_initiated_disconnect = true;
      m_waiting_for_disconnect_evt    = true;
      err_code = sd_ble_gap_disconnect(m_conn_handle, BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
      
      if (err_code != NRF_SUCCESS)
      {
               NRF_LOG_ERROR("disconnect_stop_adv, sd_ble_gap_disconnect() failed: 0x%0x.", err_code);
               m_app_initiated_disconnect = false;
               m_waiting_for_disconnect_evt    = false;
      }
      while (m_waiting_for_disconnect_evt == true)
      {
       // Wait until BLE_GAP_EVT_DISCONNECT has occured.
      }
     
    }
}

/**@brief Function for setting up advertising data. */
static void advertising_data_set(void)
{

    ret_code_t ret;

    ble_gap_adv_params_t adv_params =
    {
        .properties    =
        {
          .type = BLE_GAP_ADV_TYPE_CONNECTABLE_SCANNABLE_UNDIRECTED,
        },
        .p_peer_addr   = NULL,
        .filter_policy = BLE_GAP_ADV_FP_ANY,
        .interval      = ADV_INTERVAL,
        .duration      = 0,

        .primary_phy   = BLE_GAP_PHY_1MBPS, // Must be changed to connect in long range. (BLE_GAP_PHY_CODED)
        .secondary_phy = BLE_GAP_PHY_1MBPS,
        .scan_req_notification = 1,
    };

    ble_advdata_t const adv_data =
    {
        .name_type          = BLE_ADVDATA_FULL_NAME,
        .flags              = BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE,
        .include_appearance = false,
    };

    if(m_adv_scan_phy_selected == SELECTION_1M_PHY)
    {
        // 1M coded for adv
        NRF_LOG_INFO("Setting adv params PHY to 1M. ");
        adv_params.primary_phy     = BLE_GAP_PHY_1MBPS;
        adv_params.secondary_phy   = BLE_GAP_PHY_1MBPS;
        
        if(m_adv_scan_type_selected == SELECTION_CONNECTABLE)
        {
            NRF_LOG_INFO("Advertising type set to CONNECTABLE_SCANNABLE_UNDIRECTED ");
            adv_params.properties.type = BLE_GAP_ADV_TYPE_CONNECTABLE_SCANNABLE_UNDIRECTED;
        }
        else if(m_adv_scan_type_selected == SELECTION_NON_CONNECTABLE)
        {
            NRF_LOG_INFO("Advertising type set to NONCONNECTABLE_SCANNABLE_UNDIRECTED ");
            adv_params.properties.type = BLE_GAP_ADV_TYPE_NONCONNECTABLE_SCANNABLE_UNDIRECTED;
        }
        
        ret = ble_advdata_encode(&adv_data, m_adv_data.adv_data.p_data, &m_adv_data.adv_data.len);
        APP_ERROR_CHECK(ret);

        ret = sd_ble_gap_adv_set_configure(&m_adv_handle, &m_adv_data, &adv_params);
        APP_ERROR_CHECK(ret);
    }

     else if(m_adv_scan_phy_selected == SELECTION_CODED_PHY)
    {
        // only extended advertising will allow primary phy to be coded
        NRF_LOG_INFO("Setting adv params phy to coded phy .. ");
        adv_params.primary_phy     = BLE_GAP_PHY_CODED;
        adv_params.secondary_phy   = BLE_GAP_PHY_CODED;
        
        if(m_adv_scan_type_selected == SELECTION_CONNECTABLE)
        {
            NRF_LOG_INFO("Advertising type set to EXTENDED_CONNECTABLE_NONSCANNABLE_UNDIRECTED ");
            adv_params.properties.type = BLE_GAP_ADV_TYPE_EXTENDED_CONNECTABLE_NONSCANNABLE_UNDIRECTED;
                      
            ret = ble_advdata_encode(&adv_data, m_adv_data.adv_data.p_data, &m_adv_data.adv_data.len);
            APP_ERROR_CHECK(ret);

            ret = sd_ble_gap_adv_set_configure(&m_adv_handle, &m_adv_data, &adv_params);
            APP_ERROR_CHECK(ret);
        }
        else if(m_adv_scan_type_selected == SELECTION_NON_CONNECTABLE)
        {
            NRF_LOG_INFO("Advertising type set to EXTENDED_NONCONNECTABLE_SCANNABLE_UNDIRECTED ");
            adv_params.properties.type = BLE_GAP_ADV_TYPE_EXTENDED_NONCONNECTABLE_SCANNABLE_UNDIRECTED;


            // ret = ble_advdata_encode(&adv_data, m_adv_data_ext.scan_rsp_data.p_data, &m_adv_data_ext.scan_rsp_data.len);
            // APP_ERROR_CHECK(ret);

            ret = sd_ble_gap_adv_set_configure(&m_adv_handle, NULL, &adv_params);
            APP_ERROR_CHECK(ret);			
        }
    }
}

/**@brief Function for starting advertising. */
static void advertising_start(void)
{

  ret_code_t err_code;
   NRF_LOG_INFO("Starting advertising.");

    err_code = sd_ble_gap_tx_power_set(BLE_GAP_TX_POWER_ROLE_ADV, m_adv_handle, m_output_power_selected);
    APP_ERROR_CHECK(err_code);

     NRF_LOG_INFO("Output power set to %d dBm", m_output_power_selected);

    err_code = sd_ble_gap_adv_start(m_adv_handle, APP_BLE_CONN_CFG_TAG);
    APP_ERROR_CHECK(err_code);
    m_app_initiated_disconnect = false;
}


/**@brief Function for initializing the log module.
 */
static void log_init(void)
{
    ret_code_t err_code = NRF_LOG_INIT(NULL);
    APP_ERROR_CHECK(err_code);

    NRF_LOG_DEFAULT_BACKENDS_INIT();
}


/**@brief Function for the Timer initialization.
 *
 * @details Initializes the timer module. This creates application timers.
 */
static void timer_init(void)
{
  ret_code_t err_code = app_timer_init();
  APP_ERROR_CHECK(err_code);

    // Creating the timers used to indicate the state/selection mode of the board.
  err_code = app_timer_create(&m_non_conn_fast_blink_timer_id, APP_TIMER_MODE_REPEATED, non_conn_adv_fast_blink_timeout_handler);
  APP_ERROR_CHECK(err_code);
  
  err_code = app_timer_create(&m_conn_adv_fast_blink_timer_id, APP_TIMER_MODE_REPEATED, conn_adv_fast_blink_timeout_handler);
  APP_ERROR_CHECK(err_code);
  
  err_code = app_timer_create(&m_1Mbps_led_slow_blink_timer_id, APP_TIMER_MODE_REPEATED, led_1Mbps_slow_blink_timeout_handler);
  APP_ERROR_CHECK(err_code);
  
  err_code = app_timer_create(&m_8dBm_led_slow_blink_timer_id, APP_TIMER_MODE_REPEATED, led_8dBm_slow_blink_timeout_handler);
  APP_ERROR_CHECK(err_code);
}

/**@brief Function for initializing the BLE stack.
 *
 * @details Initializes the SoftDevice and the BLE event interrupt.
 */
static void ble_stack_init(void)
{
    ret_code_t err_code;

    err_code = nrf_sdh_enable_request();
    APP_ERROR_CHECK(err_code);

    // Configure the BLE stack using the default settings.
    // Fetch the start address of the application RAM.
    uint32_t ram_start = 0;
    err_code = nrf_sdh_ble_default_cfg_set(APP_BLE_CONN_CFG_TAG, &ram_start);
    APP_ERROR_CHECK(err_code);

    // Enable BLE stack.
    err_code = nrf_sdh_ble_enable(&ram_start);
    APP_ERROR_CHECK(err_code);

    // Register a handler for BLE events.
    NRF_SDH_BLE_OBSERVER(m_ble_observer, APP_BLE_OBSERVER_PRIO, ble_evt_handler, NULL);
}


/**@brief Function for initializing GAP parameters.
 *
 * @details This function sets up all the necessary GAP (Generic Access Profile) parameters of the
 *          device including the device name and the preferred connection parameters.
 */
static void gap_params_init(void)
{
    ret_code_t              err_code;
    ble_gap_conn_sec_mode_t sec_mode;

    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&sec_mode);

    err_code = sd_ble_gap_device_name_set(&sec_mode,
                                          (uint8_t const *)DEVICE_NAME,
                                          strlen(DEVICE_NAME));
    APP_ERROR_CHECK(err_code);

    err_code = sd_ble_gap_ppcp_set(&m_conn_param);
    APP_ERROR_CHECK(err_code);
}


/**@brief Function for initializing the GATT library. */


/**@brief Function for initializing power management.
 */
static void power_management_init(void)
{
    ret_code_t ret;
    ret = nrf_pwr_mgmt_init();
    APP_ERROR_CHECK(ret);
}


/**@brief Function for handling the idle state (main loop).
 *
 * @details Handle any pending operation(s), then sleep until the next event occurs.
 */
static void idle_state_handle(void)
{
    if (NRF_LOG_PROCESS() == false)
    {
        nrf_pwr_mgmt_run();
    }
}


/**@brief Function for setting new output power selection: LEDs start to blink according to the selected state.
 * Note: this function sets the output power/TX power.
 *       The new output power will be set when (re-)starting to scan.
 */
static void output_power_selection_set(output_power_seclection_t output_power)
{
 ret_code_t err_code;
 m_output_power_selected = output_power;
  switch ( m_output_power_selected)
  {
    case SELECTION_8_dBm: 
    {
    	// 8 dBm is the current output power, start blinking LED.
        bsp_board_led_off(OUTPUT_POWER_SELECTION_LED); // not necessary because the LED should start to blink. 
        err_code = app_timer_start(m_8dBm_led_slow_blink_timer_id, SLOW_BLINK_INTERVAL, NULL);
    	APP_ERROR_CHECK(err_code);
    	UG_ButtonSetText(&window_1, BTN_ID_1, "8 dBm");
    } break;
    
    case SELECTION_0_dBm:
    {
    	// 0 dBm is the current output power, turn on LED.
        err_code = app_timer_stop(m_8dBm_led_slow_blink_timer_id); 
    	APP_ERROR_CHECK(err_code);
    	
    	bsp_board_led_on(OUTPUT_POWER_SELECTION_LED);
        UG_ButtonSetText(&window_1, BTN_ID_1, "0 dBm");
    } break;
  }
}


/**@brief Function for switching output power/TX power: 0 dBm or 8 dBm.
 * Note:  this function does only set the internal state, it does apply the new setting. 
 *        The new output power will be set when (re-)starting to scan.
 */
static void on_output_power_selection_button(void)
{
	// Change the output power.
  output_power_seclection_t new_output_power;
  switch ( m_output_power_selected)
  {
    case SELECTION_0_dBm:  // 0 dBm is the previous output power.
    {
    	// 8 dBm is the current output power, start blinking LED.
    	new_output_power = SELECTION_8_dBm; 
    	
    } break;
    
    case SELECTION_8_dBm:
    {
    	// 0 dBm is the current output power, turn on LED.
    	new_output_power = SELECTION_0_dBm;   	
    	
    } break;
    
  }
  output_power_selection_set(new_output_power);
}

/**@brief Function for setting new phy selection. LEDs start to blink according to the selected state.
 * Note: this function does only set the internal state, it does apply the new setting.
 *       The new phy will be be used when (re-)starting scanning.
 */
static void phy_selection_set_state(adv_scan_phy_seclection_t new_phy_selection)
{
  ret_code_t err_code;
  m_adv_scan_phy_selected = new_phy_selection;


  switch (new_phy_selection)
  {
    case SELECTION_1M_PHY:  // SELECTION_1M_PHY is the current "state".
    {
      // 1 Mbps is the current state, LED should start blinking.

      err_code = app_timer_start(m_1Mbps_led_slow_blink_timer_id, SLOW_BLINK_INTERVAL, NULL);
      APP_ERROR_CHECK(err_code);
      UG_ButtonSetText(&window_1, BTN_ID_0, "1 Mbps");
    } break;
    
    case SELECTION_CODED_PHY:
    {
      // Coded phy is the current sate, turn on LED.
      
      err_code = app_timer_stop(m_1Mbps_led_slow_blink_timer_id); 
      APP_ERROR_CHECK(err_code);
      bsp_board_led_on(PHY_SELECTION_LED);
      UG_ButtonSetText(&window_1, BTN_ID_0, "Coded");
    } break;
  }

}

/**@brief Function for switching PHY: coded phy or 1 Mbps
 * Note: this function does only set the internal state, it does apply the new setting.
 *       The new phy will be be used when (re-)starting scanning.
 */
static void on_phy_selection_button(void)
{
  // Change the selected phy.
  adv_scan_phy_seclection_t new_phy_selection;
  switch (m_adv_scan_phy_selected)
  {
    case SELECTION_CODED_PHY:  // SELECTION_CODED_PHY is the previous "state".
    {
      // 1 Mbps is the current state, LED should start blinking.
      new_phy_selection = SELECTION_1M_PHY; 
    } break;
    
    case SELECTION_1M_PHY:
    {
      // Coded phy is the current sate, turn on LED.
      new_phy_selection = SELECTION_CODED_PHY;
    } break;
    

  }
  phy_selection_set_state(new_phy_selection);
}

/**@brief Function setting the internal advertising state.
 * Note: this function does only set the internal "advertising state", it does not start advertising.
 *       The new advertising mode will be used when (re-)starting to advertise.
 */
static void on_non_conn_or_conn_adv_selection_state_set(adv_scan_type_seclection_t adv_selection)
{
 ret_code_t err_code;

  m_adv_scan_type_selected = adv_selection;
  bsp_board_led_off(CONN_ADV_CONN_STATE_LED);
  switch (adv_selection)
  {
    case SELECTION_NON_CONNECTABLE: 
    {
      // Current state is non-connectable advertising. Start blinking associated LED.
      
      err_code = app_timer_stop(m_conn_adv_fast_blink_timer_id); 
      APP_ERROR_CHECK(err_code);
      bsp_board_led_off(CONN_ADV_CONN_STATE_LED);
      UG_TextboxSetBackColor (&window_1 , TXB_ID_9 , C_DARK_RED );   

      err_code = app_timer_start(m_non_conn_fast_blink_timer_id, FAST_BLINK_INTERVAL, NULL);
      APP_ERROR_CHECK(err_code);

      UG_ButtonSetText(&window_1, BTN_ID_2, "Non-connectable");
    } break;
    
    case SELECTION_CONNECTABLE:
    {
      // Current state is connectable advertising. Start blinking associated LED.
     
      err_code = app_timer_stop(m_non_conn_fast_blink_timer_id); 
      APP_ERROR_CHECK(err_code);
      bsp_board_led_off(NON_CONN_ADV_LED);


      err_code = app_timer_start(m_conn_adv_fast_blink_timer_id, FAST_BLINK_INTERVAL, NULL);
      APP_ERROR_CHECK(err_code);

      UG_ButtonSetText(&window_1, BTN_ID_2, "Connectable");
    } break;
  }



}

/**@brief Function for switching between "non-connectable and connectable advertising" selection.
 * Note: this function does only set the internal "advertising state", it does not start advertising.
 *       The new advertising mode will be used when (re-)starting to advertise.
 */
static void on_non_conn_or_conn_adv_selection(void)
{
 // Change the advertising type
  adv_scan_type_seclection_t new_adv_selection;

  switch (m_adv_scan_type_selected)
  {
    case SELECTION_CONNECTABLE:  // Connectable advertising is the previous state.
    {
      // Current state is non-connectable advertising. Start blinking associated LED.
      new_adv_selection = SELECTION_NON_CONNECTABLE; 

    } break;
    
    case SELECTION_NON_CONNECTABLE:
    {
      // Current state is connectable advertising. Start blinking associated LED.
      new_adv_selection = SELECTION_CONNECTABLE;

    } break;
  }
  on_non_conn_or_conn_adv_selection_state_set(new_adv_selection);

}

/**@brief Function for handling events from the button handler module.
 *
 * @param[in] pin_no        The pin that the event applies to.
 * @param[in] button_action The button action (press/release).
 */
void bsp_evt_handler(bsp_event_t event)
{
  ret_code_t err_code;	
   
    //if(event != BUTTON_NOT_IN_USE_EVENT)
    //{
        
    
	 // Set the correct parameters, depending on the button pushed.		
        switch (event)
        {
          case PHY_SELECTION_BUTTON_EVENT:
          {
            on_phy_selection_button();
          } break;
						
	  case OUTPUT_POWER_SELECTION_BUTTON_EVENT:
	  {
            on_output_power_selection_button();
	  } break;

          case NON_CONN_OR_CONN_ADV_BUTTON_EVENT:
          {
            on_non_conn_or_conn_adv_selection();
          } break;
          
          //MQ
          case PER_RESET_BUTTON_EVENT:
          {
            on_per_reset_selection();
            return; //no need to modify connection so just exit
          }

          default:
            break;
        }
        
        disconnect_stop_adv();
	advertising_data_set(); 
	advertising_start();
    //}
   
}


/**@brief Function for initializing buttons and leds.
 */
static void buttons_leds_init(void)
{
    ret_code_t err_code;
   
    err_code = bsp_init(BSP_INIT_LEDS | BSP_INIT_BUTTONS, bsp_evt_handler);
    APP_ERROR_CHECK(err_code);

}

/**@brief Function for starting advertising with the current selections of output power, phy, and connectable or non-connectable advertising.
 */
static void set_current_adv_params_and_start_advertising(void)
{
  
  phy_selection_set_state(m_adv_scan_phy_selected);
  on_non_conn_or_conn_adv_selection_state_set(m_adv_scan_type_selected);
  output_power_selection_set(m_output_power_selected);
  
  advertising_data_set();
  advertising_start();

}

static void gpiote_out_init(uint32_t index, uint32_t pin, uint32_t polarity, uint32_t init_val) 
{
    NRF_GPIOTE->CONFIG[index] |= ((GPIOTE_CONFIG_MODE_Task << GPIOTE_CONFIG_MODE_Pos) & GPIOTE_CONFIG_MODE_Msk) |
                            ((pin << GPIOTE_CONFIG_PSEL_Pos) & GPIOTE_CONFIG_PSEL_Msk) |
                            ((polarity << GPIOTE_CONFIG_POLARITY_Pos) & GPIOTE_CONFIG_POLARITY_Msk) |
                            ((init_val << GPIOTE_CONFIG_OUTINIT_Pos) & GPIOTE_CONFIG_OUTINIT_Msk);
}

static void init_ppi(void) 
{
    const uint32_t GPIO_CH0 = 0;
    const uint32_t GPIO_CH1 = 1;
    const uint32_t GPIO_CH2 = 2;
    const uint32_t GPIO_CH3 = 3;
    const uint32_t PPI_CH0 = 10;
    const uint32_t PPI_CH1 = 11;
    const uint32_t PPI_CH2 = 12;
    const uint32_t PPI_CH3 = 13;    
        
    nrf_gpio_cfg_output(PIN_EVENTS_END);
    nrf_gpio_cfg_output(PIN_EVENTS_CRC_OK);
    nrf_gpio_cfg_output(PIN_EVENTS_CRC_ERROR);
    nrf_gpio_cfg_output(PIN_EVENTS_RXREADY);
        
    gpiote_out_init(GPIO_CH0, PIN_EVENTS_END, GPIOTE_CONFIG_POLARITY_Toggle, GPIOTE_CONFIG_OUTINIT_Low); // end
    gpiote_out_init(GPIO_CH1, PIN_EVENTS_CRC_OK, GPIOTE_CONFIG_POLARITY_Toggle, GPIOTE_CONFIG_OUTINIT_Low); // crc ok
    gpiote_out_init(GPIO_CH2, PIN_EVENTS_CRC_ERROR, GPIOTE_CONFIG_POLARITY_Toggle, GPIOTE_CONFIG_OUTINIT_Low); // crc error
    gpiote_out_init(GPIO_CH3, PIN_EVENTS_RXREADY, GPIOTE_CONFIG_POLARITY_Toggle, GPIOTE_CONFIG_OUTINIT_Low); // events ready
        
    NRF_PPI->CH[PPI_CH0].EEP = (uint32_t) &(NRF_RADIO->EVENTS_END);
    NRF_PPI->CH[PPI_CH0].TEP = (uint32_t) &(NRF_GPIOTE->TASKS_OUT[GPIO_CH0]);

    NRF_PPI->CH[PPI_CH1].EEP = (uint32_t) &(NRF_RADIO->EVENTS_CRCOK);
    NRF_PPI->CH[PPI_CH1].TEP = (uint32_t) &(NRF_GPIOTE->TASKS_OUT[GPIO_CH1]);

    NRF_PPI->CH[PPI_CH2].EEP = (uint32_t) &(NRF_RADIO->EVENTS_CRCERROR);
    NRF_PPI->CH[PPI_CH2].TEP = (uint32_t) &(NRF_GPIOTE->TASKS_OUT[GPIO_CH2]);
               
    NRF_PPI->CH[PPI_CH3].EEP = (uint32_t) &(NRF_RADIO->EVENTS_RXREADY);
    NRF_PPI->CH[PPI_CH3].TEP = (uint32_t) &(NRF_GPIOTE->TASKS_OUT[GPIO_CH3]);

    //MQ  Fork tasks to activate EGU.        
    NRF_PPI->FORK[PPI_CH0].TEP = (uint32_t) &(NRF_EGU3->TASKS_TRIGGER[EGU_CH0]);
    NRF_PPI->FORK[PPI_CH1].TEP = (uint32_t) &(NRF_EGU3->TASKS_TRIGGER[EGU_CH1]);
    NRF_PPI->FORK[PPI_CH2].TEP = (uint32_t) &(NRF_EGU3->TASKS_TRIGGER[EGU_CH2]);
    NRF_PPI->FORK[PPI_CH3].TEP = (uint32_t) &(NRF_EGU3->TASKS_TRIGGER[EGU_CH3]);  
       
    NRF_EGU3->INTENSET = EGU_INTENSET_TRIGGERED0_Msk | EGU_INTENSET_TRIGGERED1_Msk | EGU_INTENSET_TRIGGERED2_Msk | EGU_INTENSET_TRIGGERED3_Msk;
    NVIC_ClearPendingIRQ(SWI3_EGU3_IRQn);
    NVIC_SetPriority(SWI3_EGU3_IRQn, APP_IRQ_PRIORITY_LOW);
    NVIC_EnableIRQ(SWI3_EGU3_IRQn);

    NRF_PPI->CHENSET = (1 << PPI_CH0) | (1 << PPI_CH1) | (1 << PPI_CH2) | (1 << PPI_CH3);
}

void SWI3_EGU3_IRQHandler(void)
{
    if (NRF_EGU3->EVENTS_TRIGGERED[EGU_CH0]) {     
        NRF_EGU3->EVENTS_TRIGGERED[EGU_CH0] = 0;
        ++end_event;
       
    }
    if (NRF_EGU3->EVENTS_TRIGGERED[EGU_CH1]) {        
        NRF_EGU3->EVENTS_TRIGGERED[EGU_CH1] = 0;
        ++crc_ok_event;
    }
    if (NRF_EGU3->EVENTS_TRIGGERED[EGU_CH2]) {        
        NRF_EGU3->EVENTS_TRIGGERED[EGU_CH2] = 0;
        ++crc_error_event;
    }
    if (NRF_EGU3->EVENTS_TRIGGERED[EGU_CH3]) {        
        NRF_EGU3->EVENTS_TRIGGERED[EGU_CH3] = 0;
        ++ready_rxevent;
    }
}

void window_1_callback ( UG_MESSAGE* msg )
{
    UNUSED_PARAMETER(msg);    
    return;
}

UG_GUI gui;
UG_TEXTBOX textbox_toggle_phy;
UG_TEXTBOX textbox_toggle_power;
UG_TEXTBOX textbox_toggle_adv_type; //advertising type (non-connectable / connectable)
UG_TEXTBOX textbox_rssi_label;
UG_TEXTBOX textbox_per_label;
UG_TEXTBOX textbox_rssi_number;
UG_TEXTBOX textbox_per_number;
UG_TEXTBOX textbox_dbm_label; 
UG_TEXTBOX textbox_percentage_label;
UG_TEXTBOX textbox_link_indicator;
UG_BUTTON phy_button;
UG_BUTTON power_button;
UG_BUTTON adv_button;
#define MAX_OBJECTS 15
UG_OBJECT obj_buff_wnd_1[MAX_OBJECTS];

int main(void)
{
    // Initialize.
    log_init();
   
    timer_init();
    buttons_leds_init();
    init_ppi();

    power_management_init();
    ble_stack_init();
    
    gap_params_init();

    UG_Init(&gui, 240, 320, p_lcd);


    /* Create the window */
    UG_WindowCreate ( &window_1 , obj_buff_wnd_1 , MAX_OBJECTS, window_1_callback ) ;
    /* Modify the window t i t l e */
    UG_WindowSetTitleText (&window_1 , "Nordic Range Demo") ;
    UG_WindowSetTitleTextFont (&window_1 , &FONT_10X16) ;
    UG_WindowSetTitleTextAlignment(&window_1, ALIGN_CENTER);

#define TXT_ID_0_X_LOCATION 15
#define TXT_ID_0_Y_LOCATION 10
#define TXT_ID_0_WIDTH 200
#define TXT_ID_0_HEIGHT 25

#define INTERWIDGET_SPACE 10

#define BTN_ID_0_X_LOCATION TXT_ID_0_X_LOCATION
#define BTN_ID_0_Y_LOCATION TXT_ID_0_Y_LOCATION+TXT_ID_0_HEIGHT+INTERWIDGET_SPACE
#define BTN_ID_0_WIDTH TXT_ID_0_WIDTH
#define BTN_ID_0_HEIGHT 30

#define TXT_ID_1_X_LOCATION TXT_ID_0_X_LOCATION
#define TXT_ID_1_Y_LOCATION BTN_ID_0_Y_LOCATION+BTN_ID_0_HEIGHT+INTERWIDGET_SPACE
#define TXT_ID_1_WIDTH TXT_ID_0_WIDTH
#define TXT_ID_1_HEIGHT TXT_ID_0_HEIGHT

#define BTN_ID_1_X_LOCATION TXT_ID_0_X_LOCATION
#define BTN_ID_1_Y_LOCATION TXT_ID_1_Y_LOCATION+TXT_ID_1_HEIGHT+INTERWIDGET_SPACE
#define BTN_ID_1_WIDTH TXT_ID_0_WIDTH
#define BTN_ID_1_HEIGHT BTN_ID_0_HEIGHT

#define TXT_ID_2_X_LOCATION TXT_ID_0_X_LOCATION
#define TXT_ID_2_Y_LOCATION BTN_ID_1_Y_LOCATION+BTN_ID_1_HEIGHT+INTERWIDGET_SPACE
#define TXT_ID_2_WIDTH TXT_ID_0_WIDTH
#define TXT_ID_2_HEIGHT TXT_ID_0_HEIGHT

#define BTN_ID_2_X_LOCATION TXT_ID_0_X_LOCATION
#define BTN_ID_2_Y_LOCATION TXT_ID_2_Y_LOCATION+TXT_ID_2_HEIGHT+INTERWIDGET_SPACE
#define BTN_ID_2_WIDTH TXT_ID_0_WIDTH
#define BTN_ID_2_HEIGHT BTN_ID_0_HEIGHT

#define TXT_ID_3_X_LOCATION TXT_ID_0_X_LOCATION 
#define TXT_ID_3_Y_LOCATION BTN_ID_2_Y_LOCATION+BTN_ID_2_HEIGHT+INTERWIDGET_SPACE - 5
#define TXT_ID_3_WIDTH 50
#define TXT_ID_3_HEIGHT 30 

#define TXT_ID_4_X_LOCATION TXT_ID_3_X_LOCATION
#define TXT_ID_4_Y_LOCATION TXT_ID_3_Y_LOCATION+TXT_ID_3_HEIGHT+2
#define TXT_ID_4_WIDTH TXT_ID_3_WIDTH
#define TXT_ID_4_HEIGHT TXT_ID_3_HEIGHT

#define TXT_ID_5_X_LOCATION TXT_ID_3_X_LOCATION + TXT_ID_3_WIDTH + 2
#define TXT_ID_5_Y_LOCATION TXT_ID_3_Y_LOCATION  
#define TXT_ID_5_WIDTH 100
#define TXT_ID_5_HEIGHT TXT_ID_3_HEIGHT

#define TXT_ID_6_X_LOCATION TXT_ID_4_X_LOCATION + TXT_ID_4_WIDTH + 2
#define TXT_ID_6_Y_LOCATION TXT_ID_4_Y_LOCATION  
#define TXT_ID_6_WIDTH TXT_ID_5_WIDTH
#define TXT_ID_6_HEIGHT TXT_ID_4_HEIGHT

#define TXT_ID_7_X_LOCATION TXT_ID_5_X_LOCATION + TXT_ID_5_WIDTH + 2
#define TXT_ID_7_Y_LOCATION TXT_ID_5_Y_LOCATION  
#define TXT_ID_7_WIDTH 27
#define TXT_ID_7_HEIGHT TXT_ID_5_HEIGHT

#define TXT_ID_8_X_LOCATION TXT_ID_6_X_LOCATION + TXT_ID_6_WIDTH + 2
#define TXT_ID_8_Y_LOCATION TXT_ID_6_Y_LOCATION  
#define TXT_ID_8_WIDTH TXT_ID_7_WIDTH
#define TXT_ID_8_HEIGHT TXT_ID_7_HEIGHT

#define TXT_ID_9_X_LOCATION TXT_ID_7_X_LOCATION + TXT_ID_7_WIDTH + 2
#define TXT_ID_9_Y_LOCATION TXT_ID_7_Y_LOCATION  
#define TXT_ID_9_WIDTH 16
#define TXT_ID_9_HEIGHT ((TXT_ID_7_HEIGHT * 2) + 2)

    /* Create "Toggle PHY" textbox (TXB_ID_0) */
    UG_TextboxCreate(&window_1, &textbox_toggle_phy, TXB_ID_0, TXT_ID_0_X_LOCATION, TXT_ID_0_Y_LOCATION, TXT_ID_0_X_LOCATION+TXT_ID_0_WIDTH, TXT_ID_0_Y_LOCATION+TXT_ID_0_HEIGHT);  
    UG_TextboxSetFont(&window_1, TXB_ID_0, &FONT_8X12);
    UG_TextboxSetText(&window_1 , TXB_ID_0 , "Button 1: PHY") ;
    UG_TextboxSetForeColor (&window_1 , TXB_ID_0 , C_MAROON ) ;
    UG_TextboxSetBackColor (&window_1 , TXB_ID_0 , C_DODGER_BLUE ); 
    UG_TextboxSetAlignment (&window_1 , TXB_ID_0 , ALIGN_CENTER );

    /* Create "PHY" selection button (BTN_ID_0) */
    UG_ButtonCreate(&window_1, &phy_button, BTN_ID_0, BTN_ID_0_X_LOCATION, BTN_ID_0_Y_LOCATION, BTN_ID_0_X_LOCATION+BTN_ID_0_WIDTH, BTN_ID_0_Y_LOCATION+BTN_ID_0_HEIGHT);
    UG_ButtonSetStyle(&window_1, BTN_ID_0, BTN_STYLE_3D|BTN_STYLE_USE_ALTERNATE_COLORS);
    UG_ButtonSetForeColor(&window_1, BTN_ID_0, C_YELLOW);
    UG_ButtonSetBackColor(&window_1, BTN_ID_0, C_MEDIUM_BLUE);  
    UG_ButtonSetFont(&window_1, BTN_ID_0, &FONT_10X16);
    UG_ButtonSetText(&window_1, BTN_ID_0, "1 Mbps");    

    /* Create "Toggle output power" textbox (TXB_ID_1) */
    UG_TextboxCreate(&window_1, &textbox_toggle_power, TXB_ID_1, TXT_ID_1_X_LOCATION, TXT_ID_1_Y_LOCATION, TXT_ID_1_X_LOCATION+TXT_ID_1_WIDTH, TXT_ID_1_Y_LOCATION+TXT_ID_1_HEIGHT);  
    UG_TextboxSetFont(&window_1, TXB_ID_1, &FONT_8X12);
    UG_TextboxSetText(&window_1 , TXB_ID_1 , "Button 2: Power") ;
    UG_TextboxSetForeColor (&window_1 , TXB_ID_1, C_MAROON ) ;
    UG_TextboxSetBackColor (&window_1 , TXB_ID_1, C_DODGER_BLUE ); 
    UG_TextboxSetAlignment (&window_1 , TXB_ID_1, ALIGN_CENTER );  

    /* Create "Power" selection button (BTN_ID_1) */
    UG_ButtonCreate(&window_1, &power_button, BTN_ID_1, BTN_ID_1_X_LOCATION, BTN_ID_1_Y_LOCATION, BTN_ID_1_X_LOCATION+BTN_ID_1_WIDTH, BTN_ID_1_Y_LOCATION+BTN_ID_1_HEIGHT);
    UG_ButtonSetStyle(&window_1, BTN_ID_1, BTN_STYLE_3D|BTN_STYLE_USE_ALTERNATE_COLORS);
    UG_ButtonSetForeColor(&window_1, BTN_ID_1, C_YELLOW);
    UG_ButtonSetBackColor(&window_1, BTN_ID_1, C_MEDIUM_BLUE);  
    UG_ButtonSetFont(&window_1, BTN_ID_1, &FONT_10X16);
    UG_ButtonSetText(&window_1, BTN_ID_1, "0 dBm"); 

    /* Create "Toggle output power" textbox (TXB_ID_2) */
    UG_TextboxCreate(&window_1, &textbox_toggle_adv_type, TXB_ID_2, TXT_ID_2_X_LOCATION, TXT_ID_2_Y_LOCATION, TXT_ID_2_X_LOCATION+TXT_ID_2_WIDTH, TXT_ID_2_Y_LOCATION+TXT_ID_2_HEIGHT);  
    UG_TextboxSetFont(&window_1, TXB_ID_2, &FONT_8X12);
    UG_TextboxSetText(&window_1 , TXB_ID_2 , "Button 3: Adv. type") ;
    UG_TextboxSetForeColor (&window_1 , TXB_ID_2, C_MAROON ) ;
    UG_TextboxSetBackColor (&window_1 , TXB_ID_2, C_DODGER_BLUE ); 
    UG_TextboxSetAlignment (&window_1 , TXB_ID_2, ALIGN_CENTER );  

    /* Create "Advertising type" selection button (BTN_ID_2) */
    UG_ButtonCreate(&window_1, &adv_button, BTN_ID_2, BTN_ID_2_X_LOCATION, BTN_ID_2_Y_LOCATION, BTN_ID_2_X_LOCATION+BTN_ID_2_WIDTH, BTN_ID_2_Y_LOCATION+BTN_ID_2_HEIGHT);
    UG_ButtonSetStyle(&window_1, BTN_ID_2, BTN_STYLE_3D|BTN_STYLE_USE_ALTERNATE_COLORS);
    UG_ButtonSetForeColor(&window_1, BTN_ID_2, C_YELLOW);
    UG_ButtonSetBackColor(&window_1, BTN_ID_2, C_MEDIUM_BLUE);  
    UG_ButtonSetFont(&window_1, BTN_ID_2, &FONT_10X16);
    UG_ButtonSetText(&window_1, BTN_ID_2, "Connectable"); 

    /* Create "RSSI" textbox (TXB_ID_3) */
    UG_TextboxCreate(&window_1, &textbox_rssi_label, TXB_ID_3, TXT_ID_3_X_LOCATION, TXT_ID_3_Y_LOCATION, TXT_ID_3_X_LOCATION+TXT_ID_3_WIDTH, TXT_ID_3_Y_LOCATION+TXT_ID_3_HEIGHT);  
    UG_TextboxSetFont(&window_1, TXB_ID_3, &FONT_8X12);
    UG_TextboxSetText(&window_1 , TXB_ID_3 , "RSSI: ") ;
    UG_TextboxSetForeColor (&window_1 , TXB_ID_3 , C_MAROON ) ;
    UG_TextboxSetBackColor (&window_1 , TXB_ID_3 , C_DODGER_BLUE ); 
    UG_TextboxSetAlignment (&window_1 , TXB_ID_3 , ALIGN_CENTER );

    /* Create "PER" textbox (TXB_ID_4) */
    UG_TextboxCreate(&window_1, &textbox_per_label, TXB_ID_4, TXT_ID_4_X_LOCATION, TXT_ID_4_Y_LOCATION, TXT_ID_4_X_LOCATION+TXT_ID_4_WIDTH, TXT_ID_4_Y_LOCATION+TXT_ID_4_HEIGHT);  
    UG_TextboxSetFont(&window_1, TXB_ID_4, &FONT_8X12);
    UG_TextboxSetText(&window_1 , TXB_ID_4 , "PER: ") ;
    UG_TextboxSetForeColor (&window_1 , TXB_ID_4 , C_MAROON ) ;
    UG_TextboxSetBackColor (&window_1 , TXB_ID_4 , C_DODGER_BLUE ); 
    UG_TextboxSetAlignment (&window_1 , TXB_ID_4 , ALIGN_CENTER );

    /* Create "RSSI" textbox for the numbers (TXB_ID_5) */
    UG_TextboxCreate(&window_1, &textbox_rssi_number, TXB_ID_5, TXT_ID_5_X_LOCATION, TXT_ID_5_Y_LOCATION, TXT_ID_5_X_LOCATION+TXT_ID_5_WIDTH, TXT_ID_5_Y_LOCATION+TXT_ID_5_HEIGHT);  
    UG_TextboxSetFont(&window_1, TXB_ID_5, &FONT_8X12);
    UG_TextboxSetText(&window_1 , TXB_ID_5 , "###") ;
    UG_TextboxSetForeColor (&window_1 , TXB_ID_5 , C_MAROON ) ;
    UG_TextboxSetBackColor (&window_1 , TXB_ID_5 , C_DODGER_BLUE ); 
    UG_TextboxSetAlignment (&window_1 , TXB_ID_5 , ALIGN_CENTER );

    /* Create "PER" textbox for the numbers (TXB_ID_6) */
    UG_TextboxCreate(&window_1, &textbox_per_number, TXB_ID_6, TXT_ID_6_X_LOCATION, TXT_ID_6_Y_LOCATION, TXT_ID_6_X_LOCATION+TXT_ID_6_WIDTH, TXT_ID_6_Y_LOCATION+TXT_ID_6_HEIGHT);  
    UG_TextboxSetFont(&window_1, TXB_ID_6, &FONT_8X12);
    UG_TextboxSetText(&window_1 , TXB_ID_6 , "###") ;
    UG_TextboxSetForeColor (&window_1 , TXB_ID_6 , C_MAROON ) ;
    UG_TextboxSetBackColor (&window_1 , TXB_ID_6 , C_DODGER_BLUE ); 
    UG_TextboxSetAlignment (&window_1 , TXB_ID_6 , ALIGN_CENTER );

    /* Create "dBm" textbox for the RSSI metric (TXB_ID_7) */
    UG_TextboxCreate(&window_1, &textbox_dbm_label, TXB_ID_7, TXT_ID_7_X_LOCATION, TXT_ID_7_Y_LOCATION, TXT_ID_7_X_LOCATION+TXT_ID_7_WIDTH, TXT_ID_7_Y_LOCATION+TXT_ID_7_HEIGHT);  
    UG_TextboxSetFont(&window_1, TXB_ID_7, &FONT_8X12);
    UG_TextboxSetText(&window_1 , TXB_ID_7 , "dBm") ;
    UG_TextboxSetForeColor (&window_1 , TXB_ID_7 , C_MAROON ) ;
    UG_TextboxSetBackColor (&window_1 , TXB_ID_7 , C_DODGER_BLUE ); 
    UG_TextboxSetAlignment (&window_1 , TXB_ID_7 , ALIGN_CENTER );

    /* Create "%" textbox for the PER metric (TXB_ID_8) */
    UG_TextboxCreate(&window_1, &textbox_percentage_label, TXB_ID_8, TXT_ID_8_X_LOCATION, TXT_ID_8_Y_LOCATION, TXT_ID_8_X_LOCATION+TXT_ID_8_WIDTH, TXT_ID_8_Y_LOCATION+TXT_ID_8_HEIGHT);  
    UG_TextboxSetFont(&window_1, TXB_ID_8, &FONT_8X12);
    UG_TextboxSetText(&window_1 , TXB_ID_8 , "%") ;
    UG_TextboxSetForeColor (&window_1 , TXB_ID_8 , C_MAROON ) ;
    UG_TextboxSetBackColor (&window_1 , TXB_ID_8 , C_DODGER_BLUE ); 
    UG_TextboxSetAlignment (&window_1 , TXB_ID_8 , ALIGN_CENTER );

    /* Create Link textbox for the Link indicator (TXB_ID_9) */
    UG_TextboxCreate(&window_1, &textbox_link_indicator, TXB_ID_9, TXT_ID_9_X_LOCATION, TXT_ID_9_Y_LOCATION, TXT_ID_9_X_LOCATION+TXT_ID_9_WIDTH, TXT_ID_9_Y_LOCATION+TXT_ID_9_HEIGHT);      
    UG_TextboxSetBackColor (&window_1 , TXB_ID_9 , C_DARK_RED );   

    // Start execution.  
    NRF_LOG_INFO("Long range demo  --peripheral--");
    instructions_print();

    set_current_adv_params_and_start_advertising();

    /* Finally , show the window */
    UG_WindowShow( &window_1 ) ;

    // Enter main loop.
    for (;;)
    {
        UG_Update();
        idle_state_handle();
    }
}


/**
 * @}
 */
