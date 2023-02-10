// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_FLOSS_BLUETOOTH_LOCAL_GATT_CHARACTERISTIC_FLOSS_H_
#define DEVICE_BLUETOOTH_FLOSS_BLUETOOTH_LOCAL_GATT_CHARACTERISTIC_FLOSS_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "device/bluetooth/bluetooth_gatt_characteristic.h"
#include "device/bluetooth/bluetooth_local_gatt_characteristic.h"
#include "device/bluetooth/floss/bluetooth_local_gatt_descriptor_floss.h"
#include "device/bluetooth/floss/bluetooth_local_gatt_service_floss.h"
#include "device/bluetooth/public/cpp/bluetooth_uuid.h"

namespace floss {

class BluetoothLocalGattDescriptorFloss;

// The BluetoothLocalGattCharacteristicFloss class implements
// BluetoothLocalGattCharacteristic for local GATT characteristics for
// platforms that use Floss.
class DEVICE_BLUETOOTH_EXPORT BluetoothLocalGattCharacteristicFloss
    : public device::BluetoothLocalGattCharacteristic {
 public:
  static base::WeakPtr<BluetoothLocalGattCharacteristicFloss> Create(
      const device::BluetoothUUID& uuid,
      Properties properties,
      Permissions permissions,
      BluetoothLocalGattServiceFloss* service);

  BluetoothLocalGattCharacteristicFloss(
      const BluetoothLocalGattCharacteristicFloss&) = delete;
  BluetoothLocalGattCharacteristicFloss& operator=(
      const BluetoothLocalGattCharacteristicFloss&) = delete;

  ~BluetoothLocalGattCharacteristicFloss() override;

  // device::BluetoothGattCharacteristic overrides:
  std::string GetIdentifier() const override;
  device::BluetoothUUID GetUUID() const override;
  Properties GetProperties() const override;
  Permissions GetPermissions() const override;

  // device::BluetoothLocalGattCharacteristic overrides:
  NotificationStatus NotifyValueChanged(const device::BluetoothDevice* device,
                                        const std::vector<uint8_t>& new_value,
                                        bool indicate) override;
  device::BluetoothLocalGattService* GetService() const override;

  const std::vector<std::unique_ptr<BluetoothLocalGattDescriptorFloss>>&
  GetDescriptors() const;

 private:
  friend class BluetoothLocalGattDescriptorFloss;

  BluetoothLocalGattCharacteristicFloss(
      const device::BluetoothUUID& uuid,
      Properties properties,
      Permissions permissions,
      BluetoothLocalGattServiceFloss* service);

  // Adds a descriptor to this characteristic.
  void AddDescriptor(
      std::unique_ptr<BluetoothLocalGattDescriptorFloss> descriptor);

  // Characteristic represented by this class.
  GattCharacteristic characteristic_;

  // Service that contains this characteristic.
  raw_ref<BluetoothLocalGattServiceFloss> service_;

  // Descriptors contained by this characteristic.
  std::vector<std::unique_ptr<BluetoothLocalGattDescriptorFloss>> descriptors_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<BluetoothLocalGattCharacteristicFloss> weak_ptr_factory_{
      this};
};

}  // namespace floss

#endif  // DEVICE_BLUETOOTH_FLOSS_BLUETOOTH_LOCAL_GATT_CHARACTERISTIC_FLOSS_H_
