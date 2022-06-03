// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/test/fake_bluetooth_le_advertisement_publisher_winrt.h"

#include <utility>

#include "base/check.h"
#include "device/bluetooth/test/fake_bluetooth_le_advertisement_publisher_status_changed_event_args_winrt.h"

namespace device {

namespace {

using ABI::Windows::Devices::Bluetooth::Advertisement::
    BluetoothLEAdvertisementPublisher;
using ABI::Windows::Devices::Bluetooth::Advertisement::
    BluetoothLEAdvertisementPublisherStatus;
using ABI::Windows::Devices::Bluetooth::Advertisement::
    BluetoothLEAdvertisementPublisherStatus_Aborted;
using ABI::Windows::Devices::Bluetooth::Advertisement::
    BluetoothLEAdvertisementPublisherStatus_Started;
using ABI::Windows::Devices::Bluetooth::Advertisement::
    BluetoothLEAdvertisementPublisherStatus_Stopped;
using ABI::Windows::Devices::Bluetooth::Advertisement::
    BluetoothLEAdvertisementPublisherStatus_Stopping;
using ABI::Windows::Devices::Bluetooth::Advertisement::
    BluetoothLEAdvertisementPublisherStatus_Waiting;
using ABI::Windows::Devices::Bluetooth::Advertisement::
    BluetoothLEAdvertisementPublisherStatusChangedEventArgs;
using ABI::Windows::Devices::Bluetooth::Advertisement::
    IBluetoothLEAdvertisement;
using ABI::Windows::Devices::Bluetooth::Advertisement::
    IBluetoothLEAdvertisementPublisher;
using ABI::Windows::Devices::Bluetooth::BluetoothError_OtherError;
using ABI::Windows::Devices::Bluetooth::BluetoothError_NotSupported;
using ABI::Windows::Devices::Bluetooth::BluetoothError_RadioNotAvailable;
using ABI::Windows::Foundation::ITypedEventHandler;
using Microsoft::WRL::ComPtr;
using Microsoft::WRL::Make;

}  // namespace

FakeBluetoothLEAdvertisementPublisherWinrt::
    FakeBluetoothLEAdvertisementPublisherWinrt(
        ComPtr<IBluetoothLEAdvertisement> advertisement)
    : advertisement_(std::move(advertisement)) {}

FakeBluetoothLEAdvertisementPublisherWinrt::
    ~FakeBluetoothLEAdvertisementPublisherWinrt() = default;

HRESULT FakeBluetoothLEAdvertisementPublisherWinrt::get_Status(
    BluetoothLEAdvertisementPublisherStatus* value) {
  *value = status_;
  return S_OK;
}

HRESULT FakeBluetoothLEAdvertisementPublisherWinrt::get_Advertisement(
    IBluetoothLEAdvertisement** value) {
  return advertisement_.CopyTo(value);
}

HRESULT FakeBluetoothLEAdvertisementPublisherWinrt::Start() {
  status_ = BluetoothLEAdvertisementPublisherStatus_Waiting;
  return S_OK;
}

HRESULT FakeBluetoothLEAdvertisementPublisherWinrt::Stop() {
  status_ = BluetoothLEAdvertisementPublisherStatus_Stopping;
  return S_OK;
}

HRESULT FakeBluetoothLEAdvertisementPublisherWinrt::add_StatusChanged(
    ITypedEventHandler<
        BluetoothLEAdvertisementPublisher*,
        BluetoothLEAdvertisementPublisherStatusChangedEventArgs*>* handler,
    EventRegistrationToken* token) {
  handler_ = handler;
  return S_OK;
}

HRESULT FakeBluetoothLEAdvertisementPublisherWinrt::remove_StatusChanged(
    EventRegistrationToken token) {
  handler_.Reset();
  return S_OK;
}

void FakeBluetoothLEAdvertisementPublisherWinrt::
    SimulateAdvertisementStarted() {
  if (status_ == BluetoothLEAdvertisementPublisherStatus_Started)
    return;

  DCHECK(handler_);
  handler_->Invoke(
      this,
      Make<FakeBluetoothLEAdvertisementPublisherStatusChangedEventArgsWinrt>(
          BluetoothLEAdvertisementPublisherStatus_Waiting)
          .Get());

  status_ = BluetoothLEAdvertisementPublisherStatus_Started;
  handler_->Invoke(
      this,
      Make<FakeBluetoothLEAdvertisementPublisherStatusChangedEventArgsWinrt>(
          BluetoothLEAdvertisementPublisherStatus_Started)
          .Get());
}

void FakeBluetoothLEAdvertisementPublisherWinrt::
    SimulateAdvertisementStopped() {
  if (status_ == BluetoothLEAdvertisementPublisherStatus_Stopped)
    return;

  DCHECK(handler_);
  handler_->Invoke(
      this,
      Make<FakeBluetoothLEAdvertisementPublisherStatusChangedEventArgsWinrt>(
          BluetoothLEAdvertisementPublisherStatus_Stopping)
          .Get());

  status_ = BluetoothLEAdvertisementPublisherStatus_Stopped;
  handler_->Invoke(
      this,
      Make<FakeBluetoothLEAdvertisementPublisherStatusChangedEventArgsWinrt>(
          BluetoothLEAdvertisementPublisherStatus_Stopped)
          .Get());
}

void FakeBluetoothLEAdvertisementPublisherWinrt::SimulateAdvertisementError(
    BluetoothAdvertisement::ErrorCode error_code) {
  const auto bluetooth_error = [error_code] {
    switch (error_code) {
      case BluetoothAdvertisement::ERROR_ADAPTER_POWERED_OFF:
        return BluetoothError_RadioNotAvailable;
      case BluetoothAdvertisement::ERROR_UNSUPPORTED_PLATFORM:
        return BluetoothError_NotSupported;
      default:
        return BluetoothError_OtherError;
    }
  }();

  DCHECK(handler_);
  handler_->Invoke(
      this,
      Make<FakeBluetoothLEAdvertisementPublisherStatusChangedEventArgsWinrt>(
          BluetoothLEAdvertisementPublisherStatus_Aborted, bluetooth_error)
          .Get());
}

FakeBluetoothLEAdvertisementPublisherFactoryWinrt::
    FakeBluetoothLEAdvertisementPublisherFactoryWinrt() = default;

FakeBluetoothLEAdvertisementPublisherFactoryWinrt::
    ~FakeBluetoothLEAdvertisementPublisherFactoryWinrt() = default;

HRESULT FakeBluetoothLEAdvertisementPublisherFactoryWinrt::Create(
    IBluetoothLEAdvertisement* advertisement,
    IBluetoothLEAdvertisementPublisher** value) {
  return Make<FakeBluetoothLEAdvertisementPublisherWinrt>(advertisement)
      .CopyTo(value);
}

}  // namespace device
