// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluez/bluetooth_gatt_characteristic_bluez.h"

#include "device/bluetooth/bluez/bluetooth_gatt_descriptor_bluez.h"
#include "device/bluetooth/bluez/bluetooth_gatt_service_bluez.h"

namespace bluez {

BluetoothGattCharacteristicBlueZ::BluetoothGattCharacteristicBlueZ(
    dbus::ObjectPath object_path)
    : object_path_(object_path) {}

BluetoothGattCharacteristicBlueZ::~BluetoothGattCharacteristicBlueZ() = default;

std::string BluetoothGattCharacteristicBlueZ::GetIdentifier() const {
  return object_path_.value();
}

}  // namespace bluez
