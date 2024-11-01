// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef DEVICE_BLUETOOTH_FLOSS_BLUETOOTH_REMOTE_GATT_CHARACTERISTIC_FLOSS_H_
#define DEVICE_BLUETOOTH_FLOSS_BLUETOOTH_REMOTE_GATT_CHARACTERISTIC_FLOSS_H_

#include <tuple>

#include "device/bluetooth/bluetooth_export.h"
#include "device/bluetooth/bluetooth_remote_gatt_characteristic.h"
#include "device/bluetooth/floss/bluetooth_gatt_characteristic_floss.h"
#include "device/bluetooth/floss/floss_dbus_client.h"
#include "device/bluetooth/floss/floss_gatt_manager_client.h"

namespace device {
class BluetoothRemoteGattDescriptor;
}

namespace floss {

class BluetoothRemoteGattServiceFloss;

class DEVICE_BLUETOOTH_EXPORT BluetoothRemoteGattCharacteristicFloss
    : public BluetoothGattCharacteristicFloss,
      public device::BluetoothRemoteGattCharacteristic,
      public FlossGattClientObserver {
 public:
  // Construct remote characteristic.
  static std::unique_ptr<BluetoothRemoteGattCharacteristicFloss> Create(
      BluetoothRemoteGattServiceFloss* service,
      GattCharacteristic* characteristic);

  BluetoothRemoteGattCharacteristicFloss(
      const BluetoothRemoteGattCharacteristicFloss&) = delete;
  BluetoothRemoteGattCharacteristicFloss& operator=(
      const BluetoothRemoteGattCharacteristicFloss&) = delete;

  ~BluetoothRemoteGattCharacteristicFloss() override;

  // device::BluetoothGattCharacteristic overrides.
  std::string GetIdentifier() const override;
  device::BluetoothUUID GetUUID() const override;
  Properties GetProperties() const override;
  Permissions GetPermissions() const override;

  // device::BluetoothRemoteGattCharacteristic overrides.
  const std::vector<uint8_t>& GetValue() const override;
  device::BluetoothRemoteGattService* GetService() const override;
  void ReadRemoteCharacteristic(ValueCallback callback) override;
  void WriteRemoteCharacteristic(
      const std::vector<uint8_t>& value,
      device::BluetoothRemoteGattCharacteristic::WriteType write_type,
      base::OnceClosure callback,
      ErrorCallback error_callback) override;
  // While this function should be deprecated, it is still called in many
  // places. This simply calls |WriteRemoteCharacteristic| with a default value
  // for |WriteType| to make it easy to remove in the future.
  void DeprecatedWriteRemoteCharacteristic(
      const std::vector<uint8_t>& value,
      base::OnceClosure callback,
      ErrorCallback error_callback) override;
#if BUILDFLAG(IS_CHROMEOS)
  void PrepareWriteRemoteCharacteristic(const std::vector<uint8_t>& value,
                                        base::OnceClosure callback,
                                        ErrorCallback error_callback) override;
#endif  // BUILDFLAG(IS_CHROMEOS)

  // floss::FlossGattClientObserver overrides.
  void GattCharacteristicRead(std::string address,
                              GattStatus status,
                              int32_t handle,
                              const std::vector<uint8_t>& data) override;

  void GattCharacteristicWrite(std::string address,
                               GattStatus status,
                               int32_t handle) override;
  void GattNotify(std::string address,
                  int32_t handle,
                  const std::vector<uint8_t>& data) override;

  // Authentication required to read this characteristic and its descriptors.
  AuthRequired GetAuthForRead() const;

  // Authentication required to write to this characteristic and its
  // descriptors.
  AuthRequired GetAuthForWrite() const;

 protected:
  // Additional BluetoothRemoteGattCharacteristic overrides.
#if BUILDFLAG(IS_CHROMEOS)
  void SubscribeToNotifications(
      device::BluetoothRemoteGattDescriptor* ccc_descriptor,
      NotificationType notification_type,
      base::OnceClosure callback,
      ErrorCallback error_callback) override;
#else
  void SubscribeToNotifications(
      device::BluetoothRemoteGattDescriptor* ccc_descriptor,
      base::OnceClosure callback,
      ErrorCallback error_callback) override;
#endif  // BUILDFLAG(IS_CHROMEOS)
  void UnsubscribeFromNotifications(
      device::BluetoothRemoteGattDescriptor* ccc_descriptor,
      base::OnceClosure callback,
      ErrorCallback error_callback) override;

  // Common impl for various writes calls.
  void WriteRemoteCharacteristicImpl(const std::vector<uint8_t>& value,
                                     floss::WriteType write_type,
                                     base::OnceClosure callback,
                                     ErrorCallback error_callback);

  // Handle result of calling |ReadRemoteCharacteristic|.
  void OnReadCharacteristic(ValueCallback callback, DBusResult<Void> result);
  // Handle result of calling |WriteRemoteCharacteristic|.
  void OnWriteCharacteristic(base::OnceClosure callback,
                             ErrorCallback error_callback,
                             std::vector<uint8_t> data,
                             DBusResult<GattWriteRequestStatus> result);

 private:
  friend class BluetoothRemoteGattServiceFloss;

  BluetoothRemoteGattCharacteristicFloss(
      BluetoothRemoteGattServiceFloss* service,
      GattCharacteristic* characteristic);

  // Handles response to |RegisterForNotification| and
  // |UnregisterForNotification|.
  void OnRegisterForNotification(
      device::BluetoothRemoteGattDescriptor* ccc_descriptor,
      const std::vector<uint8_t>& value,
      base::OnceClosure callback,
      ErrorCallback error_callback,
      DBusResult<GattStatus> result);

  // Send notifications to observer on adapter.
  void NotifyValueChanged();

  // Cached data from the last read that was done.
  std::vector<uint8_t> cached_data_;

  // Characteristic represented by this class. The contents are owned by the
  // |service_| so we keep a pointer to it only here.
  raw_ptr<GattCharacteristic> characteristic_;

  // Number of gatt read requests in progress.
  int num_of_reads_in_progress_ = 0;

  // Callback for pending |ReadRemoteCharacteristic|.
  ValueCallback pending_read_callback_;

  // Callback for pending |WriteRemoteCharacteristic|.
  std::tuple<base::OnceClosure, ErrorCallback, std::vector<uint8_t>>
      pending_write_callbacks_;

  // Service which this characteristic belongs to. We can keep a raw_ptr<> here
  // because the Service linked here owns a unique_ptr<> to this class instance
  // so the lifetime of the two objects are tied together.
  raw_ptr<BluetoothRemoteGattServiceFloss> service_;

  // Address of the device this characteristic and parent service belongs to.
  std::string device_address_;

  base::WeakPtrFactory<BluetoothRemoteGattCharacteristicFloss>
      weak_ptr_factory_{this};
};
}  // namespace floss

#endif  // DEVICE_BLUETOOTH_FLOSS_BLUETOOTH_REMOTE_GATT_CHARACTERISTIC_FLOSS_H_
