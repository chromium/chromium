// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef SERVICES_DEVICE_BATTERY_BATTERY_STATUS_MANAGER_LINUX_H_
#define SERVICES_DEVICE_BATTERY_BATTERY_STATUS_MANAGER_LINUX_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "services/device/battery/battery_status_manager.h"

namespace dbus {
class Bus;
class ObjectProxy;
}  // namespace dbus

namespace device {
// UPowerDeviceState reflects the possible UPower.Device.State values,
// see upower.freedesktop.org/docs/Device.html#Device:State.
enum UPowerDeviceState {
  UPOWER_DEVICE_STATE_UNKNOWN = 0,
  UPOWER_DEVICE_STATE_CHARGING = 1,
  UPOWER_DEVICE_STATE_DISCHARGING = 2,
  UPOWER_DEVICE_STATE_EMPTY = 3,
  UPOWER_DEVICE_STATE_FULL = 4,
  UPOWER_DEVICE_STATE_PENDING_CHARGE = 5,
  UPOWER_DEVICE_STATE_PENDING_DISCHARGE = 6,
};

// UPowerDeviceType reflects the possible UPower.Device.Type values,
// see upower.freedesktop.org/docs/Device.html#Device:Type.
enum UPowerDeviceType {
  UPOWER_DEVICE_TYPE_UNKNOWN = 0,
  UPOWER_DEVICE_TYPE_LINE_POWER = 1,
  UPOWER_DEVICE_TYPE_BATTERY = 2,
  UPOWER_DEVICE_TYPE_UPS = 3,
  UPOWER_DEVICE_TYPE_MONITOR = 4,
  UPOWER_DEVICE_TYPE_MOUSE = 5,
  UPOWER_DEVICE_TYPE_KEYBOARD = 6,
  UPOWER_DEVICE_TYPE_PDA = 7,
  UPOWER_DEVICE_TYPE_PHONE = 8,
};

// Creates a notification thread and delegates Start/Stop calls to it.
class BatteryStatusManagerLinux : public BatteryStatusManager {
 public:
  explicit BatteryStatusManagerLinux(
      const BatteryStatusService::BatteryUpdateCallback& callback);

  BatteryStatusManagerLinux(const BatteryStatusManagerLinux&) = delete;
  BatteryStatusManagerLinux& operator=(const BatteryStatusManagerLinux&) =
      delete;

  ~BatteryStatusManagerLinux() override;

 private:
  friend class BatteryStatusManagerLinuxTest;
  class BatteryProperties;

  // BatteryStatusManager:
  bool StartListeningBatteryChange() override;
  void StopListeningBatteryChange() override;

  void NotifyBatteryStatus(const BatteryProperties* properties);

  void ShutdownDBusConnection();

  mojom::BatteryStatus ComputeWebBatteryStatus(
      const BatteryProperties* properties);

  void SetDBusForTesting(dbus::Bus* bus);

  static std::unique_ptr<BatteryStatusManagerLinux> CreateForTesting(
      const BatteryStatusService::BatteryUpdateCallback& callback,
      dbus::Bus* bus);

  BatteryStatusService::BatteryUpdateCallback callback_;
  scoped_refptr<dbus::Bus> system_bus_;
  // Owned by `system_bus_`.
  raw_ptr<dbus::ObjectProxy> display_device_proxy_;
  std::unique_ptr<BatteryProperties> properties_;
  bool notifying_battery_status_ = false;

  base::WeakPtrFactory<BatteryStatusManagerLinux> weak_ptr_factory_{this};
};

}  // namespace device

#endif  // SERVICES_DEVICE_BATTERY_BATTERY_STATUS_MANAGER_LINUX_H_
