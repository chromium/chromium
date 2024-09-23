// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_METRICS_MODEL_CONSTANTS_H_
#define IOS_CHROME_BROWSER_METRICS_MODEL_CONSTANTS_H_

// Stores the latest activity bucket the user was on.
extern const char kActivityBucketKey[];

// Histogram name for the feed activity buckets filter.
extern const char kAllFeedsActivityBucketsByProviderHistogram[];

// Histogram name for the Notification Authorization Status filter.
extern const char kNotifAuthorizationStatusByProviderHistogram[];

// Histogram name for the Content Notification Client Status filter.
extern const char kContentNotifClientStatusByProviderHistogram[];

// Histogram name for the Sports Notification Client Status filter.
extern const char kSportsNotifClientStatusByProviderHistogram[];

// Histogram name for the Tips Notification Client Status filter.
extern const char kTipsNotifClientStatusByProviderHistogram[];

// Histogram name for the Safety Check Notification Client Status filter.
extern const char kSafetyCheckNotifClientStatusByProviderHistogram[];

// Histogram name for the Send Tab Notification Client Status filter.
extern const char kSendTabNotifClientStatusByProviderHistogram[];

// Histogram name for the feed enabled metric.
extern const char kFeedEnabledHistogram[];

#endif  // IOS_CHROME_BROWSER_METRICS_MODEL_CONSTANTS_H_
