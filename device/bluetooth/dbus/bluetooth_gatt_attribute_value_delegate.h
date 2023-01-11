// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_DBUS_BLUETOOTH_GATT_ATTRIBUTE_VALUE_DELEGATE_H_
#define DEVICE_BLUETOOTH_DBUS_BLUETOOTH_GATT_ATTRIBUTE_VALUE_DELEGATE_H_

#include <cstdint>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "device/bluetooth/bluetooth_gatt_characteristic.h"
#include "device/bluetooth/bluetooth_local_gatt_service.h"

namespace dbus {
class ObjectPath;
}

namespace device {
class BluetoothDevice;
}

namespace bluez {

class BluetoothLocalGattServiceBlueZ;

// A simpler interface for reacting to GATT attribute value requests by the
// DBus attribute service providers.
class BluetoothGattAttributeValueDelegate {
 public:
  explicit BluetoothGattAttributeValueDelegate(
      BluetoothLocalGattServiceBlueZ* service);

  BluetoothGattAttributeValueDelegate(
      const BluetoothGattAttributeValueDelegate&) = delete;
  BluetoothGattAttributeValueDelegate& operator=(
      const BluetoothGattAttributeValueDelegate&) = delete;

  virtual ~BluetoothGattAttributeValueDelegate();

  // This method will be called when a remote device requests to read the
  // value of the exported GATT attribute. Invoke |callback| with a value
  // to return that value to the requester. Invoke |error_callback| to report
  // a failure to read the value. This can happen, for example, if the
  // attribute has no read permission set. Either callback should be
  // invoked after a reasonable amount of time, since the request will time
  // out if left pending for too long causing a disconnection.
  virtual void GetValue(
      const dbus::ObjectPath& device_path,
      device::BluetoothLocalGattService::Delegate::ValueCallback callback) = 0;

  // This method will be called, when a remote device requests to write the
  // value of the exported GATT attribute. Invoke |callback| to report
  // that the value was successfully written. Invoke |error_callback| to
  // report a failure to write the value. This can happen, for example, if the
  // attribute has no write permission set. Either callback should be
  // invoked after a reasonable amount of time, since the request will time
  // out if left pending for too long causing a disconnection.
  virtual void SetValue(
      const dbus::ObjectPath& device_path,
      const std::vector<uint8_t>& value,
      base::OnceClosure callback,
      device::BluetoothLocalGattService::Delegate::ErrorCallback
          error_callback) = 0;

  // This method will be called, when a remote device requests to start sending
  // notifications for this characteristic. This will never be called for
  // descriptors.
  virtual void StartNotifications(
      const dbus::ObjectPath& device_path,
      device::BluetoothGattCharacteristic::NotificationType
          notification_type) = 0;

  // This method will be called, when a remote device requests to stop sending
  // notifications for this characteristic. This will never be called for
  // descriptors.
  virtual void StopNotifications(const dbus::ObjectPath& device_path) = 0;

  // This method will be called, when a remote device requests to prepare
  // write the value of the exported GATT characteristic. Invoke |callback| to
  // report that the request was successful. Invoke |error_callback| to report
  // a failure. This will never be called for descriptors.
  virtual void PrepareSetValue(
      const dbus::ObjectPath& device_path,
      const std::vector<uint8_t>& value,
      int offset,
      bool has_subsequent_request,
      base::OnceClosure callback,
      device::BluetoothLocalGattService::Delegate::ErrorCallback
          error_callback) {}

 protected:
  // Gets the Bluetooth device object on the current service's adapter with
  // the given object path.
  device::BluetoothDevice* GetDeviceWithPath(
      const dbus::ObjectPath& object_path);

  const BluetoothLocalGattServiceBlueZ* service() { return service_; }

 private:
  raw_ptr<const BluetoothLocalGattServiceBlueZ> service_;
};

}  // namespace bluez

#endif  // DEVICE_BLUETOOTH_DBUS_BLUETOOTH_GATT_ATTRIBUTE_VALUE_DELEGATE_H_
