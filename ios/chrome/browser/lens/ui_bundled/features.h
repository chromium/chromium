// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_LENS_UI_BUNDLED_FEATURES_H_
#define IOS_CHROME_BROWSER_LENS_UI_BUNDLED_FEATURES_H_

#import "base/feature_list.h"

// Whether to enable the filters ablation mode.
BASE_DECLARE_FEATURE(kLensFiltersAblationModeEnabled);

// The feature parameter that indicates the filters ablation mode.
extern const char kLensFiltersAblationMode[];

// Integer that maps to the filters ablation mode enum.
int LensFiltersAblationMode();

// Whether to enable the translate toggle mode.
BASE_DECLARE_FEATURE(kLensTranslateToggleModeEnabled);

// The feature parameter that indicates the translate toggle mode.
extern const char kLensTranslateToggleMode[];

// Integer that maps to the translate toggle mode enum.
int LensTranslateToggleMode();

// Whether to enable the Lens web page load optimization.
BASE_DECLARE_FEATURE(kLensWebPageLoadOptimizationEnabled);

// Whether to use unary APIs with HTTP transport.
BASE_DECLARE_FEATURE(kLensUnaryApisWithHttpTransportEnabled);

// Whether to use HTTP transport for unary requests.
BASE_DECLARE_FEATURE(kLensUnaryHttpTransportEnabled);

// Whether to enable the unary salient text API.
BASE_DECLARE_FEATURE(kLensUnaryApiSalientTextEnabled);

// Whether to enable background uploading for clearcut logs.
BASE_DECLARE_FEATURE(kLensClearcutBackgroundUploadEnabled);

// Whether to use fast QOS for clearcut logging.
BASE_DECLARE_FEATURE(kLensClearcutLoggerFastQosEnabled);

// Whether to disable single tap text selection.
BASE_DECLARE_FEATURE(kLensSingleTapTextSelectionDisabled);

// Whether to disable the Ink library's multi-sample mode.
BASE_DECLARE_FEATURE(kLensInkMultiSampleModeDisabled);

// Whether to disable gesture text selection.
BASE_DECLARE_FEATURE(kLensGestureTextSelectionDisabled);

// Whether to enable the vsint param.
BASE_DECLARE_FEATURE(kLensVsintParamEnabled);

// Whether to enable the unary client data header.
BASE_DECLARE_FEATURE(kLensUnaryClientDataHeaderEnabled);

// Whether to block fetch objects interaction RPCs on separate handshake.
BASE_DECLARE_FEATURE(kLensBlockFetchObjectsInteractionRPCsOnSeparateHandshake);

// Whether to prewarm hard stickiness in Input Selection.
BASE_DECLARE_FEATURE(kLensPrewarmHardStickinessInInputSelection);

// Whether to prewarm hard stickiness in query formulation.
BASE_DECLARE_FEATURE(kLensPrewarmHardStickinessInQueryFormulation);

// Whether to enable the fetch srp API.
BASE_DECLARE_FEATURE(kLensFetchSrpApiEnabled);

// Whether to enable the QR parsing code fix.
BASE_DECLARE_FEATURE(kLensQRCodeParsingFix);

// Whether to enable exact matches.
BASE_DECLARE_FEATURE(kLensExactMatchesEnabled);

#endif  // IOS_CHROME_BROWSER_LENS_UI_BUNDLED_FEATURES_H_
