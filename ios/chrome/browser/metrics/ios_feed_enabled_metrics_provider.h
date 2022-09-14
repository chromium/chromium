// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_METRICS_IOS_FEED_ENABLED_METRICS_PROVIDER_H_
#define IOS_CHROME_BROWSER_METRICS_IOS_FEED_ENABLED_METRICS_PROVIDER_H_

#import "components/metrics/metrics_provider.h"

class PrefService;

// Log a metric indicating whether the Feed can be shown to the user.
class IOSFeedEnabledMetricsProvider : public metrics::MetricsProvider {
 public:
  explicit IOSFeedEnabledMetricsProvider(PrefService* pref_service);

  IOSFeedEnabledMetricsProvider(const IOSFeedEnabledMetricsProvider&) = delete;
  IOSFeedEnabledMetricsProvider& operator=(
      const IOSFeedEnabledMetricsProvider&) = delete;

  ~IOSFeedEnabledMetricsProvider() override;

  // metrics::MetricsProvider
  void ProvideCurrentSessionData(
      metrics::ChromeUserMetricsExtension* uma_proto) override;

 private:
  PrefService* pref_service_;
};

#endif  // IOS_CHROME_BROWSER_METRICS_IOS_FEED_ENABLED_METRICS_PROVIDER_H_
