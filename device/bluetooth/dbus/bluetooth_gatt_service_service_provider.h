// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_DBUS_BLUETOOTH_GATT_SERVICE_SERVICE_PROVIDER_H_
#define DEVICE_BLUETOOTH_DBUS_BLUETOOTH_GATT_SERVICE_SERVICE_PROVIDER_H_

#include <string>
#include <vector>

#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_path.h"
#include "device/bluetooth/bluetooth_export.h"

namespace bluez {

// BluetoothGattServiceServiceProvider is used to provide a D-Bus object that
// the Bluetooth daemon can communicate with to register GATT service
// hierarchies.
//
// Instantiate with a chosen D-Bus object path (that conforms to the BlueZ GATT
// service specification), service UUID, and the list of included services, and
// pass the D-Bus object path as the |service_path| argument to the
// bluez::BluetoothGattManagerClient::RegisterService method. Make sure to
// create characteristic and descriptor objects using the appropriate service
// providers before registering a GATT service with the Bluetooth daemon.
class DEVICE_BLUETOOTH_EXPORT BluetoothGattServiceServiceProvider {
 public:
  BluetoothGattServiceServiceProvider(
      const BluetoothGattServiceServiceProvider&) = delete;
  BluetoothGattServiceServiceProvider& operator=(
      const BluetoothGattServiceServiceProvider&) = delete;

  virtual ~BluetoothGattServiceServiceProvider();

  // Writes an array of the service's properties into the provided writer.
  virtual void WriteProperties(dbus::MessageWriter* writer) {}

  virtual const dbus::ObjectPath& object_path() const = 0;

  // Creates the instance where |bus| is the D-Bus bus connection to export the
  // object onto, |object_path| is the object path that it should have, |uuid|
  // is the 128-bit GATT service UUID, and |includes| are a list of object paths
  // belonging to other exported GATT services that are included by the GATT
  // service being created. Make sure that all included services have been
  // exported before registering a GATT services with the GATT manager.
  static BluetoothGattServiceServiceProvider* Create(
      dbus::Bus* bus,
      const dbus::ObjectPath& object_path,
      const std::string& uuid,
      bool is_primary,
      const std::vector<dbus::ObjectPath>& includes);

 protected:
  BluetoothGattServiceServiceProvider();
};

}  // namespace bluez

#endif  // DEVICE_BLUETOOTH_DBUS_BLUETOOTH_GATT_SERVICE_SERVICE_PROVIDER_H_
