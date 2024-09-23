// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_BLUEZ_BLUETOOTH_LOCAL_GATT_CHARACTERISTIC_BLUEZ_H_
#define DEVICE_BLUETOOTH_BLUEZ_BLUETOOTH_LOCAL_GATT_CHARACTERISTIC_BLUEZ_H_

#include <cstdint>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "device/bluetooth/bluetooth_gatt_characteristic.h"
#include "device/bluetooth/bluetooth_local_gatt_characteristic.h"
#include "device/bluetooth/bluez/bluetooth_gatt_characteristic_bluez.h"
#include "device/bluetooth/bluez/bluetooth_local_gatt_descriptor_bluez.h"
#include "device/bluetooth/bluez/bluetooth_local_gatt_service_bluez.h"
#include "device/bluetooth/public/cpp/bluetooth_uuid.h"

namespace bluez {

class BluetoothLocalGattServiceBlueZ;
class BluetoothLocalGattDescriptorBlueZ;

// The BluetoothLocalGattCharacteristicBlueZ class implements
// BluetoothLocalGattCharacteristic for local GATT characteristics for
// platforms that use BlueZ.
class DEVICE_BLUETOOTH_EXPORT BluetoothLocalGattCharacteristicBlueZ
    : public BluetoothGattCharacteristicBlueZ,
      public device::BluetoothLocalGattCharacteristic {
 public:
  static base::WeakPtr<BluetoothLocalGattCharacteristicBlueZ> Create(
      const device::BluetoothUUID& uuid,
      Properties properties,
      Permissions permissions,
      BluetoothLocalGattServiceBlueZ* service);

  BluetoothLocalGattCharacteristicBlueZ(
      const BluetoothLocalGattCharacteristicBlueZ&) = delete;
  BluetoothLocalGattCharacteristicBlueZ& operator=(
      const BluetoothLocalGattCharacteristicBlueZ&) = delete;

  ~BluetoothLocalGattCharacteristicBlueZ() override;

  // device::BluetoothGattCharacteristic overrides:
  device::BluetoothUUID GetUUID() const override;
  Properties GetProperties() const override;
  Permissions GetPermissions() const override;

  // device::BluetoothLocalGattCharacteristic overrides:
  NotificationStatus NotifyValueChanged(const device::BluetoothDevice* device,
                                        const std::vector<uint8_t>& new_value,
                                        bool indicate) override;
  device::BluetoothLocalGattService* GetService() const override;
  std::vector<device::BluetoothLocalGattDescriptor*> GetDescriptors()
      const override;

 private:
  friend class BluetoothLocalGattDescriptorBlueZ;

BluetoothLocalGattCharacteristicBlueZ(
      const device::BluetoothUUID& uuid,
      Properties properties,
      Permissions permissions,
      BluetoothLocalGattServiceBlueZ* service);

  // Adds a descriptor to this characteristic.
  void AddDescriptor(
      std::unique_ptr<BluetoothLocalGattDescriptorBlueZ> descriptor);

  // UUID of this characteristic.
  device::BluetoothUUID uuid_;

  // Properties of this characteristic.
  Properties properties_;

  // Permissions of this characteristic.
  Permissions permissions_;

  // Service that contains this characteristic.
  raw_ptr<BluetoothLocalGattServiceBlueZ> service_;

  // Descriptors contained by this characteristic.
  std::vector<std::unique_ptr<BluetoothLocalGattDescriptorBlueZ>> descriptors_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<BluetoothLocalGattCharacteristicBlueZ> weak_ptr_factory_{
      this};
};

}  // namespace bluez

#endif  // DEVICE_BLUETOOTH_BLUEZ_BLUETOOTH_LOCAL_GATT_CHARACTERISTIC_BLUEZ_H_
