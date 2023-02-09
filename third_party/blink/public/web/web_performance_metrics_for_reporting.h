// Copyright 2010 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_PERFORMANCE_METRICS_FOR_REPORTING_H_
#define THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_PERFORMANCE_METRICS_FOR_REPORTING_H_

#include "base/time/time.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/performance/largest_contentful_paint_type.h"
#include "third_party/blink/public/platform/web_common.h"
#include "third_party/blink/public/platform/web_private_ptr.h"
#include "third_party/blink/public/platform/web_url_request.h"
#include "third_party/blink/public/platform/web_vector.h"
#include "third_party/blink/public/web/web_navigation_type.h"

namespace blink {

class WindowPerformance;

// This class is used for reporting purposes (e.g. ukm) of non-web-exposed
// metrics.
class BLINK_EXPORT WebPerformanceMetricsForReporting {
 public:
  // The count to record the times on requestAnimationFrame after the page is
  // restored from the back-forward cache.
  static constexpr int
      kRequestAnimationFramesToRecordAfterBackForwardCacheRestore = 3;

  struct BackForwardCacheRestoreTiming {
    double navigation_start = 0;
    double first_paint = 0;
    std::array<double,
               kRequestAnimationFramesToRecordAfterBackForwardCacheRestore>
        request_animation_frames = {};
    absl::optional<base::TimeDelta> first_input_delay;
  };

  using BackForwardCacheRestoreTimings =
      WebVector<BackForwardCacheRestoreTiming>;

  ~WebPerformanceMetricsForReporting() { Reset(); }

  WebPerformanceMetricsForReporting() = default;

  WebPerformanceMetricsForReporting(
      const WebPerformanceMetricsForReporting& p) {
    Assign(p);
  }

  WebPerformanceMetricsForReporting& operator=(
      const WebPerformanceMetricsForReporting& p) {
    Assign(p);
    return *this;
  }

  void Reset();
  void Assign(const WebPerformanceMetricsForReporting&);

  // This only returns one of {Other|Reload|BackForward}.
  // Form submits and link clicks all fall under other.
  WebNavigationType GetNavigationType() const;

  // These functions return time in seconds (not milliseconds) since the epoch.
  double InputForNavigationStart() const;
  double NavigationStart() const;
  base::TimeTicks NavigationStartAsMonotonicTime() const;
  BackForwardCacheRestoreTimings BackForwardCacheRestore() const;
  double ResponseStart() const;
  double DomContentLoadedEventStart() const;
  double DomContentLoadedEventEnd() const;
  double LoadEventStart() const;
  double LoadEventEnd() const;
  double FirstPaint() const;
  double FirstImagePaint() const;
  double FirstContentfulPaint() const;
  base::TimeTicks FirstContentfulPaintAsMonotonicTime() const;
  base::TimeTicks FirstContentfulPaintRenderedButNotPresentedAsMonotonicTime()
      const;
  double FirstMeaningfulPaint() const;
  double LargestImagePaintForMetrics() const;
  uint64_t LargestImagePaintSizeForMetrics() const;
  absl::optional<base::TimeDelta> LargestContentfulPaintImageLoadStart() const;
  absl::optional<base::TimeDelta> LargestContentfulPaintImageLoadEnd() const;
  double LargestTextPaintForMetrics() const;
  uint64_t LargestTextPaintSizeForMetrics() const;
  base::TimeTicks LargestContentfulPaintAsMonotonicTimeForMetrics() const;
  double ExperimentalLargestImagePaint() const;
  uint64_t ExperimentalLargestImagePaintSize() const;
  blink::LargestContentfulPaintType LargestContentfulPaintTypeForMetrics()
      const;
  double LargestContentfulPaintImageBPPForMetrics() const;
  absl::optional<WebURLRequest::Priority>
  LargestContentfulPaintImageRequestPriorityForMetrics() const;
  double ExperimentalLargestTextPaint() const;
  uint64_t ExperimentalLargestTextPaintSize() const;
  double FirstEligibleToPaint() const;
  double FirstInputOrScrollNotifiedTimestamp() const;
  absl::optional<base::TimeDelta> FirstInputDelay() const;
  absl::optional<base::TimeDelta> FirstInputTimestamp() const;
  absl::optional<base::TimeTicks> FirstInputTimestampAsMonotonicTime() const;
  absl::optional<base::TimeDelta> LongestInputDelay() const;
  absl::optional<base::TimeDelta> LongestInputTimestamp() const;
  absl::optional<base::TimeDelta> FirstInputProcessingTime() const;
  absl::optional<base::TimeDelta> FirstScrollDelay() const;
  absl::optional<base::TimeDelta> FirstScrollTimestamp() const;
  double ParseStart() const;
  double ParseStop() const;
  double ParseBlockedOnScriptLoadDuration() const;
  double ParseBlockedOnScriptLoadFromDocumentWriteDuration() const;
  double ParseBlockedOnScriptExecutionDuration() const;
  double ParseBlockedOnScriptExecutionFromDocumentWriteDuration() const;
  absl::optional<base::TimeTicks> LastPortalActivatedPaint() const;
  absl::optional<base::TimeDelta> PrerenderActivationStart() const;
  absl::optional<base::TimeDelta> UserTimingMarkFullyLoaded() const;
  absl::optional<base::TimeDelta> UserTimingMarkFullyVisible() const;
  absl::optional<base::TimeDelta> UserTimingMarkInteractive() const;

#if INSIDE_BLINK
  explicit WebPerformanceMetricsForReporting(WindowPerformance*);
  WebPerformanceMetricsForReporting& operator=(WindowPerformance*);
#endif

 private:
  WebPrivatePtr<WindowPerformance> private_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_PERFORMANCE_METRICS_FOR_REPORTING_H_
