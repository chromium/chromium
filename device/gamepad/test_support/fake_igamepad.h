// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_GAMEPAD_TEST_SUPPORT_FAKE_IGAMEPAD_H_
#define DEVICE_GAMEPAD_TEST_SUPPORT_FAKE_IGAMEPAD_H_

#include <Windows.Gaming.Input.h>
#include <wrl.h>

namespace device {

class FakeIGamepad final
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<
              Microsoft::WRL::WinRt | Microsoft::WRL::InhibitRoOriginateError>,
          ABI::Windows::Gaming::Input::IGamepad> {
 public:
  FakeIGamepad();
  FakeIGamepad(const FakeIGamepad&) = delete;
  FakeIGamepad& operator=(const FakeIGamepad&) = delete;

  // ABI::Windows::Gaming::Input::IGamepad fake implementation.
  HRESULT WINAPI
  get_Vibration(ABI::Windows::Gaming::Input::GamepadVibration*) override;
  HRESULT WINAPI
      put_Vibration(ABI::Windows::Gaming::Input::GamepadVibration) override;
  HRESULT WINAPI
  GetCurrentReading(ABI::Windows::Gaming::Input::GamepadReading*) override;

 private:
  ~FakeIGamepad() final;
};

}  // namespace device

#endif  // DEVICE_GAMEPAD_TEST_SUPPORT_FAKE_IGAMEPAD_H_
