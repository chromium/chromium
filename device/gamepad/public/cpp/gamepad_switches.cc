// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/gamepad/public/cpp/gamepad_switches.h"

namespace switches {

// Enables gamepadbuttondown, gamepadbuttonup, gamepadbuttonchange,
// gamepadaxismove non-standard gamepad events.
const char kEnableGamepadButtonAxisEvents[] =
    "enable-gamepad-button-axis-events";

// Enables Feature Policy and Secure Context requirements on getGamepads.
const char kRestrictGamepadAccess[] = "restrict-gamepad-access";

// Overrides the gamepad polling interval. Decreasing the interval improves
// input latency of buttons and axes but may negatively affect performance due
// to more CPU time spent in the input polling thread.
const char kGamepadPollingInterval[] = "gamepad-polling-interval";

}  // namespace switches
