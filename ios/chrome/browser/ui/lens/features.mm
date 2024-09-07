// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/lens/features.h"

#import "base/metrics/field_trial_params.h"

BASE_FEATURE(kLensFiltersAblationModeEnabled,
             "LensFiltersAblationModeEnabled",
             base::FEATURE_DISABLED_BY_DEFAULT);

const char kLensFiltersAblationMode[] = "LensFilterAblationMode";

int LensFiltersAblationMode() {
  return base::GetFieldTrialParamByFeatureAsInt(kLensFiltersAblationModeEnabled,
                                                kLensFiltersAblationMode, 0);
}

BASE_FEATURE(kLensTranslateToggleModeEnabled,
             "LensTranslateToggleModeEnabled",
             base::FEATURE_DISABLED_BY_DEFAULT);

const char kLensTranslateToggleMode[] = "LensTranslateToggleMode";

int LensTranslateToggleMode() {
  return base::GetFieldTrialParamByFeatureAsInt(kLensTranslateToggleModeEnabled,
                                                kLensTranslateToggleMode, 0);
}

BASE_FEATURE(kLensWebPageLoadOptimizationEnabled,
             "LensWebPageLoadOptimizationEnabled",
             base::FEATURE_DISABLED_BY_DEFAULT);
