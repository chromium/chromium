// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_TEST_FAKE_LOCAL_GATT_SERVICE_H_
#define DEVICE_BLUETOOTH_TEST_FAKE_LOCAL_GATT_SERVICE_H_

#include "base/containers/flat_map.h"
#include "base/memory/weak_ptr.h"
#include "device/bluetooth/bluetooth_local_gatt_characteristic.h"
#include "device/bluetooth/bluetooth_local_gatt_service.h"
#include "device/bluetooth/public/cpp/bluetooth_uuid.h"
#include "device/bluetooth/test/fake_local_gatt_characteristic.h"

namespace bluetooth {

// Implements device::BluetoothLocalGattService to be used in testing.
// Not intended for direct use by clients.
class FakeLocalGattService : public device::BluetoothLocalGattService {
 public:
  FakeLocalGattService(const std::string& service_id,
                       const device::BluetoothUUID& service_uuid,
                       bool is_primary);
  ~FakeLocalGattService() override;

  // Adds a fake characteristic with |characteristic_uuid|
  // to this service.
  FakeLocalGattCharacteristic* AddFakeCharacteristic(
      const std::string& characteristic_id,
      const device::BluetoothUUID& characteristic_uuid,
      device::BluetoothGattCharacteristic::Properties properties =
          device::BluetoothGattCharacteristic::Property::PROPERTY_NONE,
      device::BluetoothGattCharacteristic::Permissions permissions =
          device::BluetoothGattCharacteristic::Permission::PERMISSION_NONE);

  // device::BluetoothGattService:
  std::string GetIdentifier() const override;
  device::BluetoothUUID GetUUID() const override;
  bool IsPrimary() const override;

  // device::BluetoothLocalGattService:
  void Register(base::OnceClosure callback,
                ErrorCallback error_callback) override;
  void Unregister(base::OnceClosure callback,
                  ErrorCallback error_callback) override;
  bool IsRegistered() override;
  void Delete() override;
  device::BluetoothLocalGattCharacteristic* GetCharacteristic(
      const std::string& identifier) override;
  base::WeakPtr<device::BluetoothLocalGattCharacteristic> CreateCharacteristic(
      const device::BluetoothUUID& uuid,
      device::BluetoothGattCharacteristic::Properties properties,
      device::BluetoothGattCharacteristic::Permissions permissions) override;

  base::WeakPtr<FakeLocalGattService> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  void set_should_create_local_gatt_characteristic_succeed(bool success) {
    should_create_local_gatt_characteristic_succeed_ = success;
  }

  bool WasDeleted() { return deleted_; }

  void set_on_deleted_callback(base::OnceClosure callback) {
    on_deleted_callback_ = std::move(callback);
  }

  void set_should_registration_succeed(bool success) {
    set_should_registration_succeed_ = success;
  }

 private:
  bool deleted_ = false;
  base::OnceClosure on_deleted_callback_;
  bool should_create_local_gatt_characteristic_succeed_ = true;
  bool set_should_registration_succeed_ = true;
  const std::string service_id_;
  const device::BluetoothUUID service_uuid_;
  const bool is_primary_;
  base::flat_map<device::BluetoothUUID,
                 std::unique_ptr<device::BluetoothLocalGattCharacteristic>>
      uuid_to_fake_characteristic_map_;
  base::WeakPtrFactory<FakeLocalGattService> weak_ptr_factory_{this};
};

}  // namespace bluetooth

#endif  // DEVICE_BLUETOOTH_TEST_FAKE_LOCAL_GATT_SERVICE_H_
