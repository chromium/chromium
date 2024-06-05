// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_LENS_FEATURES_H_
#define IOS_CHROME_BROWSER_UI_LENS_FEATURES_H_

#import "base/feature_list.h"

// Whether to enable the early transition from the Lens UI to web page.
BASE_DECLARE_FEATURE(kLensWebPageEarlyTransitionEnabled);

// The feature parameter that indicates the loading progress threshold.
extern const char kLoadingProgressThreshold[];

// Loading progress threshold to transition from the Lens UI to web page. Value
// should be between 0.0 and 1.0.
double LensWebPageEarlyTransitionLoadingProgressThreshold();

#endif  // IOS_CHROME_BROWSER_UI_LENS_FEATURES_H_
