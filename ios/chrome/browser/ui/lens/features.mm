// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/lens/features.h"

#import "base/metrics/field_trial_params.h"

BASE_FEATURE(kLensWebPageEarlyTransitionEnabled,
             "LensWebPageEarlyTransitionEnabled",
             base::FEATURE_DISABLED_BY_DEFAULT);

const char kLoadingProgressThreshold[] = "LoadingProgressThreshold";

double LensWebPageEarlyTransitionLoadingProgressThreshold() {
  return base::GetFieldTrialParamByFeatureAsDouble(
      kLensWebPageEarlyTransitionEnabled, kLoadingProgressThreshold, 0.5);
}
