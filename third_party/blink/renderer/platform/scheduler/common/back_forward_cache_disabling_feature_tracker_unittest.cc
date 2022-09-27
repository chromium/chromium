// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/common/back_forward_cache_disabling_feature_tracker.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {
namespace scheduler {

class BackForwardCacheDisablingFeatureTrackerTest : public testing::Test {
 protected:
  TraceableVariableController* tracing_controller() {
    return &tracing_controller_;
  }

 private:
  TraceableVariableController tracing_controller_;
};

TEST_F(BackForwardCacheDisablingFeatureTrackerTest, AddAndRemove) {
  BackForwardCacheDisablingFeatureTracker tracker(tracing_controller(),
                                                  nullptr);

  EXPECT_THAT(tracker.GetActiveFeaturesTrackedForBackForwardCacheMetrics(),
              testing::UnorderedElementsAre());
  EXPECT_THAT(tracker.GetActiveFeaturesTrackedForBackForwardCacheMetricsMask(),
              0);

  // Add kWebSocket.
  tracker.Add(SchedulingPolicy::Feature::kWebSocket);
  EXPECT_THAT(
      tracker.GetActiveFeaturesTrackedForBackForwardCacheMetrics(),
      testing::UnorderedElementsAre(SchedulingPolicy::Feature::kWebSocket));
  EXPECT_THAT(
      tracker.GetActiveFeaturesTrackedForBackForwardCacheMetricsMask(),
      1 << static_cast<uint64_t>(SchedulingPolicy::Feature::kWebSocket));

  // Add kWebRTC.
  tracker.Add(SchedulingPolicy::Feature::kWebRTC);
  EXPECT_THAT(
      tracker.GetActiveFeaturesTrackedForBackForwardCacheMetrics(),
      testing::UnorderedElementsAre(SchedulingPolicy::Feature::kWebSocket,
                                    SchedulingPolicy::Feature::kWebRTC));
  EXPECT_THAT(
      tracker.GetActiveFeaturesTrackedForBackForwardCacheMetricsMask(),
      (1 << static_cast<uint64_t>(SchedulingPolicy::Feature::kWebSocket)) |
          (1 << static_cast<uint64_t>(SchedulingPolicy::Feature::kWebRTC)));

  // Add kWebSocket again.
  tracker.Add(SchedulingPolicy::Feature::kWebSocket);
  EXPECT_THAT(
      tracker.GetActiveFeaturesTrackedForBackForwardCacheMetrics(),
      testing::UnorderedElementsAre(SchedulingPolicy::Feature::kWebSocket,
                                    SchedulingPolicy::Feature::kWebRTC));
  EXPECT_THAT(
      tracker.GetActiveFeaturesTrackedForBackForwardCacheMetricsMask(),
      (1 << static_cast<uint64_t>(SchedulingPolicy::Feature::kWebSocket)) |
          (1 << static_cast<uint64_t>(SchedulingPolicy::Feature::kWebRTC)));

  // Remove kWebRTC.
  tracker.Remove(SchedulingPolicy::Feature::kWebRTC);
  EXPECT_THAT(
      tracker.GetActiveFeaturesTrackedForBackForwardCacheMetrics(),
      testing::UnorderedElementsAre(SchedulingPolicy::Feature::kWebSocket));
  EXPECT_THAT(
      tracker.GetActiveFeaturesTrackedForBackForwardCacheMetricsMask(),
      1 << static_cast<uint64_t>(SchedulingPolicy::Feature::kWebSocket));

  // Remove kWebSocket. As kWebSocket was added twice, removing it once doesn't
  // change the metrics result.
  tracker.Remove(SchedulingPolicy::Feature::kWebSocket);
  EXPECT_THAT(
      tracker.GetActiveFeaturesTrackedForBackForwardCacheMetrics(),
      testing::UnorderedElementsAre(SchedulingPolicy::Feature::kWebSocket));
  EXPECT_THAT(
      tracker.GetActiveFeaturesTrackedForBackForwardCacheMetricsMask(),
      1 << static_cast<uint64_t>(SchedulingPolicy::Feature::kWebSocket));

  // Remove kWebSocket again.
  tracker.Remove(SchedulingPolicy::Feature::kWebSocket);
  EXPECT_THAT(tracker.GetActiveFeaturesTrackedForBackForwardCacheMetrics(),
              testing::UnorderedElementsAre());
  EXPECT_THAT(tracker.GetActiveFeaturesTrackedForBackForwardCacheMetricsMask(),
              0);
}

}  // namespace scheduler
}  // namespace blink
