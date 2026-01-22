// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/metrics/model/mobile_session_shutdown_metrics_provider.h"

#import "base/metrics/histogram_macros.h"
#import "components/metrics/metrics_service.h"
#import "components/previous_session_info/previous_session_info.h"

MobileSessionShutdownMetricsProvider::MobileSessionShutdownMetricsProvider(
    metrics::MetricsService* metrics_service)
    : metrics_service_(metrics_service) {
  DCHECK(metrics_service_);
}

MobileSessionShutdownMetricsProvider::~MobileSessionShutdownMetricsProvider() {}

bool MobileSessionShutdownMetricsProvider::HasPreviousSessionData() {
  return true;
}

void MobileSessionShutdownMetricsProvider::ProvidePreviousSessionData(
    metrics::ChromeUserMetricsExtension* uma_proto) {
  if (IsFirstLaunchAfterUpgrade()) {
    return;
  }

  PreviousSessionInfo* session_info = [PreviousSessionInfo sharedInstance];
  NSInteger allTabCount = session_info.tabCount + session_info.OTRTabCount;

  if (metrics_service_->WasLastShutdownClean()) {
    UMA_STABILITY_HISTOGRAM_COUNTS_100(
        "Stability.iOS.TabCountBeforeCleanShutdown", allTabCount);
  } else {
    UMA_STABILITY_HISTOGRAM_COUNTS_100("Stability.iOS.TabCountBeforeCrash",
                                       allTabCount);
  }
}

bool MobileSessionShutdownMetricsProvider::IsFirstLaunchAfterUpgrade() {
  return [[PreviousSessionInfo sharedInstance] isFirstSessionAfterUpgrade];
}
