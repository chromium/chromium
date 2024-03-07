// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_BLUEZ_BLUETOOTH_LOCAL_GATT_SERVICE_BLUEZ_H_
#define DEVICE_BLUETOOTH_BLUEZ_BLUETOOTH_LOCAL_GATT_SERVICE_BLUEZ_H_

#include <string>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "device/bluetooth/bluetooth_local_gatt_service.h"
#include "device/bluetooth/bluez/bluetooth_adapter_bluez.h"
#include "device/bluetooth/bluez/bluetooth_gatt_service_bluez.h"
#include "device/bluetooth/bluez/bluetooth_local_gatt_characteristic_bluez.h"
#include "device/bluetooth/public/cpp/bluetooth_uuid.h"

namespace bluez {

// The BluetoothLocalGattServiceBlueZ class implements BluetootGattService
// for local GATT services for platforms that use BlueZ.
class BluetoothLocalGattServiceBlueZ
    : public BluetoothGattServiceBlueZ,
      public device::BluetoothLocalGattService {
 public:
  static base::WeakPtr<BluetoothLocalGattServiceBlueZ> Create(
      BluetoothAdapterBlueZ* adapter,
      const device::BluetoothUUID& uuid,
      bool is_primary,
      device::BluetoothLocalGattService::Delegate* delegate);

  BluetoothLocalGattServiceBlueZ(const BluetoothLocalGattServiceBlueZ&) =
      delete;
  BluetoothLocalGattServiceBlueZ& operator=(
      const BluetoothLocalGattServiceBlueZ&) = delete;

  ~BluetoothLocalGattServiceBlueZ() override;

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
  base::WeakPtr<device::BluetoothLocalGattCharacteristic> CreateCharacteristic(
      const device::BluetoothUUID& uuid,
      device::BluetoothGattCharacteristic::Properties properties,
      device::BluetoothGattCharacteristic::Permissions permissions) override;

  const std::map<dbus::ObjectPath,
                 std::unique_ptr<BluetoothLocalGattCharacteristicBlueZ>>&
  GetCharacteristics() const;

  Delegate* GetDelegate() const { return delegate_; }

  static dbus::ObjectPath AddGuidToObjectPath(const std::string& path);

 private:
  friend class BluetoothLocalGattCharacteristicBlueZ;

  BluetoothLocalGattServiceBlueZ(
      BluetoothAdapterBlueZ* adapter,
      const device::BluetoothUUID& uuid,
      bool is_primary,
      device::BluetoothLocalGattService::Delegate* delegate);

  // Called by dbus:: on unsuccessful completion of a request to register a
  // local service.
  void OnRegistrationError(const ErrorCallback& error_callback,
                           const std::string& error_name,
                           const std::string& error_message);

  void AddCharacteristic(
      std::unique_ptr<BluetoothLocalGattCharacteristicBlueZ> characteristic);

  // UUID of this service.
  device::BluetoothUUID uuid_;

  // If this service is primary.
  bool is_primary_;

  // Delegate to receive read/write requests for attribute  values contained
  // in this service.
  raw_ptr<device::BluetoothLocalGattService::Delegate, DanglingUntriaged>
      delegate_;

  // Characteristics contained by this service.
  std::map<dbus::ObjectPath,
           std::unique_ptr<BluetoothLocalGattCharacteristicBlueZ>>
      characteristics_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<BluetoothLocalGattServiceBlueZ> weak_ptr_factory_{this};
};

}  // namespace bluez

#endif  // DEVICE_BLUETOOTH_BLUEZ_BLUETOOTH_LOCAL_GATT_SERVICE_BLUEZ_H_
