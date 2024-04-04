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
class DEVICE_BLUETOOTH_EXPORT BluetoothLocalGattServiceFloss
    : public BluetoothGattServiceFloss,
      public device::BluetoothLocalGattService {
 public:
  static base::WeakPtr<BluetoothLocalGattServiceFloss> Create(
      BluetoothAdapterFloss* adapter,
      const device::BluetoothUUID& uuid,
      bool is_primary,
      device::BluetoothLocalGattService::Delegate* delegate);

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
  base::WeakPtr<device::BluetoothLocalGattCharacteristic> CreateCharacteristic(
      const device::BluetoothUUID& uuid,
      device::BluetoothGattCharacteristic::Properties properties,
      device::BluetoothGattCharacteristic::Permissions permissions) override;

  // BluetoothGattServiceFloss overrides.
  void GattServerServiceAdded(GattStatus status, GattService service) override;
  void GattServerServiceRemoved(GattStatus status, int32_t handle) override;

  void SetRegistered(bool is_registered);
  GattService ToGattService();
  void ResolveInstanceId(const GattService& service);
  int32_t InstanceId() const { return floss_instance_id_; }

 private:
  friend class BluetoothLocalGattCharacteristicFloss;
  friend class BluetoothLocalGattDescriptorFloss;
  friend class BluetoothLocalGattServiceFlossTest;

  BluetoothLocalGattServiceFloss(
      BluetoothAdapterFloss* adapter,
      const device::BluetoothUUID& uuid,
      bool is_primary,
      device::BluetoothLocalGattService::Delegate* delegate);

  // Called by dbus:: on unsuccessful completion of a request to register a
  // local service.
  void OnRegistrationError(const ErrorCallback& error_callback,
                           const std::string& error_name,
                           const std::string& error_message);

  // Adds a characteristic to this service. Returns the index of the
  // characteristic.
  int32_t AddCharacteristic(
      std::unique_ptr<BluetoothLocalGattCharacteristicFloss> characteristic);

  // Whether or not this service is an included service.
  bool is_included_service_ = false;

  // Function to generate a new, unique instance id for each GATT attribute.
  static uint32_t NewInstanceId();

  // A tracker to guarantee unique instance ids for newly created GATT
  // attributes.
  static uint32_t instance_id_tracker_;

  // If this service is primary.
  const bool is_primary_;

  // If this service is registered.
  bool is_registered_ = false;

  // UUID of this service.
  device::BluetoothUUID uuid_;

  // Client and Floss-assigned instance ids.
  int32_t client_instance_id_;
  int32_t floss_instance_id_ = -1;

  // Manage callbacks.
  std::pair<base::OnceClosure, device::BluetoothGattService::ErrorCallback>
      register_callbacks_;
  std::pair<base::OnceClosure, device::BluetoothGattService::ErrorCallback>
      unregister_callbacks_;

  // Delegate to send event notifications.
  raw_ptr<device::BluetoothLocalGattService::Delegate> delegate_;

  // Services included by this service.
  std::vector<std::unique_ptr<BluetoothLocalGattServiceFloss>>
      included_services_;

  // Characteristics contained by this service.
  std::vector<std::unique_ptr<BluetoothLocalGattCharacteristicFloss>>
      characteristics_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<BluetoothLocalGattServiceFloss> weak_ptr_factory_{this};
};

}  // namespace floss

#endif  // DEVICE_BLUETOOTH_FLOSS_BLUETOOTH_LOCAL_GATT_SERVICE_FLOSS_H_
