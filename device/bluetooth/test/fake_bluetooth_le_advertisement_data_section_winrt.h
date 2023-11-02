// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_TEST_FAKE_BLUETOOTH_LE_ADVERTISEMENT_DATA_SECTION_WINRT_H_
#define DEVICE_BLUETOOTH_TEST_FAKE_BLUETOOTH_LE_ADVERTISEMENT_DATA_SECTION_WINRT_H_

#include <windows.devices.bluetooth.advertisement.h>
#include <wrl/implements.h>

#include <stdint.h>

#include <vector>

namespace device {

class FakeBluetoothLEAdvertisementDataSectionWinrt
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<
              Microsoft::WRL::WinRt | Microsoft::WRL::InhibitRoOriginateError>,
          ABI::Windows::Devices::Bluetooth::Advertisement::
              IBluetoothLEAdvertisementDataSection> {
 public:
  explicit FakeBluetoothLEAdvertisementDataSectionWinrt(
      std::vector<uint8_t> data);

  FakeBluetoothLEAdvertisementDataSectionWinrt(
      const FakeBluetoothLEAdvertisementDataSectionWinrt&) = delete;
  FakeBluetoothLEAdvertisementDataSectionWinrt& operator=(
      const FakeBluetoothLEAdvertisementDataSectionWinrt&) = delete;

  ~FakeBluetoothLEAdvertisementDataSectionWinrt() override;

  // IBluetoothLEAdvertisementDataSection:
  IFACEMETHODIMP get_DataType(uint8_t* value) override;
  IFACEMETHODIMP put_DataType(uint8_t value) override;
  IFACEMETHODIMP get_Data(
      ABI::Windows::Storage::Streams::IBuffer** value) override;
  IFACEMETHODIMP put_Data(
      ABI::Windows::Storage::Streams::IBuffer* value) override;

 private:
  std::vector<uint8_t> data_;
};

}  // namespace device

#endif  // DEVICE_BLUETOOTH_TEST_FAKE_BLUETOOTH_LE_ADVERTISEMENT_DATA_SECTION_WINRT_H_
