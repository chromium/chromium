// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/start_surface/ui_bundled/start_surface_features.h"

#import "base/metrics/field_trial_params.h"

namespace {
// Default value for kDefaultShowTabGridInactiveDurationInSeconds.
constexpr base::TimeDelta kDefaultShowTabGroupInGridInactiveDuration =
    base::Hours(1);
}  // anonymous namespace

BASE_FEATURE(kShowTabGroupInGridOnStart, base::FEATURE_DISABLED_BY_DEFAULT);

const char kShowTabGroupInGridInactiveDurationInSeconds[] =
    "ShowTabGridInactiveDurationInSeconds";

const char kReturnToStartSurfaceInactiveDurationInSeconds[] =
    "ReturnToStartSurfaceInactiveDurationInSeconds";

bool IsShowTabGroupInGridOnStartEnabled() {
  return base::FeatureList::IsEnabled(kShowTabGroupInGridOnStart);
}

base::TimeDelta GetReturnToTabGroupInGridDuration() {
  return base::Seconds(base::GetFieldTrialParamByFeatureAsDouble(
      kShowTabGroupInGridOnStart, kShowTabGroupInGridInactiveDurationInSeconds,
      kDefaultShowTabGroupInGridInactiveDuration.InSecondsF()));
}
