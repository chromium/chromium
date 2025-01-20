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
#include "extensions/common/mojom/message_port.mojom-shared.h"
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

  void WaitForMessageHung() { on_message_hung_runloop.Run(); }

 private:
  void OnStageTimeoutRan(const base::UnguessableToken& message_id) override {
    if (observed_message_id_ == message_id) {
      on_message_hung_runloop.Quit();
    }
  }

 private:
  base::UnguessableToken observed_message_id_;
  base::RunLoop on_message_hung_runloop;
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

// Tests that the tracker correctly records and reports metrics when an
// extension message succeeds in its message stage.
TEST_F(MessageTrackerUnitTest, NotifyMessage) {
  base::UnguessableToken message_id = base::UnguessableToken::Create();
  MessageTrackerTestObserver observer(message_id);
  message_tracker()->StartTrackingMessagingStage(
      message_id, "Extensions.MessagePipeline.OpenChannelStatus",
      mojom::ChannelType::kSendMessage);

  message_tracker()->StopTrackingMessagingStage(
      message_id, MessageTracker::OpenChannelMessagePipelineResult::kOpened);

  histogram_tester().ExpectTotalCount(
      "Extensions.MessagePipeline.OpenChannelStatus.SendMessageChannel",
      /*expected_count=*/1);
  histogram_tester().ExpectBucketCount(
      "Extensions.MessagePipeline.OpenChannelStatus.SendMessageChannel",
      MessageTracker::OpenChannelMessagePipelineResult::kOpened,
      /*expected_count=*/1);
}

// Tests that the tracker correctly records and reports metrics when an
// extension message remains too long in its stage and becomes "hung".
TEST_F(MessageTrackerUnitTest, NotifyHungMessage) {
  base::UnguessableToken message_id = base::UnguessableToken::Create();
  MessageTrackerTestObserver observer(message_id);
  message_tracker()->SetStageHungTimeoutForTest(base::Microseconds(1));
  message_tracker()->StartTrackingMessagingStage(
      message_id, "Extensions.MessagePipeline.OpenChannelStatus",
      mojom::ChannelType::kSendMessage);

  {
    SCOPED_TRACE(
        "waiting for timeout check to run after starting new message tracking");
    observer.WaitForMessageHung();
  }

  histogram_tester().ExpectTotalCount(
      "Extensions.MessagePipeline.OpenChannelStatus.SendMessageChannel",
      /*expected_count=*/1);
  histogram_tester().ExpectBucketCount(
      "Extensions.MessagePipeline.OpenChannelStatus.SendMessageChannel",
      MessageTracker::OpenChannelMessagePipelineResult::kHung,
      /*expected_count=*/1);
}

// Tests that the tracker emits success metrics when an extension message hung
// check occurs *after* a message has successfully completed its messaging
// stage.
TEST_F(MessageTrackerUnitTest, NotifyOpenedMessageIfStoppedBeforeHung) {
  base::UnguessableToken message_id = base::UnguessableToken::Create();
  MessageTrackerTestObserver observer(message_id);
  message_tracker()->SetStageHungTimeoutForTest(base::Seconds(1));
  message_tracker()->StartTrackingMessagingStage(
      message_id, "Extensions.MessagePipeline.OpenChannelStatus",
      mojom::ChannelType::kSendMessage);
  message_tracker()->StopTrackingMessagingStage(
      message_id, MessageTracker::OpenChannelMessagePipelineResult::kOpened);

  // This will wait for the hung check created by
  // StartTrackingMessagingStage(), then proceed.
  {
    SCOPED_TRACE(
        "waiting for hung check to run after starting and updating new "
        "message tracking");
    observer.WaitForMessageHung();
  }

  histogram_tester().ExpectTotalCount(
      "Extensions.MessagePipeline.OpenChannelStatus.SendMessageChannel",
      /*expected_count=*/1);
  histogram_tester().ExpectBucketCount(
      "Extensions.MessagePipeline.OpenChannelStatus.SendMessageChannel",
      MessageTracker::OpenChannelMessagePipelineResult::kOpened,
      /*expected_count=*/1);
}

// Tests that the tracker emits failure metrics when an extension message hung
// check occurs *before* a message has successfully completed its messaging
// stage.
TEST_F(MessageTrackerUnitTest, NotifyHungMessageIfStoppedAfterHung) {
  base::UnguessableToken message_id = base::UnguessableToken::Create();
  MessageTrackerTestObserver observer(message_id);
  message_tracker()->SetStageHungTimeoutForTest(base::Seconds(1));
  message_tracker()->StartTrackingMessagingStage(
      message_id, "Extensions.MessagePipeline.OpenChannelStatus",
      mojom::ChannelType::kSendMessage);

  // This will wait for the hung check created by
  // StartTrackingMessagingStage(), then proceed.
  {
    SCOPED_TRACE(
        "waiting for timeout check to run after starting and updating new "
        "message tracking");
    observer.WaitForMessageHung();
  }

  message_tracker()->StopTrackingMessagingStage(
      message_id, MessageTracker::OpenChannelMessagePipelineResult::kOpened);

  histogram_tester().ExpectTotalCount(
      "Extensions.MessagePipeline.OpenChannelStatus.SendMessageChannel",
      /*expected_count=*/1);
  histogram_tester().ExpectBucketCount(
      "Extensions.MessagePipeline.OpenChannelStatus.SendMessageChannel",
      MessageTracker::OpenChannelMessagePipelineResult::kHung,
      /*expected_count=*/1);
}

}  // namespace extensions
