// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_METRICS_MODEL_IOS_FEED_ACTIVITY_METRICS_PROVIDER_H_
#define IOS_CHROME_BROWSER_METRICS_MODEL_IOS_FEED_ACTIVITY_METRICS_PROVIDER_H_

#include "components/metrics/metrics_provider.h"

class PrefService;

class IOSFeedActivityMetricsProvider : public metrics::MetricsProvider {
 public:
  explicit IOSFeedActivityMetricsProvider(PrefService* pref_service);
  IOSFeedActivityMetricsProvider(const IOSFeedActivityMetricsProvider&) =
      delete;
  IOSFeedActivityMetricsProvider& operator=(
      const IOSFeedActivityMetricsProvider&) = delete;

  ~IOSFeedActivityMetricsProvider() override;

  // metrics::MetricsProvider
  void ProvideCurrentSessionData(
      metrics::ChromeUserMetricsExtension* uma_proto) override;

 private:
  PrefService* pref_service_;
};

#endif  // IOS_CHROME_BROWSER_METRICS_MODEL_IOS_FEED_ACTIVITY_METRICS_PROVIDER_H_
