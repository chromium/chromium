// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/dbus/bluetooth_gatt_characteristic_service_provider.h"

#include "base/logging.h"
#include "device/bluetooth/dbus/bluetooth_gatt_characteristic_service_provider_impl.h"
#include "device/bluetooth/dbus/bluez_dbus_manager.h"
#include "device/bluetooth/dbus/fake_bluetooth_gatt_characteristic_service_provider.h"

namespace bluez {

BluetoothGattCharacteristicServiceProvider::
    BluetoothGattCharacteristicServiceProvider() = default;

BluetoothGattCharacteristicServiceProvider::
    ~BluetoothGattCharacteristicServiceProvider() = default;

// static
BluetoothGattCharacteristicServiceProvider*
BluetoothGattCharacteristicServiceProvider::Create(
    dbus::Bus* bus,
    const dbus::ObjectPath& object_path,
    std::unique_ptr<BluetoothGattAttributeValueDelegate> delegate,
    const std::string& uuid,
    const std::vector<std::string>& flags,
    const dbus::ObjectPath& service_path) {
  if (!bluez::BluezDBusManager::Get()->IsUsingFakes()) {
    return new BluetoothGattCharacteristicServiceProviderImpl(
        bus, object_path, std::move(delegate), uuid, flags, service_path);
  }
#if defined(USE_REAL_DBUS_CLIENTS)
  LOG(FATAL) << "Fake is unavailable if USE_REAL_DBUS_CLIENTS is defined.";
#else
  return new FakeBluetoothGattCharacteristicServiceProvider(
      object_path, std::move(delegate), uuid, flags, service_path);
#endif  // defined(USE_REAL_DBUS_CLIENTS)
}

}  // namespace bluez
