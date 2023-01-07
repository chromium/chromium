// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/dbus/fake_bluetooth_gatt_service_service_provider.h"

#include "base/logging.h"
#include "device/bluetooth/dbus/bluez_dbus_manager.h"
#include "device/bluetooth/dbus/fake_bluetooth_gatt_manager_client.h"

namespace bluez {

FakeBluetoothGattServiceServiceProvider::
    FakeBluetoothGattServiceServiceProvider(
        const dbus::ObjectPath& object_path,
        const std::string& uuid,
        const std::vector<dbus::ObjectPath>& includes)
    : object_path_(object_path), uuid_(uuid), includes_(includes) {
  DVLOG(1) << "Creating Bluetooth GATT service: " << object_path_.value();

  FakeBluetoothGattManagerClient* fake_bluetooth_gatt_manager_client =
      static_cast<FakeBluetoothGattManagerClient*>(
          bluez::BluezDBusManager::Get()->GetBluetoothGattManagerClient());
  fake_bluetooth_gatt_manager_client->RegisterServiceServiceProvider(this);
}

FakeBluetoothGattServiceServiceProvider::
    ~FakeBluetoothGattServiceServiceProvider() {
  DVLOG(1) << "Cleaning up Bluetooth GATT service: " << object_path_.value();

  FakeBluetoothGattManagerClient* fake_bluetooth_gatt_manager_client =
      static_cast<FakeBluetoothGattManagerClient*>(
          bluez::BluezDBusManager::Get()->GetBluetoothGattManagerClient());
  fake_bluetooth_gatt_manager_client->UnregisterServiceServiceProvider(this);
}

const dbus::ObjectPath& FakeBluetoothGattServiceServiceProvider::object_path()
    const {
  return object_path_;
}

}  // namespace bluez
