// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/battery/battery_status_manager.h"

#include <memory>

#include "base/notreached.h"

namespace device {

namespace {

class BatteryStatusManagerDefault : public BatteryStatusManager {
 public:
  explicit BatteryStatusManagerDefault(
      const BatteryStatusService::BatteryUpdateCallback& callback) {}

  BatteryStatusManagerDefault(const BatteryStatusManagerDefault&) = delete;
  BatteryStatusManagerDefault& operator=(const BatteryStatusManagerDefault&) =
      delete;

  ~BatteryStatusManagerDefault() override {}

 private:
  // BatteryStatusManager:
  bool StartListeningBatteryChange() override {
    NOTIMPLEMENTED();
    return false;
  }

  void StopListeningBatteryChange() override { NOTIMPLEMENTED(); }
};

}  // namespace

// static
std::unique_ptr<BatteryStatusManager> BatteryStatusManager::Create(
    const BatteryStatusService::BatteryUpdateCallback& callback) {
  return std::make_unique<BatteryStatusManagerDefault>(callback);
}

}  // namespace device
