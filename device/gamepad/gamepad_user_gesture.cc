// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/gamepad/gamepad_user_gesture.h"

#include <math.h>

#include <algorithm>

#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "device/gamepad/public/cpp/gamepads.h"

namespace {
// A big enough deadzone to detect accidental presses.
const float kAxisMoveAmountThreshold = 0.5;
}

namespace device {

bool GamepadsHaveUserGesture(const Gamepads& gamepads) {
  for (size_t i = 0; i < Gamepads::kItemsLengthCap; i++) {
    const Gamepad& pad = gamepads.items[i];

    // If the device is physically connected, then check the buttons and axes
    // to see if there is currently an intentional user action.
    if (pad.connected) {
      // Only VR Controllers have a display id, and are only reported as
      // connected during WebVR presentation, so the user is definitely
      // expecting their controller to be used. Note that this will also
      // satisfy the gesture requirement for all other connected controllers,
      // exposing them too. This is unfortunate, but worth the tradeoff and will
      // go away in the future when WebVR is fully replaced with WebXR.
      if (pad.display_id != 0)
        return true;

      const auto buttons = base::span(pad.buttons).first(pad.buttons_length);
      for (const auto& button : buttons) {
        if (button.pressed) {
          return true;
        }
      }

      const auto axes = base::span(pad.axes).first(pad.axes_length);
      for (const auto& axis : axes) {
        if (fabs(axis) > kAxisMoveAmountThreshold) {
          return true;
        }
      }
    }
  }
  return false;
}

}  // namespace device
