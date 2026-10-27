// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/test/perf/scroll_jank_metrics_util.h"

#include <cstdint>
#include <vector>

#include "testing/gtest/include/gtest/gtest.h"

namespace ios_perf {
namespace {

constexpr int64_t kMsToNs = 1'000'000;

// Appends `frame_count` frames spaced by `interval_ns` with `work_ms` per frame
// to `frames`, advancing `current_ts_ns`.
void AppendFrames(std::vector<DisplayFrameSample>& frames,
                  int64_t& current_ts_ns,
                  size_t frame_count,
                  int64_t interval_ns,
                  double work_ms) {
  for (size_t i = 0; i < frame_count; ++i) {
    if (!frames.empty()) {
      current_ts_ns += interval_ns;
    }
    frames.push_back({current_ts_ns, work_ms});
  }
}

// Tests that `DeduplicateDisplayFrames` coalesces consecutive frames separated
// by less than 4 ms and sums their main-thread work.
TEST(ScrollJankMetricsUtilTest,
     DeduplicateDisplayFramesCoalescesSameVsyncCommits) {
  const std::vector<DisplayFrameSample> raw_frames = {
      {100 * kMsToNs, 1.2},
      // Exact same timestamp (0 ms delta): coalesced into first frame.
      {100 * kMsToNs, 0.8},
      // 2 microseconds later (0.002 ms < 4.0 ms): coalesced into first frame.
      {100 * kMsToNs + 2'000, 0.5},
      // 8.333 ms later (120 Hz VSync): distinct frame.
      {108'333'000, 1.5},
      // 3.9 ms later (< 4.0 ms): coalesced into second frame.
      {112'233'000, 0.4},
      // 16.667 ms after second frame's timestamp: distinct frame.
      {125'000'000, 2.0},
  };

  const std::vector<DisplayFrameSample> dedup =
      DeduplicateDisplayFrames(raw_frames);
  ASSERT_EQ(dedup.size(), 3u);
  EXPECT_EQ(dedup[0].display_ts_ns, 100 * kMsToNs);
  EXPECT_NEAR(dedup[0].main_thread_work_ms, 2.5, 1e-6);
  EXPECT_EQ(dedup[1].display_ts_ns, 108'333'000);
  EXPECT_NEAR(dedup[1].main_thread_work_ms, 1.9, 1e-6);
  EXPECT_EQ(dedup[2].display_ts_ns, 125'000'000);
  EXPECT_NEAR(dedup[2].main_thread_work_ms, 2.0, 1e-6);
}

// Tests that `ComputeScrollJankSwipeMetrics` splits swipes at idle gaps > 180
// ms and discards short inter-swipe scrollbar fade-out animations (< 20
// frames).
TEST(ScrollJankMetricsUtilTest,
     ComputeScrollJankSwipeMetricsSegmentsSwipesAndFiltersShortAnimations) {
  std::vector<DisplayFrameSample> frames;
  int64_t ts_ns = 1'000 * kMsToNs;

  // Swipe 1: 30 frames at 120 Hz (8.333 ms), work = 1.2 ms/frame.
  AppendFrames(frames, ts_ns, 30, 8'333'000, 1.2);

  // Inter-swipe gap (500 ms) followed by a 12-frame scrollbar fade-out at 60 Hz
  // (which must be filtered out because 12 < kMinFramesPerSwipe).
  ts_ns += 500 * kMsToNs;
  frames.push_back({ts_ns, 0.5});
  AppendFrames(frames, ts_ns, 11, 16'667'000, 0.5);

  // Another inter-swipe gap (300 ms), then Swipe 2: 25 frames at 60 Hz (16.667
  // ms), work = 2.4 ms/frame.
  ts_ns += 300 * kMsToNs;
  frames.push_back({ts_ns, 2.4});
  AppendFrames(frames, ts_ns, 24, 16'667'000, 2.4);

  const std::vector<SwipeScrollJankMetrics> swipes =
      ComputeScrollJankSwipeMetrics(frames);
  ASSERT_EQ(swipes.size(), 2u);

  EXPECT_DOUBLE_EQ(swipes[0].hitch_time_ratio_ms_per_s, 0.0);
  EXPECT_NEAR(swipes[0].frame_interval_p95_ms, 8.333, 0.01);
  EXPECT_NEAR(swipes[0].main_thread_frame_work_ms, 1.2, 1e-6);

  EXPECT_DOUBLE_EQ(swipes[1].hitch_time_ratio_ms_per_s, 0.0);
  EXPECT_NEAR(swipes[1].frame_interval_p95_ms, 16.667, 0.01);
  EXPECT_NEAR(swipes[1].main_thread_frame_work_ms, 2.4, 1e-6);
}

// Tests that ProMotion refresh-rate step-downs (120 Hz -> 80 Hz -> 60 Hz)
// during scroll deceleration produce zero hitch time ratio.
TEST(ScrollJankMetricsUtilTest,
     ComputeScrollJankSwipeMetricsIgnoresProMotionStepDowns) {
  std::vector<DisplayFrameSample> frames;
  int64_t ts_ns = 1'000 * kMsToNs;

  AppendFrames(frames, ts_ns, 15, 8'333'000, 1.0);
  AppendFrames(frames, ts_ns, 10, 12'500'000, 1.0);
  AppendFrames(frames, ts_ns, 15, 16'667'000, 1.0);

  const std::vector<SwipeScrollJankMetrics> swipes =
      ComputeScrollJankSwipeMetrics(frames);
  ASSERT_EQ(swipes.size(), 1u);
  EXPECT_DOUBLE_EQ(swipes[0].hitch_time_ratio_ms_per_s, 0.0);
  EXPECT_NEAR(swipes[0].frame_interval_p95_ms, 16.667, 0.01);
  EXPECT_NEAR(swipes[0].main_thread_frame_work_ms, 1.0, 1e-6);
}

// Tests that periodic 75 ms main-thread stalls move
// `hitch_time_ratio_ms_per_s`, `frame_interval_p95_ms`, and
// `main_thread_frame_work_ms` even when stalled commits emit duplicate
// same-VSync catch-up slices.
TEST(ScrollJankMetricsUtilTest,
     ComputeScrollJankSwipeMetricsDetectsStallsAndCoalescesCatchUpCommits) {
  std::vector<DisplayFrameSample> frames;
  int64_t ts_ns = 1'000 * kMsToNs;
  frames.push_back({ts_ns, 1.5});

  // 24 deduplicated intervals: every 4th interval is a 75 ms stall with 70 ms
  // of work plus a same-VSync catch-up slice 2 microseconds later.
  for (int i = 1; i <= 24; ++i) {
    if (i % 4 == 0) {
      ts_ns += 75 * kMsToNs;
      frames.push_back({ts_ns, 70.0});
      // Same-VSync catch-up slice 2 microseconds later.
      frames.push_back({ts_ns + 2'000, 2.0});
    } else {
      ts_ns += 8'333'000;
      frames.push_back({ts_ns, 1.5});
    }
  }

  const std::vector<SwipeScrollJankMetrics> swipes =
      ComputeScrollJankSwipeMetrics(frames);
  ASSERT_EQ(swipes.size(), 1u);
  EXPECT_GT(swipes[0].hitch_time_ratio_ms_per_s, 100.0);
  EXPECT_NEAR(swipes[0].frame_interval_p95_ms, 75.0, 0.01);
  EXPECT_GT(swipes[0].main_thread_frame_work_ms, 15.0);
}

// Tests that when `max_swipes` is specified and an inter-swipe bounce/fade
// animation slightly exceeds `kMinFramesPerSwipe`, the shortest segment by
// frame count is discarded and the real swipes are preserved in order.
TEST(ScrollJankMetricsUtilTest,
     ComputeScrollJankSwipeMetricsKeepsLongestSwipesWhenMaxSwipesSpecified) {
  std::vector<DisplayFrameSample> frames;
  int64_t ts_ns = 1'000 * kMsToNs;

  // Swipe 1: 80 frames, work = 1.1 ms.
  AppendFrames(frames, ts_ns, 80, 16'667'000, 1.1);

  // Inter-swipe bounce animation: 21 frames, work = 0.05 ms.
  ts_ns += 400 * kMsToNs;
  frames.push_back({ts_ns, 0.05});
  AppendFrames(frames, ts_ns, 20, 16'667'000, 0.05);

  // Swipe 2: 75 frames, work = 1.4 ms.
  ts_ns += 400 * kMsToNs;
  frames.push_back({ts_ns, 1.4});
  AppendFrames(frames, ts_ns, 74, 16'667'000, 1.4);

  const std::vector<SwipeScrollJankMetrics> swipes =
      ComputeScrollJankSwipeMetrics(frames, /*max_swipes=*/2);
  ASSERT_EQ(swipes.size(), 2u);
  EXPECT_NEAR(swipes[0].main_thread_frame_work_ms, 1.1, 1e-6);
  EXPECT_NEAR(swipes[1].main_thread_frame_work_ms, 1.4, 1e-6);
}

}  // namespace
}  // namespace ios_perf
