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
  const String& url = "https://a.com";
  const String& function = "foo";
  const String& function_2 = "bar";
  const unsigned line_number = 1;
  const unsigned column_number = 1;
  FeatureAndJSLocationBlockingBFCache feature_and_js_location_socket =
      FeatureAndJSLocationBlockingBFCache(SchedulingPolicy::Feature::kWebSocket,
                                          url, function, line_number,
                                          column_number);
  FeatureAndJSLocationBlockingBFCache feature_and_js_location_socket_two =
      FeatureAndJSLocationBlockingBFCache(SchedulingPolicy::Feature::kWebSocket,
                                          url, function_2, line_number,
                                          column_number);
  FeatureAndJSLocationBlockingBFCache feature_and_js_location_webrtc =
      FeatureAndJSLocationBlockingBFCache(SchedulingPolicy::Feature::kWebRTC,
                                          url, function, line_number,
                                          column_number);
  std::unique_ptr<SourceLocation> source_location =
      std::make_unique<SourceLocation>(url, function, line_number,
                                       column_number, nullptr, 0);
  std::unique_ptr<SourceLocation> source_location_2 =
      std::make_unique<SourceLocation>(url, function_2, line_number,
                                       column_number, nullptr, 0);
  BackForwardCacheDisablingFeatureTracker tracker(tracing_controller(),
                                                  nullptr);
  EXPECT_THAT(tracker.GetActiveFeaturesTrackedForBackForwardCacheMetrics(),
              testing::UnorderedElementsAre());

  FrameOrWorkerScheduler::SchedulingAffectingFeatureHandle handle_socket =
      FrameOrWorkerScheduler::SchedulingAffectingFeatureHandle(
          SchedulingPolicy::Feature::kWebSocket, SchedulingPolicy(),
          source_location->Clone(), nullptr);
  FrameOrWorkerScheduler::SchedulingAffectingFeatureHandle handle_webrtc =
      FrameOrWorkerScheduler::SchedulingAffectingFeatureHandle(
          SchedulingPolicy::Feature::kWebRTC, SchedulingPolicy(),
          source_location->Clone(), nullptr);
  FrameOrWorkerScheduler::SchedulingAffectingFeatureHandle
      handle_socket_second =
          FrameOrWorkerScheduler::SchedulingAffectingFeatureHandle(
              SchedulingPolicy::Feature::kWebSocket, SchedulingPolicy(),
              source_location_2->Clone(), nullptr);

  BFCacheBlockingFeatureAndLocations& stored_feature_and_js_location =
      tracker.GetActiveNonStickyFeaturesTrackedForBackForwardCache();

  // Add kWebSocket.
  tracker.AddNonStickyFeature(SchedulingPolicy::Feature::kWebSocket,
                              source_location->Clone(), &handle_socket);
  EXPECT_TRUE(stored_feature_and_js_location.details_list.Contains(
      feature_and_js_location_socket));

  // Add kWebRTC.
  tracker.AddNonStickyFeature(SchedulingPolicy::Feature::kWebRTC,
                              source_location->Clone(), &handle_webrtc);
  EXPECT_TRUE(stored_feature_and_js_location.details_list.Contains(
      feature_and_js_location_webrtc));

  // Add kWebSocket again with different source location.
  tracker.AddNonStickyFeature(SchedulingPolicy::Feature::kWebSocket,
                              source_location_2->Clone(),
                              &handle_socket_second);
  EXPECT_TRUE(stored_feature_and_js_location.details_list.Contains(
      feature_and_js_location_socket_two));

  // Remove kWebSocket.
  tracker.Remove(feature_and_js_location_socket);
  EXPECT_TRUE(stored_feature_and_js_location.details_list.Contains(
      feature_and_js_location_socket_two));
  EXPECT_EQ(stored_feature_and_js_location.details_list.size(), 2u);

  // Remove kWebRTC.
  tracker.Remove(feature_and_js_location_webrtc);
  EXPECT_FALSE(stored_feature_and_js_location.details_list.Contains(
      feature_and_js_location_webrtc));
  EXPECT_EQ(stored_feature_and_js_location.details_list.size(), 1u);

  // Remove kWebSocket again.
  tracker.Remove(feature_and_js_location_socket_two);
  EXPECT_FALSE(stored_feature_and_js_location.details_list.Contains(
      feature_and_js_location_socket_two));
  EXPECT_TRUE(stored_feature_and_js_location.details_list.empty());
}

TEST_F(BackForwardCacheDisablingFeatureTrackerTest, AddStickyFeature) {
  const String& url = "https://a.com";
  const String& function = "foo";
  const unsigned line_number = 1;
  const unsigned column_number = 1;
  std::unique_ptr<SourceLocation> source_location =
      std::make_unique<SourceLocation>(url, function, line_number,
                                       column_number, nullptr, 0);
  FeatureAndJSLocationBlockingBFCache feature_and_js_location_socket =
      FeatureAndJSLocationBlockingBFCache(
          SchedulingPolicy::Feature::kMainResourceHasCacheControlNoStore, url,
          function, line_number, column_number);
  BackForwardCacheDisablingFeatureTracker tracker(tracing_controller(),
                                                  nullptr);

  EXPECT_THAT(tracker.GetActiveFeaturesTrackedForBackForwardCacheMetrics(),
              testing::UnorderedElementsAre());

  // Add kMainResourceHasCacheControlNoStore.
  tracker.AddStickyFeature(
      SchedulingPolicy::Feature::kMainResourceHasCacheControlNoStore,
      source_location->Clone());
  EXPECT_TRUE(tracker.GetActiveStickyFeaturesTrackedForBackForwardCache()
                  .details_list.Contains(feature_and_js_location_socket));
}

TEST_F(BackForwardCacheDisablingFeatureTrackerTest, AddDuplicateFeature) {
  BackForwardCacheDisablingFeatureTracker tracker(tracing_controller(),
                                                  nullptr);
  const String& url = "https://a.com";
  const String& function = "foo";
  const String& function_two = "bar";
  const unsigned line_number = 1;
  const unsigned column_number = 1;
  std::unique_ptr<SourceLocation> source_location =
      std::make_unique<SourceLocation>(url, function, line_number,
                                       column_number, nullptr, 0);
  std::unique_ptr<SourceLocation> source_location_2 =
      std::make_unique<SourceLocation>(url, function_two, line_number,
                                       column_number, nullptr, 0);
  FeatureAndJSLocationBlockingBFCache feature_and_js_location_socket =
      FeatureAndJSLocationBlockingBFCache(SchedulingPolicy::Feature::kWebSocket,
                                          url, function, line_number,
                                          column_number);
  FrameOrWorkerScheduler::SchedulingAffectingFeatureHandle handle_socket =
      FrameOrWorkerScheduler::SchedulingAffectingFeatureHandle(
          SchedulingPolicy::Feature::kWebSocket, SchedulingPolicy(),
          source_location->Clone(), nullptr);
  FrameOrWorkerScheduler::SchedulingAffectingFeatureHandle
      handle_socket_second =
          FrameOrWorkerScheduler::SchedulingAffectingFeatureHandle(
              SchedulingPolicy::Feature::kWebSocket, SchedulingPolicy(),
              source_location_2->Clone(), nullptr);

  EXPECT_THAT(tracker.GetActiveFeaturesTrackedForBackForwardCacheMetrics(),
              testing::UnorderedElementsAre());

  // Add kWebSocket.
  tracker.AddNonStickyFeature(SchedulingPolicy::Feature::kWebSocket,
                              source_location->Clone(), &handle_socket);
  EXPECT_TRUE(tracker.GetActiveNonStickyFeaturesTrackedForBackForwardCache()
                  .details_list.Contains(feature_and_js_location_socket));
  EXPECT_EQ(tracker.GetActiveNonStickyFeaturesTrackedForBackForwardCache()
                .details_list.size(),
            1u);

  // Try to add the same kWebSocket location, but it should not add a duplicate.
  tracker.AddNonStickyFeature(SchedulingPolicy::Feature::kWebSocket,
                              source_location->Clone(), &handle_socket);
  EXPECT_EQ(tracker.GetActiveNonStickyFeaturesTrackedForBackForwardCache()
                .details_list.size(),
            1u);

  // Add kWebSocket but from a different location.
  tracker.AddNonStickyFeature(SchedulingPolicy::Feature::kWebSocket,
                              source_location_2->Clone(),
                              &handle_socket_second);
  EXPECT_EQ(tracker.GetActiveNonStickyFeaturesTrackedForBackForwardCache()
                .details_list.size(),
            2u);
}

TEST_F(BackForwardCacheDisablingFeatureTrackerTest,
       AddFeatureMoreThanTenTimes) {
  BackForwardCacheDisablingFeatureTracker tracker(tracing_controller(),
                                                  nullptr);
  const String& url = "https://a.com";
  const String& function = "foo";
  const String& function_two = "bar";
  const unsigned column_number = 1;

  EXPECT_THAT(tracker.GetActiveFeaturesTrackedForBackForwardCacheMetrics(),
              testing::UnorderedElementsAre());

  // Add kMainResourceHasCacheControlNoStore with different line numbers 20
  // times.
  for (int i = 0; i < 20; i++) {
    std::unique_ptr<SourceLocation> source_location =
        std::make_unique<SourceLocation>(url, function, i, column_number,
                                         nullptr, 0);
    tracker.AddStickyFeature(
        SchedulingPolicy::Feature::kMainResourceHasCacheControlNoStore,
        std::move(source_location));
  }
  // Make sure that only 10 details are added.
  EXPECT_EQ(tracker.GetActiveStickyFeaturesTrackedForBackForwardCache()
                .details_list.size(),
            10u);
}

}  // namespace scheduler
}  // namespace blink
