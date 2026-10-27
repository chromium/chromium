// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_TEST_PERF_SCROLL_JANK_METRICS_UTIL_H_
#define IOS_CHROME_TEST_PERF_SCROLL_JANK_METRICS_UTIL_H_

#include <cstddef>
#include <cstdint>
#include <vector>

#include "base/containers/span.h"

namespace ios_perf {

// Minimum interval (in ms) between distinct `Display` scanout events. Commits
// that drop frames have their estimated `presentationTime` shifted forward by
// `droppedFrames * framePeriod`, landing on the same target VSync as the next
// catch-up commit (`< 4.0 ms` apart, less than half of a 120 Hz VSync).
// Coalescing frames `< 4.0 ms` apart prevents same-VSync duplicate slices from
// skewing frame intervals downward while summing their main-thread work.
inline constexpr double kMinDistinctDisplayIntervalMs = 4.0;

// Maximum interval (in ms) between consecutive deduplicated `Display` frames
// within a single active scroll swipe before treating a gap as an inter-swipe
// EarlGrey idle pause (~250-1200 ms).
inline constexpr double kMaxActiveScrollIntervalMs = 180.0;

// Minimum number of deduplicated frames required to treat a contiguous active
// segment as a scroll swipe (filtering out short scrollbar fade-out animations
// between swipes).
inline constexpr size_t kMinFramesPerSwipe = 20;

// Threshold (in ms) above which a frame interval is always a multi-VSync hitch
// (> 2 VSyncs at 60 Hz / > 4 VSyncs at 120 Hz), unaffected by ProMotion
// 120/80/60 Hz refresh-rate step-downs during scroll deceleration.
inline constexpr double kMultiVsyncHitchThresholdMs = 34.0;

// Standard 60 Hz frame budget (in ms) subtracted from multi-VSync hitch
// intervals when computing excess hitch time.
inline constexpr double kBaselineFrameBudgetMs = 1000.0 / 60.0;

// Raw frame sample extracted from a `SceneViewController` `Display` slice and
// its flow-linked `CADisplayLinkDispatch` and `CATransactionCommit` slices.
struct DisplayFrameSample {
  int64_t display_ts_ns = 0;
  double main_thread_work_ms = 0.0;
};

// Per-swipe scroll jank summary metrics computed over a single segmented swipe.
struct SwipeScrollJankMetrics {
  double hitch_time_ratio_ms_per_s = 0.0;
  double frame_interval_p95_ms = 0.0;
  double main_thread_frame_work_ms = 0.0;
};

// Coalesces consecutive `DisplayFrameSample` entries whose `display_ts_ns`
// timestamps are separated by less than `kMinDistinctDisplayIntervalMs` (4 ms),
// retaining the first timestamp on that VSync and summing
// `main_thread_work_ms`. Input `raw_frames` must be sorted by `display_ts_ns`.
std::vector<DisplayFrameSample> DeduplicateDisplayFrames(
    base::span<const DisplayFrameSample> raw_frames);

// Deduplicates `raw_frames`, segments them into active scroll swipes where
// consecutive frame intervals are `<= kMaxActiveScrollIntervalMs` (180 ms) with
// at least `kMinFramesPerSwipe` (20) frames, and computes
// `SwipeScrollJankMetrics` for each swipe. If `max_swipes > 0` and more than
// `max_swipes` candidate segments remain (e.g., an inter-swipe bounce/fade
// animation slightly exceeding `kMinFramesPerSwipe`), the shortest segments by
// frame count are discarded, preserving the `max_swipes` longest swipes in
// chronological order.
std::vector<SwipeScrollJankMetrics> ComputeScrollJankSwipeMetrics(
    base::span<const DisplayFrameSample> raw_frames,
    size_t max_swipes = 0);

}  // namespace ios_perf

#endif  // IOS_CHROME_TEST_PERF_SCROLL_JANK_METRICS_UTIL_H_
