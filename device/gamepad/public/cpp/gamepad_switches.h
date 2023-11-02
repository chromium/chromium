// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_GAMEPAD_PUBLIC_CPP_GAMEPAD_SWITCHES_H_
#define DEVICE_GAMEPAD_PUBLIC_CPP_GAMEPAD_SWITCHES_H_

#include "device/gamepad/public/cpp/gamepad_features_export.h"

namespace switches {

// All switches in alphabetical order. The switches should be documented
// alongside the definition of their values in the .cc file.
GAMEPAD_FEATURES_EXPORT extern const char kEnableGamepadButtonAxisEvents[];
GAMEPAD_FEATURES_EXPORT extern const char kGamepadPollingInterval[];
GAMEPAD_FEATURES_EXPORT extern const char kRestrictGamepadAccess[];

}  // namespace switches

#endif  // DEVICE_GAMEPAD_PUBLIC_CPP_GAMEPAD_SWITCHES_H_
