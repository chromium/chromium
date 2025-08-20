// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/lens/ui_bundled/features.h"

#import "base/metrics/field_trial_params.h"

BASE_FEATURE(LensFiltersAblationModeEnabled, base::FEATURE_DISABLED_BY_DEFAULT);

const char kLensFiltersAblationMode[] = "LensFilterAblationMode";

int LensFiltersAblationMode() {
  return base::GetFieldTrialParamByFeatureAsInt(kLensFiltersAblationModeEnabled,
                                                kLensFiltersAblationMode, 0);
}

BASE_FEATURE(LensTranslateToggleModeEnabled, base::FEATURE_DISABLED_BY_DEFAULT);

const char kLensTranslateToggleMode[] = "LensTranslateToggleMode";

int LensTranslateToggleMode() {
  return base::GetFieldTrialParamByFeatureAsInt(kLensTranslateToggleModeEnabled,
                                                kLensTranslateToggleMode, 0);
}

BASE_FEATURE(LensWebPageLoadOptimizationEnabled,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(LensUnaryApisWithHttpTransportEnabled,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(LensUnaryHttpTransportEnabled, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(LensUnaryApiSalientTextEnabled, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(LensSingleTapTextSelectionDisabled,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(LensGestureTextSelectionDisabled,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(LensUnaryClientDataHeaderEnabled,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(LensBlockFetchObjectsInteractionRPCsOnSeparateHandshake,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(LensPrewarmHardStickinessInInputSelection,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(LensPrewarmHardStickinessInQueryFormulation,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(LensFetchSrpApiEnabled, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(LensExactMatchesEnabled, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(LensCameraNoStillOutputRequired,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(LensCameraUnbinnedCaptureFormatsPreferred,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(LensContinuousZoomEnabled, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(LensInitialLvfZoomLevel90Percent,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(LensTripleCameraEnabled, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(LensStrokesAPIEnabled, base::FEATURE_DISABLED_BY_DEFAULT);
