// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_TEST_FAKE_GATT_VALUE_CHANGED_EVENT_ARGS_WINRT_H_
#define DEVICE_BLUETOOTH_TEST_FAKE_GATT_VALUE_CHANGED_EVENT_ARGS_WINRT_H_

#include <windows.devices.bluetooth.genericattributeprofile.h>
#include <wrl/implements.h>

#include <stdint.h>

#include <vector>

namespace device {

class FakeGattValueChangedEventArgsWinrt
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<
              Microsoft::WRL::WinRt | Microsoft::WRL::InhibitRoOriginateError>,
          ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
              IGattValueChangedEventArgs> {
 public:
  explicit FakeGattValueChangedEventArgsWinrt(std::vector<uint8_t> value);

  FakeGattValueChangedEventArgsWinrt(
      const FakeGattValueChangedEventArgsWinrt&) = delete;
  FakeGattValueChangedEventArgsWinrt& operator=(
      const FakeGattValueChangedEventArgsWinrt&) = delete;

  ~FakeGattValueChangedEventArgsWinrt() override;

  // IGattValueChangedEventArgs:
  IFACEMETHODIMP get_CharacteristicValue(
      ABI::Windows::Storage::Streams::IBuffer** value) override;
  IFACEMETHODIMP get_Timestamp(
      ABI::Windows::Foundation::DateTime* timestamp) override;

 private:
  std::vector<uint8_t> value_;
};

}  // namespace device

#endif  // DEVICE_BLUETOOTH_TEST_FAKE_GATT_VALUE_CHANGED_EVENT_ARGS_WINRT_H_
