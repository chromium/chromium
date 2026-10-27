// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/test/perf/scroll_jank_metrics_util.h"

#include <algorithm>
#include <utility>

namespace ios_perf {

std::vector<DisplayFrameSample> DeduplicateDisplayFrames(
    base::span<const DisplayFrameSample> raw_frames) {
  std::vector<DisplayFrameSample> dedup_frames;
  dedup_frames.reserve(raw_frames.size());
  for (const DisplayFrameSample& frame : raw_frames) {
    if (!dedup_frames.empty()) {
      const double delta_ms =
          (frame.display_ts_ns - dedup_frames.back().display_ts_ns) / 1e6;
      if (delta_ms < kMinDistinctDisplayIntervalMs) {
        dedup_frames.back().main_thread_work_ms += frame.main_thread_work_ms;
        continue;
      }
    }
    dedup_frames.push_back(frame);
  }
  return dedup_frames;
}

std::vector<SwipeScrollJankMetrics> ComputeScrollJankSwipeMetrics(
    base::span<const DisplayFrameSample> raw_frames,
    size_t max_swipes) {
  const std::vector<DisplayFrameSample> dedup_frames =
      DeduplicateDisplayFrames(raw_frames);

  std::vector<std::vector<DisplayFrameSample>> swipes;
  std::vector<DisplayFrameSample> current_swipe;
  for (const DisplayFrameSample& frame : dedup_frames) {
    if (!current_swipe.empty()) {
      const double interval_ms =
          (frame.display_ts_ns - current_swipe.back().display_ts_ns) / 1e6;
      if (interval_ms > kMaxActiveScrollIntervalMs) {
        if (current_swipe.size() >= kMinFramesPerSwipe) {
          swipes.push_back(std::move(current_swipe));
        }
        current_swipe.clear();
      }
    }
    current_swipe.push_back(frame);
  }
  if (current_swipe.size() >= kMinFramesPerSwipe) {
    swipes.push_back(std::move(current_swipe));
  }

  // If an inter-swipe bounce or scrollbar fade animation slightly exceeded
  // `kMinFramesPerSwipe`, discard the shortest candidate segment(s) by frame
  // count so the `max_swipes` real scroll swipes remain in chronological order.
  while (max_swipes > 0 && swipes.size() > max_swipes) {
    auto shortest_it = std::min_element(
        swipes.begin(), swipes.end(),
        [](const auto& a, const auto& b) { return a.size() < b.size(); });
    swipes.erase(shortest_it);
  }

  std::vector<SwipeScrollJankMetrics> results;
  results.reserve(swipes.size());
  for (const auto& swipe : swipes) {
    double total_work_ms = 0.0;
    for (const DisplayFrameSample& frame : swipe) {
      total_work_ms += frame.main_thread_work_ms;
    }

    std::vector<double> intervals_ms;
    intervals_ms.reserve(swipe.size() - 1);
    double total_duration_ms = 0.0;
    double excess_hitch_ms = 0.0;
    for (size_t i = 1; i < swipe.size(); ++i) {
      const double dt_ms =
          (swipe[i].display_ts_ns - swipe[i - 1].display_ts_ns) / 1e6;
      intervals_ms.push_back(dt_ms);
      total_duration_ms += dt_ms;
      if (dt_ms > kMultiVsyncHitchThresholdMs) {
        excess_hitch_ms += (dt_ms - kBaselineFrameBudgetMs);
      }
    }

    std::sort(intervals_ms.begin(), intervals_ms.end());
    const size_t p95_index =
        static_cast<size_t>(0.95 * (intervals_ms.size() - 1));

    SwipeScrollJankMetrics metrics;
    metrics.hitch_time_ratio_ms_per_s =
        total_duration_ms > 0.0 ? (1000.0 * excess_hitch_ms / total_duration_ms)
                                : 0.0;
    metrics.frame_interval_p95_ms = intervals_ms[p95_index];
    metrics.main_thread_frame_work_ms = total_work_ms / swipe.size();
    results.push_back(metrics);
  }

  return results;
}

}  // namespace ios_perf
