// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_BLUETOOTH_REMOTE_GATT_DESCRIPTOR_WINRT_H_
#define DEVICE_BLUETOOTH_BLUETOOTH_REMOTE_GATT_DESCRIPTOR_WINRT_H_

#include <windows.devices.bluetooth.genericattributeprofile.h>
#include <wrl/client.h>

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "device/bluetooth/bluetooth_export.h"
#include "device/bluetooth/bluetooth_remote_gatt_characteristic.h"
#include "device/bluetooth/bluetooth_remote_gatt_descriptor.h"
#include "device/bluetooth/public/cpp/bluetooth_uuid.h"

namespace device {

class DEVICE_BLUETOOTH_EXPORT BluetoothRemoteGattDescriptorWinrt
    : public BluetoothRemoteGattDescriptor {
 public:
  static std::unique_ptr<BluetoothRemoteGattDescriptorWinrt> Create(
      BluetoothRemoteGattCharacteristic* characteristic,
      Microsoft::WRL::ComPtr<ABI::Windows::Devices::Bluetooth::
                                 GenericAttributeProfile::IGattDescriptor>
          descriptor);

  BluetoothRemoteGattDescriptorWinrt(
      const BluetoothRemoteGattDescriptorWinrt&) = delete;
  BluetoothRemoteGattDescriptorWinrt& operator=(
      const BluetoothRemoteGattDescriptorWinrt&) = delete;

  ~BluetoothRemoteGattDescriptorWinrt() override;

  // BluetoothGattDescriptor:
  std::string GetIdentifier() const override;
  BluetoothUUID GetUUID() const override;
  BluetoothGattCharacteristic::Permissions GetPermissions() const override;

  // BluetoothRemoteGattDescriptor:
  const std::vector<uint8_t>& GetValue() const override;
  BluetoothRemoteGattCharacteristic* GetCharacteristic() const override;
  void ReadRemoteDescriptor(ValueCallback callback) override;
  void WriteRemoteDescriptor(const std::vector<uint8_t>& value,
                             base::OnceClosure callback,
                             ErrorCallback error_callback) override;

  ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::IGattDescriptor*
  GetDescriptorForTesting();

 private:
  struct PendingWriteCallbacks {
    PendingWriteCallbacks(base::OnceClosure callback,
                          ErrorCallback error_callback);
    ~PendingWriteCallbacks();

    base::OnceClosure callback;
    ErrorCallback error_callback;
  };

  BluetoothRemoteGattDescriptorWinrt(
      BluetoothRemoteGattCharacteristic* characteristic,
      Microsoft::WRL::ComPtr<ABI::Windows::Devices::Bluetooth::
                                 GenericAttributeProfile::IGattDescriptor>
          descriptor,
      BluetoothUUID uuid,
      uint16_t attribute_handle);

  void OnReadValue(Microsoft::WRL::ComPtr<
                   ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
                       IGattReadResult> read_result);

  void OnWriteValueWithResult(
      Microsoft::WRL::ComPtr<ABI::Windows::Devices::Bluetooth::
                                 GenericAttributeProfile::IGattWriteResult>
          write_result);

  // Weak. This object is owned by |characteristic_|.
  raw_ptr<BluetoothRemoteGattCharacteristic> characteristic_;
  Microsoft::WRL::ComPtr<ABI::Windows::Devices::Bluetooth::
                             GenericAttributeProfile::IGattDescriptor>
      descriptor_;
  BluetoothUUID uuid_;
  std::string identifier_;
  std::vector<uint8_t> value_;
  ValueCallback pending_read_callback_;
  std::unique_ptr<PendingWriteCallbacks> pending_write_callbacks_;

  base::WeakPtrFactory<BluetoothRemoteGattDescriptorWinrt> weak_ptr_factory_{
      this};
};

}  // namespace device

#endif  // DEVICE_BLUETOOTH_BLUETOOTH_REMOTE_GATT_DESCRIPTOR_WINRT_H_
