// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_TEST_FAKE_GATT_READ_RESULT_WINRT_H_
#define DEVICE_BLUETOOTH_TEST_FAKE_GATT_READ_RESULT_WINRT_H_

#include <windows.devices.bluetooth.genericattributeprofile.h>
#include <wrl/implements.h>

#include <stdint.h>

#include <vector>

#include "device/bluetooth/bluetooth_gatt_service.h"

namespace device {

class FakeGattReadResultWinrt
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<
              Microsoft::WRL::WinRt | Microsoft::WRL::InhibitRoOriginateError>,
          ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
              IGattReadResult,
          ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
              IGattReadResult2> {
 public:
  explicit FakeGattReadResultWinrt(
      BluetoothGattService::GattErrorCode error_code);
  explicit FakeGattReadResultWinrt(std::vector<uint8_t> data);

  FakeGattReadResultWinrt(const FakeGattReadResultWinrt&) = delete;
  FakeGattReadResultWinrt& operator=(const FakeGattReadResultWinrt&) = delete;

  ~FakeGattReadResultWinrt() override;

  // IGattReadResult:
  IFACEMETHODIMP get_Status(
      ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
          GattCommunicationStatus* value) override;
  IFACEMETHODIMP get_Value(
      ABI::Windows::Storage::Streams::IBuffer** value) override;

  // IGattReadResult2:
  IFACEMETHODIMP get_ProtocolError(
      ABI::Windows::Foundation::IReference<uint8_t>** value) override;

 private:
  ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
      GattCommunicationStatus status_ = ABI::Windows::Devices::Bluetooth::
          GenericAttributeProfile::GattCommunicationStatus_Success;
  std::vector<uint8_t> data_;
  uint8_t protocol_error_ = 0;
};

}  // namespace device

#endif  // DEVICE_BLUETOOTH_TEST_FAKE_GATT_READ_RESULT_WINRT_H_
