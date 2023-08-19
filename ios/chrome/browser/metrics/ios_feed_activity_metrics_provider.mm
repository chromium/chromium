// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/metrics/ios_feed_activity_metrics_provider.h"
#import "base/apple/foundation_util.h"
#import "base/metrics/histogram_functions.h"

IOSFeedActivityMetricsProvider::IOSFeedActivityMetricsProvider() {}

IOSFeedActivityMetricsProvider::~IOSFeedActivityMetricsProvider() {}

void IOSFeedActivityMetricsProvider::ProvideCurrentSessionData(
    metrics::ChromeUserMetricsExtension* uma_proto) {
  // Retrieve activity bucket from storage.
  int activityBucket = (int)[[NSUserDefaults standardUserDefaults]
      integerForKey:kActivityBucketKey];
  base::UmaHistogramExactLinear(kAllFeedsActivityBucketsByProviderHistogram,
                                activityBucket, 4);
}
