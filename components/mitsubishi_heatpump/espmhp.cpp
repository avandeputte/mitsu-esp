
#include "espmhp.h"
using namespace esphome;

/**
 * Create a new MitsubishiHeatPump object
 *
 * Args:
 *   hw_serial: pointer to an Arduino HardwareSerial instance
 *   poll_interval: polling interval in milliseconds
 */
MitsubishiHeatPump::MitsubishiHeatPump(
        HardwareSerial* hw_serial,
        uint32_t poll_interval
) :
    PollingComponent{poll_interval}, // member initializers list
    hw_serial_{hw_serial}
{
    this->traits_.set_supports_action(true);
    this->traits_.set_supports_current_temperature(true);
    this->traits_.set_supports_two_point_target_temperature(false);
    this->traits_.set_visual_min_temperature(ESPMHP_MIN_TEMPERATURE);
    this->traits_.set_visual_max_temperature(ESPMHP_MAX_TEMPERATURE);
    this->traits_.set_visual_temperature_step(ESPMHP_TEMPERATURE_STEP);
}

void MitsubishiHeatPump::check_logger_conflict_() {
#ifdef USE_LOGGER
    if (this->get_hw_serial_() == logger::global_logger->get_hw_serial()) {
        ESP_LOGW(TAG, "  You're using the same serial port for logging"
                " and the MitsubishiHeatPump component. Please disable"
                " logging over the serial port by setting"
                " logger:baud_rate to 0.");
    }
#endif
}

void MitsubishiHeatPump::update() {
    // This will be called every "update_interval" milliseconds.
    this->hp->sync();
}

void MitsubishiHeatPump::set_baud_rate(int baud) {
    this->baud_ = baud;
}

climate::ClimateTraits MitsubishiHeatPump::traits() {
    return traits_;
}

climate::ClimateTraits& MitsubishiHeatPump::config_traits() {
    return traits_;
}


// Implement control of a MitsubishiHeatPump.
// Maps HomeAssistant/ESPHome modes to Mitsubishi modes.
void MitsubishiHeatPump::control(const climate::ClimateCall &call) {
    ESP_LOGI("control", "Received control request from HA");

    bool updated = false;
    bool has_mode = call.get_mode().has_value();
    bool has_temp = call.get_target_temperature().has_value();
    if (has_mode){
        this->mode = *call.get_mode();
    }
    switch (this->mode) {
        case climate::CLIMATE_MODE_COOL:
            hp->setModeSetting("COOL");
            hp->setPowerSetting("ON");
            if (has_mode){
                if (cool_setpoint.has_value() && !has_temp) {
                    hp->setTemperature(cool_setpoint.value());
                    this->target_temperature = cool_setpoint.value();
                }
                this->action = climate::CLIMATE_ACTION_IDLE;
                updated = true;
            }
            break;
        case climate::CLIMATE_MODE_HEAT:
            hp->setModeSetting("HEAT");
            hp->setPowerSetting("ON");
            if (has_mode){
                if (heat_setpoint.has_value() && !has_temp) {
                    hp->setTemperature(heat_setpoint.value());
                    this->target_temperature = heat_setpoint.value();
                }
                this->action = climate::CLIMATE_ACTION_IDLE;
                updated = true;
            }
            break;
        case climate::CLIMATE_MODE_DRY:
            hp->setModeSetting("DRY");
            hp->setPowerSetting("ON");
            if (has_mode){
                this->action = climate::CLIMATE_ACTION_DRYING;
                updated = true;
            }
            break;
        case climate::CLIMATE_MODE_HEAT_COOL:
            hp->setModeSetting("AUTO");
            hp->setPowerSetting("ON");
            if (has_mode){
                if (auto_setpoint.has_value() && !has_temp) {
                    hp->setTemperature(auto_setpoint.value());
                    this->target_temperature = auto_setpoint.value();
                }
                this->action = climate::CLIMATE_ACTION_IDLE;
            }
            updated = true;
            break;
        case climate::CLIMATE_MODE_FAN_ONLY:
            hp->setModeSetting("FAN");
            hp->setPowerSetting("ON");
            if (has_mode){
                this->action = climate::CLIMATE_ACTION_FAN;
                updated = true;
            }
            break;
        case climate::CLIMATE_MODE_OFF:
        default:
            if (has_mode){
                hp->setPowerSetting("OFF");
                this->action = climate::CLIMATE_ACTION_OFF;
                updated = true;
            }
            break;
    }

    if (has_temp){
        ESP_LOGI("control", "Sending target temp: %.1f",*call.get_target_temperature());
        hp->setTemperature(*call.get_target_temperature());
        this->target_temperature = *call.get_target_temperature();
        updated = true;
    }
    if (call.get_fan_mode().has_value()) {
        this->fan_mode = *call.get_fan_mode();
        switch(*call.get_fan_mode()) {
            case climate::CLIMATE_FAN_OFF:
                hp->setPowerSetting("OFF");
                updated = true;
                break;
            case climate::CLIMATE_FAN_LOW:
                hp->setFanSpeed("QUIET");
                updated = true;
                ESP_LOGI("control", "Fan ha:LOW --> hp:QUIET");
                break;
            case climate::CLIMATE_FAN_MEDIUM:
                hp->setFanSpeed("1");
                updated = true;
                ESP_LOGI("control", "Fan ha:MEDIUM --> hp:1");
                break;
            case climate::CLIMATE_FAN_HIGH:
                hp->setFanSpeed("2");
                updated = true;
                ESP_LOGI("control", "Fan ha:HIGH --> hp:2");
                break;
            case climate::CLIMATE_FAN_FOCUS:
                hp->setFanSpeed("3");
                updated = true;
                ESP_LOGI("control", "Fan ha:FOCUS --> hp:3");
                break;
            case climate::CLIMATE_FAN_DIFFUSE:
                hp->setFanSpeed("4");
                updated = true;
                ESP_LOGI("control", "Fan ha:DIFFUSE --> hp:4");
                break;
            default:
                hp->setFanSpeed("AUTO");
                updated = true;
                ESP_LOGI("control", "Fan ha:default --> hp:AUTO");
                break;
        }
    }

    if (call.get_swing_mode().has_value()) {
 
        this->swing_mode = *call.get_swing_mode();
        switch(*call.get_swing_mode()) {
            case climate::CLIMATE_SWING_OFF:
                hp->setVaneSetting("AUTO");
                updated = true;
                ESP_LOGI("control", "Vane ha:OFF --> hp:AUTO");
                break;
            case climate::CLIMATE_SWING_BOTH:
                hp->setVaneSetting("1");
                updated = true;
                ESP_LOGI("control", "Vane ha:BOTH --> hp:1");
                break;
            case climate::CLIMATE_SWING_HORIZONTAL:
                hp->setVaneSetting("5");
                updated = true;
                ESP_LOGI("control", "Vane ha:HORIZONTAL --> hp:5");
                break;
            case climate::CLIMATE_SWING_VERTICAL:
                hp->setVaneSetting("3");
                updated = true;
                ESP_LOGI("control", "Vane ha:VERTICAL --> hp:3");
                break;
            default:
                ESP_LOGW("control", "control - received unsupported swing mode request.");

        }
    }
    ESP_LOGD("control", "Was HeatPump updated? %s", YESNO(updated));

    // send the update back to esphome:
    this->publish_state();
    // and the heat pump:
    hp->update();
}

void MitsubishiHeatPump::hpSettingsChanged() {
    ESP_LOGI("SettingsChanged", "Received a setting change from HP.");
    heatpumpSettings currentSettings = hp->getSettings();

    if (currentSettings.power == NULL) {
        /*
         * We should always get a valid pointer here once the HeatPump
         * component fully initializes. If HeatPump hasn't read the settings
         * from the unit yet (hp->connect() doesn't do this, sadly), we'll need
         * to punt on the update. Likely not an issue when run in callback
         * mode, but that isn't working right yet.
         */
        ESP_LOGD("SettingsChanged", "Waiting for HeatPump to read the settings the first time.");
        esphome::delay(10);
        return;
    }

    if (strcmp(currentSettings.power, "ON") == 0) {
        if (strcmp(currentSettings.mode, "HEAT") == 0) {
            this->mode = climate::CLIMATE_MODE_HEAT;
            if (heat_setpoint != currentSettings.temperature) {
                heat_setpoint = currentSettings.temperature;
                save(currentSettings.temperature, heat_storage);
            }
            this->action = climate::CLIMATE_ACTION_IDLE;
        } else if (strcmp(currentSettings.mode, "DRY") == 0) {
            this->mode = climate::CLIMATE_MODE_DRY;
            this->action = climate::CLIMATE_ACTION_DRYING;
        } else if (strcmp(currentSettings.mode, "COOL") == 0) {
            this->mode = climate::CLIMATE_MODE_COOL;
            if (cool_setpoint != currentSettings.temperature) {
                cool_setpoint = currentSettings.temperature;
                save(currentSettings.temperature, cool_storage);
            }
            this->action = climate::CLIMATE_ACTION_IDLE;
        } else if (strcmp(currentSettings.mode, "FAN") == 0) {
            this->mode = climate::CLIMATE_MODE_FAN_ONLY;
            this->action = climate::CLIMATE_ACTION_FAN;
        } else if (strcmp(currentSettings.mode, "AUTO") == 0) {
            this->mode = climate::CLIMATE_MODE_HEAT_COOL;
            if (auto_setpoint != currentSettings.temperature) {
                auto_setpoint = currentSettings.temperature;
                save(currentSettings.temperature, auto_storage);
            }
            this->action = climate::CLIMATE_ACTION_IDLE;
        } else {
            ESP_LOGW("SettingsChanged","Unknown climate mode value %s received from HeatPump",currentSettings.mode);
        }
    } else {
        this->mode = climate::CLIMATE_MODE_OFF;
        this->action = climate::CLIMATE_ACTION_OFF;
    }

    if (strcmp(currentSettings.fan, "QUIET") == 0) {
        this->fan_mode = climate::CLIMATE_FAN_LOW;
        ESP_LOGI("SettingsChanged", "fan hp:QUIET --> ha:LOW");
    } else if (strcmp(currentSettings.fan, "1") == 0) {
        this->fan_mode = climate::CLIMATE_FAN_MEDIUM;
        ESP_LOGI("SettingsChanged", "fan hp:1 --> ha:MEDIUM");
    } else if (strcmp(currentSettings.fan, "2") == 0) {
        this->fan_mode = climate::CLIMATE_FAN_HIGH;
        ESP_LOGI("SettingsChanged", "fan hp:2 --> ha:HIGH");
    } else if (strcmp(currentSettings.fan, "3") == 0) {
        this->fan_mode = climate::CLIMATE_FAN_FOCUS;
        ESP_LOGI("SettingsChanged", "fan hp:3 --> ha:FOCUS");
    } else if (strcmp(currentSettings.fan, "4") == 0) {
        this->fan_mode = climate::CLIMATE_FAN_DIFFUSE;
        ESP_LOGI("SettingsChanged", "fan hp:4 --> ha:DIFFUSE");
    } else { //case "AUTO" or default:
        this->fan_mode = climate::CLIMATE_FAN_AUTO;
        ESP_LOGI("SettingsChanged", "fan hp:AUTO --> ha:AUTO");
    }

    if (strcmp(currentSettings.vane, "1") == 0) {
        this->swing_mode = climate::CLIMATE_SWING_BOTH;
        ESP_LOGI("SettingsChanged", "vane hp:1 --> ha:BOTH");
    } else if (strcmp(currentSettings.vane, "2") == 0) {
        this->swing_mode = climate::CLIMATE_SWING_VERTICAL;
        ESP_LOGI("SettingsChanged", "vane hp:2 --> ha:VERTICAL");
    } else if (strcmp(currentSettings.vane, "3") == 0) {
        this->swing_mode = climate::CLIMATE_SWING_VERTICAL;
        ESP_LOGI("SettingsChanged", "vane hp:3 --> ha:VERTICAL");
    } else if (strcmp(currentSettings.vane, "4") == 0) {
        this->swing_mode = climate::CLIMATE_SWING_HORIZONTAL;
        ESP_LOGI("SettingsChanged", "vane hp:4 --> ha:HORIZONTAL");
    } else if (strcmp(currentSettings.vane, "5") == 0) {
        this->swing_mode = climate::CLIMATE_SWING_HORIZONTAL;
        ESP_LOGI("SettingsChanged", "vane hp:5 --> ha:HORIZONTAL");
    } else {
        this->swing_mode = climate::CLIMATE_SWING_OFF;
        ESP_LOGI("SettingsChanged", "vane hp:AUTO --> ha:OFF");
    }
    /*
     * ******** HANDLE TARGET TEMPERATURE CHANGES ********
     */
    this->target_temperature = currentSettings.temperature;
    ESP_LOGI("SettingsChanged", "Target temp: %f", this->target_temperature);

    /*
     * ******** Publish state back to ESPHome. ********
     */
    this->publish_state();
}

/**
 * Report changes in the current temperature sensed by the HeatPump.
 */
void MitsubishiHeatPump::hpStatusChanged(heatpumpStatus currentStatus) {
    ESP_LOGI("StatusChanged", "Received a status change from HP.");
    this->current_temperature = currentStatus.roomTemperature;
    switch (this->mode) {
        case climate::CLIMATE_MODE_HEAT:
            if (currentStatus.operating) {
                this->action = climate::CLIMATE_ACTION_HEATING;
            } else {
                this->action = climate::CLIMATE_ACTION_IDLE;
            }
            break;
        case climate::CLIMATE_MODE_COOL:
            if (currentStatus.operating) {
                this->action = climate::CLIMATE_ACTION_COOLING;
            } else {
                this->action = climate::CLIMATE_ACTION_IDLE;
            }
            break;
        case climate::CLIMATE_MODE_HEAT_COOL:
            this->action = climate::CLIMATE_ACTION_IDLE;
            if (currentStatus.operating) {
              if (this->current_temperature > this->target_temperature) {
                  this->action = climate::CLIMATE_ACTION_COOLING;
              } else if (this->current_temperature < this->target_temperature) {
                  this->action = climate::CLIMATE_ACTION_HEATING;
              }
            }
            break;
        case climate::CLIMATE_MODE_DRY:
            if (currentStatus.operating) {
                this->action = climate::CLIMATE_ACTION_DRYING;
            } else {
                this->action = climate::CLIMATE_ACTION_IDLE;
            }
            break;
        case climate::CLIMATE_MODE_FAN_ONLY:
            this->action = climate::CLIMATE_ACTION_FAN;
            break;
        default:
            this->action = climate::CLIMATE_ACTION_OFF;
    }

    this->publish_state();
}

void MitsubishiHeatPump::set_remote_temperature(float temp) {
    ESP_LOGD("SetRemoteTemp", "Setting remote temp: %.1f", temp);
    this->hp->setRemoteTemperature(temp);
}

void MitsubishiHeatPump::setup() {
    // This will be called by App.setup()
    this->banner();
    ESP_LOGCONFIG("setup", "Setting up UART...");
    if (!this->get_hw_serial_()) {
        ESP_LOGCONFIG(
                "setup",
                "No HardwareSerial was provided. "
                "Software serial ports are unsupported by this component.");
        this->mark_failed();
        return;
    }
    this->check_logger_conflict_();

    ESP_LOGCONFIG("setup", "Intializing new HeatPump object.");
    this->hp = new HeatPump();
    this->current_temperature = NAN;
    this->target_temperature = NAN;
    this->fan_mode = climate::CLIMATE_FAN_OFF;
    this->swing_mode = climate::CLIMATE_SWING_OFF;


    hp->setSettingsChangedCallback(
            [this]() {
                this->hpSettingsChanged();
            }
    );

    hp->setStatusChangedCallback(
            [this](heatpumpStatus currentStatus) {
                this->hpStatusChanged(currentStatus);
            }
    );


    ESP_LOGCONFIG(
            "setup",
            "hw_serial(%p) is &Serial(%p)? %s",
            this->get_hw_serial_(),
            &Serial,
            YESNO(this->get_hw_serial_() == &Serial)
    );

    ESP_LOGCONFIG("setup", "Calling hp->connect(%p)", this->get_hw_serial_());

    if (hp->connect(this->get_hw_serial_(), this->baud_, -1, -1)) {
        hp->sync();
    } else {
        ESP_LOGCONFIG(
                "setup",
                "Connection to HeatPump failed."
                " Marking MitsubishiHeatPump component as failed."
        );
        this->mark_failed();
    }

    // create various setpoint persistence:
    cool_storage = global_preferences->make_preference<uint8_t>(this->get_object_id_hash() + 1);
    heat_storage = global_preferences->make_preference<uint8_t>(this->get_object_id_hash() + 2);
    auto_storage = global_preferences->make_preference<uint8_t>(this->get_object_id_hash() + 3);

    // load values from storage:
    cool_setpoint = load(cool_storage);
    heat_setpoint = load(heat_storage);
    auto_setpoint = load(auto_storage);

    this->dump_config();
}

/**
 * The ESP only has a few bytes of rtc storage, so instead
 * of storing floats directly, we'll store the number of
 * TEMPERATURE_STEPs from MIN_TEMPERATURE.
 **/
void MitsubishiHeatPump::save(float value, ESPPreferenceObject& storage) {
    uint8_t steps = (value - ESPMHP_MIN_TEMPERATURE) / ESPMHP_TEMPERATURE_STEP;
    storage.save(&steps);
}

optional<float> MitsubishiHeatPump::load(ESPPreferenceObject& storage) {
    uint8_t steps = 0;
    if (!storage.load(&steps)) {
        return {};
    }
    return ESPMHP_MIN_TEMPERATURE + (steps * ESPMHP_TEMPERATURE_STEP);
}

void MitsubishiHeatPump::dump_config() {
    this->banner();
    ESP_LOGI("config", "  Supports HEAT: %s", YESNO(true));
    ESP_LOGI("config", "  Supports COOL: %s", YESNO(true));
    ESP_LOGI("config", "  Supports AWAY mode: %s", YESNO(false));
    ESP_LOGI("config", "  Saved heat: %.1f", heat_setpoint.value_or(-1));
    ESP_LOGI("config", "  Saved cool: %.1f", cool_setpoint.value_or(-1));
    ESP_LOGI("config", "  Saved auto: %.1f", auto_setpoint.value_or(-1));
}

void MitsubishiHeatPump::dump_state() {
    LOG_CLIMATE("", "MitsubishiHeatPump Climate", this);
    ESP_LOGI("dumpState", "HELLO");
}
