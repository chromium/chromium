// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/gamepad/test_support/fake_igamepad.h"

#include "base/containers/flat_map.h"
#include "base/notreached.h"
#include "device/gamepad/test_support/fake_winrt_wgi_environment.h"

using ABI::Windows::Foundation::ITypedEventHandler;
using ABI::Windows::Gaming::Input::GameControllerButtonLabel;
using ABI::Windows::Gaming::Input::GamepadButtons;
using ABI::Windows::Gaming::Input::Headset;
using ABI::Windows::Gaming::Input::IGameController;
using ABI::Windows::Gaming::Input::IHeadset;
using ABI::Windows::System::IUser;
using ABI::Windows::System::UserChangedEventArgs;

namespace device {

FakeIGamepad::FakeIGamepad() = default;
FakeIGamepad::~FakeIGamepad() = default;

HRESULT WINAPI FakeIGamepad::get_Vibration(
    ABI::Windows::Gaming::Input::GamepadVibration* gamepad_vibration) {
  gamepad_vibration->LeftMotor = fake_gamepad_vibration_.LeftMotor;
  gamepad_vibration->LeftTrigger = fake_gamepad_vibration_.LeftTrigger;
  gamepad_vibration->RightMotor = fake_gamepad_vibration_.RightMotor;
  gamepad_vibration->RightTrigger = fake_gamepad_vibration_.RightTrigger;
  return S_OK;
}

HRESULT WINAPI FakeIGamepad::put_Vibration(
    ABI::Windows::Gaming::Input::GamepadVibration gamepad_vibration) {
  fake_gamepad_vibration_ = gamepad_vibration;
  return S_OK;
}

HRESULT WINAPI FakeIGamepad::GetCurrentReading(
    ABI::Windows::Gaming::Input::GamepadReading* gamepad_reading) {
  if (FakeWinrtWgiEnvironment::GetError() ==
      WgiTestErrorCode::kErrorWgiGamepadGetCurrentReadingFailed) {
    return E_FAIL;
  }
  gamepad_reading->Buttons = fake_gamepad_reading_.Buttons;
  gamepad_reading->LeftThumbstickX = fake_gamepad_reading_.LeftThumbstickX;
  gamepad_reading->LeftThumbstickY = fake_gamepad_reading_.LeftThumbstickY;
  gamepad_reading->RightThumbstickX = fake_gamepad_reading_.RightThumbstickX;
  gamepad_reading->RightThumbstickY = fake_gamepad_reading_.RightThumbstickY;
  gamepad_reading->LeftTrigger = fake_gamepad_reading_.LeftTrigger;
  gamepad_reading->RightTrigger = fake_gamepad_reading_.RightTrigger;
  return S_OK;
}

HRESULT WINAPI FakeIGamepad::GetButtonLabel(
    ABI::Windows::Gaming::Input::GamepadButtons button,
    ABI::Windows::Gaming::Input::GameControllerButtonLabel* value) {
  if (FakeWinrtWgiEnvironment::GetError() ==
      WgiTestErrorCode::kErrorWgiGamepadGetButtonLabelFailed) {
    return E_FAIL;
  }
  // We only need to simulate this functionality for paddle buttons.
  static const base::flat_map<GamepadButtons, GameControllerButtonLabel>
      button_label_mapping = {
          {GamepadButtons::GamepadButtons_Paddle1,
           GameControllerButtonLabel::GameControllerButtonLabel_Paddle1},
          {GamepadButtons::GamepadButtons_Paddle2,
           GameControllerButtonLabel::GameControllerButtonLabel_Paddle2},
          {GamepadButtons::GamepadButtons_Paddle3,
           GameControllerButtonLabel::GameControllerButtonLabel_Paddle3},
          {GamepadButtons::GamepadButtons_Paddle4,
           GameControllerButtonLabel::GameControllerButtonLabel_Paddle4}};
  auto it = button_label_mapping.find(button);
  if (!has_paddles_ || it == button_label_mapping.end()) {
    *value = ABI::Windows::Gaming::Input::GameControllerButtonLabel::
        GameControllerButtonLabel_None;
  } else {
    *value = it->second;
  }
  return S_OK;
}

HRESULT WINAPI FakeIGamepad::add_HeadsetConnected(
    ITypedEventHandler<IGameController*, Headset*>* value,
    EventRegistrationToken* token) {
  NOTIMPLEMENTED();
  return S_OK;
}

HRESULT WINAPI
FakeIGamepad::remove_HeadsetConnected(EventRegistrationToken token) {
  NOTIMPLEMENTED();
  return S_OK;
}

HRESULT WINAPI FakeIGamepad::add_HeadsetDisconnected(
    ITypedEventHandler<IGameController*, Headset*>* value,
    EventRegistrationToken* token) {
  NOTIMPLEMENTED();
  return S_OK;
}

HRESULT WINAPI
FakeIGamepad::remove_HeadsetDisconnected(EventRegistrationToken token) {
  NOTIMPLEMENTED();
  return S_OK;
}

HRESULT WINAPI FakeIGamepad::add_UserChanged(
    ITypedEventHandler<IGameController*, UserChangedEventArgs*>* value,
    EventRegistrationToken* token) {
  NOTIMPLEMENTED();
  return S_OK;
}

HRESULT WINAPI FakeIGamepad::remove_UserChanged(EventRegistrationToken token) {
  NOTIMPLEMENTED();
  return S_OK;
}

HRESULT WINAPI FakeIGamepad::get_Headset(IHeadset** value) {
  NOTIMPLEMENTED();
  return S_OK;
}

HRESULT WINAPI FakeIGamepad::get_IsWireless(boolean* value) {
  NOTIMPLEMENTED();
  return S_OK;
}

HRESULT WINAPI FakeIGamepad::get_User(IUser** value) {
  NOTIMPLEMENTED();
  return S_OK;
}

void FakeIGamepad::SetCurrentReading(
    const ABI::Windows::Gaming::Input::GamepadReading& gamepad_reading) {
  fake_gamepad_reading_.Buttons = gamepad_reading.Buttons;
  fake_gamepad_reading_.LeftThumbstickX = gamepad_reading.LeftThumbstickX;
  fake_gamepad_reading_.LeftThumbstickY = gamepad_reading.LeftThumbstickY;
  fake_gamepad_reading_.RightThumbstickX = gamepad_reading.RightThumbstickX;
  fake_gamepad_reading_.RightThumbstickY = gamepad_reading.RightThumbstickY;
  fake_gamepad_reading_.LeftTrigger = gamepad_reading.LeftTrigger;
  fake_gamepad_reading_.RightTrigger = gamepad_reading.RightTrigger;
}

}  // namespace device
