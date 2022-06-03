// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/gamepad/test_support/fake_igamepad.h"

#include "base/notreached.h"

namespace device {

FakeIGamepad::FakeIGamepad() = default;
FakeIGamepad::~FakeIGamepad() = default;

HRESULT WINAPI
FakeIGamepad::get_Vibration(ABI::Windows::Gaming::Input::GamepadVibration*) {
  NOTIMPLEMENTED();
  return E_NOTIMPL;
}

HRESULT WINAPI
FakeIGamepad::put_Vibration(ABI::Windows::Gaming::Input::GamepadVibration) {
  NOTIMPLEMENTED();
  return E_NOTIMPL;
}

HRESULT WINAPI
FakeIGamepad::GetCurrentReading(ABI::Windows::Gaming::Input::GamepadReading*) {
  NOTIMPLEMENTED();
  return E_NOTIMPL;
}

}  // namespace device
