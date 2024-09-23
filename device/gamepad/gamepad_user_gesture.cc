// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "device/gamepad/gamepad_user_gesture.h"

#include <math.h>

#include <algorithm>

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

      for (size_t button_index = 0; button_index < pad.buttons_length;
           button_index++) {
        if (pad.buttons[button_index].pressed)
          return true;
      }

      for (size_t axes_index = 0; axes_index < pad.axes_length; axes_index++) {
        if (fabs(pad.axes[axes_index]) > kAxisMoveAmountThreshold)
          return true;
      }
    }
  }
  return false;
}

}  // namespace device
