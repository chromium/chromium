// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web/model/web_performance_metrics/web_performance_metrics_java_script_feature_util.h"

#import <algorithm>
#import <limits>

#import "base/containers/extend.h"

namespace web_performance_metrics {

base::TimeDelta CalculateAggregateFirstContentfulPaint(
    double aggregate_absolute_first_contentful_paint,
    web_performance_metrics::FirstContentfulPaint main_frame) {
  if (aggregate_absolute_first_contentful_paint < main_frame.absolute_time) {
    // Converts the aggregate absolute iframe's first contentful paint
    // time to a relative first contenful paint time with respect to
    // the main frame's navigation start time.
    return base::Milliseconds(aggregate_absolute_first_contentful_paint -
                              main_frame.navigation_start_time);
  }
  return base::Milliseconds(main_frame.relative_time);
}

double CalculateAbsoluteFirstContentfulPaint(
    double navigation_start_time,
    double relative_first_contentful_paint) {
  return navigation_start_time + relative_first_contentful_paint;
}

// Calculates the Interaction to Next Paint metric for a frame.
// This calculation matches
// `InteractionToNextPaintCalculator::ApproximateHighPercentile` in
// `components/page_load_metrics`.
std::optional<base::TimeDelta> CalculateInteractionToNextPaint(
    std::vector<base::TimeDelta> longest_durations,
    int interaction_count) {
  if (longest_durations.empty() || interaction_count <= 0) {
    return std::nullopt;
  }

  // Sort the durations by the longest first.
  std::ranges::sort(longest_durations, std::greater<>());

  // Calculate the 98th percentile index by ignoring 1 outlier per 50
  // interactions. Clamped to 10th worse duration if we go above 500
  // interactions (kMaxInteractions * kInteractionsPerOutlier),
  // which effectively samples a higher percentile (e.g. 99th percentile
  // at 1,000 interactions).
  // From W3C Doc: "For pages with less than 50 interactions, INP is the
  // interaction with the worst latency. For pages with 50 or more
  // interactions, INP is most often the 98th percentile of interaction
  // latency, calculated by ignoring 1 outlier interaction for every 50
  // interactions."
  size_t outlier_count =
      static_cast<size_t>(interaction_count / kInteractionsPerOutlier);
  size_t index = std::min(longest_durations.size() - 1, outlier_count);
  return longest_durations[index];
}

// Calculates the Interaction to Next Paint metric across all frames.
// This calculation matches
// `InteractionToNextPaintCalculator::ApproximateHighPercentile` in
// `components/page_load_metrics`.
std::optional<base::TimeDelta> CalculateAggregateInteractionToNextPaint(
    const std::map<std::string, FrameInteractionData>& frame_interactions) {
  if (frame_interactions.empty()) {
    return std::nullopt;
  }

  // Combine all frames interaction data.
  std::vector<base::TimeDelta> all_longest_durations;
  int total_page_interactions = 0;

  for (const auto& [id, data] : frame_interactions) {
    total_page_interactions += data.interaction_count;
    base::Extend(all_longest_durations, data.longest_durations);
  }

  // Keep only the top K longest durations across all frames.
  if (all_longest_durations.size() > kMaxInteractions) {
    std::ranges::sort(all_longest_durations, std::greater<>());
    all_longest_durations.resize(kMaxInteractions);
  }

  return CalculateInteractionToNextPaint(std::move(all_longest_durations),
                                         total_page_interactions);
}

}  // namespace web_performance_metrics
