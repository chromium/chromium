// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef SERVICES_DEVICE_BATTERY_BATTERY_STATUS_MANAGER_LINUX_H_
#define SERVICES_DEVICE_BATTERY_BATTERY_STATUS_MANAGER_LINUX_H_

#include "services/device/battery/battery_status_manager.h"
#include "services/device/public/mojom/battery_status.mojom.h"

namespace base {
class Thread;
}

namespace dbus {
class Bus;
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
  ~BatteryStatusManagerLinux() override;

 private:
  friend class BatteryStatusManagerLinuxTest;
  class BatteryStatusNotificationThread;

  // BatteryStatusManager:
  bool StartListeningBatteryChange() override;
  void StopListeningBatteryChange() override;

  // Starts the notifier thread if not already started and returns true on
  // success.
  bool StartNotifierThreadIfNecessary();
  base::Thread* GetNotifierThreadForTesting();

  static std::unique_ptr<BatteryStatusManagerLinux> CreateForTesting(
      const BatteryStatusService::BatteryUpdateCallback& callback,
      dbus::Bus* bus);

  BatteryStatusService::BatteryUpdateCallback callback_;
  std::unique_ptr<BatteryStatusNotificationThread> notifier_thread_;

  DISALLOW_COPY_AND_ASSIGN(BatteryStatusManagerLinux);
};

}  // namespace device

#endif  // SERVICES_DEVICE_BATTERY_BATTERY_STATUS_MANAGER_LINUX_H_
