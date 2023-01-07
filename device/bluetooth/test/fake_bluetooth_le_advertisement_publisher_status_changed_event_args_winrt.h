// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_TEST_FAKE_BLUETOOTH_LE_ADVERTISEMENT_PUBLISHER_STATUS_CHANGED_EVENT_ARGS_WINRT_H_
#define DEVICE_BLUETOOTH_TEST_FAKE_BLUETOOTH_LE_ADVERTISEMENT_PUBLISHER_STATUS_CHANGED_EVENT_ARGS_WINRT_H_

#include <windows.devices.bluetooth.advertisement.h>
#include <wrl/implements.h>

namespace device {

class FakeBluetoothLEAdvertisementPublisherStatusChangedEventArgsWinrt
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<
              Microsoft::WRL::WinRt | Microsoft::WRL::InhibitRoOriginateError>,
          ABI::Windows::Devices::Bluetooth::Advertisement::
              IBluetoothLEAdvertisementPublisherStatusChangedEventArgs> {
 public:
  explicit FakeBluetoothLEAdvertisementPublisherStatusChangedEventArgsWinrt(
      ABI::Windows::Devices::Bluetooth::Advertisement::
          BluetoothLEAdvertisementPublisherStatus status);
  FakeBluetoothLEAdvertisementPublisherStatusChangedEventArgsWinrt(
      ABI::Windows::Devices::Bluetooth::Advertisement::
          BluetoothLEAdvertisementPublisherStatus status,
      ABI::Windows::Devices::Bluetooth::BluetoothError error);

  FakeBluetoothLEAdvertisementPublisherStatusChangedEventArgsWinrt(
      const FakeBluetoothLEAdvertisementPublisherStatusChangedEventArgsWinrt&) =
      delete;
  FakeBluetoothLEAdvertisementPublisherStatusChangedEventArgsWinrt& operator=(
      const FakeBluetoothLEAdvertisementPublisherStatusChangedEventArgsWinrt&) =
      delete;

  ~FakeBluetoothLEAdvertisementPublisherStatusChangedEventArgsWinrt() override;

  // IBluetoothLEAdvertisementPublisherStatusChangedEventArgs:
  IFACEMETHODIMP get_Status(
      ABI::Windows::Devices::Bluetooth::Advertisement::
          BluetoothLEAdvertisementPublisherStatus* value) override;
  IFACEMETHODIMP get_Error(
      ABI::Windows::Devices::Bluetooth::BluetoothError* value) override;

 private:
  ABI::Windows::Devices::Bluetooth::Advertisement::
      BluetoothLEAdvertisementPublisherStatus status_ =
          ABI::Windows::Devices::Bluetooth::Advertisement::
              BluetoothLEAdvertisementPublisherStatus_Created;

  ABI::Windows::Devices::Bluetooth::BluetoothError error_ =
      ABI::Windows::Devices::Bluetooth::BluetoothError_Success;
};

}  // namespace device

#endif  // DEVICE_BLUETOOTH_TEST_FAKE_BLUETOOTH_LE_ADVERTISEMENT_PUBLISHER_STATUS_CHANGED_EVENT_ARGS_WINRT_H_
