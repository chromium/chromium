// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_TEST_FAKE_BLUETOOTH_LE_ADVERTISEMENT_PUBLISHER_WINRT_H_
#define DEVICE_BLUETOOTH_TEST_FAKE_BLUETOOTH_LE_ADVERTISEMENT_PUBLISHER_WINRT_H_

#include <windows.devices.bluetooth.advertisement.h>
#include <wrl/client.h>
#include <wrl/implements.h>

#include "device/bluetooth/bluetooth_advertisement.h"

namespace device {

class FakeBluetoothLEAdvertisementPublisherWinrt
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<
              Microsoft::WRL::WinRt | Microsoft::WRL::InhibitRoOriginateError>,
          ABI::Windows::Devices::Bluetooth::Advertisement::
              IBluetoothLEAdvertisementPublisher> {
 public:
  explicit FakeBluetoothLEAdvertisementPublisherWinrt(
      Microsoft::WRL::ComPtr<ABI::Windows::Devices::Bluetooth::Advertisement::
                                 IBluetoothLEAdvertisement> advertisement);

  FakeBluetoothLEAdvertisementPublisherWinrt(
      const FakeBluetoothLEAdvertisementPublisherWinrt&) = delete;
  FakeBluetoothLEAdvertisementPublisherWinrt& operator=(
      const FakeBluetoothLEAdvertisementPublisherWinrt&) = delete;

  ~FakeBluetoothLEAdvertisementPublisherWinrt() override;

  // IBluetoothLEAdvertisementPublisher:
  IFACEMETHODIMP get_Status(
      ABI::Windows::Devices::Bluetooth::Advertisement::
          BluetoothLEAdvertisementPublisherStatus* value) override;
  IFACEMETHODIMP get_Advertisement(
      ABI::Windows::Devices::Bluetooth::Advertisement::
          IBluetoothLEAdvertisement** value) override;
  IFACEMETHODIMP Start() override;
  IFACEMETHODIMP Stop() override;
  IFACEMETHODIMP add_StatusChanged(
      ABI::Windows::Foundation::ITypedEventHandler<
          ABI::Windows::Devices::Bluetooth::Advertisement::
              BluetoothLEAdvertisementPublisher*,
          ABI::Windows::Devices::Bluetooth::Advertisement::
              BluetoothLEAdvertisementPublisherStatusChangedEventArgs*>*
          handler,
      EventRegistrationToken* token) override;
  IFACEMETHODIMP remove_StatusChanged(EventRegistrationToken token) override;

  void SimulateAdvertisementStarted();
  void SimulateAdvertisementStopped();
  void SimulateAdvertisementError(BluetoothAdvertisement::ErrorCode error_code);

 private:
  Microsoft::WRL::ComPtr<ABI::Windows::Devices::Bluetooth::Advertisement::
                             IBluetoothLEAdvertisement>
      advertisement_;

  ABI::Windows::Devices::Bluetooth::Advertisement::
      BluetoothLEAdvertisementPublisherStatus status_ =
          ABI::Windows::Devices::Bluetooth::Advertisement::
              BluetoothLEAdvertisementPublisherStatus_Created;

  Microsoft::WRL::ComPtr<ABI::Windows::Foundation::ITypedEventHandler<
      ABI::Windows::Devices::Bluetooth::Advertisement::
          BluetoothLEAdvertisementPublisher*,
      ABI::Windows::Devices::Bluetooth::Advertisement::
          BluetoothLEAdvertisementPublisherStatusChangedEventArgs*>>
      handler_;
};

class FakeBluetoothLEAdvertisementPublisherFactoryWinrt
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<
              Microsoft::WRL::WinRt | Microsoft::WRL::InhibitRoOriginateError>,
          ABI::Windows::Devices::Bluetooth::Advertisement::
              IBluetoothLEAdvertisementPublisherFactory> {
 public:
  FakeBluetoothLEAdvertisementPublisherFactoryWinrt();

  FakeBluetoothLEAdvertisementPublisherFactoryWinrt(
      const FakeBluetoothLEAdvertisementPublisherFactoryWinrt&) = delete;
  FakeBluetoothLEAdvertisementPublisherFactoryWinrt& operator=(
      const FakeBluetoothLEAdvertisementPublisherFactoryWinrt&) = delete;

  ~FakeBluetoothLEAdvertisementPublisherFactoryWinrt() override;

  // IBluetoothLEAdvertisementPublisherFactory:
  IFACEMETHODIMP Create(
      ABI::Windows::Devices::Bluetooth::Advertisement::
          IBluetoothLEAdvertisement* advertisement,
      ABI::Windows::Devices::Bluetooth::Advertisement::
          IBluetoothLEAdvertisementPublisher** value) override;
};

}  // namespace device

#endif  // DEVICE_BLUETOOTH_TEST_FAKE_BLUETOOTH_LE_ADVERTISEMENT_PUBLISHER_WINRT_H_
