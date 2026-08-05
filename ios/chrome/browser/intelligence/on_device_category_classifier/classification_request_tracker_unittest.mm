// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/on_device_category_classifier/classification_request_tracker.h"

#import <utility>
#import <vector>

#import "base/functional/callback.h"
#import "base/strings/string_number_conversions.h"
#import "base/test/bind.h"
#import "components/page_content_annotations/core/page_content_annotations_common.h"
#import "services/metrics/public/cpp/ukm_source_id.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "url/gurl.h"

using ClassificationRequestTrackerTest = PlatformTest;

// Tests queuing and draining pending classification requests.
TEST_F(ClassificationRequestTrackerTest, EnqueueAndDrainPending) {
  ClassificationRequestTracker tracker;
  EXPECT_TRUE(tracker.DrainPending().empty());

  GURL url1("https://example.com/1");
  GURL url2("https://example.com/2");

  bool callback1_called = false;
  tracker.EnqueuePending({
      url1,
      "Title 1",
      "Content 1",
      ukm::SourceId(),
      base::BindLambdaForTesting(
          [&](const std::vector<page_content_annotations::Category>& res) {
            callback1_called = true;
          }),
  });
  tracker.EnqueuePending({
      url2,
      "Title 2",
      "Content 2",
      ukm::SourceId(),
      base::BindLambdaForTesting(
          [&](const std::vector<page_content_annotations::Category>& res) {}),
  });

  auto pending = tracker.DrainPending();
  ASSERT_EQ(2u, pending.size());
  EXPECT_EQ(url1, pending[0].url);
  EXPECT_EQ("Title 1", pending[0].title);
  EXPECT_EQ("Content 1", pending[0].page_content);
  EXPECT_EQ(url2, pending[1].url);

  std::move(pending[0].callback).Run({});
  EXPECT_TRUE(callback1_called);

  EXPECT_TRUE(tracker.DrainPending().empty());
}

// Tests attaching in-flight callbacks and completing a URL.
TEST_F(ClassificationRequestTrackerTest, AttachInFlightAndCompleteUrl) {
  ClassificationRequestTracker tracker;
  GURL url("https://example.com");

  bool callback1_called = false;
  std::vector<page_content_annotations::Category> callback1_results;
  bool first_attach = tracker.AttachInFlight(
      url, base::BindLambdaForTesting(
               [&](const std::vector<page_content_annotations::Category>& res) {
                 callback1_called = true;
                 callback1_results = res;
               }));
  EXPECT_FALSE(first_attach);

  bool callback2_called = false;
  bool second_attach = tracker.AttachInFlight(
      url, base::BindLambdaForTesting(
               [&](const std::vector<page_content_annotations::Category>& res) {
                 callback2_called = true;
               }));
  EXPECT_TRUE(second_attach);

  std::vector<page_content_annotations::Category> results;
  results.push_back({page_content_annotations::CategoryType::kEducation, 0.9f});
  tracker.CompleteUrl(url, results);

  EXPECT_TRUE(callback1_called);
  EXPECT_TRUE(callback2_called);
  ASSERT_EQ(1u, callback1_results.size());
  EXPECT_EQ(page_content_annotations::CategoryType::kEducation,
            callback1_results[0].category_type);

  // Completing again or completing an unknown URL should be a no-op.
  tracker.CompleteUrl(url, results);
  tracker.CompleteUrl(GURL("https://other.com"), results);
}

// Tests that CancelAll invokes callbacks with empty results and clears queues.
TEST_F(ClassificationRequestTrackerTest, CancelAll) {
  ClassificationRequestTracker tracker;
  GURL url1("https://example.com/1");
  GURL url2("https://example.com/2");

  bool pending_callback_called = false;
  std::vector<page_content_annotations::Category> pending_results;
  tracker.EnqueuePending({
      url1,
      "Title",
      "Content",
      ukm::SourceId(),
      base::BindLambdaForTesting(
          [&](const std::vector<page_content_annotations::Category>& res) {
            pending_callback_called = true;
            pending_results = res;
          }),
  });

  bool inflight_callback_called = false;
  std::vector<page_content_annotations::Category> inflight_results;
  tracker.AttachInFlight(
      url2,
      base::BindLambdaForTesting(
          [&](const std::vector<page_content_annotations::Category>& res) {
            inflight_callback_called = true;
            inflight_results = res;
          }));

  tracker.CancelAll();

  EXPECT_TRUE(pending_callback_called);
  EXPECT_TRUE(pending_results.empty());
  EXPECT_TRUE(inflight_callback_called);
  EXPECT_TRUE(inflight_results.empty());

  EXPECT_TRUE(tracker.DrainPending().empty());
}

// Tests that EnqueuePending caps the queue size and invokes callbacks for
// dropped oldest requests.
TEST_F(ClassificationRequestTrackerTest, EnqueuePendingCapped) {
  ClassificationRequestTracker tracker;
  int dropped_callbacks_called = 0;

  for (size_t i = 0;
       i < ClassificationRequestTracker::kMaxPendingClassifications + 2; ++i) {
    tracker.EnqueuePending({
        GURL("https://example.com/" + base::NumberToString(i)),
        "Title " + base::NumberToString(i),
        "Content " + base::NumberToString(i),
        ukm::SourceId(),
        base::BindLambdaForTesting(
            [&](const std::vector<page_content_annotations::Category>& res) {
              EXPECT_TRUE(res.empty());
              dropped_callbacks_called++;
            }),
    });
  }

  EXPECT_EQ(2, dropped_callbacks_called);
  auto pending = tracker.DrainPending();
  ASSERT_EQ(ClassificationRequestTracker::kMaxPendingClassifications,
            pending.size());
  EXPECT_EQ(GURL("https://example.com/2"), pending[0].url);
}
