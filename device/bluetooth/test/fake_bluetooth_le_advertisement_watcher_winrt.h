// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_TEST_FAKE_BLUETOOTH_LE_ADVERTISEMENT_WATCHER_WINRT_H_
#define DEVICE_BLUETOOTH_TEST_FAKE_BLUETOOTH_LE_ADVERTISEMENT_WATCHER_WINRT_H_

#include <windows.devices.bluetooth.h>
#include <wrl/client.h>
#include <wrl/implements.h>

#include <stdint.h>

#include "device/bluetooth/test/bluetooth_test.h"

namespace device {

class FakeBluetoothLEAdvertisementWatcherWinrt
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<
              Microsoft::WRL::WinRt | Microsoft::WRL::InhibitRoOriginateError>,
          ABI::Windows::Devices::Bluetooth::Advertisement::
              IBluetoothLEAdvertisementWatcher> {
 public:
  FakeBluetoothLEAdvertisementWatcherWinrt();

  FakeBluetoothLEAdvertisementWatcherWinrt(
      const FakeBluetoothLEAdvertisementWatcherWinrt&) = delete;
  FakeBluetoothLEAdvertisementWatcherWinrt& operator=(
      const FakeBluetoothLEAdvertisementWatcherWinrt&) = delete;

  ~FakeBluetoothLEAdvertisementWatcherWinrt() override;

  // IBluetoothLEAdvertisementWatcher:
  IFACEMETHODIMP get_MinSamplingInterval(
      ABI::Windows::Foundation::TimeSpan* value) override;
  IFACEMETHODIMP get_MaxSamplingInterval(
      ABI::Windows::Foundation::TimeSpan* value) override;
  IFACEMETHODIMP get_MinOutOfRangeTimeout(
      ABI::Windows::Foundation::TimeSpan* value) override;
  IFACEMETHODIMP get_MaxOutOfRangeTimeout(
      ABI::Windows::Foundation::TimeSpan* value) override;
  IFACEMETHODIMP get_Status(
      ABI::Windows::Devices::Bluetooth::Advertisement::
          BluetoothLEAdvertisementWatcherStatus* value) override;
  IFACEMETHODIMP get_ScanningMode(
      ABI::Windows::Devices::Bluetooth::Advertisement::BluetoothLEScanningMode*
          value) override;
  IFACEMETHODIMP put_ScanningMode(
      ABI::Windows::Devices::Bluetooth::Advertisement::BluetoothLEScanningMode
          value) override;
  IFACEMETHODIMP get_SignalStrengthFilter(
      ABI::Windows::Devices::Bluetooth::IBluetoothSignalStrengthFilter** value)
      override;
  IFACEMETHODIMP put_SignalStrengthFilter(
      ABI::Windows::Devices::Bluetooth::IBluetoothSignalStrengthFilter* value)
      override;
  IFACEMETHODIMP get_AdvertisementFilter(
      ABI::Windows::Devices::Bluetooth::Advertisement::
          IBluetoothLEAdvertisementFilter** value) override;
  IFACEMETHODIMP put_AdvertisementFilter(
      ABI::Windows::Devices::Bluetooth::Advertisement::
          IBluetoothLEAdvertisementFilter* value) override;
  IFACEMETHODIMP Start() override;
  IFACEMETHODIMP Stop() override;
  IFACEMETHODIMP add_Received(
      ABI::Windows::Foundation::ITypedEventHandler<
          ABI::Windows::Devices::Bluetooth::Advertisement::
              BluetoothLEAdvertisementWatcher*,
          ABI::Windows::Devices::Bluetooth::Advertisement::
              BluetoothLEAdvertisementReceivedEventArgs*>* handler,
      EventRegistrationToken* token) override;
  IFACEMETHODIMP remove_Received(EventRegistrationToken token) override;
  IFACEMETHODIMP add_Stopped(
      ABI::Windows::Foundation::ITypedEventHandler<
          ABI::Windows::Devices::Bluetooth::Advertisement::
              BluetoothLEAdvertisementWatcher*,
          ABI::Windows::Devices::Bluetooth::Advertisement::
              BluetoothLEAdvertisementWatcherStoppedEventArgs*>* handler,
      EventRegistrationToken* token) override;
  IFACEMETHODIMP remove_Stopped(EventRegistrationToken token) override;

  void SimulateLowEnergyDevice(
      const BluetoothTestBase::LowEnergyDeviceData& device_data);
  void SimulateDiscoveryError();

 private:
  ABI::Windows::Devices::Bluetooth::Advertisement::
      BluetoothLEAdvertisementWatcherStatus status_ =
          ABI::Windows::Devices::Bluetooth::Advertisement::
              BluetoothLEAdvertisementWatcherStatus_Created;

  Microsoft::WRL::ComPtr<ABI::Windows::Foundation::ITypedEventHandler<
      ABI::Windows::Devices::Bluetooth::Advertisement::
          BluetoothLEAdvertisementWatcher*,
      ABI::Windows::Devices::Bluetooth::Advertisement::
          BluetoothLEAdvertisementReceivedEventArgs*>>
      received_handler_;

  Microsoft::WRL::ComPtr<ABI::Windows::Foundation::ITypedEventHandler<
      ABI::Windows::Devices::Bluetooth::Advertisement::
          BluetoothLEAdvertisementWatcher*,
      ABI::Windows::Devices::Bluetooth::Advertisement::
          BluetoothLEAdvertisementWatcherStoppedEventArgs*>>
      stopped_handler_;
};

}  // namespace device

#endif  // DEVICE_BLUETOOTH_TEST_FAKE_BLUETOOTH_LE_ADVERTISEMENT_WATCHER_WINRT_H_
