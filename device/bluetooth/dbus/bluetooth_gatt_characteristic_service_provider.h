// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_DBUS_BLUETOOTH_GATT_CHARACTERISTIC_SERVICE_PROVIDER_H_
#define DEVICE_BLUETOOTH_DBUS_BLUETOOTH_GATT_CHARACTERISTIC_SERVICE_PROVIDER_H_

#include <stdint.h>
#include <string>
#include <vector>

#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_path.h"
#include "device/bluetooth/bluetooth_export.h"
#include "device/bluetooth/dbus/bluetooth_gatt_attribute_value_delegate.h"

namespace bluez {

// BluetoothGattCharacteristicServiceProvider is used to provide a D-Bus object
// that represents a local GATT characteristic that the Bluetooth daemon can
// communicate with.
//
// Instantiate with a chosen D-Bus object path, delegate, and other fields.
// The Bluetooth daemon communicates with a GATT characteristic using the
// standard DBus.Properties interface. While most properties of the GATT
// characteristic interface are read-only and don't change throughout the
// life-time of the object, the "Value" property is both writeable and its
// value can change. Both Get and Set operations performed on the "Value"
// property are delegated to the Delegate object, an instance of which is
// mandatory during initialization. In addition, a "SendValueChanged" method is
// provided, which emits a DBus.Properties.PropertyChanged signal for the
// "Value" property.
class DEVICE_BLUETOOTH_EXPORT BluetoothGattCharacteristicServiceProvider {
 public:
  BluetoothGattCharacteristicServiceProvider(
      const BluetoothGattCharacteristicServiceProvider&) = delete;
  BluetoothGattCharacteristicServiceProvider& operator=(
      const BluetoothGattCharacteristicServiceProvider&) = delete;

  virtual ~BluetoothGattCharacteristicServiceProvider();

  // Send a PropertyChanged signal to notify the Bluetooth daemon that the value
  // of the "Value" property has changed to |value|.
  virtual void SendValueChanged(const std::vector<uint8_t>& value) = 0;

  // Writes the characteristics's properties into the provided writer. If
  // value is not null, it is written also, otherwise no value property is
  // written.
  virtual void WriteProperties(dbus::MessageWriter* writer) {}

  virtual const dbus::ObjectPath& object_path() const = 0;

  // Creates the instance, where |bus| is the D-Bus bus connection to export
  // the object onto, |uuid| is the 128-bit GATT characteristic UUID,
  // |flags| is the list of GATT characteristic properties, |flags| is the
  // list of flags for this characteristic, |service_path| is the object path
  // of the exported GATT service the characteristic belongs to, |object_path|
  // is the object path that the characteristic should have, and |delegate| is
  // the object that "Value" Get/Set requests will be passed to and responses
  // generated from.
  //
  // Object paths of GATT characteristics must be hierarchical to the path of
  // the GATT service they belong to. Hence, |object_path| must have
  // |service_path| as its prefix. Ownership of |delegate| is not taken, thus
  // the delegate should outlive this instance. A delegate should handle only
  // a single exported characteristic and own it.
  static BluetoothGattCharacteristicServiceProvider* Create(
      dbus::Bus* bus,
      const dbus::ObjectPath& object_path,
      std::unique_ptr<BluetoothGattAttributeValueDelegate> delegate,
      const std::string& uuid,
      const std::vector<std::string>& flags,
      const dbus::ObjectPath& service_path);

 protected:
  BluetoothGattCharacteristicServiceProvider();
};

}  // namespace bluez

#endif  // DEVICE_BLUETOOTH_DBUS_BLUETOOTH_GATT_CHARACTERISTIC_SERVICE_PROVIDER_H_
