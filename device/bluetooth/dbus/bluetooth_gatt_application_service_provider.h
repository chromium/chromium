// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_DBUS_BLUETOOTH_GATT_APPLICATION_SERVICE_PROVIDER_H_
#define DEVICE_BLUETOOTH_DBUS_BLUETOOTH_GATT_APPLICATION_SERVICE_PROVIDER_H_

#include <map>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "dbus/bus.h"
#include "dbus/object_path.h"
#include "device/bluetooth/bluetooth_export.h"
#include "device/bluetooth/bluez/bluetooth_local_gatt_service_bluez.h"
#include "device/bluetooth/dbus/bluetooth_gatt_characteristic_service_provider.h"
#include "device/bluetooth/dbus/bluetooth_gatt_descriptor_service_provider.h"
#include "device/bluetooth/dbus/bluetooth_gatt_service_service_provider.h"

namespace bluez {

class BluetoothLocalGattServiceBlueZ;

// BluetoothGattApplicationServiceProvider is used to provide a D-Bus object
// that the Bluetooth daemon can communicate with to register GATT service
// hierarchies.
class DEVICE_BLUETOOTH_EXPORT BluetoothGattApplicationServiceProvider {
 public:
  BluetoothGattApplicationServiceProvider(
      const BluetoothGattApplicationServiceProvider&) = delete;
  BluetoothGattApplicationServiceProvider& operator=(
      const BluetoothGattApplicationServiceProvider&) = delete;

  virtual ~BluetoothGattApplicationServiceProvider();

  // Creates individual service providers for all the attributes managed by the
  // object manager interface implemented by this application service provider.
  void CreateAttributeServiceProviders(
      dbus::Bus* bus,
      const std::map<dbus::ObjectPath,
                     raw_ptr<BluetoothLocalGattServiceBlueZ, CtnExperimental>>&
          services);

  // Creates the instance where |bus| is the D-Bus bus connection to export the
  // object onto, |object_path| is the object path that it should have, |uuid|
  // is the 128-bit GATT service UUID, and |includes| are a list of object paths
  // belonging to other exported GATT services that are included by the GATT
  // service being created. Make sure that all included services have been
  // exported before registering a GATT services with the GATT manager.
  static std::unique_ptr<BluetoothGattApplicationServiceProvider> Create(
      dbus::Bus* bus,
      const dbus::ObjectPath& object_path,
      const std::map<dbus::ObjectPath,
                     raw_ptr<BluetoothLocalGattServiceBlueZ, CtnExperimental>>&
          services);

  void SendValueChanged(const dbus::ObjectPath& characteristic_path,
                        const std::vector<uint8_t>& value);

 protected:
  BluetoothGattApplicationServiceProvider();

  // List of GATT Service service providers managed by this object manager.
  std::vector<std::unique_ptr<BluetoothGattServiceServiceProvider>>
      service_providers_;
  // List of GATT Characteristic service providers managed by this object
  // manager.
  std::vector<std::unique_ptr<BluetoothGattCharacteristicServiceProvider>>
      characteristic_providers_;
  // List of GATT Descriptor service providers managed by this object manager.
  std::vector<std::unique_ptr<BluetoothGattDescriptorServiceProvider>>
      descriptor_providers_;
};

}  // namespace bluez

#endif  // DEVICE_BLUETOOTH_DBUS_BLUETOOTH_GATT_APPLICATION_SERVICE_PROVIDER_H_
