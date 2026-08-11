// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/test/webxr_test_gamepad_utils.h"

namespace device {

device::GamepadButton* GetGamepadButton(device::mojom::Gamepad* gamepad,
                                        XrButtonId id) {
  if (!gamepad) {
    return nullptr;
  }
  auto index = GamepadButtonIndexFromButtonId(id);
  if (!index || *index >= gamepad->buttons.size()) {
    return nullptr;
  }
  gamepad->buttons[*index].used = true;
  return &gamepad->buttons[*index];
}

const device::GamepadButton* GetGamepadButton(
    const device::mojom::Gamepad* gamepad,
    XrButtonId id) {
  if (!gamepad) {
    return nullptr;
  }
  auto index = GamepadButtonIndexFromButtonId(id);
  if (!index || *index >= gamepad->buttons.size()) {
    return nullptr;
  }
  return &gamepad->buttons[*index];
}

}  // namespace device
