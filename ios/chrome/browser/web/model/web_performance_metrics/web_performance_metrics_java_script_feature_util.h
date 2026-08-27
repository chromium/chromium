// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEB_MODEL_WEB_PERFORMANCE_METRICS_WEB_PERFORMANCE_METRICS_JAVA_SCRIPT_FEATURE_UTIL_H_
#define IOS_CHROME_BROWSER_WEB_MODEL_WEB_PERFORMANCE_METRICS_WEB_PERFORMANCE_METRICS_JAVA_SCRIPT_FEATURE_UTIL_H_

#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/time/time.h"

namespace web_performance_metrics {

// Metric names.
inline constexpr std::string_view kFirstContentfulPaintMetric =
    "FirstContentfulPaint";
inline constexpr std::string_view kFirstInputDelayMetric = "FirstInputDelay";
inline constexpr std::string_view kInteractionToNextPaintMetric =
    "InteractionToNextPaint";

// Message dictionary keys.
inline constexpr std::string_view kMetricKey = "metric";
inline constexpr std::string_view kValueKey = "value";
inline constexpr std::string_view kFrameNavigationStartTimeKey =
    "frameNavigationStartTime";
inline constexpr std::string_view kCachedKey = "cached";
inline constexpr std::string_view kDurationsKey = "durations";
inline constexpr std::string_view kFrameIdKey = "frameId";
inline constexpr std::string_view kInteractionCountKey = "interactionCount";

// Histogram names for Interaction to Next Paint (INP).
inline constexpr std::string_view kInteractionToNextPaintMainFrameHistogram =
    "IOS.Frame.InteractionToNextPaint.MainFrame";
inline constexpr std::string_view kInteractionToNextPaintSubFrameHistogram =
    "IOS.Frame.InteractionToNextPaint.SubFrame";
inline constexpr std::string_view kAggregateInteractionToNextPaintHistogram =
    "PageLoad.InteractiveTiming.UserInteractionLatency.HighPercentile2."
    "MaxEventDuration";

// Histogram constants for First Contentful Paint (FCP).
inline constexpr base::TimeDelta kTimeRangePaintHistogramMin =
    base::Milliseconds(10);
inline constexpr base::TimeDelta kTimeRangePaintHistogramMax =
    base::Minutes(10);
inline constexpr int kTimeRangePaintHistogramBucketCount = 100;

// Histogram constants for user interaction timing metrics (FID and INP).
inline constexpr base::TimeDelta kTimeRangeInteractionTimingHistogramMin =
    base::Milliseconds(1);
inline constexpr base::TimeDelta kTimeRangeInteractionTimingHistogramMax =
    base::Seconds(60);
inline constexpr int kTimeRangeInteractionTimingHistogramBucketCount = 50;

// Frequency of user interactions used to approximate the 98th percentile for
// Interaction to Next Paint (INP). For every 50 interactions, 1 worst-case
// outlier is ignored, which corresponds to the 98th percentile (1/50 = 2%).
// For pages with fewer than 50 interactions, the worst interaction (index 0)
// is reported. Corresponds to `kHighPercentileUpdateFrequency` in
// `components/page_load_metrics`.
inline constexpr int kInteractionsPerOutlier = 50;

// Maximum number of merged worst interactions across all frames to keep.
// A cap of 10 worst interaction durations yields the exact 98th percentile for
// up to 500 interactions per page (10 * `kInteractionsPerOutlier`). For pages
// with over 500 interactions, capping to the 10th worst duration effectively
// samples a slightly higher percentile, providing a conservative approximation
// without consuming unbounded memory. Corresponds to `kMaxInteractions` in
// `components/page_load_metrics`.
inline constexpr size_t kMaxInteractions = 10;

struct FirstContentfulPaint {
  // The time at which the frame started loading.
  double navigation_start_time;
  // The first contentful paint time relative to the frame's
  // navigation start time.
  double relative_time;
  // The sum of the frame's navigation start time and its
  // first contentful paint.
  double absolute_time;

  FirstContentfulPaint(double start_time = std::numeric_limits<double>::max(),
                       double r_time = std::numeric_limits<double>::max(),
                       double a_time = std::numeric_limits<double>::max())
      : navigation_start_time(start_time),
        relative_time(r_time),
        absolute_time(a_time) {}
};

// Stores interaction timing data reported by a frame.
struct FrameInteractionData {
  // The list of the longest interaction durations in this frame.
  std::vector<base::TimeDelta> longest_durations;
  // The total number of interactions in this frame.
  int interaction_count = 0;
  // Whether the frame is the main frame.
  bool is_main_frame = false;
};

// The function calculates the First Contentful Paint
// across main and subframes that
// occurred at the earliest point in time relative to the
// main frame's navigation start time.
base::TimeDelta CalculateAggregateFirstContentfulPaint(
    double aggregate_absolute_first_contentful_paint,
    FirstContentfulPaint main_frame);

// The function calculates the absolute first contentful paint time
// relative to the given frame's navigation start time.
double CalculateAbsoluteFirstContentfulPaint(
    double navigation_start_time,
    double relative_first_contentful_paint);

// Calculates Interaction to Next Paint (INP) for a frame.
std::optional<base::TimeDelta> CalculateInteractionToNextPaint(
    std::vector<base::TimeDelta> longest_durations,
    int interaction_count);

// Calculates the page-wide aggregate Interaction to Next Paint (INP)
// by merging the per-frame interaction data.
std::optional<base::TimeDelta> CalculateAggregateInteractionToNextPaint(
    const std::map<std::string, FrameInteractionData>& frame_interactions);

}  // namespace web_performance_metrics

#endif  // IOS_CHROME_BROWSER_WEB_MODEL_WEB_PERFORMANCE_METRICS_WEB_PERFORMANCE_METRICS_JAVA_SCRIPT_FEATURE_UTIL_H_
