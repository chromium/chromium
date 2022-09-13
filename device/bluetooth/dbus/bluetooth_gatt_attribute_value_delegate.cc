// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/dbus/bluetooth_gatt_attribute_value_delegate.h"

#include "dbus/object_path.h"
#include "device/bluetooth/bluez/bluetooth_adapter_bluez.h"
#include "device/bluetooth/bluez/bluetooth_device_bluez.h"
#include "device/bluetooth/bluez/bluetooth_local_gatt_service_bluez.h"

namespace bluez {

BluetoothGattAttributeValueDelegate::BluetoothGattAttributeValueDelegate(
    BluetoothLocalGattServiceBlueZ* service)
    : service_(service) {}

BluetoothGattAttributeValueDelegate::~BluetoothGattAttributeValueDelegate() =
    default;

device::BluetoothDevice* BluetoothGattAttributeValueDelegate::GetDeviceWithPath(
    const dbus::ObjectPath& object_path) {
  return service_->GetAdapter()->GetDeviceWithPath(object_path);
}

}  // namespace bluez
