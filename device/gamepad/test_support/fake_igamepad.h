// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_GAMEPAD_TEST_SUPPORT_FAKE_IGAMEPAD_H_
#define DEVICE_GAMEPAD_TEST_SUPPORT_FAKE_IGAMEPAD_H_

#include <stdint.h>
#include <windows.foundation.collections.h>
#include <windows.gaming.input.h>
#include <wrl.h>

namespace device {

class FakeIGamepad final
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<
              Microsoft::WRL::WinRt | Microsoft::WRL::InhibitRoOriginateError>,
          ABI::Windows::Gaming::Input::IGamepad,
          ABI::Windows::Gaming::Input::IGamepad2,
          ABI::Windows::Gaming::Input::IGameController> {
 public:
  FakeIGamepad();
  FakeIGamepad(const FakeIGamepad&) = delete;
  FakeIGamepad& operator=(const FakeIGamepad&) = delete;

  // ABI::Windows::Gaming::Input::IGamepad fake implementation.
  HRESULT WINAPI get_Vibration(ABI::Windows::Gaming::Input::GamepadVibration*
                                   gamepad_vibration) override;
  HRESULT WINAPI put_Vibration(
      ABI::Windows::Gaming::Input::GamepadVibration gamepad_vibration) override;
  HRESULT WINAPI GetCurrentReading(
      ABI::Windows::Gaming::Input::GamepadReading* gamepad_reading) override;

  // ABI::Windows::Gaming::Input::IGamepad2 fake implementation.
  HRESULT WINAPI GetButtonLabel(
      ABI::Windows::Gaming::Input::GamepadButtons button,
      ABI::Windows::Gaming::Input::GameControllerButtonLabel* value) override;

  // ABI::Windows::Gaming::Input::IGameController fake implementation.
  HRESULT WINAPI
  add_HeadsetConnected(ABI::Windows::Foundation::ITypedEventHandler<
                           ABI::Windows::Gaming::Input::IGameController*,
                           ABI::Windows::Gaming::Input::Headset*>* value,
                       EventRegistrationToken* token) override;
  HRESULT WINAPI remove_HeadsetConnected(EventRegistrationToken token) override;
  HRESULT WINAPI
  add_HeadsetDisconnected(ABI::Windows::Foundation::ITypedEventHandler<
                              ABI::Windows::Gaming::Input::IGameController*,
                              ABI::Windows::Gaming::Input::Headset*>* value,
                          EventRegistrationToken* token) override;
  HRESULT WINAPI
  remove_HeadsetDisconnected(EventRegistrationToken token) override;
  HRESULT WINAPI
  add_UserChanged(ABI::Windows::Foundation::ITypedEventHandler<
                      ABI::Windows::Gaming::Input::IGameController*,
                      ABI::Windows::System::UserChangedEventArgs*>* value,
                  EventRegistrationToken* token) override;
  HRESULT WINAPI remove_UserChanged(EventRegistrationToken token) override;
  HRESULT WINAPI
  get_Headset(ABI::Windows::Gaming::Input::IHeadset** value) override;
  HRESULT WINAPI get_IsWireless(boolean* value) override;
  HRESULT WINAPI get_User(ABI::Windows::System::IUser** value) override;

  void SetCurrentReading(
      const ABI::Windows::Gaming::Input::GamepadReading& gamepad_reading);

  uint64_t GetId() { return gamepad_id_; }
  void SetId(uint64_t gamepad_id) { gamepad_id_ = gamepad_id; }

  void SetHasPaddles(bool has_paddles) { has_paddles_ = has_paddles; }

 private:
  ~FakeIGamepad() override;

  uint64_t gamepad_id_;
  bool has_paddles_ = false;

  ABI::Windows::Gaming::Input::GamepadReading fake_gamepad_reading_;
  ABI::Windows::Gaming::Input::GamepadVibration fake_gamepad_vibration_;
};

}  // namespace device

#endif  // DEVICE_GAMEPAD_TEST_SUPPORT_FAKE_IGAMEPAD_H_
