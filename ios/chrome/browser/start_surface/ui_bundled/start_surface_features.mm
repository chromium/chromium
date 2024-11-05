// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/start_surface/ui_bundled/start_surface_features.h"

#import "base/metrics/field_trial_params.h"

namespace {

// Default value for kReturnToStartSurfaceInactiveDurationInSeconds.
constexpr base::TimeDelta kDefaultReturnToStartSurfaceInactiveDuration =
    base::Hours(4);

}  // anonymous namespace

BASE_FEATURE(kStartSurface, "StartSurface", base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kIOSStartTimeStartupRemediations,
             "IOSStartTimeStartupRemediations",
             base::FEATURE_DISABLED_BY_DEFAULT);

const char kReturnToStartSurfaceInactiveDurationInSeconds[] =
    "ReturnToStartSurfaceInactiveDurationInSeconds";

const char kIOSStartTimeStartupRemediationsSaveNTPWebState[] =
    "ios-startup-remediations-save-ntp-web-state";

bool IsStartSurfaceEnabled() {
  return base::FeatureList::IsEnabled(kStartSurface);
}

base::TimeDelta GetReturnToStartSurfaceDuration() {
  return base::Seconds(base::GetFieldTrialParamByFeatureAsDouble(
      kStartSurface, kReturnToStartSurfaceInactiveDurationInSeconds,
      kDefaultReturnToStartSurfaceInactiveDuration.InSecondsF()));
}

StartupRemediationsType GetIOSStartTimeStartupRemediationsEnabledType() {
  if (base::GetFieldTrialParamByFeatureAsBool(
          kIOSStartTimeStartupRemediations,
          kIOSStartTimeStartupRemediationsSaveNTPWebState, false)) {
    return StartupRemediationsType::kSaveNewNTPWebState;
  }
  return base::FeatureList::IsEnabled(kIOSStartTimeStartupRemediations)
             ? StartupRemediationsType::kOpenNewNTPTab
             : StartupRemediationsType::kDisabled;
}
