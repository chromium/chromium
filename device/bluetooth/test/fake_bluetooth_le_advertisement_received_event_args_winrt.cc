// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/test/fake_bluetooth_le_advertisement_received_event_args_winrt.h"

#include <string_view>
#include <utility>

#include "device/bluetooth/test/fake_bluetooth_adapter_winrt.h"

namespace device {

namespace {

using ABI::Windows::Devices::Bluetooth::Advertisement::
    BluetoothLEAdvertisementType;
using ABI::Windows::Devices::Bluetooth::Advertisement::
    IBluetoothLEAdvertisement;
using ABI::Windows::Devices::Bluetooth::IBluetoothSignalStrengthFilter;
using ABI::Windows::Foundation::DateTime;
using Microsoft::WRL::ComPtr;

}  // namespace

FakeBluetoothLEAdvertisementReceivedEventArgsWinrt::
    FakeBluetoothLEAdvertisementReceivedEventArgsWinrt(
        int16_t rssi,
        std::string_view address,
        ComPtr<IBluetoothLEAdvertisement> advertisement)
    : rssi_(rssi),
      raw_address_(FakeBluetoothAdapterWinrt::ToRawBluetoothAddress(address)),
      advertisement_(std::move(advertisement)) {}

FakeBluetoothLEAdvertisementReceivedEventArgsWinrt::
    ~FakeBluetoothLEAdvertisementReceivedEventArgsWinrt() = default;

HRESULT
FakeBluetoothLEAdvertisementReceivedEventArgsWinrt::get_RawSignalStrengthInDBm(
    int16_t* value) {
  *value = rssi_;
  return S_OK;
}

HRESULT
FakeBluetoothLEAdvertisementReceivedEventArgsWinrt::get_BluetoothAddress(
    uint64_t* value) {
  *value = raw_address_;
  return S_OK;
}

HRESULT
FakeBluetoothLEAdvertisementReceivedEventArgsWinrt::get_AdvertisementType(
    BluetoothLEAdvertisementType* value) {
  return E_NOTIMPL;
}

HRESULT FakeBluetoothLEAdvertisementReceivedEventArgsWinrt::get_Timestamp(
    DateTime* value) {
  return E_NOTIMPL;
}

HRESULT FakeBluetoothLEAdvertisementReceivedEventArgsWinrt::get_Advertisement(
    IBluetoothLEAdvertisement** value) {
  return advertisement_.CopyTo(value);
}

}  // namespace device
