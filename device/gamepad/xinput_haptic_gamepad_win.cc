// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/gamepad/xinput_haptic_gamepad_win.h"

#include "base/trace_event/trace_event.h"

namespace {
const long kRumbleMagnitudeMax = 0xffff;
}  // namespace

namespace device {

XInputHapticGamepadWin::XInputHapticGamepadWin(
    int pad_id,
    XInputSetStateFunc xinput_set_state)
    : pad_id_(pad_id), xinput_set_state_(xinput_set_state) {}

XInputHapticGamepadWin::~XInputHapticGamepadWin() = default;

void XInputHapticGamepadWin::SetVibration(double strong_magnitude,
                                          double weak_magnitude) {
  if (pad_id_ < 0 || pad_id_ > XUSER_MAX_COUNT || xinput_set_state_ == nullptr)
    return;
  XINPUT_VIBRATION vibration;
  vibration.wLeftMotorSpeed =
      static_cast<long>(strong_magnitude * kRumbleMagnitudeMax);
  vibration.wRightMotorSpeed =
      static_cast<long>(weak_magnitude * kRumbleMagnitudeMax);

  TRACE_EVENT_BEGIN1("GAMEPAD", "XInputSetState", "id", pad_id_);
  xinput_set_state_(pad_id_, &vibration);
  TRACE_EVENT_END1("GAMEPAD", "XInputSetState", "id", pad_id_);
}

base::WeakPtr<AbstractHapticGamepad> XInputHapticGamepadWin::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

}  // namespace device
