// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/message_tracker.h"

#include "base/strings/strcat.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "content/public/browser/browser_context.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "extensions/browser/extensions_test.h"
#include "extensions/browser/test_extensions_browser_client.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

namespace {

class MessageTrackerTestObserver : public MessageTracker::TestObserver {
 public:
  explicit MessageTrackerTestObserver(const base::UnguessableToken message_id)
      : observed_message_id_(message_id) {
    MessageTracker::SetObserverForTest(this);
  }

  ~MessageTrackerTestObserver() override {
    MessageTracker::SetObserverForTest(nullptr);
  }

  void WaitForTrackingStale() { on_tracking_stale_runloop.Run(); }

 private:
  void OnTrackingStale(const base::UnguessableToken& message_id) override {
    if (observed_message_id_ == message_id) {
      on_tracking_stale_runloop.Quit();
    }
  }

 private:
  base::UnguessableToken observed_message_id_;
  base::RunLoop on_tracking_stale_runloop;
};

}  // namespace

class MessageTrackerUnitTest : public ExtensionsTest {
 protected:
  void SetUp() override {
    ExtensionsTest::SetUp();
    message_tracker_ = MessageTracker::Get(browser_context());
  }

  void TearDown() override {
    message_tracker_ = nullptr;
    ExtensionsTest::TearDown();
  }

  MessageTracker* message_tracker() { return message_tracker_; }
  base::HistogramTester& histogram_tester() { return histogram_tester_; }

 private:
  raw_ptr<MessageTracker> message_tracker_;
  base::HistogramTester histogram_tester_;
};

// Tests that the tracker correctly records and keeps track of an
// extension message as it progresses through its delivery stages.
TEST_F(MessageTrackerUnitTest, NotifyStartTrackingMessageDelivery) {
  base::UnguessableToken message_id = base::UnguessableToken::Create();
  message_tracker()->NotifyStartTrackingMessageDelivery(
      message_id, MessageTracker::MessageDeliveryStage::kUnknown,
      MessageTracker::MessageDestinationType::kServiceWorker);

  message_tracker()->NotifyUpdateMessageDelivery(
      message_id,
      MessageTracker::MessageDeliveryStage::kOpenChannelRequestReceived);

  // Tracker continues to track messages as they move through each messaging
  // stage.
  EXPECT_EQ(message_tracker()->GetNumberOfTrackedMessagesForTest(), 1u);
}

class MessageTrackerMultiBackgroundDestinationUnitTest
    : public MessageTrackerUnitTest,
      public testing::WithParamInterface<
          MessageTracker::MessageDestinationType> {};

// Tests that the tracker correctly records and reports metrics when an
// extension message finishes its delivery and stops being tracked.
TEST_P(MessageTrackerMultiBackgroundDestinationUnitTest,
       NotifyStopTrackingMessageDelivery_ForDestinationType) {
  base::UnguessableToken message_id = base::UnguessableToken::Create();
  MessageTrackerTestObserver observer(message_id);
  message_tracker()->SetMessageStaleTimeoutForTest(base::Seconds(1));
  MessageTracker::MessageDestinationType destination_background_type =
      GetParam();
  message_tracker()->NotifyStartTrackingMessageDelivery(
      message_id, MessageTracker::MessageDeliveryStage::kUnknown,
      destination_background_type);
  message_tracker()->NotifyStopTrackingMessageDelivery(message_id);

  {
    SCOPED_TRACE(
        "waiting for stale check to run after stopping new message tracking");
    observer.WaitForTrackingStale();
  }

  // Tracker stops tracking messages when they complete the process.
  EXPECT_EQ(message_tracker()->GetNumberOfTrackedMessagesForTest(), 0u);

  switch (destination_background_type) {
    case MessageTracker::MessageDestinationType::kUnknown:
      histogram_tester().ExpectBucketCount(
          "Extensions.MessagePipeline.MessageCompleted.Unknown",
          /*sample=*/true, /*expected_count=*/1);
      histogram_tester().ExpectTotalCount(
          "Extensions.MessagePipeline.MessageCompletedTime.Unknown",
          /*expected_count=*/1);
      histogram_tester().ExpectTotalCount(
          "Extensions.MessagePipeline.MessageStaleAtStage.Unknown",
          /*expected_count=*/0);
      break;
    case MessageTracker::MessageDestinationType::kNonServiceWorker:
      histogram_tester().ExpectBucketCount(
          "Extensions.MessagePipeline.MessageCompleted.NonServiceWorker",
          /*sample=*/true, /*expected_count=*/1);
      histogram_tester().ExpectTotalCount(
          "Extensions.MessagePipeline.MessageCompletedTime.NonServiceWorker",
          /*expected_count=*/1);
      histogram_tester().ExpectTotalCount(
          "Extensions.MessagePipeline.MessageStaleAtStage.NonServiceWorker",
          /*expected_count=*/0);
      break;
    case MessageTracker::MessageDestinationType::kServiceWorker:
      histogram_tester().ExpectBucketCount(
          "Extensions.MessagePipeline.MessageCompleted.ServiceWorker",
          /*sample=*/true, /*expected_count=*/1);
      histogram_tester().ExpectTotalCount(
          "Extensions.MessagePipeline.MessageCompletedTime.ServiceWorker",
          /*expected_count=*/1);
      histogram_tester().ExpectTotalCount(
          "Extensions.MessagePipeline.MessageStaleAtStage.ServiceWorker",
          /*expected_count=*/0);
      break;
  }
}

INSTANTIATE_TEST_SUITE_P(
    BackgroundDestination,
    MessageTrackerMultiBackgroundDestinationUnitTest,
    testing::Values(MessageTracker::MessageDestinationType::kNonServiceWorker,
                    MessageTracker::MessageDestinationType::kServiceWorker));

// Tests that the tracker correctly records and reports metrics when an
// extension message becomes stale.
TEST_F(MessageTrackerUnitTest, NotifyStaleMessage) {
  base::UnguessableToken message_id = base::UnguessableToken::Create();
  MessageTrackerTestObserver observer(message_id);
  message_tracker()->SetMessageStaleTimeoutForTest(base::Microseconds(1));
  message_tracker()->NotifyStartTrackingMessageDelivery(
      message_id, MessageTracker::MessageDeliveryStage::kUnknown,
      MessageTracker::MessageDestinationType::kServiceWorker);

  {
    SCOPED_TRACE(
        "waiting for stale check to run after starting new message tracking");
    observer.WaitForTrackingStale();
  }

  histogram_tester().ExpectBucketCount(
      "Extensions.MessagePipeline.MessageCompleted.ServiceWorker",
      /*sample=*/false,
      /*expected_count=*/1);
  histogram_tester().ExpectBucketCount(
      "Extensions.MessagePipeline.MessageStaleAtStage.ServiceWorker",
      MessageTracker::MessageDeliveryStage::kUnknown, /*expected_count=*/1);
}

// Tests that the tracker correctly records and reports metrics when an
// extension message stale check occurs *after* an update call occurs which
// means the message is not stale.
TEST_F(MessageTrackerUnitTest, NotifyStaleMessageAfterUpdateToNewStage) {
  base::UnguessableToken message_id = base::UnguessableToken::Create();
  MessageTrackerTestObserver observer(message_id);
  message_tracker()->SetMessageStaleTimeoutForTest(base::Seconds(1));
  message_tracker()->NotifyStartTrackingMessageDelivery(
      message_id, MessageTracker::MessageDeliveryStage::kUnknown,
      MessageTracker::MessageDestinationType::kServiceWorker);
  message_tracker()->NotifyUpdateMessageDelivery(
      message_id,
      MessageTracker::MessageDeliveryStage::kOpenChannelRequestReceived);

  // This will wait for the stale check created by
  // NotifyStartTrackingMessageDelivery(), then proceed.
  {
    SCOPED_TRACE(
        "waiting for stale check to run after starting and updating new "
        "message tracking");
    observer.WaitForTrackingStale();
  }

  // This will prevent the stale check that runs from
  // NotifyUpdateMessageDelivery() to prevent emitting stale metrics.
  message_tracker()->NotifyStopTrackingMessageDelivery(message_id);

  histogram_tester().ExpectBucketCount(
      "Extensions.MessagePipeline.MessageCompleted.ServiceWorker",
      /*sample=*/true, /*expected_count=*/1);
  histogram_tester().ExpectTotalCount(
      "Extensions.MessagePipeline.MessageCompletedTime.ServiceWorker",
      /*expected_count=*/1);
  histogram_tester().ExpectTotalCount(
      "Extensions.MessagePipeline.MessageStaleAtStage.ServiceWorker",
      /*expected_count=*/0);
}

// Tests that the tracker correctly records and reports metrics when an
// extension message stale check occurs *before* an update call occurs which
// means the message is stale.
TEST_F(MessageTrackerUnitTest, NotifyStaleMessageBeforeUpdateToNewStage) {
  base::UnguessableToken message_id = base::UnguessableToken::Create();
  MessageTrackerTestObserver observer(message_id);
  message_tracker()->SetMessageStaleTimeoutForTest(base::Microseconds(1));
  message_tracker()->NotifyStartTrackingMessageDelivery(
      message_id, MessageTracker::MessageDeliveryStage::kUnknown,
      MessageTracker::MessageDestinationType::kServiceWorker);

  {
    SCOPED_TRACE(
        "waiting for stale check to run after starting new message tracking");
    observer.WaitForTrackingStale();
  }

  message_tracker()->NotifyUpdateMessageDelivery(
      message_id,
      MessageTracker::MessageDeliveryStage::kOpenChannelRequestReceived);

  histogram_tester().ExpectBucketCount(
      "Extensions.MessagePipeline.MessageCompleted.ServiceWorker",
      /*sample=*/false,
      /*expected_count=*/1);
  histogram_tester().ExpectBucketCount(
      "Extensions.MessagePipeline.MessageStaleAtStage.ServiceWorker",
      MessageTracker::MessageDeliveryStage::kUnknown, /*expected_count=*/1);
}

// Tests that the tracker correctly records and reports metrics when an
// extension message becomes stale *before* a late subsequent stop tracking call
// occurs.
TEST_F(MessageTrackerUnitTest, NotifyStaleMessageWithLateStop) {
  base::UnguessableToken message_id = base::UnguessableToken::Create();
  MessageTrackerTestObserver observer(message_id);
  message_tracker()->SetMessageStaleTimeoutForTest(base::Microseconds(1));
  message_tracker()->NotifyStartTrackingMessageDelivery(
      message_id, MessageTracker::MessageDeliveryStage::kUnknown,
      MessageTracker::MessageDestinationType::kServiceWorker);

  {
    SCOPED_TRACE(
        "waiting for stale check to run after starting new message tracking");
    observer.WaitForTrackingStale();
  }

  message_tracker()->NotifyStopTrackingMessageDelivery(message_id);

  histogram_tester().ExpectBucketCount(
      "Extensions.MessagePipeline.MessageCompleted.ServiceWorker",
      /*sample=*/false,
      /*expected_count=*/1);
  histogram_tester().ExpectBucketCount(
      "Extensions.MessagePipeline.MessageStaleAtStage.ServiceWorker",
      MessageTracker::MessageDeliveryStage::kUnknown, /*expected_count=*/1);
}

}  // namespace extensions
