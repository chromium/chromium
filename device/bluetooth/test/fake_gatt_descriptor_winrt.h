// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_TEST_FAKE_GATT_DESCRIPTOR_WINRT_H_
#define DEVICE_BLUETOOTH_TEST_FAKE_GATT_DESCRIPTOR_WINRT_H_

#include <windows.devices.bluetooth.genericattributeprofile.h>
#include <wrl/client.h>
#include <wrl/implements.h>

#include <stdint.h>

#include <string_view>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "device/bluetooth/bluetooth_gatt_service.h"

namespace device {

class BluetoothTestWinrt;

class FakeGattDescriptorWinrt
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<
              Microsoft::WRL::WinRt | Microsoft::WRL::InhibitRoOriginateError>,
          ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
              IGattDescriptor,
          ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
              IGattDescriptor2> {
 public:
  FakeGattDescriptorWinrt(BluetoothTestWinrt* bluetooth_test_winrt,
                          std::string_view uuid,
                          uint16_t attribute_handle);

  FakeGattDescriptorWinrt(const FakeGattDescriptorWinrt&) = delete;
  FakeGattDescriptorWinrt& operator=(const FakeGattDescriptorWinrt&) = delete;

  ~FakeGattDescriptorWinrt() override;

  // IGattDescriptor:
  IFACEMETHODIMP get_ProtectionLevel(
      ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
          GattProtectionLevel* value) override;
  IFACEMETHODIMP put_ProtectionLevel(
      ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
          GattProtectionLevel value) override;
  IFACEMETHODIMP get_Uuid(GUID* value) override;
  IFACEMETHODIMP get_AttributeHandle(uint16_t* value) override;
  IFACEMETHODIMP ReadValueAsync(
      ABI::Windows::Foundation::IAsyncOperation<
          ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
              GattReadResult*>** value) override;
  IFACEMETHODIMP ReadValueWithCacheModeAsync(
      ABI::Windows::Devices::Bluetooth::BluetoothCacheMode cache_mode,
      ABI::Windows::Foundation::IAsyncOperation<
          ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
              GattReadResult*>** value) override;
  IFACEMETHODIMP WriteValueAsync(
      ABI::Windows::Storage::Streams::IBuffer* value,
      ABI::Windows::Foundation::IAsyncOperation<
          ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
              GattCommunicationStatus>** action) override;

  // IGattDescriptor2:
  IFACEMETHODIMP WriteValueWithResultAsync(
      ABI::Windows::Storage::Streams::IBuffer* value,
      ABI::Windows::Foundation::IAsyncOperation<
          ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
              GattWriteResult*>** operation) override;

  void SimulateGattDescriptorRead(const std::vector<uint8_t>& data);
  void SimulateGattDescriptorReadError(
      BluetoothGattService::GattErrorCode error_code);
  void SimulateGattDescriptorWrite();
  void SimulateGattDescriptorWriteError(
      BluetoothGattService::GattErrorCode error_code);

 private:
  raw_ptr<BluetoothTestWinrt> bluetooth_test_winrt_;
  GUID uuid_;
  uint16_t attribute_handle_;

  base::OnceCallback<void(
      Microsoft::WRL::ComPtr<ABI::Windows::Devices::Bluetooth::
                                 GenericAttributeProfile::IGattReadResult>)>
      read_value_callback_;

  base::OnceCallback<void(
      Microsoft::WRL::ComPtr<ABI::Windows::Devices::Bluetooth::
                                 GenericAttributeProfile::IGattWriteResult>)>
      write_value_callback_;
};

}  // namespace device

#endif  // DEVICE_BLUETOOTH_TEST_FAKE_GATT_DESCRIPTOR_WINRT_H_
