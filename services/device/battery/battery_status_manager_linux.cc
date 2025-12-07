// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/battery/battery_status_manager_linux.h"

#include <stddef.h>
#include <stdint.h>

#include <limits>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/values.h"
#include "base/version.h"
#include "components/dbus/thread_linux/dbus_thread_linux.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_path.h"
#include "dbus/object_proxy.h"
#include "dbus/property.h"
#include "dbus/values_util.h"
#include "services/device/battery/battery_status_manager_linux-inl.h"

namespace device {

class BatteryStatusManagerLinux::BatteryProperties : public dbus::PropertySet {
 public:
  using PropertiesChangedCallback =
      base::RepeatingCallback<void(const BatteryProperties*)>;

  BatteryProperties(dbus::ObjectProxy* object_proxy,
                    PropertiesChangedCallback callback)
      : dbus::PropertySet(
            object_proxy,
            kUPowerDeviceInterfaceName,
            base::BindRepeating(&BatteryProperties::OnPropertyChanged,
                                base::Unretained(this))),
        properties_changed_callback_(std::move(callback)) {
    RegisterProperty(kUPowerDevicePropertyIsPresent, &is_present_);
    RegisterProperty(kUPowerDevicePropertyPercentage, &percentage_);
    RegisterProperty(kUPowerDevicePropertyState, &state_);
    RegisterProperty(kUPowerDevicePropertyTimeToEmpty, &time_to_empty_);
    RegisterProperty(kUPowerDevicePropertyTimeToFull, &time_to_full_);
    RegisterProperty(kUPowerDevicePropertyType, &type_);
    ConnectSignals();
    GetAll();
  }

  BatteryProperties(const BatteryProperties&) = delete;
  BatteryProperties& operator=(const BatteryProperties&) = delete;

  ~BatteryProperties() override = default;

  double percentage(double default_value = 100) const {
    return percentage_.is_valid() ? percentage_.value() : default_value;
  }
  uint32_t state(uint32_t default_value = UPOWER_DEVICE_STATE_UNKNOWN) const {
    return state_.is_valid() ? state_.value() : default_value;
  }
  int64_t time_to_empty(int64_t default_value = 0) const {
    return time_to_empty_.is_valid() ? time_to_empty_.value() : default_value;
  }
  int64_t time_to_full(int64_t default_value = 0) const {
    return time_to_full_.is_valid() ? time_to_full_.value() : default_value;
  }

  bool IsValid() const {
    return is_present() && type() == UPOWER_DEVICE_TYPE_BATTERY;
  }

 private:
  void OnPropertyChanged(const std::string& property_name) {
    if (got_all_properties_) {
      properties_changed_callback_.Run(this);
    }
  }

  void OnGetAll(dbus::Response* response) override {
    dbus::PropertySet::OnGetAll(response);
    got_all_properties_ = true;
    properties_changed_callback_.Run(this);
  }

  bool is_present(bool default_value = false) const {
    return is_present_.is_valid() ? is_present_.value() : default_value;
  }
  uint32_t type(uint32_t default_value = UPOWER_DEVICE_TYPE_UNKNOWN) const {
    return type_.is_valid() ? type_.value() : default_value;
  }

  PropertiesChangedCallback properties_changed_callback_;

  dbus::Property<bool> is_present_;
  dbus::Property<double> percentage_;
  dbus::Property<uint32_t> state_;
  dbus::Property<int64_t> time_to_empty_;
  dbus::Property<int64_t> time_to_full_;
  dbus::Property<uint32_t> type_;

  bool got_all_properties_ = false;
};

BatteryStatusManagerLinux::BatteryStatusManagerLinux(
    const BatteryStatusService::BatteryUpdateCallback& callback)
    : callback_(callback) {}

BatteryStatusManagerLinux::~BatteryStatusManagerLinux() {
  ShutdownDBusConnection();
}

bool BatteryStatusManagerLinux::StartListeningBatteryChange() {
  // The `system_bus_` may already be set in testing.
  if (!system_bus_) {
    system_bus_ = dbus_thread_linux::GetSharedSystemBus();
  }
  display_device_proxy_ = system_bus_->GetObjectProxy(
      kUPowerServiceName, dbus::ObjectPath(kUPowerDevicePath));
  properties_ = std::make_unique<BatteryProperties>(
      display_device_proxy_,
      base::BindRepeating(&BatteryStatusManagerLinux::NotifyBatteryStatus,
                          weak_ptr_factory_.GetWeakPtr()));
  return true;
}

void BatteryStatusManagerLinux::StopListeningBatteryChange() {
  ShutdownDBusConnection();
}

void BatteryStatusManagerLinux::ShutdownDBusConnection() {
  if (!system_bus_) {
    return;
  }

  properties_.reset();
  auto object_path = display_device_proxy_->object_path();
  display_device_proxy_ = nullptr;
  system_bus_->RemoveObjectProxy(kUPowerServiceName, object_path,
                                 base::DoNothing());
  system_bus_ = nullptr;
}

void BatteryStatusManagerLinux::NotifyBatteryStatus(
    const BatteryProperties* properties) {
  if (notifying_battery_status_) {
    return;
  }

  notifying_battery_status_ = true;
  callback_.Run(ComputeWebBatteryStatus(properties));
  notifying_battery_status_ = false;
}

mojom::BatteryStatus BatteryStatusManagerLinux::ComputeWebBatteryStatus(
    const BatteryProperties* properties) {
  mojom::BatteryStatus status;
  if (!properties->IsValid()) {
    return status;
  }

  uint32_t state = properties->state();
  status.charging = state != UPOWER_DEVICE_STATE_DISCHARGING &&
                    state != UPOWER_DEVICE_STATE_EMPTY;
  // Convert percentage to a value between 0 and 1 with 2 digits of precision.
  // This is to bring it in line with other platforms like Mac and Android where
  // we report level with 1% granularity. It also serves the purpose of reducing
  // the possibility of fingerprinting and triggers less level change events on
  // the blink side.
  // TODO(timvolodine): consider moving this rounding to the blink side.
  status.level = round(properties->percentage()) / 100.f;

  switch (state) {
    case UPOWER_DEVICE_STATE_CHARGING: {
      int64_t time_to_full = properties->time_to_full();
      status.charging_time = (time_to_full > 0)
                                 ? time_to_full
                                 : std::numeric_limits<double>::infinity();
      break;
    }
    case UPOWER_DEVICE_STATE_DISCHARGING: {
      int64_t time_to_empty = properties->time_to_empty();
      // Set dischargingTime if it's available. Otherwise leave the default
      // value which is +infinity.
      if (time_to_empty > 0) {
        status.discharging_time = time_to_empty;
      }
      status.charging_time = std::numeric_limits<double>::infinity();
      break;
    }
    case UPOWER_DEVICE_STATE_FULL: {
      break;
    }
    default: {
      status.charging_time = std::numeric_limits<double>::infinity();
    }
  }
  return status;
}

// static
std::unique_ptr<BatteryStatusManagerLinux>
BatteryStatusManagerLinux::CreateForTesting(
    const BatteryStatusService::BatteryUpdateCallback& callback,
    dbus::Bus* bus) {
  auto manager = std::make_unique<BatteryStatusManagerLinux>(callback);
  manager->SetDBusForTesting(bus);
  return manager;
}

void BatteryStatusManagerLinux::SetDBusForTesting(dbus::Bus* bus) {
  system_bus_ = bus;
}

// static
std::unique_ptr<BatteryStatusManager> BatteryStatusManager::Create(
    const BatteryStatusService::BatteryUpdateCallback& callback) {
  return std::make_unique<BatteryStatusManagerLinux>(callback);
}

}  // namespace device
