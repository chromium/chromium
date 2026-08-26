// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/tools/model/ios_page_stability_metrics.h"

#import "base/check.h"
#import "base/metrics/histogram_functions.h"

namespace actor {

#pragma mark - Lifecycle

IOSPageStabilityMetrics::IOSPageStabilityMetrics() = default;

IOSPageStabilityMetrics::~IOSPageStabilityMetrics() {
  RecordTimingMetrics();
}

#pragma mark - PageStabilityMetrics

void IOSPageStabilityMetrics::Start() {
  CHECK(start_waiting_time_.is_null());
  start_waiting_time_ = base::TimeTicks::Now();
}

void IOSPageStabilityMetrics::WillMoveToState(
    page_content_annotations::PageStabilityState state) {
  if (recorded_) {
    return;
  }

  switch (state) {
    case page_content_annotations::PageStabilityState::kInitial:
    case page_content_annotations::PageStabilityState::kMonitorStartDelay:
    case page_content_annotations::PageStabilityState::kWaitForNavigation:
      break;
    case page_content_annotations::PageStabilityState::kStartMonitoring:
      start_monitoring_time_ = base::TimeTicks::Now();
      break;
    case page_content_annotations::PageStabilityState::kMonitorCompleted:
      result_ = PageStabilityOutcome::kStable;
      break;
    case page_content_annotations::PageStabilityState::kDelayCallback:
      if (result_ == PageStabilityOutcome::kStable) {
        result_ = PageStabilityOutcome::kStableBeforeMinDelay;
      }
      break;
    case page_content_annotations::PageStabilityState::kTimeout:
      result_ = PageStabilityOutcome::kTimeout;
      break;
    case page_content_annotations::PageStabilityState::kRenderFrameGoingAway:
      result_ = PageStabilityOutcome::kWebFrameGoingAway;
      break;
    case page_content_annotations::PageStabilityState::kMojoDisconnected:
      break;
    case page_content_annotations::PageStabilityState::kInvokeCallback:
      break;
    case page_content_annotations::PageStabilityState::kDone:
      RecordTimingMetrics();
      break;
  }
}

void IOSPageStabilityMetrics::Flush() {
  RecordTimingMetrics();
}

#pragma mark - Private

void IOSPageStabilityMetrics::RecordTimingMetrics() {
  if (recorded_ || start_waiting_time_.is_null()) {
    return;
  }
  recorded_ = true;

  const base::TimeTicks now = base::TimeTicks::Now();
  const base::TimeDelta total_duration = now - start_waiting_time_;

  base::UmaHistogramEnumeration(kIOSActorPageStabilityOutcomeMetricName,
                                result_);

  switch (result_) {
    case PageStabilityOutcome::kStable:
    case PageStabilityOutcome::kStableBeforeMinDelay:
      base::UmaHistogramTimes(kIOSActorPageStabilityTotalTimeToStableMetricName,
                              total_duration);
      if (!start_monitoring_time_.is_null()) {
        base::UmaHistogramTimes(
            kIOSActorPageStabilityTimeFromMonitoringToStableMetricName,
            now - start_monitoring_time_);
      }
      break;
    case PageStabilityOutcome::kWebFrameGoingAway:
      base::UmaHistogramTimes(
          kIOSActorPageStabilityTotalTimeToWebFrameGoingAwayMetricName,
          total_duration);
      break;
    case PageStabilityOutcome::kTimeout:
      // We don't record a duration since it'll just be the timeout value.
    case PageStabilityOutcome::kUnknown:
      break;
  }
}

}  // namespace actor
