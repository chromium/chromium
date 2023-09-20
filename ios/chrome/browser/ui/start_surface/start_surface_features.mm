// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/start_surface/start_surface_features.h"

#import "base/metrics/field_trial_params.h"

BASE_FEATURE(kStartSurface, "StartSurface", base::FEATURE_ENABLED_BY_DEFAULT);

const char kReturnToStartSurfaceInactiveDurationInSeconds[] =
    "ReturnToStartSurfaceInactiveDurationInSeconds";

bool IsStartSurfaceEnabled() {
  return base::FeatureList::IsEnabled(kStartSurface);
}

double GetReturnToStartSurfaceDuration() {
  return base::GetFieldTrialParamByFeatureAsDouble(
      kStartSurface, kReturnToStartSurfaceInactiveDurationInSeconds,
      60 * 60 * 6 /*default to 6 hour*/);
}
