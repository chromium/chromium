// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "device/bluetooth/floss/fake_floss_battery_manager_client.h"

#include "base/task/single_thread_task_runner.h"
#include "device/bluetooth/floss/floss_dbus_client.h"

namespace floss {

FakeFlossBatteryManagerClient::FakeFlossBatteryManagerClient() = default;
FakeFlossBatteryManagerClient::~FakeFlossBatteryManagerClient() = default;

void FakeFlossBatteryManagerClient::Init(dbus::Bus* bus,
                                         const std::string& service_name,
                                         const int adapter_index) {}

void FakeFlossBatteryManagerClient::GetBatteryInformation(
    ResponseCallback<absl::optional<BatterySet>> callback,
    const FlossDeviceId& device) {
  std::move(callback).Run(DBusResult<absl::optional<BatterySet>>({}));
}

void FakeFlossBatteryManagerClient::AddObserver(
    FlossBatteryManagerClientObserver* observer) {}

}  // namespace floss
