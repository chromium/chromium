// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_TEST_FAKE_LOCAL_GATT_CHARACTERISTIC_H_
#define DEVICE_BLUETOOTH_TEST_FAKE_LOCAL_GATT_CHARACTERISTIC_H_

#include "device/bluetooth/bluetooth_local_gatt_characteristic.h"
#include "device/bluetooth/public/cpp/bluetooth_uuid.h"

namespace device {
class BluetoothLocalGattService;
}  // namespace device

namespace bluetooth {

// Implements device::BluetoothLocalGattCharacteristics. Meant to be used
// by FakeLocalGattService to keep track of the characteristic's state and
// attributes. Not intended for direct use by clients.
class FakeLocalGattCharacteristic
    : public device::BluetoothLocalGattCharacteristic {
 public:
  FakeLocalGattCharacteristic(const std::string& characteristic_id,
                              const device::BluetoothUUID& characteristic_uuid,
                              device::BluetoothLocalGattService* service,
                              Properties properties,
                              Permissions permissions);
  ~FakeLocalGattCharacteristic() override;

  // device::BluetoothGattCharacteristic:
  std::string GetIdentifier() const override;
  device::BluetoothUUID GetUUID() const override;
  Properties GetProperties() const override;
  Permissions GetPermissions() const override;

  // device::BluetoothLocalGattCharacteristic:
  NotificationStatus NotifyValueChanged(const device::BluetoothDevice* device,
                                        const std::vector<uint8_t>& new_value,
                                        bool indicate) override;
  device::BluetoothLocalGattService* GetService() const override;
  std::vector<device::BluetoothLocalGattDescriptor*> GetDescriptors()
      const override;

  base::WeakPtr<FakeLocalGattCharacteristic> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  Properties properties_ = Property::PROPERTY_NONE;
  Permissions permissions_ = Permission::PERMISSION_NONE;
  const std::string characteristic_id_;
  const device::BluetoothUUID characteristic_uuid_;
  raw_ptr<device::BluetoothLocalGattService> service_ = nullptr;
  std::vector<uint8_t> value_;
  base::WeakPtrFactory<FakeLocalGattCharacteristic> weak_ptr_factory_{this};
};

}  // namespace bluetooth

#endif  // DEVICE_BLUETOOTH_TEST_FAKE_LOCAL_GATT_CHARACTERISTIC_H_
