// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/test/fake_device_information_custom_pairing_winrt.h"

#include <windows.foundation.h>

#include <utility>

#include "base/bind.h"
#include "base/test/bind_test_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/win/async_operation.h"
#include "device/bluetooth/test/fake_device_pairing_requested_event_args_winrt.h"
#include "device/bluetooth/test/fake_device_pairing_result_winrt.h"

namespace device {

namespace {

using ABI::Windows::Devices::Enumeration::DeviceInformationCustomPairing;
using ABI::Windows::Devices::Enumeration::DevicePairingKinds;
using ABI::Windows::Devices::Enumeration::DevicePairingProtectionLevel;
using ABI::Windows::Devices::Enumeration::DevicePairingRequestedEventArgs;
using ABI::Windows::Devices::Enumeration::DevicePairingResult;
using ABI::Windows::Devices::Enumeration::DevicePairingResultStatus_Failed;
using ABI::Windows::Devices::Enumeration::DevicePairingResultStatus_Paired;
using ABI::Windows::Devices::Enumeration::IDevicePairingResult;
using ABI::Windows::Devices::Enumeration::IDevicePairingSettings;
using ABI::Windows::Foundation::IAsyncOperation;
using ABI::Windows::Foundation::ITypedEventHandler;
using Microsoft::WRL::ComPtr;
using Microsoft::WRL::Make;

}  // namespace

FakeDeviceInformationCustomPairingWinrt::
    FakeDeviceInformationCustomPairingWinrt(
        Microsoft::WRL::ComPtr<FakeDeviceInformationPairingWinrt> pairing,
        std::string pin)
    : pairing_(std::move(pairing)), pin_(std::move(pin)) {}

FakeDeviceInformationCustomPairingWinrt::
    ~FakeDeviceInformationCustomPairingWinrt() = default;

HRESULT FakeDeviceInformationCustomPairingWinrt::PairAsync(
    DevicePairingKinds pairing_kinds_supported,
    IAsyncOperation<DevicePairingResult*>** result) {
  if (!pairing_requested_handler_)
    return E_FAIL;

  auto async_op = Make<base::win::AsyncOperation<DevicePairingResult*>>();
  pair_callback_ = async_op->callback();
  *result = async_op.Detach();

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([this] {
        pairing_requested_handler_->Invoke(
            this, Make<FakeDevicePairingRequestedEventArgsWinrt>(this).Get());
      }));
  return S_OK;
}

HRESULT FakeDeviceInformationCustomPairingWinrt::PairWithProtectionLevelAsync(
    DevicePairingKinds pairing_kinds_supported,
    DevicePairingProtectionLevel min_protection_level,
    IAsyncOperation<DevicePairingResult*>** result) {
  return E_NOTIMPL;
}

HRESULT FakeDeviceInformationCustomPairingWinrt::
    PairWithProtectionLevelAndSettingsAsync(
        DevicePairingKinds pairing_kinds_supported,
        DevicePairingProtectionLevel min_protection_level,
        IDevicePairingSettings* device_pairing_settings,
        IAsyncOperation<DevicePairingResult*>** result) {
  return E_NOTIMPL;
}

HRESULT FakeDeviceInformationCustomPairingWinrt::add_PairingRequested(
    ITypedEventHandler<DeviceInformationCustomPairing*,
                       DevicePairingRequestedEventArgs*>* handler,
    EventRegistrationToken* token) {
  pairing_requested_handler_ = handler;
  return S_OK;
}

HRESULT FakeDeviceInformationCustomPairingWinrt::remove_PairingRequested(
    EventRegistrationToken token) {
  pairing_requested_handler_.Reset();
  return S_OK;
}

void FakeDeviceInformationCustomPairingWinrt::AcceptWithPin(std::string pin) {
  accepted_pin_ = std::move(pin);
}

void FakeDeviceInformationCustomPairingWinrt::Complete() {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](base::OnceCallback<void(ComPtr<IDevicePairingResult>)>
                 pair_callback,
             ComPtr<FakeDeviceInformationPairingWinrt> pairing,
             bool is_paired) {
            std::move(pair_callback)
                .Run(Make<FakeDevicePairingResultWinrt>(
                    is_paired ? DevicePairingResultStatus_Paired
                              : DevicePairingResultStatus_Failed));
            pairing->set_paired(is_paired);
          },
          std::move(pair_callback_), pairing_, pin_ == accepted_pin_));
}

}  // namespace device
