// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/gamepad/test_support/fake_igameinput.h"

#include "device/gamepad/test_support/fake_gameinput_environment.h"

namespace device {
namespace {
constexpr int kDeviceCallbackToken = 1;
constexpr int kSystemButtonCallbackToken = 2;
}  // namespace

FakeIGameInput::FakeIGameInput() = default;

FakeIGameInput::~FakeIGameInput() = default;

uint64_t FakeIGameInput::GetCurrentTimestamp() {
  return 0;
}

HRESULT FakeIGameInput::GetCurrentReading(GameInputKind inputKind,
                                          IGameInputDevice* device,
                                          IGameInputReading** reading) {
  if (mock_reading_ == nullptr) {
    return E_FAIL;
  }

  mock_reading_.CopyTo(reading);
  return S_OK;
}

HRESULT FakeIGameInput::GetNextReading(IGameInputReading*,
                                       GameInputKind,
                                       IGameInputDevice*,
                                       IGameInputReading**) {
  return E_NOTIMPL;
}

HRESULT FakeIGameInput::GetPreviousReading(IGameInputReading*,
                                           GameInputKind,
                                           IGameInputDevice*,
                                           IGameInputReading**) {
  return E_NOTIMPL;
}

HRESULT FakeIGameInput::GetTemporalReading(uint64_t,
                                           IGameInputDevice*,
                                           IGameInputReading**) {
  return E_NOTIMPL;
}

HRESULT FakeIGameInput::RegisterReadingCallback(IGameInputDevice*,
                                                GameInputKind,
                                                float,
                                                void*,
                                                GameInputReadingCallback,
                                                GameInputCallbackToken*) {
  return E_NOTIMPL;
}

HRESULT FakeIGameInput::RegisterDeviceCallback(
    IGameInputDevice* device,
    GameInputKind inputKind,
    GameInputDeviceStatus statusFilter,
    GameInputEnumerationKind enumerationKind,
    void* context,
    GameInputDeviceCallback callbackFunc,
    GameInputCallbackToken* callbackToken) {
  if (FakeGameInputEnvironment::GetError() ==
      GameInputTestErrorCode::kDeviceCallbackRegistrationFailed) {
    return E_UNEXPECTED;
  }

  if (IsCallbackSet()) {
    // ERROR: `FakeIGameInput` only supports one registered device callback.
    return E_FAIL;
  }

  device_callback_ = callbackFunc;
  device_callback_context_ = context;
  *callbackToken = kDeviceCallbackToken;
  return S_OK;
}

HRESULT FakeIGameInput::RegisterSystemButtonCallback(
    IGameInputDevice* device,
    GameInputSystemButtons buttonFilter,
    void* context,
    GameInputSystemButtonCallback callbackFunc,
    GameInputCallbackToken* callbackToken) {
  if (FakeGameInputEnvironment::GetError() ==
      GameInputTestErrorCode::kGuideButtonCallbackRegistrationFailed) {
    return E_UNEXPECTED;
  }

  if (IsSystemButtonCallbackSet()) {
    // ERROR: `FakeIGameInput` only supports one registered system button
    // callback.
    return E_FAIL;
  }

  system_button_callback_ = callbackFunc;
  system_button_callback_context_ = context;
  *callbackToken = kSystemButtonCallbackToken;
  return S_OK;
}

HRESULT FakeIGameInput::RegisterKeyboardLayoutCallback(
    IGameInputDevice*,
    void*,
    GameInputKeyboardLayoutCallback,
    GameInputCallbackToken*) {
  return E_NOTIMPL;
}

void FakeIGameInput::StopCallback(GameInputCallbackToken) {}

bool FakeIGameInput::UnregisterCallback(
    GameInputCallbackToken callback_token,
    uint64_t /*running_callback_wait_time*/) {
  if (callback_token == kDeviceCallbackToken) {
    if (!IsCallbackSet()) {
      // ERROR: No device callback registered.
      return false;
    }
    device_callback_ = nullptr;
    device_callback_context_ = nullptr;
    return true;
  }

  if (callback_token == kSystemButtonCallbackToken) {
    if (!IsSystemButtonCallbackSet()) {
      // ERROR: No system button callback registered.
      return false;
    }
    system_button_callback_ = nullptr;
    system_button_callback_context_ = nullptr;
    return true;
  }

  // ERROR: Invalid callback token.
  return false;
}

HRESULT FakeIGameInput::CreateDispatcher(IGameInputDispatcher**) {
  return E_NOTIMPL;
}

HRESULT FakeIGameInput::CreateAggregateDevice(GameInputKind,
                                              IGameInputDevice**) {
  return E_NOTIMPL;
}

HRESULT FakeIGameInput::FindDeviceFromId(APP_LOCAL_DEVICE_ID const*,
                                         IGameInputDevice**) {
  return E_NOTIMPL;
}

HRESULT FakeIGameInput::FindDeviceFromObject(IUnknown*, IGameInputDevice**) {
  return E_NOTIMPL;
}

HRESULT FakeIGameInput::FindDeviceFromPlatformHandle(HANDLE,
                                                     IGameInputDevice**) {
  return E_NOTIMPL;
}

HRESULT FakeIGameInput::FindDeviceFromPlatformString(LPCWSTR,
                                                     IGameInputDevice**) {
  return E_NOTIMPL;
}

HRESULT FakeIGameInput::EnableOemDeviceSupport(uint16_t,
                                               uint16_t,
                                               uint8_t,
                                               uint8_t) {
  return E_NOTIMPL;
}

void FakeIGameInput::SetFocusPolicy(GameInputFocusPolicy) {}

bool FakeIGameInput::IsCallbackSet() {
  return device_callback_ != nullptr;
}

void FakeIGameInput::InvokeDeviceCallback(IGameInputDevice* device,
                                          GameInputDeviceStatus status) {
  if (IsCallbackSet()) {
    device_callback_(1, device_callback_context_, device, 0, status,
                     GameInputDeviceNoStatus);
  }
}

void FakeIGameInput::AssignMockReading(IGameInputReading* reading) {
  mock_reading_ = reading;
}

bool FakeIGameInput::IsSystemButtonCallbackSet() {
  return system_button_callback_ != nullptr;
}

void FakeIGameInput::InvokeSystemButtonCallback(IGameInputDevice* device,
                                                bool is_pressed) {
  if (IsSystemButtonCallbackSet()) {
    system_button_callback_(
        kSystemButtonCallbackToken, system_button_callback_context_, device, 0,
        is_pressed ? GameInputSystemButtons::GameInputSystemButtonGuide
                   : GameInputSystemButtons::GameInputSystemButtonNone,
        is_system_button_pressed_
            ? GameInputSystemButtons::GameInputSystemButtonGuide
            : GameInputSystemButtons::GameInputSystemButtonNone);
    is_system_button_pressed_ = is_pressed;
  }
}

}  // namespace device
