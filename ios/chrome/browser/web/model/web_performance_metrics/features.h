// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEB_MODEL_WEB_PERFORMANCE_METRICS_FEATURES_H_
#define IOS_CHROME_BROWSER_WEB_MODEL_WEB_PERFORMANCE_METRICS_FEATURES_H_

#import "base/feature_list.h"

// Feature flag to control whether Interaction to Next Paint (INP) monitoring
// is enabled on iOS. Acts as a killswitch.
BASE_DECLARE_FEATURE(kIOSWebPerformanceMetricsINP);

#endif  // IOS_CHROME_BROWSER_WEB_MODEL_WEB_PERFORMANCE_METRICS_FEATURES_H_
