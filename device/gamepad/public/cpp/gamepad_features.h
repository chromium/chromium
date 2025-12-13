// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_GAMEPAD_PUBLIC_CPP_GAMEPAD_FEATURES_H_
#define DEVICE_GAMEPAD_PUBLIC_CPP_GAMEPAD_FEATURES_H_

#include "base/feature_list.h"
#include "build/build_config.h"
#include "device/gamepad/public/cpp/gamepad_features_export.h"

namespace features {

GAMEPAD_FEATURES_EXPORT BASE_DECLARE_FEATURE(
    kEnableWindowsGamingInputDataFetcher);
GAMEPAD_FEATURES_EXPORT BASE_DECLARE_FEATURE(kEnableGamepadMultitouch);
GAMEPAD_FEATURES_EXPORT BASE_DECLARE_FEATURE(
    kEnableSimulatedGamepadDataFetcher);
GAMEPAD_FEATURES_EXPORT BASE_DECLARE_FEATURE(kGamepadRawInputChangeEvent);

#if BUILDFLAG(IS_WIN)
GAMEPAD_FEATURES_EXPORT BASE_DECLARE_FEATURE(kIgnorePS5GamepadsInWgi);
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
GAMEPAD_FEATURES_EXPORT BASE_DECLARE_FEATURE(kAllowlistHidrawGamepads);
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

GAMEPAD_FEATURES_EXPORT bool IsGamepadMultitouchEnabled();

}  // namespace features

#endif  // DEVICE_GAMEPAD_PUBLIC_CPP_GAMEPAD_FEATURES_H_
