// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_GAMEPAD_PUBLIC_CPP_GAMEPAD_FEATURES_H_
#define DEVICE_GAMEPAD_PUBLIC_CPP_GAMEPAD_FEATURES_H_

#include "base/feature_list.h"
#include "device/gamepad/public/cpp/gamepad_features_export.h"

namespace features {

GAMEPAD_FEATURES_EXPORT extern const base::Feature
    kEnableGamepadButtonAxisEvents;
GAMEPAD_FEATURES_EXPORT extern const base::Feature
    kEnableWindowsGamingInputDataFetcher;
GAMEPAD_FEATURES_EXPORT extern const base::Feature kRestrictGamepadAccess;

GAMEPAD_FEATURES_EXPORT bool AreGamepadButtonAxisEventsEnabled();

}  // namespace features

#endif  // DEVICE_GAMEPAD_PUBLIC_CPP_GAMEPAD_FEATURES_H_
