// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef DEVICE_BLUETOOTH_TEST_FAKE_REMOTE_GATT_SERVICE_H_
#define DEVICE_BLUETOOTH_TEST_FAKE_REMOTE_GATT_SERVICE_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "device/bluetooth/bluetooth_remote_gatt_service.h"
#include "device/bluetooth/public/cpp/bluetooth_uuid.h"
#include "device/bluetooth/public/mojom/test/fake_bluetooth.mojom.h"
#include "device/bluetooth/test/fake_remote_gatt_characteristic.h"

namespace device {
class BluetoothDevice;
class BluetoothRemoteGattService;
}  // namespace device

namespace bluetooth {

// Implements device::BluetoothRemoteGattService. Meant to be used by
// FakePeripheral to keep track of the service's state and attributes.
//
// Not intended for direct use by clients.  See README.md.
class FakeRemoteGattService : public device::BluetoothRemoteGattService {
 public:
  FakeRemoteGattService(const std::string& service_id,
                        const device::BluetoothUUID& service_uuid,
                        bool is_primary,
                        device::BluetoothDevice* device);
  ~FakeRemoteGattService() override;

  // Returns true if there are no pending responses for any characterstics.
  bool AllResponsesConsumed();

  // Adds a fake characteristic with |characteristic_uuid| and |properties|
  // to this service. Returns the characteristic's Id.
  std::string AddFakeCharacteristic(
      const device::BluetoothUUID& characteristic_uuid,
      mojom::CharacteristicPropertiesPtr properties);

  // Removes a fake characteristic with |identifier| from this service.
  bool RemoveFakeCharacteristic(const std::string& identifier);

  // device::BluetoothGattService overrides:
  std::string GetIdentifier() const override;
  device::BluetoothUUID GetUUID() const override;
  bool IsPrimary() const override;

  // device::BluetoothRemoteGattService overrides:
  device::BluetoothDevice* GetDevice() const override;
  std::vector<device::BluetoothRemoteGattService*> GetIncludedServices()
      const override;

 private:
  const std::string service_id_;
  const device::BluetoothUUID service_uuid_;
  const bool is_primary_;
  device::BluetoothDevice* device_;

  size_t last_characteristic_id_ = 0;
};

}  // namespace bluetooth

#endif  // DEVICE_BLUETOOTH_TEST_FAKE_REMOTE_GATT_SERVICE_H_
