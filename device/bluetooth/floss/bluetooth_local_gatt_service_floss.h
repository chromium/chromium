// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_FLOSS_BLUETOOTH_LOCAL_GATT_SERVICE_FLOSS_H_
#define DEVICE_BLUETOOTH_FLOSS_BLUETOOTH_LOCAL_GATT_SERVICE_FLOSS_H_

#include <string>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "dbus/object_path.h"
#include "device/bluetooth/bluetooth_local_gatt_service.h"
#include "device/bluetooth/floss/bluetooth_adapter_floss.h"
#include "device/bluetooth/floss/bluetooth_gatt_service_floss.h"
#include "device/bluetooth/floss/bluetooth_local_gatt_characteristic_floss.h"
#include "device/bluetooth/floss/floss_gatt_manager_client.h"
#include "device/bluetooth/public/cpp/bluetooth_uuid.h"

namespace floss {

class BluetoothLocalGattCharacteristicFloss;

// The BluetoothLocalGattServiceFloss class implements BluetoothGattService
// for local GATT services for platforms that use Floss.
class BluetoothLocalGattServiceFloss
    : public BluetoothGattServiceFloss,
      public device::BluetoothLocalGattService {
 public:
  static base::WeakPtr<BluetoothLocalGattServiceFloss> Create(
      BluetoothAdapterFloss* adapter,
      const device::BluetoothUUID& uuid,
      bool is_primary);

  BluetoothLocalGattServiceFloss(const BluetoothLocalGattServiceFloss&) =
      delete;
  BluetoothLocalGattServiceFloss& operator=(
      const BluetoothLocalGattServiceFloss&) = delete;

  ~BluetoothLocalGattServiceFloss() override;

  // device::BluetoothGattService overrides.
  device::BluetoothUUID GetUUID() const override;
  bool IsPrimary() const override;

  // device::BluetoothLocalGattService overrides.
  void Register(base::OnceClosure callback,
                ErrorCallback error_callback) override;
  void Unregister(base::OnceClosure callback,
                  ErrorCallback error_callback) override;
  bool IsRegistered() override;
  void Delete() override;
  device::BluetoothLocalGattCharacteristic* GetCharacteristic(
      const std::string& identifier) override;
  std::string GetIdentifier() const override;

  void SetRegistered(bool is_registered);

 private:
  friend class BluetoothLocalGattCharacteristicFloss;

  BluetoothLocalGattServiceFloss(BluetoothAdapterFloss* adapter,
                                 const device::BluetoothUUID& uuid,
                                 bool is_primary);

  // Called by dbus:: on unsuccessful completion of a request to register a
  // local service.
  void OnRegistrationError(const ErrorCallback& error_callback,
                           const std::string& error_name,
                           const std::string& error_message);

  void AddCharacteristic(
      std::unique_ptr<BluetoothLocalGattCharacteristicFloss> characteristic);

  // If this service is primary.
  const bool is_primary_;

  // If this service is registered.
  bool is_registered_ = false;

  // Characteristics contained by this service.
  std::map<std::string, std::unique_ptr<BluetoothLocalGattCharacteristicFloss>>
      characteristics_;

  // Data about the remote gatt service represented by this class.
  GattService local_service_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<BluetoothLocalGattServiceFloss> weak_ptr_factory_{this};
};

}  // namespace floss

#endif  // DEVICE_BLUETOOTH_FLOSS_BLUETOOTH_LOCAL_GATT_SERVICE_FLOSS_H_
