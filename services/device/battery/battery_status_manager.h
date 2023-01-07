// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_BATTERY_BATTERY_STATUS_MANAGER_H_
#define SERVICES_DEVICE_BATTERY_BATTERY_STATUS_MANAGER_H_

#include <memory>

#include "services/device/battery/battery_status_service.h"

namespace device {

// Platform specific manager class for fetching battery status data.
class BatteryStatusManager {
 public:
  // Creates a BatteryStatusManager object. |callback| should be called when the
  // battery status changes.
  static std::unique_ptr<BatteryStatusManager> Create(
      const BatteryStatusService::BatteryUpdateCallback& callback);

  virtual ~BatteryStatusManager() {}

  // Start listening for battery status changes. New updates are signalled
  // by invoking the callback provided at construction time.
  // Note that this is called in the IO thread.
  virtual bool StartListeningBatteryChange() = 0;

  // Stop listening for battery status changes.
  // Note that this is called in the IO thread.
  virtual void StopListeningBatteryChange() = 0;
};

}  // namespace device

#endif  // SERVICES_DEVICE_BATTERY_BATTERY_STATUS_MANAGER_H_
