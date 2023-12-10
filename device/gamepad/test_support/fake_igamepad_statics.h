// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_GAMEPAD_TEST_SUPPORT_FAKE_IGAMEPAD_STATICS_H_
#define DEVICE_GAMEPAD_TEST_SUPPORT_FAKE_IGAMEPAD_STATICS_H_

#include <Windows.Gaming.Input.h>
#include <inspectable.h>
#include <windows.foundation.collections.h>
#include <wrl.h>

#include <string_view>
#include <unordered_map>

#include "base/containers/flat_map.h"
#include "base/functional/callback_forward.h"
#include "device/gamepad/test_support/fake_igamepad.h"
#include "device/gamepad/test_support/fake_iraw_game_controller.h"

namespace device {

class FakeIGamepadStatics final
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<
              Microsoft::WRL::WinRt | Microsoft::WRL::InhibitRoOriginateError>,
          ABI::Windows::Gaming::Input::IGamepadStatics,
          ABI::Windows::Gaming::Input::IRawGameControllerStatics> {
 public:
  FakeIGamepadStatics();

  FakeIGamepadStatics(const FakeIGamepadStatics&) = delete;
  FakeIGamepadStatics& operator=(const FakeIGamepadStatics&) = delete;

  static FakeIGamepadStatics* GetInstance();

  // ABI::Windows::Gaming::Input::IGamepadStatics fake implementation.
  HRESULT WINAPI
  add_GamepadAdded(ABI::Windows::Foundation::IEventHandler<
                       ABI::Windows::Gaming::Input::Gamepad*>* value,
                   EventRegistrationToken* token) override;
  HRESULT WINAPI
  add_GamepadRemoved(ABI::Windows::Foundation::IEventHandler<
                         ABI::Windows::Gaming::Input::Gamepad*>* value,
                     EventRegistrationToken* token) override;
  HRESULT WINAPI remove_GamepadAdded(EventRegistrationToken token) override;
  HRESULT WINAPI remove_GamepadRemoved(EventRegistrationToken token) override;
  HRESULT WINAPI
  get_Gamepads(ABI::Windows::Foundation::Collections::IVectorView<
               ABI::Windows::Gaming::Input::Gamepad*>** value) override;

  // ABI::Windows::Gaming::Input::IRawGameControllerStatics fake implementation.
  HRESULT __stdcall add_RawGameControllerAdded(
      ABI::Windows::Foundation::IEventHandler<
          ABI::Windows::Gaming::Input::RawGameController*>* value,
      EventRegistrationToken* token) override;
  HRESULT __stdcall remove_RawGameControllerAdded(
      EventRegistrationToken token) override;
  HRESULT __stdcall add_RawGameControllerRemoved(
      ABI::Windows::Foundation::IEventHandler<
          ABI::Windows::Gaming::Input::RawGameController*>* value,
      EventRegistrationToken* token) override;
  HRESULT __stdcall remove_RawGameControllerRemoved(
      EventRegistrationToken token) override;
  HRESULT __stdcall get_RawGameControllers(
      ABI::Windows::Foundation::Collections::IVectorView<
          ABI::Windows::Gaming::Input::RawGameController*>** value) override;
  HRESULT __stdcall FromGameController(
      ABI::Windows::Gaming::Input::IGameController* gameController,
      ABI::Windows::Gaming::Input::IRawGameController** value) override;

  // Adds a fake gamepad to simulate a gamepad connection operation for test.
  // Due to the multi-threaded apartment nature of IGamepadStatics COM API, the
  // callback would return on a different thread. We are using a separate
  // `SequencedTaskRunner` in this fake implementation to simulate this
  // behavior.
  void SimulateGamepadAdded(
      const Microsoft::WRL::ComPtr<FakeIGamepad>& gamepad_to_add,
      uint16_t hardware_product_id,
      uint16_t hardware_vendor_id,
      std::string_view display_name);

  // Uses a fake gamepad to simulate a gamepad disconnection operation for test.
  // Due to the multi-threaded apartment nature of IGamepadStatics COM API, the
  // callback would return on a different thread. We are using a separate
  // `SequencedTaskRunner` in this fake implementation to simulate this
  // behavior.
  void SimulateGamepadRemoved(
      const Microsoft::WRL::ComPtr<FakeIGamepad>& gamepad_to_remove);

  size_t GetGamepadAddedEventHandlerCount() const;
  size_t GetGamepadRemovedEventHandlerCount() const;

 private:
  typedef base::flat_map<
      int64_t /* event_registration_token */,
      Microsoft::WRL::ComPtr<ABI::Windows::Foundation::IEventHandler<
          ABI::Windows::Gaming::Input::Gamepad*>>>
      EventHandlerMap;

  using GamepadEventTriggerCallback = void (FakeIGamepadStatics::*)(
      const Microsoft::WRL::ComPtr<ABI::Windows::Gaming::Input::IGamepad>
          gamepad);

  ~FakeIGamepadStatics() override;

  void SimulateGamepadEvent(const Microsoft::WRL::ComPtr<
                                ABI::Windows::Gaming::Input::IGamepad>& gamepad,
                            GamepadEventTriggerCallback callback);

  void TriggerGamepadAddedCallbackOnRandomThread(
      const Microsoft::WRL::ComPtr<ABI::Windows::Gaming::Input::IGamepad>
          gamepad_to_add);

  void TriggerGamepadRemovedCallbackOnRandomThread(
      const Microsoft::WRL::ComPtr<ABI::Windows::Gaming::Input::IGamepad>
          gamepad_to_remove);

  void CacheGamepad(Microsoft::WRL::ComPtr<FakeIGamepad> fake_gamepad_to_add,
                    uint16_t hardware_product_id,
                    uint16_t hardware_vendor_id,
                    std::string_view display_name);
  void RemoveCachedGamepad(
      const Microsoft::WRL::ComPtr<FakeIGamepad>& fake_gamepad_to_remove);

  int64_t next_event_registration_token_ = 0;
  uint64_t next_gamepad_id_ = 0;

  EventHandlerMap gamepad_added_event_handler_map_;
  EventHandlerMap gamepad_removed_event_handler_map_;

  std::unordered_map<uint64_t, Microsoft::WRL::ComPtr<FakeIGamepad>>
      fake_gamepad_map_;
  std::unordered_map<uint64_t, Microsoft::WRL::ComPtr<FakeIRawGameController>>
      fake_raw_game_controller_map_;
};

}  // namespace device

#endif  // DEVICE_GAMEPAD_TEST_SUPPORT_FAKE_IGAMEPAD_STATICS_H_
