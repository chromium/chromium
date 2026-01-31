// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_METRICS_MODEL_MOBILE_SESSION_SHUTDOWN_METRICS_PROVIDER_H_
#define IOS_CHROME_BROWSER_METRICS_MODEL_MOBILE_SESSION_SHUTDOWN_METRICS_PROVIDER_H_

#import "base/memory/raw_ptr.h"
#import "components/metrics/metrics_provider.h"

namespace metrics {
class MetricsService;
}

// Provides metrics regarding the previous session's shutdown type that can be
// logged immediately on startup (e.g., tab counts).
// Metrics that require checking for crash reports (which may involve blocking
// file operations) are handled by MobileSessionCrashHelperMetricsProvider.
class MobileSessionShutdownMetricsProvider : public metrics::MetricsProvider {
 public:
  explicit MobileSessionShutdownMetricsProvider(
      metrics::MetricsService* metrics_service);

  MobileSessionShutdownMetricsProvider(
      const MobileSessionShutdownMetricsProvider&) = delete;
  MobileSessionShutdownMetricsProvider& operator=(
      const MobileSessionShutdownMetricsProvider&) = delete;

  ~MobileSessionShutdownMetricsProvider() override;

  // metrics::MetricsProvider implementation.
  bool HasPreviousSessionData() override;
  void ProvidePreviousSessionData(
      metrics::ChromeUserMetricsExtension* uma_proto) override;

 protected:
  // Returns true if the last session was a first launch after an upgrade.
  virtual bool IsFirstLaunchAfterUpgrade();

 private:
  raw_ptr<metrics::MetricsService> metrics_service_;
};

#endif  // IOS_CHROME_BROWSER_METRICS_MODEL_MOBILE_SESSION_SHUTDOWN_METRICS_PROVIDER_H_
