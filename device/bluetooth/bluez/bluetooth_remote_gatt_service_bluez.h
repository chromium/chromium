// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_BLUEZ_BLUETOOTH_REMOTE_GATT_SERVICE_BLUEZ_H_
#define DEVICE_BLUETOOTH_BLUEZ_BLUETOOTH_REMOTE_GATT_SERVICE_BLUEZ_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "dbus/object_path.h"
#include "device/bluetooth/bluetooth_remote_gatt_service.h"
#include "device/bluetooth/bluez/bluetooth_gatt_service_bluez.h"
#include "device/bluetooth/dbus/bluetooth_gatt_characteristic_client.h"
#include "device/bluetooth/dbus/bluetooth_gatt_service_client.h"
#include "device/bluetooth/public/cpp/bluetooth_uuid.h"

namespace device {

class BluetoothDevice;

}  // namespace device

namespace bluez {

class BluetoothAdapterBlueZ;
class BluetoothDeviceBlueZ;
class BluetoothRemoteGattCharacteristicBlueZ;
class BluetoothRemoteGattDescriptorBlueZ;

// The BluetoothRemoteGattServiceBlueZ class implements BluetootGattService
// for remote GATT services for platforms that use BlueZ.
class BluetoothRemoteGattServiceBlueZ
    : public BluetoothGattServiceBlueZ,
      public BluetoothGattServiceClient::Observer,
      public BluetoothGattCharacteristicClient::Observer,
      public device::BluetoothRemoteGattService {
 public:
  BluetoothRemoteGattServiceBlueZ(const BluetoothRemoteGattServiceBlueZ&) =
      delete;
  BluetoothRemoteGattServiceBlueZ& operator=(
      const BluetoothRemoteGattServiceBlueZ&) = delete;

  ~BluetoothRemoteGattServiceBlueZ() override;

  // device::BluetoothRemoteGattService overrides.
  device::BluetoothUUID GetUUID() const override;
  device::BluetoothDevice* GetDevice() const override;
  bool IsPrimary() const override;
  std::vector<device::BluetoothRemoteGattService*> GetIncludedServices()
      const override;

  // Notifies its observers that the GATT service has changed. This is mainly
  // used by BluetoothRemoteGattCharacteristicBlueZ instances to notify
  // service observers when characteristic descriptors get added and removed.
  void NotifyServiceChanged();

  // Notifies its observers that a descriptor |descriptor| belonging to
  // characteristic |characteristic| has been added or removed. This is used
  // by BluetoothRemoteGattCharacteristicBlueZ instances to notify service
  // observers when characteristic descriptors get added and removed. If |added|
  // is true, an "Added" event will be sent. Otherwise, a "Removed" event will
  // be sent.
  void NotifyDescriptorAddedOrRemoved(
      BluetoothRemoteGattCharacteristicBlueZ* characteristic,
      BluetoothRemoteGattDescriptorBlueZ* descriptor,
      bool added);

  // Notifies its observers that the value of a descriptor has changed. Called
  // by BluetoothRemoteGattCharacteristicBlueZ instances to notify service
  // observers.
  void NotifyDescriptorValueChanged(
      BluetoothRemoteGattCharacteristicBlueZ* characteristic,
      BluetoothRemoteGattDescriptorBlueZ* descriptor,
      const std::vector<uint8_t>& value);

 private:
  friend class BluetoothDeviceBlueZ;

  BluetoothRemoteGattServiceBlueZ(BluetoothAdapterBlueZ* adapter,
                                  BluetoothDeviceBlueZ* device,
                                  const dbus::ObjectPath& object_path);

  // bluez::BluetoothGattServiceClient::Observer override.
  void GattServicePropertyChanged(const dbus::ObjectPath& object_path,
                                  const std::string& property_name) override;

  // bluez::BluetoothGattCharacteristicClient::Observer override.
  void GattCharacteristicAdded(const dbus::ObjectPath& object_path) override;
  void GattCharacteristicRemoved(const dbus::ObjectPath& object_path) override;
  void GattCharacteristicPropertyChanged(
      const dbus::ObjectPath& object_path,
      const std::string& property_name) override;

  // The device this GATT service belongs to. It's ok to store a raw pointer
  // here since |device_| owns this instance.
  raw_ptr<BluetoothDeviceBlueZ> device_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<BluetoothRemoteGattServiceBlueZ> weak_ptr_factory_{this};
};

}  // namespace bluez

#endif  // DEVICE_BLUETOOTH_BLUEZ_BLUETOOTH_REMOTE_GATT_SERVICE_BLUEZ_H_
