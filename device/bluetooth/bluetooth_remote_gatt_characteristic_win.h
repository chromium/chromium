// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_BLUETOOTH_REMOTE_GATT_CHARACTERISTIC_WIN_H_
#define DEVICE_BLUETOOTH_BLUETOOTH_REMOTE_GATT_CHARACTERISTIC_WIN_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/sequenced_task_runner.h"
#include "device/bluetooth/bluetooth_low_energy_defs_win.h"
#include "device/bluetooth/bluetooth_remote_gatt_characteristic.h"
#include "device/bluetooth/bluetooth_remote_gatt_service.h"

namespace device {

class BluetoothRemoteGattDescriptorWin;
class BluetoothRemoteGattServiceWin;
class BluetoothTaskManagerWin;

// The BluetoothRemoteGattCharacteristicWin class implements
// BluetoothRemoteGattCharacteristic for remote GATT services on Windows 8 and
// later.
class DEVICE_BLUETOOTH_EXPORT BluetoothRemoteGattCharacteristicWin
    : public BluetoothRemoteGattCharacteristic {
 public:
  BluetoothRemoteGattCharacteristicWin(
      BluetoothRemoteGattServiceWin* parent_service,
      BTH_LE_GATT_CHARACTERISTIC* characteristic_info,
      scoped_refptr<base::SequencedTaskRunner> ui_task_runner);
  ~BluetoothRemoteGattCharacteristicWin() override;

  // Override BluetoothRemoteGattCharacteristic interfaces.
  std::string GetIdentifier() const override;
  BluetoothUUID GetUUID() const override;
  std::vector<uint8_t>& GetValue() const override;
  BluetoothRemoteGattService* GetService() const override;
  Properties GetProperties() const override;
  Permissions GetPermissions() const override;
  bool IsNotifying() const override;
  void ReadRemoteCharacteristic(ValueCallback callback,
                                ErrorCallback error_callback) override;
  void WriteRemoteCharacteristic(const std::vector<uint8_t>& value,
                                 base::OnceClosure callback,
                                 ErrorCallback error_callback) override;

  // Update included descriptors.
  void Update();
  uint16_t GetAttributeHandle() const;
  BluetoothRemoteGattServiceWin* GetWinService() { return parent_service_; }

 protected:
  void SubscribeToNotifications(BluetoothRemoteGattDescriptor* ccc_descriptor,
                                base::OnceClosure callback,
                                ErrorCallback error_callback) override;
  void UnsubscribeFromNotifications(
      BluetoothRemoteGattDescriptor* ccc_descriptor,
      base::OnceClosure callback,
      ErrorCallback error_callback) override;

 private:
  void OnGetIncludedDescriptorsCallback(
      std::unique_ptr<BTH_LE_GATT_DESCRIPTOR> descriptors,
      uint16_t num,
      HRESULT hr);
  void UpdateIncludedDescriptors(PBTH_LE_GATT_DESCRIPTOR descriptors,
                                 uint16_t num);

  // Checks if the descriptor with |uuid| and |attribute_handle| has already
  // been discovered as included descriptor.
  bool IsDescriptorDiscovered(const BTH_LE_UUID& uuid,
                              uint16_t attribute_handle);

  // Checks if |descriptor| still exists in this characteristic according to
  // newly discovered |num| of |descriptors|.
  static bool DoesDescriptorExist(PBTH_LE_GATT_DESCRIPTOR descriptors,
                                  uint16_t num,
                                  BluetoothRemoteGattDescriptorWin* descriptor);

  void OnReadRemoteCharacteristicValueCallback(
      std::unique_ptr<BTH_LE_GATT_CHARACTERISTIC_VALUE> value,
      HRESULT hr);
  void OnWriteRemoteCharacteristicValueCallback(HRESULT hr);
  BluetoothRemoteGattService::GattErrorCode HRESULTToGattErrorCode(HRESULT hr);
  void OnGattCharacteristicValueChanged(
      std::unique_ptr<std::vector<uint8_t>> new_value);
  void GattEventRegistrationCallback(base::OnceClosure callback,
                                     ErrorCallback error_callback,
                                     PVOID event_handle,
                                     HRESULT hr);
  void ClearIncludedDescriptors();

  BluetoothRemoteGattServiceWin* parent_service_;
  scoped_refptr<BluetoothTaskManagerWin> task_manager_;

  // Characteristic info from OS and used to interact with OS.
  std::unique_ptr<BTH_LE_GATT_CHARACTERISTIC> characteristic_info_;
  scoped_refptr<base::SequencedTaskRunner> ui_task_runner_;
  BluetoothUUID characteristic_uuid_;
  std::vector<uint8_t> characteristic_value_;
  std::string characteristic_identifier_;

  // Flag indicates if characteristic added notification of this characteristic
  // has been sent out to avoid duplicate notification.
  bool characteristic_added_notified_;

  // ReadRemoteCharacteristic request callbacks.
  std::pair<ValueCallback, ErrorCallback> read_characteristic_value_callbacks_;

  // WriteRemoteCharacteristic request callbacks.
  std::pair<base::OnceClosure, ErrorCallback>
      write_characteristic_value_callbacks_;

  bool characteristic_value_read_or_write_in_progress_;

  // GATT event handle returned by GattEventRegistrationCallback.
  PVOID gatt_event_handle_;

  // Counts the number of asynchronous operations that are discovering
  // descriptors.
  int discovery_pending_count_;

  base::WeakPtrFactory<BluetoothRemoteGattCharacteristicWin> weak_ptr_factory_{
      this};
  DISALLOW_COPY_AND_ASSIGN(BluetoothRemoteGattCharacteristicWin);
};

}  // namespace device

#endif  // DEVICE_BLUETOOTH_BLUETOOTH_REMOTE_GATT_CHARACTERISTIC_WIN_H_
