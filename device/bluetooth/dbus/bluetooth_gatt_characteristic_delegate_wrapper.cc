// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/dbus/bluetooth_gatt_characteristic_delegate_wrapper.h"

#include "base/logging.h"
#include "device/bluetooth/bluez/bluetooth_local_gatt_characteristic_bluez.h"

namespace bluez {

BluetoothGattCharacteristicDelegateWrapper::
    BluetoothGattCharacteristicDelegateWrapper(
        BluetoothLocalGattServiceBlueZ* service,
        BluetoothLocalGattCharacteristicBlueZ* characteristic)
    : BluetoothGattAttributeValueDelegate(service),
      characteristic_(characteristic) {}

void BluetoothGattCharacteristicDelegateWrapper::GetValue(
    const dbus::ObjectPath& device_path,
    device::BluetoothLocalGattService::Delegate::ValueCallback callback) {
  device::BluetoothDevice* device = GetDeviceWithPath(device_path);
  if (!device) {
    LOG(WARNING) << "Bluetooth device not found: " << device_path.value();
    return;
  }
  service()->GetDelegate()->OnCharacteristicReadRequest(device, characteristic_,
                                                        0, std::move(callback));
}

void BluetoothGattCharacteristicDelegateWrapper::SetValue(
    const dbus::ObjectPath& device_path,
    const std::vector<uint8_t>& value,
    base::OnceClosure callback,
    device::BluetoothLocalGattService::Delegate::ErrorCallback error_callback) {
  device::BluetoothDevice* device = GetDeviceWithPath(device_path);
  if (!device) {
    LOG(WARNING) << "Bluetooth device not found: " << device_path.value();
    return;
  }
  service()->GetDelegate()->OnCharacteristicWriteRequest(
      device, characteristic_, value, 0, std::move(callback),
      std::move(error_callback));
}

void BluetoothGattCharacteristicDelegateWrapper::StartNotifications(
    const dbus::ObjectPath& device_path,
    device::BluetoothGattCharacteristic::NotificationType notification_type) {
  device::BluetoothDevice* device = GetDeviceWithPath(device_path);
  if (!device) {
    LOG(WARNING) << "Bluetooth device not found: " << device_path.value();
    return;
  }
  service()->GetDelegate()->OnNotificationsStart(device, notification_type,
                                                 characteristic_);
}

void BluetoothGattCharacteristicDelegateWrapper::StopNotifications(
    const dbus::ObjectPath& device_path) {
  device::BluetoothDevice* device = GetDeviceWithPath(device_path);
  if (!device) {
    LOG(WARNING) << "Bluetooth device not found: " << device_path.value();
    return;
  }
  service()->GetDelegate()->OnNotificationsStop(device, characteristic_);
}

void BluetoothGattCharacteristicDelegateWrapper::PrepareSetValue(
    const dbus::ObjectPath& device_path,
    const std::vector<uint8_t>& value,
    int offset,
    bool has_subsequent_request,
    base::OnceClosure callback,
    device::BluetoothLocalGattService::Delegate::ErrorCallback error_callback) {
  device::BluetoothDevice* device = GetDeviceWithPath(device_path);
  if (!device) {
    LOG(WARNING) << "Bluetooth device not found: " << device_path.value();
    return;
  }
  service()->GetDelegate()->OnCharacteristicPrepareWriteRequest(
      device, characteristic_, value, offset, has_subsequent_request,
      std::move(callback), std::move(error_callback));
}

}  // namespace bluez
