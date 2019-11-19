// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_GAMEPAD_PUBLIC_CPP_GAMEPADS_H_
#define DEVICE_GAMEPAD_PUBLIC_CPP_GAMEPADS_H_

#include "base/component_export.h"

#include "device/gamepad/public/cpp/gamepad.h"

namespace device {

#pragma pack(push, 4)

// This structure is intentionally POD and fixed size so that it can be stored
// in shared memory between hardware polling threads and the rest of the
// browser.
class COMPONENT_EXPORT(GAMEPAD_PUBLIC) Gamepads {
 public:
  static constexpr size_t kItemsLengthCap = 4;

  // Gamepad data for N separate gamepad devices.
  Gamepad items[kItemsLengthCap];
};

#pragma pack(pop)

}  // namespace device

#endif  // DEVICE_GAMEPAD_PUBLIC_CPP_GAMEPADS_H_
