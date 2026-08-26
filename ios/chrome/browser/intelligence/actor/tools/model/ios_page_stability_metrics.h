// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TOOLS_MODEL_IOS_PAGE_STABILITY_METRICS_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TOOLS_MODEL_IOS_PAGE_STABILITY_METRICS_H_

#import <string_view>

#import "base/time/time.h"
#import "components/actor/core/page_stability_metrics.h"
#import "components/page_content_annotations/core/page_stability_state.h"

namespace actor {

// Terminal outcome for the page stability check on iOS.
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(IOSPageStabilityOutcome)
enum class PageStabilityOutcome {
  // The outcome is not yet known or could not be determined.
  kUnknown = 0,
  // Stability was reached, and the callback was executed immediately afterward.
  kStable = 1,
  // Stability was reached, but the callback execution was delayed to satisfy
  // the minimum wait time requirement.
  kStableBeforeMinDelay = 2,
  // Monitoring timed out before stability was reached.
  kTimeout = 3,
  // The monitored web frame was destroyed before stability was reached.
  kWebFrameGoingAway = 4,
  kMaxValue = kWebFrameGoingAway,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/ios/enums.xml:IOSPageStabilityOutcome)

// Histogram recording the final stability check outcome.
inline constexpr std::string_view kIOSActorPageStabilityOutcomeMetricName =
    "IOS.Actor.PageStability.Outcome";

// Histogram recording the total time to reach stability.
inline constexpr std::string_view
    kIOSActorPageStabilityTotalTimeToStableMetricName =
        "IOS.Actor.PageStability.TotalTimeToStable";

// Histogram recording the time from active monitoring to stability.
inline constexpr std::string_view
    kIOSActorPageStabilityTimeFromMonitoringToStableMetricName =
        "IOS.Actor.PageStability.TimeFromMonitoringToStable";

// Histogram recording total time until frame destruction.
inline constexpr std::string_view
    kIOSActorPageStabilityTotalTimeToWebFrameGoingAwayMetricName =
        "IOS.Actor.PageStability.TotalTimeToWebFrameGoingAway";

// iOS-specific implementation of `PageStabilityMetrics` that records
// timing metrics and stability outcomes.
class IOSPageStabilityMetrics : public PageStabilityMetrics {
 public:
  IOSPageStabilityMetrics();
  ~IOSPageStabilityMetrics() override;

  IOSPageStabilityMetrics(const IOSPageStabilityMetrics&) = delete;
  IOSPageStabilityMetrics& operator=(const IOSPageStabilityMetrics&) = delete;

  // PageStabilityMetrics:
  void Start() override;
  void WillMoveToState(
      page_content_annotations::PageStabilityState state) override;
  void Flush() override;

 private:
  // Records the timing metrics to UMA based on the calculated outcome.
  void RecordTimingMetrics();

  base::TimeTicks start_waiting_time_;
  base::TimeTicks start_monitoring_time_;
  PageStabilityOutcome result_ = PageStabilityOutcome::kUnknown;
  bool recorded_ = false;
};

}  // namespace actor

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TOOLS_MODEL_IOS_PAGE_STABILITY_METRICS_H_
