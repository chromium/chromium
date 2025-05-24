// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/lens/ui_bundled/features.h"

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

BASE_FEATURE(kLensUnaryApisWithHttpTransportEnabled,
             "LensUnaryApisWithHttpTransportEnabled",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kLensUnaryHttpTransportEnabled,
             "LensUnaryHttpTransportEnabled",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kLensUnaryApiSalientTextEnabled,
             "LensUnaryApiSalientTextEnabled",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kLensClearcutBackgroundUploadEnabled,
             "LensClearcutBackgroundUploadEnabled",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kLensClearcutLoggerFastQosEnabled,
             "LensClearcutLoggerFastQosEnabled",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kLensSingleTapTextSelectionDisabled,
             "LensSingleTapTextSelectionDisabled",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kLensInkMultiSampleModeDisabled,
             "LensInkMultiSampleModeDisabled",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kLensGestureTextSelectionDisabled,
             "LensGestureTextSelectionDisabled",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kLensVsintParamEnabled,
             "LensVsintParamEnabled",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kLensUnaryClientDataHeaderEnabled,
             "LensUnaryClientDataHeaderEnabled",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kLensBlockFetchObjectsInteractionRPCsOnSeparateHandshake,
             "LensBlockFetchObjectsInteractionRPCsOnSeparateHandshake",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kLensPrewarmHardStickinessInInputSelection,
             "LensPrewarmHardStickinessInInputSelection",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kLensPrewarmHardStickinessInQueryFormulation,
             "LensPrewarmHardStickinessInQueryFormulation",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kLensFetchSrpApiEnabled,
             "LensFetchSrpApiEnabled",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kLensQRCodeParsingFix,
             "LensQRCodeParsingFix",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kLensExactMatchesEnabled,
             "LensExactMatchesEnabled",
             base::FEATURE_DISABLED_BY_DEFAULT);
