// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/dbus/bluetooth_gatt_descriptor_delegate_wrapper.h"

#include "device/bluetooth/bluez/bluetooth_local_gatt_descriptor_bluez.h"

namespace bluez {

BluetoothGattDescriptorDelegateWrapper::BluetoothGattDescriptorDelegateWrapper(
    BluetoothLocalGattServiceBlueZ* service,
    BluetoothLocalGattDescriptorBlueZ* descriptor)
    : BluetoothGattAttributeValueDelegate(service), descriptor_(descriptor) {}

void BluetoothGattDescriptorDelegateWrapper::GetValue(
    const dbus::ObjectPath& device_path,
    device::BluetoothLocalGattService::Delegate::ValueCallback callback,
    device::BluetoothLocalGattService::Delegate::ErrorCallback error_callback) {
  service()->GetDelegate()->OnDescriptorReadRequest(
      GetDeviceWithPath(device_path), descriptor_, 0, std::move(callback),
      std::move(error_callback));
}

void BluetoothGattDescriptorDelegateWrapper::SetValue(
    const dbus::ObjectPath& device_path,
    const std::vector<uint8_t>& value,
    base::OnceClosure callback,
    device::BluetoothLocalGattService::Delegate::ErrorCallback error_callback) {
  service()->GetDelegate()->OnDescriptorWriteRequest(
      GetDeviceWithPath(device_path), descriptor_, value, 0,
      std::move(callback), std::move(error_callback));
}

}  // namespace bluez
