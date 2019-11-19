// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_pairing_winrt.h"

#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "base/strings/string_piece.h"
#include "base/win/post_async_results.h"
#include "base/win/scoped_hstring.h"
#include "device/bluetooth/bluetooth_device_winrt.h"
#include "device/bluetooth/event_utils_winrt.h"

namespace device {

namespace {

using ABI::Windows::Devices::Enumeration::DevicePairingKinds;
using ABI::Windows::Devices::Enumeration::DevicePairingKinds_ProvidePin;
using ABI::Windows::Devices::Enumeration::DevicePairingResult;
using ABI::Windows::Devices::Enumeration::DevicePairingResultStatus;
using ABI::Windows::Devices::Enumeration::
    DevicePairingResultStatus_AuthenticationFailure;
using ABI::Windows::Devices::Enumeration::
    DevicePairingResultStatus_AuthenticationTimeout;
using ABI::Windows::Devices::Enumeration::
    DevicePairingResultStatus_ConnectionRejected;
using ABI::Windows::Devices::Enumeration::
    DevicePairingResultStatus_AlreadyPaired;
using ABI::Windows::Devices::Enumeration::DevicePairingResultStatus_Failed;
using ABI::Windows::Devices::Enumeration::
    DevicePairingResultStatus_OperationAlreadyInProgress;
using ABI::Windows::Devices::Enumeration::DevicePairingResultStatus_Paired;
using ABI::Windows::Devices::Enumeration::
    DevicePairingResultStatus_PairingCanceled;
using ABI::Windows::Devices::Enumeration::
    DevicePairingResultStatus_RejectedByHandler;
using ABI::Windows::Devices::Enumeration::IDeviceInformationCustomPairing;
using ABI::Windows::Devices::Enumeration::IDevicePairingRequestedEventArgs;
using ABI::Windows::Devices::Enumeration::IDevicePairingResult;
using ABI::Windows::Foundation::IAsyncOperation;
using Microsoft::WRL::ComPtr;

void PostTask(BluetoothPairingWinrt::ErrorCallback error_callback,
              BluetoothDevice::ConnectErrorCode error_code) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(error_callback), error_code));
}

}  // namespace

BluetoothPairingWinrt::BluetoothPairingWinrt(
    BluetoothDeviceWinrt* device,
    BluetoothDevice::PairingDelegate* pairing_delegate,
    ComPtr<IDeviceInformationCustomPairing> custom_pairing,
    Callback callback,
    ErrorCallback error_callback)
    : device_(device),
      pairing_delegate_(pairing_delegate),
      custom_pairing_(std::move(custom_pairing)),
      callback_(std::move(callback)),
      error_callback_(std::move(error_callback)) {
  DCHECK(device_);
  DCHECK(pairing_delegate_);
  DCHECK(custom_pairing_);
}

BluetoothPairingWinrt::~BluetoothPairingWinrt() {
  if (!pairing_requested_token_)
    return;

  HRESULT hr =
      custom_pairing_->remove_PairingRequested(*pairing_requested_token_);
  if (FAILED(hr)) {
    VLOG(2) << "Removing PairingRequested Handler failed: "
            << logging::SystemErrorCodeToString(hr);
  }
}

void BluetoothPairingWinrt::StartPairing() {
  pairing_requested_token_ = AddTypedEventHandler(
      custom_pairing_.Get(),
      &IDeviceInformationCustomPairing::add_PairingRequested,
      base::BindRepeating(&BluetoothPairingWinrt::OnPairingRequested,
                          weak_ptr_factory_.GetWeakPtr()));

  if (!pairing_requested_token_) {
    PostTask(std::move(error_callback_),
             BluetoothDevice::ConnectErrorCode::ERROR_FAILED);
    return;
  }

  ComPtr<IAsyncOperation<DevicePairingResult*>> pair_op;
  HRESULT hr =
      custom_pairing_->PairAsync(DevicePairingKinds_ProvidePin, &pair_op);
  if (FAILED(hr)) {
    VLOG(2) << "DeviceInformationCustomPairing::PairAsync() failed: "
            << logging::SystemErrorCodeToString(hr);
    PostTask(std::move(error_callback_),
             BluetoothDevice::ConnectErrorCode::ERROR_FAILED);
    return;
  }

  hr = base::win::PostAsyncResults(
      std::move(pair_op), base::BindOnce(&BluetoothPairingWinrt::OnPair,
                                         weak_ptr_factory_.GetWeakPtr()));

  if (FAILED(hr)) {
    VLOG(2) << "PostAsyncResults failed: "
            << logging::SystemErrorCodeToString(hr);
    PostTask(std::move(error_callback_),
             BluetoothDevice::ConnectErrorCode::ERROR_FAILED);
    return;
  }
}

bool BluetoothPairingWinrt::ExpectingPinCode() const {
  return expecting_pin_code_;
}

void BluetoothPairingWinrt::SetPinCode(base::StringPiece pin_code) {
  VLOG(2) << "BluetoothPairingWinrt::SetPinCode(" << pin_code << ")";
  auto pin_hstring = base::win::ScopedHString::Create(pin_code);
  DCHECK(expecting_pin_code_);
  expecting_pin_code_ = false;
  DCHECK(pairing_requested_);
  HRESULT hr = pairing_requested_->AcceptWithPin(pin_hstring.get());
  if (FAILED(hr)) {
    VLOG(2) << "Accepting Pairing Request With Pin failed: "
            << logging::SystemErrorCodeToString(hr);
    std::move(error_callback_)
        .Run(BluetoothDevice::ConnectErrorCode::ERROR_FAILED);
    return;
  }

  DCHECK(pairing_deferral_);
  hr = pairing_deferral_->Complete();
  if (FAILED(hr)) {
    VLOG(2) << "Completing Deferred Pairing Request failed: "
            << logging::SystemErrorCodeToString(hr);
    std::move(error_callback_)
        .Run(BluetoothDevice::ConnectErrorCode::ERROR_FAILED);
  }
}

void BluetoothPairingWinrt::RejectPairing() {
  VLOG(2) << "BluetoothPairingWinrt::RejectPairing()";
  DCHECK(pairing_deferral_);
  HRESULT hr = pairing_deferral_->Complete();
  if (FAILED(hr)) {
    VLOG(2) << "Completing Deferred Pairing Request failed: "
            << logging::SystemErrorCodeToString(hr);
    std::move(error_callback_)
        .Run(BluetoothDevice::ConnectErrorCode::ERROR_FAILED);
    return;
  }

  std::move(error_callback_)
      .Run(BluetoothDevice::ConnectErrorCode::ERROR_AUTH_REJECTED);
}

void BluetoothPairingWinrt::CancelPairing() {
  VLOG(2) << "BluetoothPairingWinrt::CancelPairing()";
  DCHECK(pairing_deferral_);
  HRESULT hr = pairing_deferral_->Complete();
  if (FAILED(hr)) {
    VLOG(2) << "Completing Deferred Pairing Request failed: "
            << logging::SystemErrorCodeToString(hr);
    std::move(error_callback_)
        .Run(BluetoothDevice::ConnectErrorCode::ERROR_FAILED);
    return;
  }

  std::move(error_callback_)
      .Run(BluetoothDevice::ConnectErrorCode::ERROR_AUTH_CANCELED);
}

void BluetoothPairingWinrt::OnPairingRequested(
    IDeviceInformationCustomPairing* custom_pairing,
    IDevicePairingRequestedEventArgs* pairing_requested) {
  VLOG(2) << "BluetoothPairingWinrt::OnPairingRequested()";

  DevicePairingKinds pairing_kind;
  HRESULT hr = pairing_requested->get_PairingKind(&pairing_kind);
  if (FAILED(hr)) {
    VLOG(2) << "Getting Pairing Kind failed: "
            << logging::SystemErrorCodeToString(hr);
    std::move(error_callback_)
        .Run(BluetoothDevice::ConnectErrorCode::ERROR_FAILED);
    return;
  }

  VLOG(2) << "DevicePairingKind: " << static_cast<int>(pairing_kind);
  if (pairing_kind != DevicePairingKinds_ProvidePin) {
    VLOG(2) << "Unexpected DevicePairingKind.";
    std::move(error_callback_)
        .Run(BluetoothDevice::ConnectErrorCode::ERROR_FAILED);
    return;
  }

  hr = pairing_requested->GetDeferral(&pairing_deferral_);
  if (FAILED(hr)) {
    VLOG(2) << "Getting Pairing Deferral failed: "
            << logging::SystemErrorCodeToString(hr);
    std::move(error_callback_)
        .Run(BluetoothDevice::ConnectErrorCode::ERROR_FAILED);
    return;
  }

  pairing_requested_ = pairing_requested;
  expecting_pin_code_ = true;
  pairing_delegate_->RequestPinCode(device_);
}

void BluetoothPairingWinrt::OnPair(
    ComPtr<IDevicePairingResult> pairing_result) {
  DevicePairingResultStatus status;
  HRESULT hr = pairing_result->get_Status(&status);
  if (FAILED(hr)) {
    VLOG(2) << "Getting Pairing Result Status failed: "
            << logging::SystemErrorCodeToString(hr);
    std::move(error_callback_)
        .Run(BluetoothDevice::ConnectErrorCode::ERROR_FAILED);
    return;
  }

  VLOG(2) << "Pairing Result Status: " << static_cast<int>(status);
  switch (status) {
    case DevicePairingResultStatus_AlreadyPaired:
    case DevicePairingResultStatus_Paired:
      std::move(callback_).Run();
      return;
    case DevicePairingResultStatus_PairingCanceled:
      std::move(error_callback_)
          .Run(BluetoothDevice::ConnectErrorCode::ERROR_AUTH_CANCELED);
      return;
    case DevicePairingResultStatus_AuthenticationFailure:
      std::move(error_callback_)
          .Run(BluetoothDevice::ConnectErrorCode::ERROR_AUTH_FAILED);
      return;
    case DevicePairingResultStatus_ConnectionRejected:
    case DevicePairingResultStatus_RejectedByHandler:
      std::move(error_callback_)
          .Run(BluetoothDevice::ConnectErrorCode::ERROR_AUTH_REJECTED);
      return;
    case DevicePairingResultStatus_AuthenticationTimeout:
      std::move(error_callback_)
          .Run(BluetoothDevice::ConnectErrorCode::ERROR_AUTH_TIMEOUT);
      return;
    case DevicePairingResultStatus_Failed:
      std::move(error_callback_)
          .Run(BluetoothDevice::ConnectErrorCode::ERROR_FAILED);
      return;
    case DevicePairingResultStatus_OperationAlreadyInProgress:
      std::move(error_callback_)
          .Run(BluetoothDevice::ConnectErrorCode::ERROR_INPROGRESS);
      return;
    default:
      std::move(error_callback_)
          .Run(BluetoothDevice::ConnectErrorCode::ERROR_FAILED);
      return;
  }
}

}  // namespace device
