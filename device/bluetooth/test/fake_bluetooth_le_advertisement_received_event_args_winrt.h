// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_TEST_FAKE_BLUETOOTH_LE_ADVERTISEMENT_RECEIVED_EVENT_ARGS_WINRT_H_
#define DEVICE_BLUETOOTH_TEST_FAKE_BLUETOOTH_LE_ADVERTISEMENT_RECEIVED_EVENT_ARGS_WINRT_H_

#include <windows.devices.bluetooth.advertisement.h>
#include <wrl/implements.h>

#include <stdint.h>

#include <string_view>

namespace device {

class FakeBluetoothLEAdvertisementReceivedEventArgsWinrt
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<
              Microsoft::WRL::WinRt | Microsoft::WRL::InhibitRoOriginateError>,
          ABI::Windows::Devices::Bluetooth::Advertisement::
              IBluetoothLEAdvertisementReceivedEventArgs> {
 public:
  FakeBluetoothLEAdvertisementReceivedEventArgsWinrt(
      int16_t rssi,
      std::string_view address,
      Microsoft::WRL::ComPtr<ABI::Windows::Devices::Bluetooth::Advertisement::
                                 IBluetoothLEAdvertisement> advertisement);

  FakeBluetoothLEAdvertisementReceivedEventArgsWinrt(
      const FakeBluetoothLEAdvertisementReceivedEventArgsWinrt&) = delete;
  FakeBluetoothLEAdvertisementReceivedEventArgsWinrt& operator=(
      const FakeBluetoothLEAdvertisementReceivedEventArgsWinrt&) = delete;

  ~FakeBluetoothLEAdvertisementReceivedEventArgsWinrt() override;

  // IBluetoothLEAdvertisementReceivedEventArgs:
  IFACEMETHODIMP get_RawSignalStrengthInDBm(int16_t* value) override;
  IFACEMETHODIMP get_BluetoothAddress(uint64_t* value) override;
  IFACEMETHODIMP get_AdvertisementType(
      ABI::Windows::Devices::Bluetooth::Advertisement::
          BluetoothLEAdvertisementType* value) override;
  IFACEMETHODIMP get_Timestamp(
      ABI::Windows::Foundation::DateTime* value) override;
  IFACEMETHODIMP get_Advertisement(
      ABI::Windows::Devices::Bluetooth::Advertisement::
          IBluetoothLEAdvertisement** value) override;

 private:
  int16_t rssi_;
  uint64_t raw_address_;
  Microsoft::WRL::ComPtr<ABI::Windows::Devices::Bluetooth::Advertisement::
                             IBluetoothLEAdvertisement>
      advertisement_;
};

}  // namespace device

#endif  // DEVICE_BLUETOOTH_TEST_FAKE_BLUETOOTH_LE_ADVERTISEMENT_RECEIVED_EVENT_ARGS_WINRT_H_
