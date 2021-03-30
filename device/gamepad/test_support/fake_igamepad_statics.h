// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_GAMEPAD_TEST_SUPPORT_FAKE_IGAMEPAD_STATICS_H_
#define DEVICE_GAMEPAD_TEST_SUPPORT_FAKE_IGAMEPAD_STATICS_H_

#include <Windows.Gaming.Input.h>
#include <wrl.h>

#include <map>

#include "base/containers/flat_map.h"

namespace device {

class FakeIGamepadStatics final
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<
              Microsoft::WRL::WinRt | Microsoft::WRL::InhibitRoOriginateError>,
          ABI::Windows::Gaming::Input::IGamepadStatics> {
 public:
  FakeIGamepadStatics();

  FakeIGamepadStatics(const FakeIGamepadStatics&) = delete;
  FakeIGamepadStatics& operator=(const FakeIGamepadStatics&) = delete;

  static FakeIGamepadStatics* GetInstance();

  // ABI::Windows::Gaming::Input::IGamepadStatics fake implementation.
  HRESULT WINAPI
  add_GamepadAdded(ABI::Windows::Foundation::IEventHandler<
                       ABI::Windows::Gaming::Input::Gamepad*>* value,
                   EventRegistrationToken* token) final;
  HRESULT WINAPI
  add_GamepadRemoved(ABI::Windows::Foundation::IEventHandler<
                         ABI::Windows::Gaming::Input::Gamepad*>* value,
                     EventRegistrationToken* token) final;
  HRESULT WINAPI remove_GamepadAdded(EventRegistrationToken token) final;
  HRESULT WINAPI remove_GamepadRemoved(EventRegistrationToken token) final;
  HRESULT WINAPI
  get_Gamepads(ABI::Windows::Foundation::Collections::IVectorView<
               ABI::Windows::Gaming::Input::Gamepad*>** value) final;

  // Adds a fake gamepad to simulate a gamepad connection operation for test.
  void SimulateGamepadAdded(
      const Microsoft::WRL::ComPtr<ABI::Windows::Gaming::Input::IGamepad>&
          gamepad_to_add);

  // Uses a fake gamepad to simulate a gamepad disconnection operation for test.
  void SimulateGamepadRemoved(
      const Microsoft::WRL::ComPtr<ABI::Windows::Gaming::Input::IGamepad>&
          gamepad_to_remove);

  // Sets |add_gamepad_added_status_| and |add_gamepad_removed_status_| to test
  // Windows.Gaming.Input error handling logics in tests.
  void SetAddGamepadAddedStatus(HRESULT value);
  void SetAddGamepadRemovedStatus(HRESULT value);

  size_t GetGamepadAddedEventHandlerCount() const;
  size_t GetGamepadRemovedEventHandlerCount() const;

 private:
  typedef base::flat_map<
      int64_t /* event_registration_token */,
      Microsoft::WRL::ComPtr<ABI::Windows::Foundation::IEventHandler<
          ABI::Windows::Gaming::Input::Gamepad*>>>
      EventHandlerMap;

  ~FakeIGamepadStatics() final;

  int64_t next_event_registration_token_ = 0;

  // Status variable to direct tests to test Windows.Gaming.Input error
  // handling. If set to E_FAIL, add_GamepadAdded and add_GamepadRemoved would
  // return E_FAIL error code directly.
  HRESULT add_gamepad_added_status_ = S_OK;
  HRESULT add_gamepad_removed_status_ = S_OK;

  EventHandlerMap gamepad_added_event_handler_map_;
  EventHandlerMap gamepad_removed_event_handler_map_;
};

}  // namespace device

#endif  // DEVICE_GAMEPAD_TEST_SUPPORT_FAKE_IGAMEPAD_STATICS_H_
