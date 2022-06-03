// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/gamepad/wgi_data_fetcher_win.h"

#include <wrl/event.h>

#include "base/win/core_winrt_util.h"
#include "base/win/hstring_reference.h"
#include "base/win/scoped_hstring.h"

namespace device {

using GamepadPlugAndPlayHandler = ABI::Windows::Foundation::IEventHandler<
    ABI::Windows::Gaming::Input::Gamepad*>;

WgiDataFetcherWin::WgiDataFetcherWin() {
  get_activation_factory_function_ = &base::win::RoGetActivationFactory;
}

WgiDataFetcherWin::~WgiDataFetcherWin() = default;

GamepadSource WgiDataFetcherWin::source() {
  return Factory::static_source();
}

void WgiDataFetcherWin::OnAddedToProvider() {
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

  EventRegistrationToken added_event_token;
  hr = gamepad_statics_->add_GamepadAdded(
      Microsoft::WRL::Callback<GamepadPlugAndPlayHandler>(
          this, &WgiDataFetcherWin::OnGamepadAdded)
          .Get(),
      &added_event_token);
  if (FAILED(hr)) {
    initialization_state_ = InitializationState::kAddGamepadAddedFailed;
    return;
  }

  EventRegistrationToken removed_event_token;
  hr = gamepad_statics_->add_GamepadRemoved(
      Microsoft::WRL::Callback<GamepadPlugAndPlayHandler>(
          this, &WgiDataFetcherWin::OnGamepadRemoved)
          .Get(),
      &removed_event_token);
  if (FAILED(hr)) {
    initialization_state_ = InitializationState::kAddGamepadRemovedFailed;
    return;
  }

  initialization_state_ = InitializationState::kInitialized;
}

void WgiDataFetcherWin::SetGetActivationFunctionForTesting(
    GetActivationFactoryFunction value) {
  get_activation_factory_function_ = value;
}

HRESULT WgiDataFetcherWin::OnGamepadAdded(
    IInspectable* /* sender */,
    ABI::Windows::Gaming::Input::IGamepad* gamepad) {
  if (initialization_state_ != InitializationState::kInitialized)
    return S_OK;

  int source_id = next_source_id_++;
  PadState* state = GetPadState(source_id);
  if (!state)
    return S_OK;
  state->is_initialized = true;
  Gamepad& pad = state->data;
  pad.connected = true;
  pad.vibration_actuator.type = GamepadHapticActuatorType::kDualRumble;
  pad.vibration_actuator.not_null = false;
  pad.mapping = GamepadMapping::kStandard;
  gamepads_.push_back({source_id, gamepad});
  return S_OK;
}

HRESULT WgiDataFetcherWin::OnGamepadRemoved(
    IInspectable* /* sender */,
    ABI::Windows::Gaming::Input::IGamepad* gamepad) {
  if (initialization_state_ != InitializationState::kInitialized)
    return S_OK;

  auto gamepad_it =
      std::find_if(gamepads_.begin(), gamepads_.end(),
                   [=](const WindowsGamingInputControllerMapping& mapping) {
                     return mapping.gamepad.Get() == gamepad;
                   });

  if (gamepad_it != gamepads_.end()) {
    PadState* state = GetPadState(gamepad_it->source_id);
    if (!state)
      return S_OK;
    state->source = GAMEPAD_SOURCE_NONE;
    gamepads_.erase(gamepad_it);
  }
  return S_OK;
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

WgiDataFetcherWin::WindowsGamingInputControllerMapping::
    WindowsGamingInputControllerMapping(
        int input_source_id,
        Microsoft::WRL::ComPtr<ABI::Windows::Gaming::Input::IGamepad>
            input_gamepad)
    : source_id(input_source_id), gamepad(input_gamepad) {}

WgiDataFetcherWin::WindowsGamingInputControllerMapping::
    ~WindowsGamingInputControllerMapping() = default;

WgiDataFetcherWin::WindowsGamingInputControllerMapping::
    WindowsGamingInputControllerMapping(
        const WindowsGamingInputControllerMapping& other) = default;

}  // namespace device
