// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_METRICS_MODEL_IOS_CHROME_SIGNIN_AND_SYNC_STATUS_METRICS_PROVIDER_H_
#define IOS_CHROME_BROWSER_METRICS_MODEL_IOS_CHROME_SIGNIN_AND_SYNC_STATUS_METRICS_PROVIDER_H_

#include "build/build_config.h"
#include "components/metrics/metrics_provider.h"
#include "components/signin/core/browser/signin_status_metrics_provider_helpers.h"

namespace metrics {
class ChromeUserMetricsExtension;
}  // namespace metrics

// A simple class that regularly queries the sign-in and sync status
// of all profiles and add them to log records.
class IOSChromeSigninAndSyncStatusMetricsProvider
    : public metrics::MetricsProvider {
 public:
  IOSChromeSigninAndSyncStatusMetricsProvider();
  ~IOSChromeSigninAndSyncStatusMetricsProvider() override;

  IOSChromeSigninAndSyncStatusMetricsProvider(
      const IOSChromeSigninAndSyncStatusMetricsProvider&) = delete;
  IOSChromeSigninAndSyncStatusMetricsProvider& operator=(
      const IOSChromeSigninAndSyncStatusMetricsProvider&) = delete;

  void ProvideCurrentSessionData(
      metrics::ChromeUserMetricsExtension* uma_proto) override;

 private:
  signin_metrics::ProfilesStatus GetStatusOfAllProfiles() const;
};

#endif  // IOS_CHROME_BROWSER_METRICS_MODEL_IOS_CHROME_SIGNIN_AND_SYNC_STATUS_METRICS_PROVIDER_H_
