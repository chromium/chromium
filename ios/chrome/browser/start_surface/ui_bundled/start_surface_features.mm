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

const char kReturnToStartSurfaceInactiveDurationInSeconds[] =
    "ReturnToStartSurfaceInactiveDurationInSeconds";

bool IsStartSurfaceEnabled() {
  return base::FeatureList::IsEnabled(kStartSurface);
}

base::TimeDelta GetReturnToStartSurfaceDuration() {
  return base::Seconds(base::GetFieldTrialParamByFeatureAsDouble(
      kStartSurface, kReturnToStartSurfaceInactiveDurationInSeconds,
      kDefaultReturnToStartSurfaceInactiveDuration.InSecondsF()));
}
