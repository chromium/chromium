// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/dbus/fake_bluetooth_le_advertisement_service_provider.h"

#include "base/logging.h"
#include "device/bluetooth/dbus/bluez_dbus_manager.h"
#include "device/bluetooth/dbus/fake_bluetooth_le_advertising_manager_client.h"

namespace bluez {

FakeBluetoothLEAdvertisementServiceProvider::
    FakeBluetoothLEAdvertisementServiceProvider(
        const dbus::ObjectPath& object_path,
        Delegate* delegate)
    : delegate_(delegate) {
  object_path_ = object_path;
  DVLOG(1) << "Creating Bluetooth Advertisement: " << object_path_.value();

  FakeBluetoothLEAdvertisingManagerClient*
      fake_bluetooth_profile_manager_client =
          static_cast<FakeBluetoothLEAdvertisingManagerClient*>(
              bluez::BluezDBusManager::Get()
                  ->GetBluetoothLEAdvertisingManagerClient());
  fake_bluetooth_profile_manager_client->RegisterAdvertisementServiceProvider(
      this);
}

FakeBluetoothLEAdvertisementServiceProvider::
    ~FakeBluetoothLEAdvertisementServiceProvider() {
  DVLOG(1) << "Cleaning up Bluetooth Advertisement: " << object_path_.value();

  FakeBluetoothLEAdvertisingManagerClient*
      fake_bluetooth_profile_manager_client =
          static_cast<FakeBluetoothLEAdvertisingManagerClient*>(
              bluez::BluezDBusManager::Get()
                  ->GetBluetoothLEAdvertisingManagerClient());
  fake_bluetooth_profile_manager_client->UnregisterAdvertisementServiceProvider(
      this);
}

void FakeBluetoothLEAdvertisementServiceProvider::Release() {
  DVLOG(1) << object_path_.value() << ": Release";
  delegate_->Released();
}

}  // namespace bluez
