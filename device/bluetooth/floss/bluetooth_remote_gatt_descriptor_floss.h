// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef DEVICE_BLUETOOTH_FLOSS_BLUETOOTH_REMOTE_GATT_DESCRIPTOR_FLOSS_H_
#define DEVICE_BLUETOOTH_FLOSS_BLUETOOTH_REMOTE_GATT_DESCRIPTOR_FLOSS_H_

#include <memory>
#include <tuple>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "device/bluetooth/bluetooth_export.h"
#include "device/bluetooth/bluetooth_remote_gatt_characteristic.h"
#include "device/bluetooth/bluetooth_remote_gatt_descriptor.h"
#include "device/bluetooth/floss/floss_gatt_manager_client.h"
#include "device/bluetooth/public/cpp/bluetooth_uuid.h"

namespace floss {

class BluetoothRemoteGattCharacteristicFloss;
class BluetoothRemoteGattServiceFloss;
struct GattDescriptor;

class DEVICE_BLUETOOTH_EXPORT BluetoothRemoteGattDescriptorFloss
    : public device::BluetoothRemoteGattDescriptor,
      public FlossGattClientObserver {
 public:
  // Construct remote descriptor.
  static std::unique_ptr<BluetoothRemoteGattDescriptorFloss> Create(
      BluetoothRemoteGattServiceFloss* service,
      BluetoothRemoteGattCharacteristicFloss* characteristic,
      GattDescriptor* descriptor);

  BluetoothRemoteGattDescriptorFloss(
      const BluetoothRemoteGattDescriptorFloss&) = delete;
  BluetoothRemoteGattDescriptorFloss& operator=(
      const BluetoothRemoteGattDescriptorFloss&) = delete;

  ~BluetoothRemoteGattDescriptorFloss() override;

  // device::BluetoothRemoteGattDescriptor overrides.
  std::string GetIdentifier() const override;
  device::BluetoothUUID GetUUID() const override;
  const std::vector<uint8_t>& GetValue() const override;
  device::BluetoothRemoteGattCharacteristic* GetCharacteristic() const override;
  device::BluetoothRemoteGattCharacteristic::Permissions GetPermissions()
      const override;
  void ReadRemoteDescriptor(ValueCallback callback) override;
  void WriteRemoteDescriptor(const std::vector<uint8_t>& new_value,
                             base::OnceClosure callback,
                             ErrorCallback error_callback) override;

  // FlossGattClientObserver overrides.
  void GattDescriptorRead(std::string address,
                          GattStatus status,
                          int32_t handle,
                          const std::vector<uint8_t>& data) override;
  void GattDescriptorWrite(std::string address,
                           GattStatus status,
                           int32_t handle) override;
  void GattNotify(std::string address,
                  int32_t handle,
                  const std::vector<uint8_t>& data) override;

 private:
  BluetoothRemoteGattDescriptorFloss(
      BluetoothRemoteGattServiceFloss* service,
      BluetoothRemoteGattCharacteristicFloss* characteristic,
      GattDescriptor* descriptor);

  // Handle result of calling |ReadRemoteDescriptor|.
  void OnReadDescriptor(ValueCallback callback, DBusResult<Void> result);

  // Handle result of calling |WriteRemoteCharacateristic|.
  void OnWriteDescriptor(base::OnceClosure callback,
                         ErrorCallback error_callback,
                         std::vector<uint8_t> data,
                         DBusResult<Void> result);

  // Handle timeout for receiving a |GattDescriptorWrite|.
  void OnWriteTimeout();

  // Send notifications to observer on adapter.
  void NotifyValueChanged();

  // Cached data from the last read that was done.
  std::vector<uint8_t> cached_data_;

  // Characteristic which this descriptor belongs to.
  raw_ptr<BluetoothRemoteGattCharacteristicFloss> characteristic_;

  // Descriptor represented by this class. The contents are owned by the
  // parent service so we keep a pointer to it only here.
  raw_ptr<GattDescriptor> descriptor_;

  // Number of gatt read requests in progress.
  int num_of_reads_in_progress_ = 0;

  // Callback for pending |ReadRemoteDescriptor|.
  ValueCallback pending_read_callback_;

  // Callback for pending |WriteRemoteDescriptor|.
  std::tuple<base::OnceClosure, ErrorCallback, std::vector<uint8_t>>
      pending_write_callbacks_;

  // Service where this descriptor belongs. The |service_| pointer owns this
  // descriptor so we can keep a pointer to it safely.
  raw_ptr<BluetoothRemoteGattServiceFloss> service_;

  base::WeakPtrFactory<BluetoothRemoteGattDescriptorFloss> weak_ptr_factory_{
      this};
};

}  // namespace floss

#endif  // DEVICE_BLUETOOTH_FLOSS_BLUETOOTH_REMOTE_GATT_DESCRIPTOR_FLOSS_H_
