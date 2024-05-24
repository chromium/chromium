// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/test/fake_bluetooth_le_advertisement_watcher_winrt.h"

#include "device/bluetooth/test/fake_bluetooth_le_advertisement_received_event_args_winrt.h"
#include "device/bluetooth/test/fake_bluetooth_le_advertisement_winrt.h"

namespace device {

namespace {

using ABI::Windows::Devices::Bluetooth::BluetoothError;
using ABI::Windows::Devices::Bluetooth::BluetoothError_OtherError;
using ABI::Windows::Devices::Bluetooth::IBluetoothSignalStrengthFilter;
using ABI::Windows::Devices::Bluetooth::Advertisement::
    BluetoothLEAdvertisementReceivedEventArgs;
using ABI::Windows::Devices::Bluetooth::Advertisement::
    BluetoothLEAdvertisementWatcher;
using ABI::Windows::Devices::Bluetooth::Advertisement::
    BluetoothLEAdvertisementWatcherStatus;
using ABI::Windows::Devices::Bluetooth::Advertisement::
    BluetoothLEAdvertisementWatcherStatus_Started;
using ABI::Windows::Devices::Bluetooth::Advertisement::
    BluetoothLEAdvertisementWatcherStatus_Stopped;
using ABI::Windows::Devices::Bluetooth::Advertisement::
    BluetoothLEAdvertisementWatcherStoppedEventArgs;
using ABI::Windows::Devices::Bluetooth::Advertisement::BluetoothLEScanningMode;
using ABI::Windows::Devices::Bluetooth::Advertisement::
    IBluetoothLEAdvertisementFilter;
using ABI::Windows::Foundation::ITypedEventHandler;
using ABI::Windows::Foundation::TimeSpan;
using Microsoft::WRL::Make;

class FakeBluetoothLEAdvertisementWatcherStoppedEventArgsWinrt
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<
              Microsoft::WRL::WinRt | Microsoft::WRL::InhibitRoOriginateError>,
          ABI::Windows::Devices::Bluetooth::Advertisement::
              IBluetoothLEAdvertisementWatcherStoppedEventArgs> {
 public:
  FakeBluetoothLEAdvertisementWatcherStoppedEventArgsWinrt(BluetoothError error)
      : error_(error) {}
  FakeBluetoothLEAdvertisementWatcherStoppedEventArgsWinrt(
      const FakeBluetoothLEAdvertisementWatcherStoppedEventArgsWinrt&) = delete;
  FakeBluetoothLEAdvertisementWatcherStoppedEventArgsWinrt& operator=(
      const FakeBluetoothLEAdvertisementWatcherStoppedEventArgsWinrt&) = delete;
  ~FakeBluetoothLEAdvertisementWatcherStoppedEventArgsWinrt() override {}

  // IBluetoothLEAdvertisementWatcherStoppedEventArgs:
  IFACEMETHODIMP get_Error(
      ABI::Windows::Devices::Bluetooth::BluetoothError* value) override {
    *value = error_;
    return S_OK;
  }

 private:
  BluetoothError error_;
};

}  // namespace

FakeBluetoothLEAdvertisementWatcherWinrt::
    FakeBluetoothLEAdvertisementWatcherWinrt() = default;

FakeBluetoothLEAdvertisementWatcherWinrt::
    ~FakeBluetoothLEAdvertisementWatcherWinrt() = default;

HRESULT FakeBluetoothLEAdvertisementWatcherWinrt::get_MinSamplingInterval(
    TimeSpan* value) {
  return E_NOTIMPL;
}

HRESULT FakeBluetoothLEAdvertisementWatcherWinrt::get_MaxSamplingInterval(
    TimeSpan* value) {
  return E_NOTIMPL;
}

HRESULT FakeBluetoothLEAdvertisementWatcherWinrt::get_MinOutOfRangeTimeout(
    TimeSpan* value) {
  return E_NOTIMPL;
}

HRESULT FakeBluetoothLEAdvertisementWatcherWinrt::get_MaxOutOfRangeTimeout(
    TimeSpan* value) {
  return E_NOTIMPL;
}

HRESULT FakeBluetoothLEAdvertisementWatcherWinrt::get_Status(
    BluetoothLEAdvertisementWatcherStatus* value) {
  *value = status_;
  return S_OK;
}

HRESULT FakeBluetoothLEAdvertisementWatcherWinrt::get_ScanningMode(
    BluetoothLEScanningMode* value) {
  return E_NOTIMPL;
}

HRESULT FakeBluetoothLEAdvertisementWatcherWinrt::put_ScanningMode(
    BluetoothLEScanningMode value) {
  return S_OK;
}

HRESULT FakeBluetoothLEAdvertisementWatcherWinrt::get_SignalStrengthFilter(
    IBluetoothSignalStrengthFilter** value) {
  return E_NOTIMPL;
}

HRESULT FakeBluetoothLEAdvertisementWatcherWinrt::put_SignalStrengthFilter(
    IBluetoothSignalStrengthFilter* value) {
  return E_NOTIMPL;
}

HRESULT FakeBluetoothLEAdvertisementWatcherWinrt::get_AdvertisementFilter(
    IBluetoothLEAdvertisementFilter** value) {
  return E_NOTIMPL;
}

HRESULT FakeBluetoothLEAdvertisementWatcherWinrt::put_AdvertisementFilter(
    IBluetoothLEAdvertisementFilter* value) {
  return E_NOTIMPL;
}

HRESULT FakeBluetoothLEAdvertisementWatcherWinrt::Start() {
  status_ = BluetoothLEAdvertisementWatcherStatus_Started;
  return S_OK;
}

HRESULT FakeBluetoothLEAdvertisementWatcherWinrt::Stop() {
  status_ = BluetoothLEAdvertisementWatcherStatus_Stopped;
  return S_OK;
}

HRESULT FakeBluetoothLEAdvertisementWatcherWinrt::add_Received(
    ITypedEventHandler<BluetoothLEAdvertisementWatcher*,
                       BluetoothLEAdvertisementReceivedEventArgs*>* handler,
    EventRegistrationToken* token) {
  received_handler_ = handler;
  return S_OK;
}

HRESULT FakeBluetoothLEAdvertisementWatcherWinrt::remove_Received(
    EventRegistrationToken token) {
  received_handler_.Reset();
  return S_OK;
}

HRESULT FakeBluetoothLEAdvertisementWatcherWinrt::add_Stopped(
    ITypedEventHandler<BluetoothLEAdvertisementWatcher*,
                       BluetoothLEAdvertisementWatcherStoppedEventArgs*>*
        handler,
    EventRegistrationToken* token) {
  stopped_handler_ = handler;
  return S_OK;
}

HRESULT FakeBluetoothLEAdvertisementWatcherWinrt::remove_Stopped(
    EventRegistrationToken token) {
  stopped_handler_.Reset();
  return S_OK;
}

void FakeBluetoothLEAdvertisementWatcherWinrt::SimulateLowEnergyDevice(
    const BluetoothTestBase::LowEnergyDeviceData& device_data) {
  if (received_handler_) {
    received_handler_->Invoke(
        this, Make<FakeBluetoothLEAdvertisementReceivedEventArgsWinrt>(
                  device_data.rssi, device_data.address,
                  Make<FakeBluetoothLEAdvertisementWinrt>(
                      device_data.name, device_data.flags,
                      device_data.advertised_uuids, device_data.tx_power,
                      device_data.service_data, device_data.manufacturer_data))
                  .Get());
  }
}

void FakeBluetoothLEAdvertisementWatcherWinrt::SimulateDiscoveryError() {
  if (stopped_handler_) {
    stopped_handler_->Invoke(
        this, Make<FakeBluetoothLEAdvertisementWatcherStoppedEventArgsWinrt>(
                  BluetoothError_OtherError)
                  .Get());
  }
}

}  // namespace device
