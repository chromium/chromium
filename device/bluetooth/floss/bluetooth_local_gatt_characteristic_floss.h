// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_FLOSS_BLUETOOTH_LOCAL_GATT_CHARACTERISTIC_FLOSS_H_
#define DEVICE_BLUETOOTH_FLOSS_BLUETOOTH_LOCAL_GATT_CHARACTERISTIC_FLOSS_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "device/bluetooth/bluetooth_local_gatt_characteristic.h"
#include "device/bluetooth/floss/bluetooth_local_gatt_descriptor_floss.h"
#include "device/bluetooth/floss/bluetooth_local_gatt_service_floss.h"
#include "device/bluetooth/floss/floss_gatt_manager_client.h"
#include "device/bluetooth/public/cpp/bluetooth_uuid.h"

namespace floss {

class BluetoothLocalGattDescriptorFloss;

// The BluetoothLocalGattCharacteristicFloss class implements
// BluetoothLocalGattCharacteristic for local GATT characteristics for
// platforms that use Floss.
class DEVICE_BLUETOOTH_EXPORT BluetoothLocalGattCharacteristicFloss
    : public device::BluetoothLocalGattCharacteristic,
      public FlossGattServerObserver {
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
  std::vector<device::BluetoothLocalGattDescriptor*> GetDescriptors()
      const override;

  // floss::FlossGattServerObserver overrides.
  void GattServerCharacteristicReadRequest(std::string address,
                                           int32_t request_id,
                                           int32_t offset,
                                           bool is_long,
                                           int32_t handle) override;
  void GattServerCharacteristicWriteRequest(
      std::string address,
      int32_t request_id,
      int32_t offset,
      int32_t length,
      bool is_prepared_write,
      bool needs_response,
      int32_t handle,
      std::vector<uint8_t> value) override;
  void GattServerExecuteWrite(std::string address,
                              int32_t request_id,
                              bool execute_write) override;

  void ResolveInstanceId(const GattService& service);
  int32_t InstanceId() const { return floss_instance_id_; }
  NotificationType CccdNotificationType();

 private:
  friend class BluetoothLocalGattServiceFloss;
  friend class BluetoothLocalGattDescriptorFloss;
  friend class BluetoothLocalGattServiceFlossTest;

  BluetoothLocalGattCharacteristicFloss(
      const device::BluetoothUUID& uuid,
      Properties properties,
      Permissions permissions,
      BluetoothLocalGattServiceFloss* service);

  // Convert this characteristic to DBUS |GattCharacteristic| struct.
  GattCharacteristic ToGattCharacteristic();

  // Adds a descriptor to this characteristic. Returns the index of the
  // descriptor.
  int32_t AddDescriptor(
      std::unique_ptr<BluetoothLocalGattDescriptorFloss> descriptor);

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

  // Cached instance of the latest pending read/write request, if one exists.
  std::optional<GattRequest> pending_request_;

  // Timer to stop waiting for a callback response.
  base::OneShotTimer response_timer_;

  // UUID of this characteristic.
  device::BluetoothUUID uuid_;

  // Properties of this characteristic.
  Properties properties_;

  // Permissions of this characteristic.
  Permissions permissions_;

  // Service that contains this characteristic.
  raw_ref<BluetoothLocalGattServiceFloss> service_;

  // Client and Floss-assigned instance ids.
  int32_t client_instance_id_;
  int32_t floss_instance_id_ = -1;

  // Index of this characteristic within the containing service.
  int32_t index_;

  // Descriptors contained by this characteristic.
  std::vector<std::unique_ptr<BluetoothLocalGattDescriptorFloss>> descriptors_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<BluetoothLocalGattCharacteristicFloss> weak_ptr_factory_{
      this};
};

}  // namespace floss

#endif  // DEVICE_BLUETOOTH_FLOSS_BLUETOOTH_LOCAL_GATT_CHARACTERISTIC_FLOSS_H_
