// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/common/back_forward_cache_disabling_feature_tracker.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#include "third_party/blink/renderer/platform/bindings/source_location.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/frame_scheduler_impl.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/page_scheduler_impl.h"
#include "third_party/blink/renderer/platform/scheduler/public/frame_or_worker_scheduler.h"

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
  SchedulingPolicy::Feature feature_socket =
      SchedulingPolicy::Feature::kWebSocket;
  SchedulingPolicy::Feature feature_webrtc = SchedulingPolicy::Feature::kWebRTC;
  const String& url = "https://a.com";
  const String& function = "";
  unsigned line_number = 1;
  unsigned column_number = 1;

  std::unique_ptr<SourceLocation> source_location_socket =
      std::make_unique<SourceLocation>(url, function, line_number,
                                       column_number, nullptr, 0);
  std::unique_ptr<SourceLocation> source_location_webrtc =
      std::make_unique<SourceLocation>(url, function, line_number,
                                       column_number, nullptr, 0);
  std::unique_ptr<SourceLocation> source_location_socket_second =
      std::make_unique<SourceLocation>(url, function, line_number,
                                       column_number, nullptr, 0);

  FeatureAndJSLocationBlockingBFCache feature_and_js_location_socket =
      FeatureAndJSLocationBlockingBFCache(feature_socket, url, function,
                                          line_number, column_number);
  FeatureAndJSLocationBlockingBFCache feature_and_js_location_webrtc =
      FeatureAndJSLocationBlockingBFCache(feature_webrtc, url, function,
                                          line_number, column_number);

  BackForwardCacheDisablingFeatureTracker tracker(tracing_controller(),
                                                  nullptr);

  EXPECT_THAT(tracker.GetActiveFeaturesTrackedForBackForwardCacheMetrics(),
              testing::UnorderedElementsAre());
  EXPECT_THAT(tracker.GetActiveFeaturesTrackedForBackForwardCacheMetricsMask(),
              0);

  FrameOrWorkerScheduler::SchedulingAffectingFeatureHandle handle_socket =
      FrameOrWorkerScheduler::SchedulingAffectingFeatureHandle(
          feature_socket, SchedulingPolicy(), source_location_socket->Clone(),
          nullptr);
  FrameOrWorkerScheduler::SchedulingAffectingFeatureHandle handle_webrtc =
      FrameOrWorkerScheduler::SchedulingAffectingFeatureHandle(
          feature_webrtc, SchedulingPolicy(), source_location_webrtc->Clone(),
          nullptr);
  FrameOrWorkerScheduler::SchedulingAffectingFeatureHandle
      handle_socket_second =
          FrameOrWorkerScheduler::SchedulingAffectingFeatureHandle(
              feature_socket, SchedulingPolicy(),
              source_location_socket_second->Clone(), nullptr);

  BFCacheBlockingFeatureAndLocations& stored_feature_and_js_location =
      tracker.GetActiveNonStickyFeaturesTrackedForBackForwardCache();

  // Add kWebSocket.
  tracker.AddNonStickyFeature(feature_socket, std::move(source_location_socket),
                              &handle_socket);
  EXPECT_NE(stored_feature_and_js_location.Find(feature_and_js_location_socket),
            kNotFound);

  // Add kWebRTC.
  tracker.AddNonStickyFeature(feature_webrtc, std::move(source_location_webrtc),
                              &handle_webrtc);
  EXPECT_NE(stored_feature_and_js_location.Find(feature_and_js_location_webrtc),
            kNotFound);

  // Add kWebSocket again.
  tracker.AddNonStickyFeature(feature_socket,
                              std::move(source_location_socket_second),
                              &handle_socket_second);
  EXPECT_NE(stored_feature_and_js_location.Find(feature_and_js_location_socket),
            kNotFound);

  // Remove kWebSocket.
  tracker.Remove(feature_and_js_location_socket);
  EXPECT_TRUE(
      stored_feature_and_js_location.Contains(feature_and_js_location_socket));
  EXPECT_EQ(stored_feature_and_js_location.size(), 2u);

  // Remove kWebRTC.
  tracker.Remove(feature_and_js_location_webrtc);
  EXPECT_FALSE(
      stored_feature_and_js_location.Contains(feature_and_js_location_webrtc));
  EXPECT_EQ(stored_feature_and_js_location.size(), 1u);

  // Remove kWebSocket again.
  tracker.Remove(feature_and_js_location_socket);
  EXPECT_FALSE(
      stored_feature_and_js_location.Contains(feature_and_js_location_socket));
  EXPECT_TRUE(stored_feature_and_js_location.empty());
}

TEST_F(BackForwardCacheDisablingFeatureTrackerTest, AddStickyFeature) {
  SchedulingPolicy::Feature feature =
      SchedulingPolicy::Feature::kMainResourceHasCacheControlNoCache;
  const String& url = "https://a.com";
  const String& function = "";
  unsigned line_number = 1;
  unsigned column_number = 1;

  std::unique_ptr<SourceLocation> source_location =
      std::make_unique<SourceLocation>(url, function, line_number,
                                       column_number, nullptr, 0);

  FeatureAndJSLocationBlockingBFCache feature_and_js_location =
      FeatureAndJSLocationBlockingBFCache(feature, url, function, line_number,
                                          column_number);

  BackForwardCacheDisablingFeatureTracker tracker(tracing_controller(),
                                                  nullptr);

  EXPECT_THAT(tracker.GetActiveFeaturesTrackedForBackForwardCacheMetrics(),
              testing::UnorderedElementsAre());
  EXPECT_THAT(tracker.GetActiveFeaturesTrackedForBackForwardCacheMetricsMask(),
              0);

  // Add kWebSocket.
  tracker.AddStickyFeature(feature, std::move(source_location));
  EXPECT_TRUE(
      tracker.GetActiveStickyFeaturesTrackedForBackForwardCache().Contains(
          feature_and_js_location));
}
}  // namespace scheduler
}  // namespace blink
