// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_DBUS_BLUETOOTH_GATT_DESCRIPTOR_DELEGATE_WRAPPER_H_
#define DEVICE_BLUETOOTH_DBUS_BLUETOOTH_GATT_DESCRIPTOR_DELEGATE_WRAPPER_H_

#include <cstdint>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "device/bluetooth/bluetooth_gatt_characteristic.h"
#include "device/bluetooth/bluetooth_local_gatt_service.h"
#include "device/bluetooth/bluez/bluetooth_gatt_service_bluez.h"
#include "device/bluetooth/bluez/bluetooth_local_gatt_service_bluez.h"
#include "device/bluetooth/dbus/bluetooth_gatt_attribute_value_delegate.h"

namespace bluez {

class BluetoothLocalGattDescriptorBlueZ;

// Wrapper class around AttributeValueDelegate to handle descriptors.
class BluetoothGattDescriptorDelegateWrapper
    : public BluetoothGattAttributeValueDelegate {
 public:
  BluetoothGattDescriptorDelegateWrapper(
      BluetoothLocalGattServiceBlueZ* service,
      BluetoothLocalGattDescriptorBlueZ* descriptor);

  BluetoothGattDescriptorDelegateWrapper(
      const BluetoothGattDescriptorDelegateWrapper&) = delete;
  BluetoothGattDescriptorDelegateWrapper& operator=(
      const BluetoothGattDescriptorDelegateWrapper&) = delete;

  // BluetoothGattAttributeValueDelegate overrides:
  void GetValue(const dbus::ObjectPath& device_path,
                device::BluetoothLocalGattService::Delegate::ValueCallback
                    callback) override;
  void SetValue(const dbus::ObjectPath& device_path,
                const std::vector<uint8_t>& value,
                base::OnceClosure callback,
                device::BluetoothLocalGattService::Delegate::ErrorCallback
                    error_callback) override;

  void StartNotifications(const dbus::ObjectPath& device_path,
                          device::BluetoothGattCharacteristic::NotificationType
                              notification_type) override {}
  void StopNotifications(const dbus::ObjectPath& device_path) override {}

 private:
  raw_ptr<BluetoothLocalGattDescriptorBlueZ> descriptor_;
};

}  // namespace bluez

#endif  // DEVICE_BLUETOOTH_DBUS_BLUETOOTH_GATT_DESCRIPTOR_DELEGATE_WRAPPER_H_
