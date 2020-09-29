// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/gamepad/public/cpp/gamepad_features.h"

#include <algorithm>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "device/gamepad/public/cpp/gamepad_switches.h"

namespace features {

// Enables gamepadbuttondown, gamepadbuttonup, gamepadbuttonchange,
// gamepadaxismove non-standard gamepad events.
const base::Feature kEnableGamepadButtonAxisEvents{
    "EnableGamepadButtonAxisEvents", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables the Windows.Gaming.Input data fetcher.
const base::Feature kEnableWindowsGamingInputDataFetcher{
    "EnableWindowsGamingInputDataFetcher", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kRestrictGamepadAccess{"RestrictGamepadAccess",
                                           base::FEATURE_DISABLED_BY_DEFAULT};

bool AreGamepadButtonAxisEventsEnabled() {
  // Check if button and axis events are enabled by a field trial.
  if (base::FeatureList::IsEnabled(kEnableGamepadButtonAxisEvents))
    return true;

  // Check if button and axis events are enabled by a command-line flag.
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line &&
      command_line->HasSwitch(switches::kEnableGamepadButtonAxisEvents)) {
    return true;
  }

  return false;
}

}  // namespace features
