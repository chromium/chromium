// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/lens/ui_bundled/features.h"

#import "base/metrics/field_trial_params.h"

BASE_FEATURE(kLensFiltersAblationModeEnabled, base::FEATURE_ENABLED_BY_DEFAULT);

const char kLensFiltersAblationMode[] = "LensFilterAblationMode";

int LensFiltersAblationMode() {
  return base::GetFieldTrialParamByFeatureAsInt(kLensFiltersAblationModeEnabled,
                                                kLensFiltersAblationMode,
                                                /*default_value=*/1);
}

BASE_FEATURE(kLensTranslateToggleModeEnabled, base::FEATURE_ENABLED_BY_DEFAULT);

const char kLensTranslateToggleMode[] = "LensTranslateToggleMode";

int LensTranslateToggleMode() {
  return base::GetFieldTrialParamByFeatureAsInt(kLensTranslateToggleModeEnabled,
                                                kLensTranslateToggleMode,
                                                /*default_value=*/1);
}

BASE_FEATURE(kLensWebPageLoadOptimizationEnabled,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kLensUnaryApisWithHttpTransportEnabled,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kLensUnaryHttpTransportEnabled, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kLensUnaryApiSalientTextEnabled, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kLensSingleTapTextSelectionDisabled,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kLensGestureTextSelectionDisabled,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kLensBlockFetchObjectsInteractionRPCsOnSeparateHandshake,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kLensPrewarmHardStickinessInInputSelection,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kLensPrewarmHardStickinessInQueryFormulation,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kLensFetchSrpApiEnabled, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kLensExactMatchesEnabled, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kLensCameraNoStillOutputRequired,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kLensCameraUnbinnedCaptureFormatsPreferred,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kLensContinuousZoomEnabled, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kLensInitialLvfZoomLevel90Percent,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kLensTripleCameraEnabled, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kLensStrokesAPIEnabled, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kLensOmnientShaderV2Enabled, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kLensStreamServiceWebChannelTransportEnabled,
             base::FEATURE_DISABLED_BY_DEFAULT);
