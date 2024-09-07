// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_LENS_FEATURES_H_
#define IOS_CHROME_BROWSER_UI_LENS_FEATURES_H_

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

#endif  // IOS_CHROME_BROWSER_UI_LENS_FEATURES_H_
