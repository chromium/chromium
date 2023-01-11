// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/test/fake_radio_winrt.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/win/async_operation.h"

namespace device {

namespace {

using ABI::Windows::Devices::Radios::Radio;
using ABI::Windows::Devices::Radios::RadioAccessStatus;
using ABI::Windows::Devices::Radios::RadioAccessStatus_Allowed;
using ABI::Windows::Devices::Radios::RadioAccessStatus_DeniedBySystem;
using ABI::Windows::Devices::Radios::RadioKind;
using ABI::Windows::Devices::Radios::RadioState;
using ABI::Windows::Devices::Radios::RadioState_Off;
using ABI::Windows::Devices::Radios::RadioState_On;
using ABI::Windows::Foundation::Collections::IVectorView;
using ABI::Windows::Foundation::IAsyncOperation;
using ABI::Windows::Foundation::ITypedEventHandler;
using Microsoft::WRL::Make;

}  // namespace

FakeRadioWinrt::FakeRadioWinrt() = default;

FakeRadioWinrt::~FakeRadioWinrt() = default;

HRESULT FakeRadioWinrt::SetStateAsync(
    RadioState value,
    IAsyncOperation<RadioAccessStatus>** operation) {
  auto async_op = Make<base::win::AsyncOperation<RadioAccessStatus>>();
  set_state_callback_ = async_op->callback();

  // Schedule a callback that will run |set_state_callback_| with Status_Allowed
  // and invokes |state_changed_handler_| if |value| is different from |state_|.
  // Capturing |this| as safe here, as the callback won't be run if
  // |cancelable_closure_| gets destroyed first.
  cancelable_closure_.Reset(base::BindLambdaForTesting([this, value] {
    std::move(set_state_callback_).Run(RadioAccessStatus_Allowed);
    if (std::exchange(state_, value) != value)
      state_changed_handler_->Invoke(this, nullptr);
  }));

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, cancelable_closure_.callback());
  *operation = async_op.Detach();
  return S_OK;
}

HRESULT FakeRadioWinrt::add_StateChanged(
    ITypedEventHandler<Radio*, IInspectable*>* handler,
    EventRegistrationToken* event_cookie) {
  state_changed_handler_ = handler;
  return S_OK;
}

HRESULT FakeRadioWinrt::remove_StateChanged(
    EventRegistrationToken event_cookie) {
  state_changed_handler_.Reset();
  return S_OK;
}

HRESULT FakeRadioWinrt::get_State(RadioState* value) {
  *value = state_;
  return S_OK;
}

HRESULT FakeRadioWinrt::get_Name(HSTRING* value) {
  return E_NOTIMPL;
}

HRESULT FakeRadioWinrt::get_Kind(RadioKind* value) {
  return E_NOTIMPL;
}

void FakeRadioWinrt::SimulateAdapterPowerFailure() {
  DCHECK(set_state_callback_);
  // Cancel the task scheduled in SetStateAsync() and run the stored callback
  // with an error code.
  cancelable_closure_.Reset(base::BindOnce(std::move(set_state_callback_),
                                           RadioAccessStatus_DeniedBySystem));
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, cancelable_closure_.callback());
}

void FakeRadioWinrt::SimulateAdapterPoweredOn() {
  state_ = RadioState_On;
  DCHECK(state_changed_handler_);
  state_changed_handler_->Invoke(this, nullptr);
}

void FakeRadioWinrt::SimulateAdapterPoweredOff() {
  state_ = RadioState_Off;
  DCHECK(state_changed_handler_);
  state_changed_handler_->Invoke(this, nullptr);
}

void FakeRadioWinrt::SimulateSpuriousStateChangedEvent() {
  state_changed_handler_->Invoke(this, nullptr);
}

FakeRadioStaticsWinrt::FakeRadioStaticsWinrt() = default;

FakeRadioStaticsWinrt::~FakeRadioStaticsWinrt() = default;

HRESULT FakeRadioStaticsWinrt::GetRadiosAsync(
    IAsyncOperation<IVectorView<Radio*>*>** value) {
  return E_NOTIMPL;
}

HRESULT FakeRadioStaticsWinrt::GetDeviceSelector(HSTRING* device_selector) {
  return E_NOTIMPL;
}

HRESULT FakeRadioStaticsWinrt::FromIdAsync(HSTRING device_id,
                                           IAsyncOperation<Radio*>** value) {
  return E_NOTIMPL;
}

void FakeRadioStaticsWinrt::SimulateRequestAccessAsyncError(
    ABI::Windows::Devices::Radios::RadioAccessStatus status) {
  access_status_ = status;
}

HRESULT FakeRadioStaticsWinrt::RequestAccessAsync(
    IAsyncOperation<RadioAccessStatus>** operation) {
  auto async_op = Make<base::win::AsyncOperation<RadioAccessStatus>>();
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(async_op->callback(), access_status_));
  *operation = async_op.Detach();
  return S_OK;
}

}  // namespace device
