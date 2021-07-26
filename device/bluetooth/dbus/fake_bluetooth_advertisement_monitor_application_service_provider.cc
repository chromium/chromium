// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/dbus/fake_bluetooth_advertisement_monitor_application_service_provider.h"

#include "device/bluetooth/dbus/bluez_dbus_manager.h"
#include "device/bluetooth/dbus/fake_bluetooth_advertisement_monitor_manager_client.h"

namespace bluez {

FakeBluetoothAdvertisementMonitorApplicationServiceProvider::
    FakeBluetoothAdvertisementMonitorApplicationServiceProvider(
        const dbus::ObjectPath& object_path) {
  FakeBluetoothAdvertisementMonitorManagerClient*
      fake_bluetooth_advertisement_monitor_manager_client =
          static_cast<FakeBluetoothAdvertisementMonitorManagerClient*>(
              bluez::BluezDBusManager::Get()
                  ->GetBluetoothAdvertisementMonitorManagerClient());

  fake_bluetooth_advertisement_monitor_manager_client
      ->RegisterApplicationServiceProvider(this);
}

FakeBluetoothAdvertisementMonitorApplicationServiceProvider::
    ~FakeBluetoothAdvertisementMonitorApplicationServiceProvider() = default;

void FakeBluetoothAdvertisementMonitorApplicationServiceProvider::AddMonitor(
    std::unique_ptr<BluetoothAdvertisementMonitorServiceProvider>
        advertisement_monitor_service_provider) {
  advertisement_monitor_providers_.insert(std::make_pair(
      advertisement_monitor_service_provider->object_path().value(),
      std::move(advertisement_monitor_service_provider)));
}

void FakeBluetoothAdvertisementMonitorApplicationServiceProvider::RemoveMonitor(
    const dbus::ObjectPath& monitor_path) {
  advertisement_monitor_providers_.erase(monitor_path.value());
}

size_t FakeBluetoothAdvertisementMonitorApplicationServiceProvider::
    AdvertisementMonitorsCount() const {
  return advertisement_monitor_providers_.size();
}

}  // namespace bluez
