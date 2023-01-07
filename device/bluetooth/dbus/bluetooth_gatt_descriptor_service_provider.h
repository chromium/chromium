// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_DBUS_BLUETOOTH_GATT_DESCRIPTOR_SERVICE_PROVIDER_H_
#define DEVICE_BLUETOOTH_DBUS_BLUETOOTH_GATT_DESCRIPTOR_SERVICE_PROVIDER_H_

#include <stdint.h>
#include <string>
#include <vector>

#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_path.h"
#include "device/bluetooth/bluetooth_export.h"
#include "device/bluetooth/dbus/bluetooth_gatt_attribute_value_delegate.h"

namespace bluez {

// BluetoothGattDescriptorServiceProvider is used to provide a D-Bus object that
// represents a local GATT characteristic descriptor that the Bluetooth daemon
// can communicate with.
//
// Instantiate with a chosen D-Bus object path, delegate, and other fields.
// The Bluetooth daemon communicates with a GATT descriptor using the
// standard DBus.Properties interface. While most properties of the GATT
// descriptor interface are read-only and don't change throughout the
// life-time of the object, the "Value" property is both writeable and its
// value can change. Both Get and Set operations performed on the "Value"
// property are delegated to the Delegate object, an instance of which is
// mandatory during initialization. In addition, a "SendValueChanged" method is
// provided, which emits a DBus.Properties.PropertyChanged signal for the
// "Value" property.
class DEVICE_BLUETOOTH_EXPORT BluetoothGattDescriptorServiceProvider {
 public:
  BluetoothGattDescriptorServiceProvider(
      const BluetoothGattDescriptorServiceProvider&) = delete;
  BluetoothGattDescriptorServiceProvider& operator=(
      const BluetoothGattDescriptorServiceProvider&) = delete;

  virtual ~BluetoothGattDescriptorServiceProvider();

  // Send a PropertyChanged signal to notify the Bluetooth daemon that the value
  // of the "Value" property has changed to |value|.
  virtual void SendValueChanged(const std::vector<uint8_t>& value) = 0;

  // Writes the descriptor's properties into the provided writer. If
  // value is not null, it is written also, otherwise no value property is
  // written.
  virtual void WriteProperties(dbus::MessageWriter* writer) {}

  virtual const dbus::ObjectPath& object_path() const = 0;

  // Creates the instance, where |bus| is the D-Bus bus connection to export
  // the object onto, |uuid| is the 128-bit GATT descriptor UUID, |flags|
  // is the list of attribute permissions, |characteristic_path| is the object
  // path of the exported GATT characteristic the descriptor belongs to,
  // |object_path| is the object path that the descriptor should have, and
  // |delegate| is the object that value Get/Set requests will be passed to and
  // responses generated from.
  //
  // Object paths of GATT descriptors must be hierarchical to the path of the
  // GATT characteristic they belong to. Hence, |object_path| must have
  // |characteristic_path| as its prefix. Ownership of |delegate| is not taken,
  // thus the delegate should outlive this instance. A delegate should handle
  // only a single exported descriptor and own it.
  static BluetoothGattDescriptorServiceProvider* Create(
      dbus::Bus* bus,
      const dbus::ObjectPath& object_path,
      std::unique_ptr<BluetoothGattAttributeValueDelegate> delegate,
      const std::string& uuid,
      const std::vector<std::string>& flags,
      const dbus::ObjectPath& characteristic_path);

 protected:
  BluetoothGattDescriptorServiceProvider();
};

}  // namespace bluez

#endif  // DEVICE_BLUETOOTH_DBUS_BLUETOOTH_GATT_DESCRIPTOR_SERVICE_PROVIDER_H_
