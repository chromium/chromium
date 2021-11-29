// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/gamepad/wgi_data_fetcher_win.h"

#include <wrl/event.h>

#include <string>

#include "base/containers/cxx20_erase.h"
#include "base/sequence_checker.h"
#include "base/strings/string_util_win.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/win/core_winrt_util.h"
#include "base/win/hstring_reference.h"
#include "device/base/event_utils_winrt.h"

namespace device {

WgiDataFetcherWin::WgiDataFetcherWin() {
  DETACH_FROM_SEQUENCE(sequence_checker_);
  get_activation_factory_function_ = &base::win::RoGetActivationFactory;
}

WgiDataFetcherWin::~WgiDataFetcherWin() {
  UnregisterEventHandlers();
}

GamepadSource WgiDataFetcherWin::source() {
  return Factory::static_source();
}

void WgiDataFetcherWin::OnAddedToProvider() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!base::win::HStringReference::ResolveCoreWinRTStringDelayload()) {
    initialization_state_ =
        InitializationState::kCoreWinrtStringDelayLoadFailed;
    return;
  }

  HRESULT hr = get_activation_factory_function_(
      base::win::HStringReference(RuntimeClass_Windows_Gaming_Input_Gamepad)
          .Get(),
      IID_PPV_ARGS(&gamepad_statics_));
  if (FAILED(hr)) {
    initialization_state_ = InitializationState::kRoGetActivationFactoryFailed;
    return;
  }

  // Create a Windows::Foundation::IEventHandler that runs a
  // base::RepeatingCallback() on the gamepad polling thread when a gamepad
  // is added or removed. This callback stores the current sequence task runner
  // and weak pointer, so those two objects would remain active until the
  // callback returns.
  added_event_token_ = AddEventHandler(
      gamepad_statics_.Get(),
      &ABI::Windows::Gaming::Input::IGamepadStatics::add_GamepadAdded,
      base::BindRepeating(&WgiDataFetcherWin::OnGamepadAdded,
                          weak_factory_.GetWeakPtr()));
  if (!added_event_token_) {
    initialization_state_ = InitializationState::kAddGamepadAddedFailed;
    UnregisterEventHandlers();
    return;
  }

  removed_event_token_ = AddEventHandler(
      gamepad_statics_.Get(),
      &ABI::Windows::Gaming::Input::IGamepadStatics::add_GamepadRemoved,
      base::BindRepeating(&WgiDataFetcherWin::OnGamepadRemoved,
                          weak_factory_.GetWeakPtr()));
  if (!removed_event_token_) {
    initialization_state_ = InitializationState::kAddGamepadRemovedFailed;
    UnregisterEventHandlers();
    return;
  }

  initialization_state_ = InitializationState::kInitialized;
}

void WgiDataFetcherWin::SetGetActivationFunctionForTesting(
    GetActivationFactoryFunction value) {
  get_activation_factory_function_ = value;
}

void WgiDataFetcherWin::OnGamepadAdded(
    IInspectable* /* sender */,
    ABI::Windows::Gaming::Input::IGamepad* gamepad) {
  // While base::win::AddEventHandler stores the sequence_task_runner in the
  // callback function object, it post the task back to the same sequence - the
  // gamepad polling thread when the callback is returned on a different thread
  // from the IGamepadStatics COM API. Thus `OnGamepadAdded` is also running on
  // gamepad polling thread, it is the only thread that is able to access the
  // `gamepads_` object, making it thread-safe.
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (initialization_state_ != InitializationState::kInitialized)
    return;

  int source_id = next_source_id_++;
  PadState* state = GetPadState(source_id);
  if (!state)
    return;
  state->is_initialized = true;
  Gamepad& pad = state->data;
  pad.connected = true;
  pad.vibration_actuator.type = GamepadHapticActuatorType::kDualRumble;
  pad.vibration_actuator.not_null = false;
  pad.mapping = GamepadMapping::kStandard;
  gamepads_.push_back({source_id, gamepad});
}

void WgiDataFetcherWin::OnGamepadRemoved(
    IInspectable* /* sender */,
    ABI::Windows::Gaming::Input::IGamepad* gamepad) {
  // While ::device::AddEventHandler stores the sequence_task_runner in the
  // callback function object, it post the task back to the same sequence - the
  // gamepad polling thread when the callback is returned on a different thread
  // from the IGamepadStatics COM API. Thus `OnGamepadRemoved` is also running
  // on gamepad polling thread, it is the only thread that is able to access the
  // `gamepads_` object, making it thread-safe.
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(initialization_state_, InitializationState::kInitialized);

  base::EraseIf(gamepads_,
                [=](const WindowsGamingInputControllerMapping& mapping) {
                  return mapping.gamepad.Get() == gamepad;
                });
}

void WgiDataFetcherWin::GetGamepadData(bool devices_changed_hint) {
  // TODO(https://crbug.com/1105671): Implement the WgiDataFetcherWin
  // and add corresponding tests
}

const std::vector<WgiDataFetcherWin::WindowsGamingInputControllerMapping>&
WgiDataFetcherWin::GetGamepadsForTesting() const {
  return gamepads_;
}

WgiDataFetcherWin::InitializationState
WgiDataFetcherWin::GetInitializationState() const {
  return initialization_state_;
}

void WgiDataFetcherWin::UnregisterEventHandlers() {
  if (added_event_token_) {
    HRESULT hr =
        gamepad_statics_->remove_GamepadAdded(added_event_token_.value());
    if (FAILED(hr)) {
      DLOG(ERROR) << "Removing GamepadAdded Handler failed: "
                  << logging::SystemErrorCodeToString(hr);
    }
  }

  if (removed_event_token_) {
    HRESULT hr =
        gamepad_statics_->remove_GamepadRemoved(removed_event_token_.value());
    if (FAILED(hr)) {
      DLOG(ERROR) << "Removing GamepadRemoved Handler failed: "
                  << logging::SystemErrorCodeToString(hr);
    }
  }
}

WgiDataFetcherWin::WindowsGamingInputControllerMapping::
    WindowsGamingInputControllerMapping(
        int input_source_id,
        Microsoft::WRL::ComPtr<ABI::Windows::Gaming::Input::IGamepad>
            input_gamepad)
    : source_id(input_source_id), gamepad(input_gamepad) {}

WgiDataFetcherWin::WindowsGamingInputControllerMapping::
    WindowsGamingInputControllerMapping(
        const WgiDataFetcherWin::WindowsGamingInputControllerMapping& other) =
        default;

WgiDataFetcherWin::WindowsGamingInputControllerMapping&
WgiDataFetcherWin::WindowsGamingInputControllerMapping::
    WindowsGamingInputControllerMapping::operator=(
        const WgiDataFetcherWin::WindowsGamingInputControllerMapping& other) =
        default;

WgiDataFetcherWin::WindowsGamingInputControllerMapping::
    WindowsGamingInputControllerMapping(
        WgiDataFetcherWin::WindowsGamingInputControllerMapping&& other) =
        default;

WgiDataFetcherWin::WindowsGamingInputControllerMapping&
WgiDataFetcherWin::WindowsGamingInputControllerMapping::operator=(
    WgiDataFetcherWin::WindowsGamingInputControllerMapping&&) = default;

WgiDataFetcherWin::WindowsGamingInputControllerMapping::
    ~WindowsGamingInputControllerMapping() = default;

}  // namespace device
