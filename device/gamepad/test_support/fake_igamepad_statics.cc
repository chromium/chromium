// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/gamepad/test_support/fake_igamepad_statics.h"

#include "base/notreached.h"

namespace device {

FakeIGamepadStatics::FakeIGamepadStatics() = default;

FakeIGamepadStatics::~FakeIGamepadStatics() = default;

// static
FakeIGamepadStatics* FakeIGamepadStatics::GetInstance() {
  static FakeIGamepadStatics* instance;
  if (!instance)
    instance = Microsoft::WRL::Make<FakeIGamepadStatics>().Detach();
  return instance;
}

HRESULT WINAPI FakeIGamepadStatics::add_GamepadAdded(
    ABI::Windows::Foundation::IEventHandler<
        ABI::Windows::Gaming::Input::Gamepad*>* event_handler,
    EventRegistrationToken* token) {
  if (add_gamepad_added_status_ != S_OK)
    return add_gamepad_added_status_;

  token->value = next_event_registration_token_++;

  auto ret = gamepad_added_event_handler_map_.insert(
      {token->value,
       Microsoft::WRL::ComPtr<ABI::Windows::Foundation::IEventHandler<
           ABI::Windows::Gaming::Input::Gamepad*>>{event_handler}});
  if (ret.second)
    return S_OK;
  return E_FAIL;
}

HRESULT WINAPI FakeIGamepadStatics::add_GamepadRemoved(
    ABI::Windows::Foundation::IEventHandler<
        ABI::Windows::Gaming::Input::Gamepad*>* event_handler,
    EventRegistrationToken* token) {
  if (add_gamepad_removed_status_ != S_OK)
    return add_gamepad_removed_status_;

  token->value = next_event_registration_token_++;

  auto ret = gamepad_removed_event_handler_map_.insert(
      {token->value,
       Microsoft::WRL::ComPtr<ABI::Windows::Foundation::IEventHandler<
           ABI::Windows::Gaming::Input::Gamepad*>>{event_handler}});
  if (ret.second)
    return S_OK;
  return E_FAIL;
}

HRESULT WINAPI
FakeIGamepadStatics::remove_GamepadAdded(EventRegistrationToken token) {
  NOTIMPLEMENTED();
  return E_NOTIMPL;
}

HRESULT WINAPI
FakeIGamepadStatics::remove_GamepadRemoved(EventRegistrationToken token) {
  NOTIMPLEMENTED();
  return E_NOTIMPL;
}

HRESULT WINAPI FakeIGamepadStatics::get_Gamepads(
    ABI::Windows::Foundation::Collections::IVectorView<
        ABI::Windows::Gaming::Input::Gamepad*>** value) {
  NOTIMPLEMENTED();
  return E_NOTIMPL;
}

void FakeIGamepadStatics::SimulateGamepadAdded(
    const Microsoft::WRL::ComPtr<ABI::Windows::Gaming::Input::IGamepad>&
        gamepad_to_add) {
  // Iterate through |gamepad_added_event_handler_map_| invoking each
  // callback with |gamepad_to_add|.
  for (const auto& entry : gamepad_added_event_handler_map_)
    entry.second->Invoke(this, gamepad_to_add.Get());
}

void FakeIGamepadStatics::SimulateGamepadRemoved(
    const Microsoft::WRL::ComPtr<ABI::Windows::Gaming::Input::IGamepad>&
        gamepad_to_remove) {
  // Iterate through |gamepad_removed_event_handler_map_| invoking each
  // callback with |gamepad_to_remove|.
  for (const auto& entry : gamepad_removed_event_handler_map_)
    entry.second->Invoke(this, gamepad_to_remove.Get());
}

void FakeIGamepadStatics::SetAddGamepadAddedStatus(HRESULT value) {
  add_gamepad_added_status_ = value;
}

void FakeIGamepadStatics::SetAddGamepadRemovedStatus(HRESULT value) {
  add_gamepad_removed_status_ = value;
}

size_t FakeIGamepadStatics::GetGamepadAddedEventHandlerCount() const {
  return gamepad_added_event_handler_map_.size();
}

size_t FakeIGamepadStatics::GetGamepadRemovedEventHandlerCount() const {
  return gamepad_removed_event_handler_map_.size();
}

}  // namespace device
