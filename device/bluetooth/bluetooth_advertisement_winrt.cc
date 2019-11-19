// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_advertisement_winrt.h"

#include <windows.foundation.collections.h>
#include <windows.storage.streams.h>

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/strings/string_piece.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/win/core_winrt_util.h"
#include "base/win/scoped_hstring.h"
#include "base/win/winrt_storage_util.h"
#include "device/bluetooth/event_utils_winrt.h"

namespace device {

namespace {

using ABI::Windows::Devices::Bluetooth::Advertisement::
    BluetoothLEAdvertisementPublisherStatus;
using ABI::Windows::Devices::Bluetooth::Advertisement::
    BluetoothLEAdvertisementPublisherStatus_Aborted;
using ABI::Windows::Devices::Bluetooth::Advertisement::
    BluetoothLEAdvertisementPublisherStatus_Started;
using ABI::Windows::Devices::Bluetooth::Advertisement::
    BluetoothLEAdvertisementPublisherStatus_Stopped;
using ABI::Windows::Devices::Bluetooth::Advertisement::
    BluetoothLEManufacturerData;
using ABI::Windows::Devices::Bluetooth::Advertisement::
    IBluetoothLEAdvertisement;
using ABI::Windows::Devices::Bluetooth::Advertisement::
    IBluetoothLEAdvertisementPublisher;
using ABI::Windows::Devices::Bluetooth::Advertisement::
    IBluetoothLEAdvertisementPublisherFactory;
using ABI::Windows::Devices::Bluetooth::Advertisement::
    IBluetoothLEAdvertisementPublisherStatusChangedEventArgs;
using ABI::Windows::Devices::Bluetooth::Advertisement::
    IBluetoothLEManufacturerData;
using ABI::Windows::Devices::Bluetooth::Advertisement::
    IBluetoothLEManufacturerDataFactory;
using ABI::Windows::Devices::Bluetooth::BluetoothError;
using ABI::Windows::Devices::Bluetooth::BluetoothError_NotSupported;
using ABI::Windows::Devices::Bluetooth::BluetoothError_RadioNotAvailable;
using ABI::Windows::Foundation::Collections::IVector;
using ABI::Windows::Storage::Streams::IBuffer;
using Microsoft::WRL::ComPtr;

void RemoveStatusChangedHandler(IBluetoothLEAdvertisementPublisher* publisher,
                                EventRegistrationToken token) {
  HRESULT hr = publisher->remove_StatusChanged(token);
  if (FAILED(hr)) {
    VLOG(2) << "Removing StatusChanged Handler failed: "
            << logging::SystemErrorCodeToString(hr);
  }
}

}  // namespace

BluetoothAdvertisementWinrt::BluetoothAdvertisementWinrt() {}

bool BluetoothAdvertisementWinrt::Initialize(
    std::unique_ptr<BluetoothAdvertisement::Data> advertisement_data) {
  if (advertisement_data->service_uuids()) {
    VLOG(2) << "Windows does not support advertising Service UUIDs.";
    return false;
  }

  if (advertisement_data->solicit_uuids()) {
    VLOG(2) << "Windows does not support advertising Solicit UUIDs.";
    return false;
  }

  if (advertisement_data->service_data()) {
    VLOG(2) << "Windows does not support advertising Service Data.";
    return false;
  }

  auto manufacturer_data = advertisement_data->manufacturer_data();
  if (!manufacturer_data) {
    VLOG(2) << "No Manufacturer Data present.";
    return false;
  }

  ComPtr<IBluetoothLEAdvertisement> advertisement;
  HRESULT hr = ActivateBluetoothLEAdvertisementInstance(&advertisement);
  if (FAILED(hr)) {
    VLOG(2) << "ActivateBluetoothLEAdvertisementInstance failed: "
            << logging::SystemErrorCodeToString(hr);
    return false;
  }

  ComPtr<IVector<BluetoothLEManufacturerData*>> manufacturer_data_list;
  hr = advertisement->get_ManufacturerData(&manufacturer_data_list);
  if (FAILED(hr)) {
    VLOG(2) << "Getting ManufacturerData failed: "
            << logging::SystemErrorCodeToString(hr);
    return false;
  }

  ComPtr<IBluetoothLEManufacturerDataFactory> manufacturer_data_factory;
  hr = GetBluetoothLEManufacturerDataFactory(&manufacturer_data_factory);
  if (FAILED(hr)) {
    VLOG(2) << "GetBluetoothLEManufacturerDataFactory failed: "
            << logging::SystemErrorCodeToString(hr);
    return false;
  }

  for (const auto& pair : *manufacturer_data) {
    uint16_t manufacturer = pair.first;
    const std::vector<uint8_t>& data = pair.second;

    ComPtr<IBuffer> buffer;
    hr = base::win::CreateIBufferFromData(data.data(), data.size(), &buffer);
    if (FAILED(hr)) {
      VLOG(2) << "CreateIBufferFromData() failed: "
              << logging::SystemErrorCodeToString(hr);
      return false;
    }

    ComPtr<IBluetoothLEManufacturerData> manufacturer_data_entry;
    hr = manufacturer_data_factory->Create(manufacturer, buffer.Get(),
                                           &manufacturer_data_entry);
    if (FAILED(hr)) {
      VLOG(2) << "Creating BluetoothLEManufacturerData failed: "
              << logging::SystemErrorCodeToString(hr);
      return false;
    }

    hr = manufacturer_data_list->Append(manufacturer_data_entry.Get());
    if (FAILED(hr)) {
      VLOG(2) << "Appending BluetoothLEManufacturerData failed: "
              << logging::SystemErrorCodeToString(hr);
      return false;
    }
  }

  ComPtr<IBluetoothLEAdvertisementPublisherFactory> publisher_factory;
  hr =
      GetBluetoothLEAdvertisementPublisherActivationFactory(&publisher_factory);
  if (FAILED(hr)) {
    VLOG(2) << "GetBluetoothLEAdvertisementPublisherActivationFactory "
               "failed:"
            << logging::SystemErrorCodeToString(hr);
    return false;
  }

  hr = publisher_factory->Create(advertisement.Get(), &publisher_);
  if (FAILED(hr)) {
    VLOG(2) << "Creating IBluetoothLEAdvertisementPublisher failed: "
            << logging::SystemErrorCodeToString(hr);
    return false;
  }

  return true;
}

void BluetoothAdvertisementWinrt::Register(SuccessCallback callback,
                                           ErrorCallback error_callback) {
  // Register should only be called once during initialization.
  DCHECK(!status_changed_token_);
  DCHECK(!pending_register_callbacks_);
  DCHECK(!pending_unregister_callbacks_);

  // Register should only be called after successful initialization.
  DCHECK(publisher_);

  status_changed_token_ = AddTypedEventHandler(
      publisher_.Get(), &IBluetoothLEAdvertisementPublisher::add_StatusChanged,
      base::BindRepeating(&BluetoothAdvertisementWinrt::OnStatusChanged,
                          weak_ptr_factory_.GetWeakPtr()));
  if (!status_changed_token_) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(error_callback),
                                  ERROR_STARTING_ADVERTISEMENT));
    return;
  }

  HRESULT hr = publisher_->Start();
  if (FAILED(hr)) {
    VLOG(2) << "Starting IBluetoothLEAdvertisementPublisher failed: "
            << logging::SystemErrorCodeToString(hr);
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(error_callback),
                                  ERROR_STARTING_ADVERTISEMENT));
    RemoveStatusChangedHandler(publisher_.Get(), *status_changed_token_);
    status_changed_token_.reset();
    return;
  }

  pending_register_callbacks_ = std::make_unique<PendingCallbacks>(
      std::move(callback), std::move(error_callback));
}

void BluetoothAdvertisementWinrt::Unregister(
    const SuccessCallback& success_callback,
    const ErrorCallback& error_callback) {
  // Unregister() should only be called when an advertisement is registered
  // already, or during destruction. In both of these cases there should be no
  // pending register callbacks and the publisher should be present.
  DCHECK(!pending_register_callbacks_);
  DCHECK(publisher_);

  if (pending_unregister_callbacks_) {
    VLOG(2) << "An Unregister Operation is already in progress.";
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(error_callback, ERROR_RESET_ADVERTISING));
    return;
  }

  BluetoothLEAdvertisementPublisherStatus status;
  HRESULT hr = publisher_->get_Status(&status);
  if (FAILED(hr)) {
    VLOG(2) << "Getting the Publisher Status failed: "
            << logging::SystemErrorCodeToString(hr);
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(error_callback, ERROR_RESET_ADVERTISING));
    return;
  }

  if (status == BluetoothLEAdvertisementPublisherStatus_Aborted) {
    // Report an error if the publisher is in the aborted state.
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(error_callback, ERROR_RESET_ADVERTISING));
    return;
  }

  if (status == BluetoothLEAdvertisementPublisherStatus_Stopped) {
    // Report success if the publisher is already stopped.
    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE, success_callback);
    return;
  }

  hr = publisher_->Stop();
  if (FAILED(hr)) {
    VLOG(2) << "IBluetoothLEAdvertisementPublisher::Stop() failed: "
            << logging::SystemErrorCodeToString(hr);
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(error_callback, ERROR_RESET_ADVERTISING));
    return;
  }

  pending_unregister_callbacks_ =
      std::make_unique<PendingCallbacks>(success_callback, error_callback);
}

IBluetoothLEAdvertisementPublisher*
BluetoothAdvertisementWinrt::GetPublisherForTesting() {
  return publisher_.Get();
}

BluetoothAdvertisementWinrt::~BluetoothAdvertisementWinrt() {
  if (status_changed_token_) {
    DCHECK(publisher_);
    RemoveStatusChangedHandler(publisher_.Get(), *status_changed_token_);
  }

  // Stop any pending register operation.
  if (pending_register_callbacks_) {
    auto callbacks = std::move(pending_register_callbacks_);
    std::move(callbacks->error_callback).Run(ERROR_STARTING_ADVERTISEMENT);
  }

  // Unregister the advertisement on a best effort basis if it's not already in
  // process of doing so.
  if (!pending_unregister_callbacks_ && publisher_)
    Unregister(base::DoNothing(), base::DoNothing());
}

HRESULT
BluetoothAdvertisementWinrt::
    GetBluetoothLEAdvertisementPublisherActivationFactory(
        IBluetoothLEAdvertisementPublisherFactory** factory) const {
  return base::win::GetActivationFactory<
      IBluetoothLEAdvertisementPublisherFactory,
      RuntimeClass_Windows_Devices_Bluetooth_Advertisement_BluetoothLEAdvertisementPublisher>(
      factory);
}

HRESULT
BluetoothAdvertisementWinrt::ActivateBluetoothLEAdvertisementInstance(
    IBluetoothLEAdvertisement** instance) const {
  auto advertisement_hstring = base::win::ScopedHString::Create(
      RuntimeClass_Windows_Devices_Bluetooth_Advertisement_BluetoothLEAdvertisement);
  if (!advertisement_hstring.is_valid())
    return E_FAIL;

  ComPtr<IInspectable> inspectable;
  HRESULT hr =
      base::win::RoActivateInstance(advertisement_hstring.get(), &inspectable);
  if (FAILED(hr)) {
    VLOG(2) << "RoActivateInstance failed: "
            << logging::SystemErrorCodeToString(hr);
    return hr;
  }

  ComPtr<IBluetoothLEAdvertisement> advertisement;
  hr = inspectable.As(&advertisement);
  if (FAILED(hr)) {
    VLOG(2) << "As IBluetoothLEAdvertisementWatcher failed: "
            << logging::SystemErrorCodeToString(hr);
    return hr;
  }

  return advertisement.CopyTo(instance);
}

HRESULT
BluetoothAdvertisementWinrt::GetBluetoothLEManufacturerDataFactory(
    IBluetoothLEManufacturerDataFactory** factory) const {
  return base::win::GetActivationFactory<
      IBluetoothLEManufacturerDataFactory,
      RuntimeClass_Windows_Devices_Bluetooth_Advertisement_BluetoothLEManufacturerData>(
      factory);
}

BluetoothAdvertisementWinrt::PendingCallbacks::PendingCallbacks(
    SuccessCallback callback,
    ErrorCallback error_callback)
    : callback(std::move(callback)),
      error_callback(std::move(error_callback)) {}

BluetoothAdvertisementWinrt::PendingCallbacks::~PendingCallbacks() = default;

void BluetoothAdvertisementWinrt::OnStatusChanged(
    IBluetoothLEAdvertisementPublisher* publisher,
    IBluetoothLEAdvertisementPublisherStatusChangedEventArgs* changed) {
  BluetoothLEAdvertisementPublisherStatus status;
  HRESULT hr = changed->get_Status(&status);
  if (FAILED(hr)) {
    VLOG(2) << "Getting the Publisher Status failed: "
            << logging::SystemErrorCodeToString(hr);
    return;
  }

  VLOG(2) << "Publisher Status: " << static_cast<int>(status);
  if (status == BluetoothLEAdvertisementPublisherStatus_Stopped) {
    // Notify Observers.
    for (auto& observer : observers_)
      observer.AdvertisementReleased(this);
  }

  // Return early if there is no pending action.
  if (!pending_register_callbacks_ && !pending_unregister_callbacks_)
    return;

  // Register and Unregister should never be pending at the same time.
  DCHECK(!pending_register_callbacks_ || !pending_unregister_callbacks_);

  const bool is_starting = pending_register_callbacks_ != nullptr;
  ErrorCode error_code =
      is_starting ? ERROR_STARTING_ADVERTISEMENT : ERROR_RESET_ADVERTISING;

  // Clears out pending callbacks by moving them into a local variable and runs
  // the appropriate error callback with |error_code|.
  auto run_error_cb = [&](ErrorCode error_code) {
    auto callbacks = std::move(is_starting ? pending_register_callbacks_
                                           : pending_unregister_callbacks_);
    std::move(callbacks->error_callback).Run(error_code);
  };

  if (status == BluetoothLEAdvertisementPublisherStatus_Aborted) {
    VLOG(2) << "The Publisher aborted.";
    BluetoothError bluetooth_error;
    hr = changed->get_Error(&bluetooth_error);
    if (FAILED(hr)) {
      VLOG(2) << "Getting the Publisher Error failed: "
              << logging::SystemErrorCodeToString(hr);
      run_error_cb(error_code);
      return;
    }

    VLOG(2) << "Publisher Error: " << static_cast<int>(bluetooth_error);
    switch (bluetooth_error) {
      case BluetoothError_RadioNotAvailable:
        error_code = ERROR_ADAPTER_POWERED_OFF;
        break;
      case BluetoothError_NotSupported:
        error_code = ERROR_UNSUPPORTED_PLATFORM;
        break;
      default:
        break;
    }

    run_error_cb(error_code);
    return;
  }

  if (is_starting &&
      status == BluetoothLEAdvertisementPublisherStatus_Started) {
    VLOG(2) << "Starting the Publisher was successful.";
    auto callbacks = std::move(pending_register_callbacks_);
    std::move(callbacks->callback).Run();
    return;
  }

  if (!is_starting &&
      status == BluetoothLEAdvertisementPublisherStatus_Stopped) {
    VLOG(2) << "Stopping the Publisher was successful.";
    auto callbacks = std::move(pending_unregister_callbacks_);
    std::move(callbacks->callback).Run();
    return;
  }

  // The other states are temporary and we expect a future StatusChanged
  // event.
}

}  // namespace device
