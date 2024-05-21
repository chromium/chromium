// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_FLOSS_BLUETOOTH_LOCAL_GATT_DESCRIPTOR_FLOSS_H_
#define DEVICE_BLUETOOTH_FLOSS_BLUETOOTH_LOCAL_GATT_DESCRIPTOR_FLOSS_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "device/bluetooth/bluetooth_local_gatt_characteristic.h"
#include "device/bluetooth/bluetooth_local_gatt_descriptor.h"
#include "device/bluetooth/floss/bluetooth_local_gatt_characteristic_floss.h"
#include "device/bluetooth/floss/bluetooth_local_gatt_service_floss.h"
#include "device/bluetooth/floss/floss_gatt_manager_client.h"
#include "device/bluetooth/public/cpp/bluetooth_uuid.h"

namespace floss {

class BluetoothLocalGattCharacteristicFloss;

// The BluetoothLocalGattDescriptorFloss class implements
// BluetoothRemoteGattDescriptor for remote and local GATT characteristic
// descriptors for platforms that use Floss.
class DEVICE_BLUETOOTH_EXPORT BluetoothLocalGattDescriptorFloss
    : public device::BluetoothLocalGattDescriptor,
      public FlossGattServerObserver {
 public:
  static base::WeakPtr<BluetoothLocalGattDescriptorFloss> Create(
      const device::BluetoothUUID& uuid,
      device::BluetoothGattCharacteristic::Permissions permissions,
      BluetoothLocalGattCharacteristicFloss* characteristic);

  BluetoothLocalGattDescriptorFloss(const BluetoothLocalGattDescriptorFloss&) =
      delete;
  BluetoothLocalGattDescriptorFloss& operator=(
      const BluetoothLocalGattDescriptorFloss&) = delete;

  ~BluetoothLocalGattDescriptorFloss() override;

  // device::BluetoothGattDescriptor overrides.
  std::string GetIdentifier() const override;

  // device::BluetoothLocalGattDescriptor overrides.
  device::BluetoothUUID GetUUID() const override;
  device::BluetoothGattCharacteristic::Permissions GetPermissions()
      const override;
  device::BluetoothLocalGattCharacteristic* GetCharacteristic() const override;

  // floss::FlossGattServerObserver overrides.
  void GattServerDescriptorReadRequest(std::string address,
                                       int32_t request_id,
                                       int32_t offset,
                                       bool is_long,
                                       int32_t handle) override;
  void GattServerDescriptorWriteRequest(std::string address,
                                        int32_t request_id,
                                        int32_t offset,
                                        int32_t length,
                                        bool is_prepared_write,
                                        bool needs_response,
                                        int32_t handle,
                                        std::vector<uint8_t> value) override;

  void ResolveInstanceId(const GattCharacteristic& characteristic);
  int32_t InstanceId() const { return floss_instance_id_; }
  device::BluetoothGattCharacteristic::NotificationType CccdNotificationType() {
    return cccd_type_;
  }

 private:
  friend class BluetoothLocalGattCharacteristicFloss;
  friend class BluetoothLocalGattServiceFlossTest;

  BluetoothLocalGattDescriptorFloss(
      const device::BluetoothUUID& uuid,
      device::BluetoothGattCharacteristic::Permissions permissions,
      BluetoothLocalGattCharacteristicFloss* characteristic);

  // Convert this descriptor to DBUS |GattDescriptor| struct.
  GattDescriptor ToGattDescriptor();

  // Runs after the browser client has processed the read request and has sent a
  // response.
  void OnReadRequestCallback(
      int32_t request_id,
      std::optional<BluetoothGattServiceFloss::GattErrorCode> error_code,
      const std::vector<uint8_t>& value);

  // Runs after the browser client has processed the write request and has sent
  // a response.
  void OnWriteRequestCallback(int32_t request_id,
                              std::vector<uint8_t>& value,
                              bool needs_response,
                              bool success);

  // Notify browser clients if there was a change to the CCCD.
  GattStatus HandleCccDescriptor(std::string address,
                                 std::vector<uint8_t>& value);

  // Notification type of the CCCD.
  device::BluetoothGattCharacteristic::NotificationType cccd_type_ =
      device::BluetoothGattCharacteristic::NotificationType::kNone;

  // Cached instance of the latest pending read/write request, if one exists.
  std::optional<GattRequest> pending_request_;

  // Timer to stop waiting for a callback response.
  base::OneShotTimer response_timer_;

  // UUID of this descriptor.
  device::BluetoothUUID uuid_;

  // Permissions of this descriptor.
  device::BluetoothGattCharacteristic::Permissions permissions_;

  // Characteristic that contains this descriptor.
  raw_ref<BluetoothLocalGattCharacteristicFloss> characteristic_;

  // Client and Floss-assigned instance id.
  int32_t client_instance_id_;
  int32_t floss_instance_id_ = -1;

  // Index of this descriptor within the containing characteristic.
  int32_t index_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<BluetoothLocalGattDescriptorFloss> weak_ptr_factory_{
      this};
};

}  // namespace floss

#endif  // DEVICE_BLUETOOTH_FLOSS_BLUETOOTH_LOCAL_GATT_DESCRIPTOR_FLOSS_H_
