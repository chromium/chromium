// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PRIVATE_INSIGHTS_MODEL_FEATURES_H_
#define IOS_CHROME_BROWSER_PRIVATE_INSIGHTS_MODEL_FEATURES_H_

#import "base/feature_list.h"

// Enables creating the `PrivateInsightsService` on iOS, so that the iOS launch
// is controlled independently of other platforms. The service configuration
// (e.g. the FCP server URI and the upload interval) is still read from the
// params of `private_insights::kPrivateInsightsFeature`.
BASE_DECLARE_FEATURE(kIOSPrivateInsightsService);

#endif  // IOS_CHROME_BROWSER_PRIVATE_INSIGHTS_MODEL_FEATURES_H_
