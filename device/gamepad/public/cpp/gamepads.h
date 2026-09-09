// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_GAMEPAD_PUBLIC_CPP_GAMEPADS_H_
#define DEVICE_GAMEPAD_PUBLIC_CPP_GAMEPADS_H_

#include <array>

#include "base/component_export.h"
#include "device/gamepad/public/cpp/gamepad.h"

namespace device {

// This structure is intentionally POD and fixed size so that it can be stored
// in shared memory between hardware polling threads and the rest of the
// browser.
class COMPONENT_EXPORT(GAMEPAD_PUBLIC) Gamepads {
 public:
  // Maximum number of gamepads Chromium can expose through its shared gamepad
  // state. This is an implementation limit, not a limit imposed by the
  // Gamepad API. Individual platform data fetchers may expose fewer gamepads.
  static constexpr size_t kItemsLengthCap = 8;

  // Gamepad data for N separate gamepad devices.
  std::array<Gamepad, kItemsLengthCap> items;
};

}  // namespace device

#endif  // DEVICE_GAMEPAD_PUBLIC_CPP_GAMEPADS_H_
