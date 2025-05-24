// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/gamepad/public/cpp/gamepad_features.h"

#include <algorithm>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/strings/string_number_conversions.h"
#include "device/gamepad/public/cpp/gamepad_switches.h"

namespace features {

// Enables the Windows.Gaming.Input data fetcher.
//
// Note: This feature is used by the "never expire" flag
// chrome://flags/#enable-windows-gaming-input-data-fetcher and should not be
// removed. See crbug.com/40287784.
BASE_FEATURE(kEnableWindowsGamingInputDataFetcher,
             "EnableWindowsGamingInputDataFetcher",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables gamepad multitouch
BASE_FEATURE(kEnableGamepadMultitouch,
             "EnableGamepadMultitouch",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsGamepadMultitouchEnabled() {
  if (base::FeatureList::IsEnabled(kEnableGamepadMultitouch)) {
    return true;
  }

  return false;
}

}  // namespace features
