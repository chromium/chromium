// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_NTP_METRICS_FEED_METRICS_PROVIDER_H_
#define IOS_CHROME_BROWSER_UI_NTP_METRICS_FEED_METRICS_PROVIDER_H_

#import "components/metrics/metrics_provider.h"

class FeedMetricsProvider : public metrics::MetricsProvider {
 public:
  explicit FeedMetricsProvider();
  FeedMetricsProvider(const FeedMetricsProvider&) = delete;
  FeedMetricsProvider& operator=(const FeedMetricsProvider&) = delete;

  ~FeedMetricsProvider() override;

  // metrics::MetricsProvider
  void ProvideCurrentSessionData(
      metrics::ChromeUserMetricsExtension* uma_proto) override;
};

#endif  // IOS_CHROME_BROWSER_UI_NTP_METRICS_FEED_METRICS_PROVIDER_H_
