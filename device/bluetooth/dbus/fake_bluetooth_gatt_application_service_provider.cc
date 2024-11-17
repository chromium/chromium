// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/dbus/fake_bluetooth_gatt_application_service_provider.h"

#include <string>

#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "device/bluetooth/dbus/bluez_dbus_manager.h"
#include "device/bluetooth/dbus/fake_bluetooth_gatt_manager_client.h"

namespace bluez {

FakeBluetoothGattApplicationServiceProvider::
    FakeBluetoothGattApplicationServiceProvider(
        const dbus::ObjectPath& object_path,
        const std::map<
            dbus::ObjectPath,
            raw_ptr<BluetoothLocalGattServiceBlueZ, CtnExperimental>>& services)
    : object_path_(object_path) {
  DVLOG(1) << "Creating Bluetooth GATT application: " << object_path_.value();

  FakeBluetoothGattManagerClient* fake_bluetooth_gatt_manager_client =
      static_cast<FakeBluetoothGattManagerClient*>(
          bluez::BluezDBusManager::Get()->GetBluetoothGattManagerClient());
  fake_bluetooth_gatt_manager_client->RegisterApplicationServiceProvider(this);

  BluetoothGattApplicationServiceProvider::CreateAttributeServiceProviders(
      nullptr, services);
}

FakeBluetoothGattApplicationServiceProvider::
    ~FakeBluetoothGattApplicationServiceProvider() {
  DVLOG(1) << "Cleaning up Bluetooth GATT application: "
           << object_path_.value();

  FakeBluetoothGattManagerClient* fake_bluetooth_gatt_manager_client =
      static_cast<FakeBluetoothGattManagerClient*>(
          bluez::BluezDBusManager::Get()->GetBluetoothGattManagerClient());
  fake_bluetooth_gatt_manager_client->UnregisterApplicationServiceProvider(
      this);
}

}  // namespace bluez
