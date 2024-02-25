// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_pairing_winrt.h"

#include <utility>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/win/com_init_util.h"
#include "base/win/post_async_results.h"
#include "base/win/scoped_hstring.h"
#include "device/base/features.h"
#include "device/bluetooth/bluetooth_device_winrt.h"
#include "device/bluetooth/event_utils_winrt.h"

namespace device {

namespace {

using ABI::Windows::Devices::Enumeration::DevicePairingKinds;
using ABI::Windows::Devices::Enumeration::DevicePairingKinds_ConfirmOnly;
using ABI::Windows::Devices::Enumeration::DevicePairingKinds_ConfirmPinMatch;
using ABI::Windows::Devices::Enumeration::DevicePairingKinds_DisplayPin;
using ABI::Windows::Devices::Enumeration::DevicePairingKinds_ProvidePin;
using ABI::Windows::Devices::Enumeration::DevicePairingResult;
using ABI::Windows::Devices::Enumeration::DevicePairingResultStatus;
using ABI::Windows::Devices::Enumeration::
    DevicePairingResultStatus_AlreadyPaired;
using ABI::Windows::Devices::Enumeration::
    DevicePairingResultStatus_AuthenticationFailure;
using ABI::Windows::Devices::Enumeration::
    DevicePairingResultStatus_AuthenticationTimeout;
using ABI::Windows::Devices::Enumeration::
    DevicePairingResultStatus_ConnectionRejected;
using ABI::Windows::Devices::Enumeration::DevicePairingResultStatus_Failed;
using ABI::Windows::Devices::Enumeration::
    DevicePairingResultStatus_OperationAlreadyInProgress;
using ABI::Windows::Devices::Enumeration::DevicePairingResultStatus_Paired;
using ABI::Windows::Devices::Enumeration::
    DevicePairingResultStatus_PairingCanceled;
using ABI::Windows::Devices::Enumeration::
    DevicePairingResultStatus_RejectedByHandler;
using CompletionCallback = base::OnceCallback<void(HRESULT hr)>;
using ConnectErrorCode = BluetoothDevice::ConnectErrorCode;
using ABI::Windows::Devices::Enumeration::IDeviceInformationCustomPairing;
using ABI::Windows::Devices::Enumeration::IDevicePairingRequestedEventArgs;
using ABI::Windows::Devices::Enumeration::IDevicePairingResult;
using ABI::Windows::Foundation::IAsyncOperation;
using Microsoft::WRL::ComPtr;

void PostTask(BluetoothPairingWinrt::ConnectCallback callback,
              std::optional<BluetoothDevice::ConnectErrorCode> error_code) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), error_code));
}

HRESULT CompleteDeferral(
    Microsoft::WRL::ComPtr<ABI::Windows::Foundation::IDeferral> deferral) {
  // Apparently deferrals may be created (aka obtained) on the main thread
  // initialized for STA, but must be completed on a thread with COM initialized
  // for MTA. If the deferral is completed on the main thread then the
  // Complete() call will succeed, i.e return S_OK, but the Windows Device
  // Association Service will be hung and all Bluetooth association changes
  // (system wide) will fail.
  base::win::AssertComApartmentType(base::win::ComApartmentType::MTA);
  return deferral->Complete();
}

// TODO: https://crbug.com/1345471, once we refactor the
// BluetoothDevice::ConfirmPasskey() to use std::u16string instead of uint32_t
// we can then get rid of HstringToUint32()
bool HstringToUint32(HSTRING in, uint32_t& out) {
  if (!in) {
    DVLOG(2) << "HstringToUint32: HSTRING PIN is NULL.";
    return false;
  }

  base::win::ScopedHString scoped_hstring{in};
  std::string str = scoped_hstring.GetAsUTF8();

  // PIN has to be <= 6 digits
  if (str.length() > 6) {
    DVLOG(2) << "HstringToUint32: PIN code = " << str
             << " which is more than 6 digits.";
    return false;
  }

  // Remove leading '0' before being converted into uint32_t
  str.erase(0, str.find_first_not_of('0'));

  // If we failed to convert str into unsigned int we cancel pairing by return
  // false
  if (base::StringToUint(str, &out)) {
    return true;
  } else {
    DVLOG(2) << "HstringToUint32: failed to convert pin = " << str
             << " into uint32_t";
    return false;
  }
}

}  // namespace

BluetoothPairingWinrt::BluetoothPairingWinrt(
    BluetoothDeviceWinrt* device,
    BluetoothDevice::PairingDelegate* pairing_delegate,
    ComPtr<IDeviceInformationCustomPairing> custom_pairing,
    ConnectCallback callback)
    : device_(device),
      pairing_delegate_(pairing_delegate),
      custom_pairing_(std::move(custom_pairing)),
      callback_(std::move(callback)) {
  DCHECK(device_);
  DCHECK(pairing_delegate_);
  DCHECK(custom_pairing_);
}

BluetoothPairingWinrt::~BluetoothPairingWinrt() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!pairing_requested_token_)
    return;

  HRESULT hr =
      custom_pairing_->remove_PairingRequested(*pairing_requested_token_);
  if (FAILED(hr)) {
    DVLOG(2) << "Removing PairingRequested Handler failed: "
             << logging::SystemErrorCodeToString(hr);
  }
}

void BluetoothPairingWinrt::StartPairing() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  pairing_requested_token_ = AddTypedEventHandler(
      custom_pairing_.Get(),
      &IDeviceInformationCustomPairing::add_PairingRequested,
      base::BindRepeating(&BluetoothPairingWinrt::OnPairingRequested,
                          weak_ptr_factory_.GetWeakPtr()));

  if (!pairing_requested_token_) {
    PostTask(std::move(callback_),
             BluetoothDevice::ConnectErrorCode::ERROR_FAILED);
    return;
  }

  ComPtr<IAsyncOperation<DevicePairingResult*>> pair_op;
  HRESULT hr = custom_pairing_->PairAsync(
      DevicePairingKinds_ConfirmOnly | DevicePairingKinds_ProvidePin |
          DevicePairingKinds_ConfirmPinMatch,
      &pair_op);
  if (FAILED(hr)) {
    DVLOG(2) << "DeviceInformationCustomPairing::PairAsync() failed: "
             << logging::SystemErrorCodeToString(hr);
    PostTask(std::move(callback_),
             BluetoothDevice::ConnectErrorCode::ERROR_FAILED);
    return;
  }

  hr = base::win::PostAsyncResults(
      std::move(pair_op), base::BindOnce(&BluetoothPairingWinrt::OnPair,
                                         weak_ptr_factory_.GetWeakPtr()));

  if (FAILED(hr)) {
    DVLOG(2) << "PostAsyncResults failed: "
             << logging::SystemErrorCodeToString(hr);
    PostTask(std::move(callback_),
             BluetoothDevice::ConnectErrorCode::ERROR_FAILED);
    return;
  }
}

bool BluetoothPairingWinrt::ExpectingPinCode() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return expecting_pin_code_;
}

void BluetoothPairingWinrt::OnSetPinCodeDeferralCompletion(HRESULT hr) {
  if (FAILED(hr)) {
    DVLOG(2) << "Completing Deferred Pairing Request failed: "
             << logging::SystemErrorCodeToString(hr);
    std::move(callback_).Run(BluetoothDevice::ConnectErrorCode::ERROR_FAILED);
  }
}

void BluetoothPairingWinrt::OnConfirmPairingDeferralCompletion(HRESULT hr) {
  if (FAILED(hr)) {
    DVLOG(2) << "Completing Deferred Pairing Request failed: "
             << logging::SystemErrorCodeToString(hr);
    std::move(callback_).Run(BluetoothDevice::ConnectErrorCode::ERROR_FAILED);
  }
}

void BluetoothPairingWinrt::SetPinCode(std::string_view pin_code) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOG(2) << "BluetoothPairingWinrt::SetPinCode(" << pin_code << ")";
  auto pin_hstring = base::win::ScopedHString::Create(pin_code);
  DCHECK(expecting_pin_code_);
  expecting_pin_code_ = false;
  DCHECK(pairing_requested_);
  HRESULT hr = pairing_requested_->AcceptWithPin(pin_hstring.get());
  if (FAILED(hr)) {
    DVLOG(2) << "Accepting Pairing Request With Pin failed: "
             << logging::SystemErrorCodeToString(hr);
    std::move(callback_).Run(BluetoothDevice::ConnectErrorCode::ERROR_FAILED);
    return;
  }

  DCHECK(pairing_deferral_);
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&CompleteDeferral, std::move(pairing_deferral_)),
      base::BindOnce(&BluetoothPairingWinrt::OnSetPinCodeDeferralCompletion,
                     weak_ptr_factory_.GetWeakPtr()));
}

void BluetoothPairingWinrt::ConfirmPairing() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOG(2) << "BluetoothPairingWinrt::ConfirmPairing() is called";
  DCHECK(pairing_requested_);
  HRESULT hr = pairing_requested_->Accept();
  if (FAILED(hr)) {
    DVLOG(2) << "Accepting Pairing Request in ConfirmPairing failed: "
             << logging::SystemErrorCodeToString(hr);
    std::move(callback_).Run(BluetoothDevice::ConnectErrorCode::ERROR_FAILED);
    return;
  }

  DCHECK(pairing_deferral_);
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&CompleteDeferral, std::move(pairing_deferral_)),
      base::BindOnce(&BluetoothPairingWinrt::OnConfirmPairingDeferralCompletion,
                     weak_ptr_factory_.GetWeakPtr()));
}

void BluetoothPairingWinrt::OnRejectPairing(HRESULT hr) {
  if (FAILED(hr)) {
    DVLOG(2) << "Completing Deferred Pairing Request failed: "
             << logging::SystemErrorCodeToString(hr);
    std::move(callback_).Run(BluetoothDevice::ConnectErrorCode::ERROR_FAILED);
    return;
  }
  std::move(callback_).Run(
      BluetoothDevice::ConnectErrorCode::ERROR_AUTH_REJECTED);
}

void BluetoothPairingWinrt::RejectPairing() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOG(2) << "BluetoothPairingWinrt::RejectPairing()";
  DCHECK(pairing_deferral_);
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&CompleteDeferral, std::move(pairing_deferral_)),
      base::BindOnce(&BluetoothPairingWinrt::OnRejectPairing,
                     weak_ptr_factory_.GetWeakPtr()));
}

void BluetoothPairingWinrt::OnCancelPairing(HRESULT hr) {
  // This method is normally never called. Usually when CancelPairing() is
  // invoked the deferral is completed, which immediately calls OnPair(), which
  // runs |callback_| and destroys this object before this method can be
  // executed. However, if the deferral fails to complete, this will be run.
  if (FAILED(hr)) {
    DVLOG(2) << "Completing Deferred Pairing Request failed: "
             << logging::SystemErrorCodeToString(hr);
    std::move(callback_).Run(BluetoothDevice::ConnectErrorCode::ERROR_FAILED);
    return;
  }

  std::move(callback_).Run(
      BluetoothDevice::ConnectErrorCode::ERROR_AUTH_CANCELED);
}

void BluetoothPairingWinrt::CancelPairing() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOG(2) << "BluetoothPairingWinrt::CancelPairing()";
  DCHECK(pairing_deferral_);
  // There is no way to explicitly cancel an in-progress pairing as
  // DevicePairingRequestedEventArgs has no Cancel() method. Our approach is to
  // complete the deferral, without accepting, which results in a
  // RejectedByHandler result status. |was_cancelled_| is set so that OnPair(),
  // which is called when the deferral is completed, will know that cancellation
  // was the actual result.
  was_cancelled_ = true;
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&CompleteDeferral, std::move(pairing_deferral_)),
      base::BindOnce(&BluetoothPairingWinrt::OnCancelPairing,
                     weak_ptr_factory_.GetWeakPtr()));
}

void BluetoothPairingWinrt::OnPairingRequested(
    IDeviceInformationCustomPairing* custom_pairing,
    IDevicePairingRequestedEventArgs* pairing_requested) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOG(2) << "BluetoothPairingWinrt::OnPairingRequested()";

  DevicePairingKinds pairing_kind;
  HRESULT hr = pairing_requested->get_PairingKind(&pairing_kind);
  if (FAILED(hr)) {
    DVLOG(2) << "Getting Pairing Kind failed: "
             << logging::SystemErrorCodeToString(hr);
    std::move(callback_).Run(BluetoothDevice::ConnectErrorCode::ERROR_FAILED);
    return;
  }

  DVLOG(2) << "DevicePairingKind: " << static_cast<int>(pairing_kind);

  hr = pairing_requested->GetDeferral(&pairing_deferral_);
  if (FAILED(hr)) {
    DVLOG(2) << "Getting Pairing Deferral failed: "
             << logging::SystemErrorCodeToString(hr);
    std::move(callback_).Run(BluetoothDevice::ConnectErrorCode::ERROR_FAILED);
    return;
  }

  switch (pairing_kind) {
    case DevicePairingKinds_ProvidePin:
      pairing_requested_ = pairing_requested;
      expecting_pin_code_ = true;
      pairing_delegate_->RequestPinCode(device_);
      return;
    case DevicePairingKinds_ConfirmOnly:
      if (base::FeatureList::IsEnabled(
              features::kWebBluetoothConfirmPairingSupport)) {
        pairing_requested_ = pairing_requested;
        pairing_delegate_->AuthorizePairing(device_);
        return;
      } else {
        DVLOG(2) << "DevicePairingKind = " << static_cast<int>(pairing_kind)
                 << " is not enabled by "
                    "enable-web-bluetooth-confirm-pairing-support";
      }
      break;
    case DevicePairingKinds_ConfirmPinMatch:
      if (base::FeatureList::IsEnabled(
              features::kWebBluetoothConfirmPairingSupport)) {
        pairing_requested_ = pairing_requested;

        HSTRING hstring_pin;
        pairing_requested->get_Pin(&hstring_pin);

        uint32_t pin;
        if (HstringToUint32(hstring_pin, pin)) {
          pairing_delegate_->ConfirmPasskey(device_, pin);
          return;
        } else {
          DVLOG(2) << "DevicePairingKind = " << static_cast<int>(pairing_kind)
                   << " has invalid PIN to display, cancel pairing procedure.";
        }

      } else {
        DVLOG(2) << "DevicePairingKind = " << static_cast<int>(pairing_kind)
                 << " is not enabled by "
                    "enable-web-bluetooth-confirm-pairing-support";
      }
      break;
    default:
      DVLOG(2) << "Unsupported DevicePairingKind = "
               << static_cast<int>(pairing_kind);
      break;
  }
  std::move(callback_).Run(
      BluetoothDevice::ConnectErrorCode::ERROR_AUTH_FAILED);
}

void BluetoothPairingWinrt::OnPair(
    ComPtr<IDevicePairingResult> pairing_result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DevicePairingResultStatus status;
  HRESULT hr = pairing_result->get_Status(&status);
  if (FAILED(hr)) {
    DVLOG(2) << "Getting Pairing Result Status failed: "
             << logging::SystemErrorCodeToString(hr);
    std::move(callback_).Run(BluetoothDevice::ConnectErrorCode::ERROR_FAILED);
    return;
  }

  if (was_cancelled_ && status == DevicePairingResultStatus_RejectedByHandler) {
    // See comment in CancelPairing() for explanation of why was_cancelled_
    // is used.
    status = DevicePairingResultStatus_PairingCanceled;
  }

  DVLOG(2) << "Pairing Result Status: " << static_cast<int>(status);
  switch (status) {
    case DevicePairingResultStatus_AlreadyPaired:
    case DevicePairingResultStatus_Paired:
      std::move(callback_).Run(/*error_code=*/std::nullopt);
      return;
    case DevicePairingResultStatus_PairingCanceled:
      std::move(callback_).Run(
          BluetoothDevice::ConnectErrorCode::ERROR_AUTH_CANCELED);
      return;
    case DevicePairingResultStatus_AuthenticationFailure:
      std::move(callback_).Run(
          BluetoothDevice::ConnectErrorCode::ERROR_AUTH_FAILED);
      return;
    case DevicePairingResultStatus_ConnectionRejected:
    case DevicePairingResultStatus_RejectedByHandler:
      std::move(callback_).Run(
          BluetoothDevice::ConnectErrorCode::ERROR_AUTH_REJECTED);
      return;
    case DevicePairingResultStatus_AuthenticationTimeout:
      std::move(callback_).Run(
          BluetoothDevice::ConnectErrorCode::ERROR_AUTH_TIMEOUT);
      return;
    case DevicePairingResultStatus_Failed:
      std::move(callback_).Run(BluetoothDevice::ConnectErrorCode::ERROR_FAILED);
      return;
    case DevicePairingResultStatus_OperationAlreadyInProgress:
      std::move(callback_).Run(
          BluetoothDevice::ConnectErrorCode::ERROR_INPROGRESS);
      return;
    default:
      std::move(callback_).Run(BluetoothDevice::ConnectErrorCode::ERROR_FAILED);
      return;
  }
}

}  // namespace device
