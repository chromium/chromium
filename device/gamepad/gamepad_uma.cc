// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/gamepad/gamepad_uma.h"

#include <type_traits>

#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "device/gamepad/gamepad_id_list.h"

namespace device {

void RecordConnectedGamepad(GamepadId gamepad_id) {
  // Avoid recording metrics for non-gamepads.
  if (gamepad_id == GamepadId::kUnknownGamepad)
    return;
  auto gamepad_id_as_underlying_type =
      static_cast<std::underlying_type<GamepadId>::type>(gamepad_id);
  base::UmaHistogramSparse("Gamepad.KnownGamepadConnectedWithId",
                           static_cast<int32_t>(gamepad_id_as_underlying_type));
}

void RecordUnknownGamepad(GamepadSource source) {
  UMA_HISTOGRAM_ENUMERATION("Gamepad.UnknownGamepadConnected", source);
}

}  // namespace device
