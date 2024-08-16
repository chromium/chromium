// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_METRICS_MODEL_IOS_FEED_ENABLED_METRICS_PROVIDER_H_
#define IOS_CHROME_BROWSER_METRICS_MODEL_IOS_FEED_ENABLED_METRICS_PROVIDER_H_

#import "components/metrics/metrics_provider.h"

// Log a metric indicating whether the Feed can be shown to the user.
class IOSFeedEnabledMetricsProvider : public metrics::MetricsProvider {
 public:
  IOSFeedEnabledMetricsProvider();
  ~IOSFeedEnabledMetricsProvider() override;

  // metrics::MetricsProvider
  void ProvideCurrentSessionData(
      metrics::ChromeUserMetricsExtension* uma_proto) override;
};

#endif  // IOS_CHROME_BROWSER_METRICS_MODEL_IOS_FEED_ENABLED_METRICS_PROVIDER_H_
