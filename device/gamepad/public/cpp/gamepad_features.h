// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_GAMEPAD_PUBLIC_CPP_GAMEPAD_FEATURES_H_
#define DEVICE_GAMEPAD_PUBLIC_CPP_GAMEPAD_FEATURES_H_

#include "base/feature_list.h"
#include "device/gamepad/public/cpp/gamepad_features_export.h"

namespace features {

GAMEPAD_FEATURES_EXPORT BASE_DECLARE_FEATURE(kEnableGamepadButtonAxisEvents);
GAMEPAD_FEATURES_EXPORT BASE_DECLARE_FEATURE(
    kEnableWindowsGamingInputDataFetcher);
GAMEPAD_FEATURES_EXPORT BASE_DECLARE_FEATURE(kRestrictGamepadAccess);
GAMEPAD_FEATURES_EXPORT BASE_DECLARE_FEATURE(kEnableGamepadMultitouch);

#if BUILDFLAG(IS_ANDROID)
GAMEPAD_FEATURES_EXPORT BASE_DECLARE_FEATURE(kEnableAndroidGamepadVibration);
#endif  // BUILDFLAG(IS_ANDROID)

GAMEPAD_FEATURES_EXPORT bool AreGamepadButtonAxisEventsEnabled();
GAMEPAD_FEATURES_EXPORT bool IsGamepadMultitouchEnabled();

}  // namespace features

#endif  // DEVICE_GAMEPAD_PUBLIC_CPP_GAMEPAD_FEATURES_H_
