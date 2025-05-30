// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/input/input_transfer_handler_android.h"

#include <utility>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "components/input/features.h"
#include "components/input/utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/android/motion_event_android_java.h"
#include "ui/events/velocity_tracker/motion_event_generic.h"

namespace content {

using ::testing::_;
using ::testing::Return;

namespace {

const int kSuccessfullyTransferred =
    static_cast<int>(TransferInputToVizResult::kSuccessfullyTransferred);
// Arbitrary failure reason.
const int kFailureTransferring = kSuccessfullyTransferred + 1;

class FakeInputTransferHandlerClient final
    : public InputTransferHandlerAndroidClient {
 public:
  gpu::SurfaceHandle GetRootSurfaceHandle() override {
    return gpu::kNullSurfaceHandle;
  }

  MOCK_METHOD((void),
              SendStateOnTouchTransfer,
              (const ui::MotionEvent&, bool),
              (override));

  bool IsMojoRIRDelegateConnectionSetup() override { return true; }
};

class MockJniDelegate : public InputTransferHandlerAndroid::JniDelegate {
 public:
  ~MockJniDelegate() override = default;

  MOCK_METHOD((int), MaybeTransferInputToViz, (int), (override));
  MOCK_METHOD((int), TransferInputToViz, (int), (override));
};

ui::MotionEventAndroidJava GetMotionEventAndroid(
    ui::MotionEvent::Action action,
    base::TimeTicks event_time,
    base::TimeTicks down_time,
    const ui::MotionEventAndroid::Pointer& pointer) {
  float pix_to_dip = 1.0;

  return ui::MotionEventAndroidJava(
      nullptr, nullptr, pix_to_dip, 0.f, 0.f, 0.f, event_time, event_time,
      down_time, ui::MotionEventAndroid::GetAndroidAction(action), 1, 0, 0, 0,
      0, 0, 0, 0, 0, 0, false, &pointer, nullptr, false);
}

}  // namespace

// TODO(crbug.com/397939487): Add helper methods to generate test input
// to help simplifying test logic and use mock time source.
class InputTransferHandlerTest : public testing::Test {
 public:
  InputTransferHandlerTest() : finger_pointer_(0, 0, 0, 0, 0, 0, 0, 0, 0) {}

  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(input::features::kInputOnViz);

    if (!input::InputUtils::IsTransferInputToVizSupported()) {
      GTEST_SKIP()
          << "The class is only used when transfer input to viz is supported.";
    }

    input_transfer_handler_client_ =
        std::make_unique<FakeInputTransferHandlerClient>();
    transfer_handler_ = std::make_unique<InputTransferHandlerAndroid>(
        input_transfer_handler_client_.get());

    auto delegate = std::unique_ptr<InputTransferHandlerAndroid::JniDelegate>(
        new MockJniDelegate());
    mock_ = static_cast<MockJniDelegate*>(delegate.get());
    transfer_handler_->set_jni_delegate_for_testing(std::move(delegate));
    finger_pointer_.tool_type =
        static_cast<int>(ui::MotionEvent::ToolType::FINGER);
  }

  void TearDown() override {
    transfer_handler_.reset();
    input_transfer_handler_client_.reset();
  }

 protected:
  std::unique_ptr<InputTransferHandlerAndroid> transfer_handler_;
  raw_ptr<MockJniDelegate> mock_;
  std::unique_ptr<FakeInputTransferHandlerClient>
      input_transfer_handler_client_;
  ui::MotionEventAndroid::Pointer finger_pointer_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(InputTransferHandlerTest, ConsumeEventsIfSequenceTransferred) {
  base::TimeTicks event_time = base::TimeTicks::Now();

  ui::MotionEventAndroidJava down_event = GetMotionEventAndroid(
      ui::MotionEvent::Action::DOWN, event_time, event_time, finger_pointer_);

  EXPECT_CALL(*mock_, MaybeTransferInputToViz(_))
      .WillOnce(Return(kSuccessfullyTransferred));
  EXPECT_CALL(*input_transfer_handler_client_,
              SendStateOnTouchTransfer(_, /*browser_would_have_handled=*/false))
      .Times(1);
  EXPECT_TRUE(transfer_handler_->OnTouchEvent(down_event));

  ui::MotionEventAndroidJava move_event = GetMotionEventAndroid(
      ui::MotionEvent::Action::MOVE, event_time, event_time, finger_pointer_);
  EXPECT_TRUE(transfer_handler_->OnTouchEvent(move_event));
  EXPECT_TRUE(transfer_handler_->OnTouchEvent(move_event));

  ui::MotionEventAndroidJava cancel_event = GetMotionEventAndroid(
      ui::MotionEvent::Action::CANCEL, event_time, event_time, finger_pointer_);
  EXPECT_TRUE(transfer_handler_->OnTouchEvent(cancel_event));

  transfer_handler_->OnTouchEnd(event_time + base::Milliseconds(10));

  EXPECT_CALL(*mock_, MaybeTransferInputToViz(_))
      .WillOnce(Return(kFailureTransferring));
  // New events shouldn't be consumed due the expectation set in line above.
  EXPECT_FALSE(transfer_handler_->OnTouchEvent(down_event));
  EXPECT_FALSE(transfer_handler_->OnTouchEvent(move_event));
}

TEST_F(InputTransferHandlerTest, EmitsTouchMovesSeenAfterTransferHistogram) {
  base::TimeTicks event_time = base::TimeTicks::Now();
  ui::MotionEventAndroidJava down_event = GetMotionEventAndroid(
      ui::MotionEvent::Action::DOWN, event_time, event_time, finger_pointer_);
  ui::MotionEventAndroidJava move_event = GetMotionEventAndroid(
      ui::MotionEvent::Action::MOVE, event_time, event_time, finger_pointer_);
  ui::MotionEventAndroidJava cancel_event = GetMotionEventAndroid(
      ui::MotionEvent::Action::CANCEL, event_time, event_time, finger_pointer_);

  for (int touch_moves_seen = 0; touch_moves_seen <= 2; touch_moves_seen++) {
    base::HistogramTester histogram_tester;
    EXPECT_CALL(*mock_, MaybeTransferInputToViz(_))
        .WillOnce(Return(kSuccessfullyTransferred));
    EXPECT_TRUE(transfer_handler_->OnTouchEvent(down_event));
    for (int ind = 1; ind <= touch_moves_seen; ind++) {
      EXPECT_TRUE(transfer_handler_->OnTouchEvent(move_event));
    }
    EXPECT_TRUE(transfer_handler_->OnTouchEvent(cancel_event));
    histogram_tester.ExpectUniqueSample(
        InputTransferHandlerAndroid::kTouchMovesSeenHistogram, touch_moves_seen,
        1);
  }
}

TEST_F(InputTransferHandlerTest, EmitsEventsAfterTransferHistogram) {
  base::TimeTicks event_time = base::TimeTicks::Now();
  ui::MotionEventAndroidJava down_event = GetMotionEventAndroid(
      ui::MotionEvent::Action::DOWN, event_time, event_time, finger_pointer_);
  ui::MotionEventAndroidJava move_event = GetMotionEventAndroid(
      ui::MotionEvent::Action::MOVE, event_time, event_time, finger_pointer_);
  ui::MotionEventAndroidJava cancel_event = GetMotionEventAndroid(
      ui::MotionEvent::Action::CANCEL, event_time, event_time, finger_pointer_);

  const std::vector<std::pair<ui::MotionEvent::Action, int>>
      event_expected_histogram_pairs = {
          {ui::MotionEvent::Action::DOWN, /*MotionEventAction::kDown*/ 1},
          {ui::MotionEvent::Action::UP, /*MotionEventAction::kUp*/ 2},
          {ui::MotionEvent::Action::MOVE, /*MotionEventAction::kMove*/ 3},
          {ui::MotionEvent::Action::POINTER_DOWN,
           /*MotionEventAction::kPointerDown*/ 5},
          {ui::MotionEvent::Action::POINTER_UP,
           /*MotionEventAction::kPointerUp*/ 6},
      };
  for (const auto& [event_action, expected_histogram_sample] :
       event_expected_histogram_pairs) {
    base::HistogramTester histogram_tester;
    EXPECT_CALL(*mock_, MaybeTransferInputToViz(_))
        .WillOnce(Return(kSuccessfullyTransferred));
    EXPECT_TRUE(transfer_handler_->OnTouchEvent(down_event));

    ui::MotionEventAndroidJava event = GetMotionEventAndroid(
        event_action, event_time, event_time, finger_pointer_);
    EXPECT_TRUE(transfer_handler_->OnTouchEvent(event));
    histogram_tester.ExpectUniqueSample(
        InputTransferHandlerAndroid::kEventsAfterTransferHistogram,
        expected_histogram_sample, 1);

    EXPECT_TRUE(transfer_handler_->OnTouchEvent(cancel_event));
  }
}

TEST_F(InputTransferHandlerTest, DoNotConsumeEventsIfSequenceNotTransferred) {
  base::TimeTicks event_time = base::TimeTicks::Now();
  ui::MotionEventAndroidJava down_event = GetMotionEventAndroid(
      ui::MotionEvent::Action::DOWN, event_time, event_time, finger_pointer_);

  EXPECT_CALL(*mock_, MaybeTransferInputToViz(_))
      .WillOnce(Return(kFailureTransferring));
  EXPECT_FALSE(transfer_handler_->OnTouchEvent(down_event));

  ui::MotionEventAndroidJava move_event = GetMotionEventAndroid(
      ui::MotionEvent::Action::MOVE, event_time, event_time, finger_pointer_);
  EXPECT_FALSE(transfer_handler_->OnTouchEvent(move_event));
  EXPECT_FALSE(transfer_handler_->OnTouchEvent(move_event));

  ui::MotionEventAndroidJava cancel_event = GetMotionEventAndroid(
      ui::MotionEvent::Action::CANCEL, event_time, event_time, finger_pointer_);
  EXPECT_FALSE(transfer_handler_->OnTouchEvent(cancel_event));
}

TEST_F(InputTransferHandlerTest, DoNotConsumeNonFingerEvents) {
  for (int tool_type = 0;
       tool_type <= static_cast<int>(ui::MotionEvent::ToolType::LAST);
       tool_type++) {
    if (tool_type == static_cast<int>(ui::MotionEvent::ToolType::FINGER)) {
      continue;
    }

    base::TimeTicks event_time = base::TimeTicks::Now();
    ui::MotionEventAndroid::Pointer non_finger_pointer(0, 0, 0, 0, 0, 0, 0, 0,
                                                       0);
    non_finger_pointer.tool_type = tool_type;
    ui::MotionEventAndroidJava down_event =
        GetMotionEventAndroid(ui::MotionEvent::Action::DOWN, event_time,
                              event_time, non_finger_pointer);
    ui::MotionEventAndroidJava up_event =
        GetMotionEventAndroid(ui::MotionEvent::Action::UP, event_time,
                              event_time, non_finger_pointer);
    EXPECT_CALL(*mock_, MaybeTransferInputToViz(_)).Times(0);
    EXPECT_FALSE(transfer_handler_->OnTouchEvent(down_event));
    EXPECT_FALSE(transfer_handler_->OnTouchEvent(up_event));
  }
}

TEST_F(InputTransferHandlerTest, EmitsTransferInputToVizResultHistogram) {
  for (int transfer_result = 0;
       transfer_result <= static_cast<int>(TransferInputToVizResult::kMaxValue);
       transfer_result++) {
    base::HistogramTester histogram_tester;
    base::TimeTicks event_time = base::TimeTicks::Now();
    ui::MotionEventAndroid::Pointer pointer = finger_pointer_;
    if (transfer_result ==
        static_cast<int>(TransferInputToVizResult::kNonFingerToolType)) {
      // Arbitrary non-finger tooltype.
      pointer.tool_type = static_cast<int>(ui::MotionEvent::ToolType::STYLUS);
    }
    ui::MotionEventAndroidJava down_event = GetMotionEventAndroid(
        ui::MotionEvent::Action::DOWN, event_time, event_time, pointer);
    ui::MotionEventAndroidJava up_event = GetMotionEventAndroid(
        ui::MotionEvent::Action::CANCEL, event_time, event_time, pointer);
    if (transfer_result !=
        static_cast<int>(TransferInputToVizResult::kNonFingerToolType)) {
      EXPECT_CALL(*mock_, MaybeTransferInputToViz(_))
          .WillOnce(Return(transfer_result));
    }
    transfer_handler_->OnTouchEvent(down_event);
    transfer_handler_->OnTouchEvent(up_event);

    histogram_tester.ExpectUniqueSample(
        InputTransferHandlerAndroid::kTransferInputToVizResultHistogram,
        static_cast<TransferInputToVizResult>(transfer_result), 1);
  }
}

TEST_F(InputTransferHandlerTest, RetryTransfer) {
  const std::vector<TransferInputToVizResult> browser_handling_cases = {
      TransferInputToVizResult::kSelectionHandlesActive,
      TransferInputToVizResult::kCanTriggerBackGesture,
      TransferInputToVizResult::kImeIsActive,
      TransferInputToVizResult::kRequestedByEmbedder,
      TransferInputToVizResult::kMultipleBrowserWindowsOpen};
  base::TimeTicks event_time =
      base::TimeTicks::Now() - base::Milliseconds(1000);
  for (int transfer_result = 0;
       transfer_result <= static_cast<int>(TransferInputToVizResult::kMaxValue);
       transfer_result++) {
    event_time += base::Milliseconds(8);
    base::TimeTicks down_time = event_time;
    ui::MotionEventAndroidJava down_event = GetMotionEventAndroid(
        ui::MotionEvent::Action::DOWN, event_time, down_time, finger_pointer_);
    event_time += base::Milliseconds(8);
    ui::MotionEventAndroidJava cancel_event =
        GetMotionEventAndroid(ui::MotionEvent::Action::CANCEL, event_time,
                              down_time, finger_pointer_);

    EXPECT_CALL(*mock_, MaybeTransferInputToViz(_))
        .WillOnce(Return(kSuccessfullyTransferred));
    EXPECT_CALL(
        *input_transfer_handler_client_,
        SendStateOnTouchTransfer(_, /*browser_would_have_handled=*/false))
        .Times(1);
    EXPECT_TRUE(transfer_handler_->OnTouchEvent(down_event));
    EXPECT_TRUE(transfer_handler_->OnTouchEvent(cancel_event));

    testing::Mock::VerifyAndClearExpectations(mock_);
    testing::Mock::VerifyAndClearExpectations(
        input_transfer_handler_client_.get());

    event_time += base::Milliseconds(20);
    down_time = event_time;
    ui::MotionEventAndroidJava down_event_2 = GetMotionEventAndroid(
        ui::MotionEvent::Action::DOWN, event_time, down_time, finger_pointer_);
    event_time += base::Milliseconds(8);
    ui::MotionEventAndroidJava cancel_event_2 =
        GetMotionEventAndroid(ui::MotionEvent::Action::CANCEL, event_time,
                              down_time, finger_pointer_);

    bool should_retransfer =
        std::find(browser_handling_cases.begin(), browser_handling_cases.end(),
                  static_cast<TransferInputToVizResult>(transfer_result)) !=
        browser_handling_cases.end();
    EXPECT_CALL(*mock_, MaybeTransferInputToViz(_))
        .WillOnce(Return(static_cast<int>(transfer_result)));
    if (should_retransfer) {
      EXPECT_CALL(*mock_, TransferInputToViz(_))
          .WillOnce(Return(kSuccessfullyTransferred));
      // Expect state is sent with browser_would_have_handled set to true.
      EXPECT_CALL(
          *input_transfer_handler_client_,
          SendStateOnTouchTransfer(_, /*browser_would_have_handled=*/true))
          .Times(1);
    }

    EXPECT_TRUE(transfer_handler_->OnTouchEvent(down_event_2));
    EXPECT_TRUE(transfer_handler_->OnTouchEvent(cancel_event_2));

    event_time += base::Milliseconds(20);
    transfer_handler_->OnTouchEnd(event_time);

    testing::Mock::VerifyAndClearExpectations(mock_);
    testing::Mock::VerifyAndClearExpectations(
        input_transfer_handler_client_.get());
  }
}

TEST_F(InputTransferHandlerTest,
       DropFailedTransferSequenceWhileVizHandlesInput) {
  base::TimeTicks event_time = base::TimeTicks::Now() - base::Milliseconds(60);
  base::TimeTicks down_time = event_time;
  ui::MotionEventAndroidJava down_event_1 = GetMotionEventAndroid(
      ui::MotionEvent::Action::DOWN, event_time, down_time, finger_pointer_);
  event_time += base::Milliseconds(8);
  ui::MotionEventAndroidJava cancel_event_1 = GetMotionEventAndroid(
      ui::MotionEvent::Action::CANCEL, event_time, down_time, finger_pointer_);

  EXPECT_CALL(*mock_, MaybeTransferInputToViz(_))
      .WillOnce(Return(kSuccessfullyTransferred));
  EXPECT_TRUE(transfer_handler_->OnTouchEvent(down_event_1));
  EXPECT_TRUE(transfer_handler_->OnTouchEvent(cancel_event_1));

  event_time += base::Milliseconds(20);
  down_time = event_time;
  ui::MotionEventAndroidJava down_event_2 = GetMotionEventAndroid(
      ui::MotionEvent::Action::DOWN, event_time, down_time, finger_pointer_);
  event_time += base::Milliseconds(8);
  ui::MotionEventAndroidJava move_event_2 = GetMotionEventAndroid(
      ui::MotionEvent::Action::MOVE, event_time, down_time, finger_pointer_);
  event_time += base::Milliseconds(8);
  ui::MotionEventAndroidJava cancel_event_2 = GetMotionEventAndroid(
      ui::MotionEvent::Action::CANCEL, event_time, down_time, finger_pointer_);
  // The next sequence fails to transfer.
  EXPECT_CALL(*mock_, MaybeTransferInputToViz(_))
      .WillOnce(Return(kFailureTransferring));

  // We haven't received a notification from Viz for TouchEnd yet.
  // The new sequence should be dropped, instead of Browser and Viz
  // potentially handling the seequence at same time.
  base::HistogramTester histogram_tester;
  EXPECT_TRUE(transfer_handler_->OnTouchEvent(down_event_2));
  EXPECT_TRUE(transfer_handler_->OnTouchEvent(move_event_2));
  EXPECT_TRUE(transfer_handler_->OnTouchEvent(cancel_event_2));
  const int num_dropped_events = 3;
  histogram_tester.ExpectUniqueSample(
      InputTransferHandlerAndroid::kEventsInDroppedSequenceHistogram,
      num_dropped_events, 1);
  histogram_tester.ExpectBucketCount(
      InputTransferHandlerAndroid::kEventTypesInDroppedSequenceHistogram,
      ui::MotionEvent::Action::DOWN, 1);
  histogram_tester.ExpectBucketCount(
      InputTransferHandlerAndroid::kEventTypesInDroppedSequenceHistogram,
      ui::MotionEvent::Action::MOVE, 1);
  histogram_tester.ExpectBucketCount(
      InputTransferHandlerAndroid::kEventTypesInDroppedSequenceHistogram,
      ui::MotionEvent::Action::CANCEL, 1);
}

TEST_F(InputTransferHandlerTest,
       BrowserHandlesSequenceAfterTouchEndNotification) {
  base::TimeTicks event_time = base::TimeTicks::Now() - base::Milliseconds(60);
  base::TimeTicks down_time = event_time;
  ui::MotionEventAndroidJava down_event_1 = GetMotionEventAndroid(
      ui::MotionEvent::Action::DOWN, event_time, down_time, finger_pointer_);
  event_time += base::Milliseconds(8);
  ui::MotionEventAndroidJava cancel_event_1 = GetMotionEventAndroid(
      ui::MotionEvent::Action::CANCEL, event_time, down_time, finger_pointer_);

  EXPECT_CALL(*mock_, MaybeTransferInputToViz(_))
      .WillOnce(Return(kSuccessfullyTransferred));
  EXPECT_TRUE(transfer_handler_->OnTouchEvent(down_event_1));
  EXPECT_TRUE(transfer_handler_->OnTouchEvent(cancel_event_1));

  event_time += base::Milliseconds(8);
  transfer_handler_->OnTouchEnd(event_time);

  event_time += base::Milliseconds(20);
  down_time = event_time;
  ui::MotionEventAndroidJava down_event_2 = GetMotionEventAndroid(
      ui::MotionEvent::Action::DOWN, event_time, down_time, finger_pointer_);
  event_time += base::Milliseconds(8);
  ui::MotionEventAndroidJava move_event_2 = GetMotionEventAndroid(
      ui::MotionEvent::Action::MOVE, event_time, down_time, finger_pointer_);
  event_time += base::Milliseconds(8);
  ui::MotionEventAndroidJava cancel_event_2 = GetMotionEventAndroid(
      ui::MotionEvent::Action::CANCEL, event_time, down_time, finger_pointer_);
  // The next sequence fails to transfer.
  EXPECT_CALL(*mock_, MaybeTransferInputToViz(_))
      .WillOnce(Return(kFailureTransferring));

  EXPECT_FALSE(transfer_handler_->OnTouchEvent(down_event_2));
  EXPECT_FALSE(transfer_handler_->OnTouchEvent(move_event_2));
  EXPECT_FALSE(transfer_handler_->OnTouchEvent(cancel_event_2));
}

TEST_F(InputTransferHandlerTest, DoNotRetryTransferIfNoActiveSequence) {
  const std::vector<TransferInputToVizResult> browser_handling_cases = {
      TransferInputToVizResult::kSelectionHandlesActive,
      TransferInputToVizResult::kCanTriggerBackGesture,
      TransferInputToVizResult::kImeIsActive,
      TransferInputToVizResult::kRequestedByEmbedder,
      TransferInputToVizResult::kMultipleBrowserWindowsOpen};
  base::TimeTicks event_time =
      base::TimeTicks::Now() - base::Milliseconds(1000);
  for (int transfer_result = 0;
       transfer_result <= static_cast<int>(TransferInputToVizResult::kMaxValue);
       transfer_result++) {
    event_time += base::Milliseconds(8);
    base::TimeTicks down_time = event_time;
    ui::MotionEventAndroidJava down_event = GetMotionEventAndroid(
        ui::MotionEvent::Action::DOWN, event_time, down_time, finger_pointer_);
    event_time += base::Milliseconds(8);
    ui::MotionEventAndroidJava cancel_event =
        GetMotionEventAndroid(ui::MotionEvent::Action::CANCEL, event_time,
                              down_time, finger_pointer_);

    EXPECT_CALL(*mock_, MaybeTransferInputToViz(_))
        .WillOnce(Return(kSuccessfullyTransferred));
    EXPECT_CALL(
        *input_transfer_handler_client_,
        SendStateOnTouchTransfer(_, /*browser_would_have_handled=*/false))
        .Times(1);
    EXPECT_TRUE(transfer_handler_->OnTouchEvent(down_event));
    EXPECT_TRUE(transfer_handler_->OnTouchEvent(cancel_event));

    testing::Mock::VerifyAndClearExpectations(mock_);
    testing::Mock::VerifyAndClearExpectations(
        input_transfer_handler_client_.get());

    event_time += base::Milliseconds(8);
    transfer_handler_->OnTouchEnd(event_time);

    event_time += base::Milliseconds(20);
    down_time = event_time;
    ui::MotionEventAndroidJava down_event_2 = GetMotionEventAndroid(
        ui::MotionEvent::Action::DOWN, event_time, down_time, finger_pointer_);
    event_time += base::Milliseconds(8);
    ui::MotionEventAndroidJava cancel_event_2 =
        GetMotionEventAndroid(ui::MotionEvent::Action::CANCEL, event_time,
                              down_time, finger_pointer_);

    // Do not attempt to retransfer.
    EXPECT_CALL(*mock_, MaybeTransferInputToViz(_))
        .WillOnce(Return(static_cast<int>(transfer_result)));
    EXPECT_CALL(*mock_, TransferInputToViz(_)).Times(0);
    if (transfer_result == kSuccessfullyTransferred) {
      EXPECT_CALL(
          *input_transfer_handler_client_,
          SendStateOnTouchTransfer(_, /*browser_would_have_handled=*/false))
          .Times(1);
    } else {
      EXPECT_CALL(*input_transfer_handler_client_,
                  SendStateOnTouchTransfer(_, _))
          .Times(0);
    }

    const bool consume_sequence = transfer_result == kSuccessfullyTransferred;
    EXPECT_EQ(transfer_handler_->OnTouchEvent(down_event_2), consume_sequence);
    EXPECT_EQ(transfer_handler_->OnTouchEvent(cancel_event_2),
              consume_sequence);

    event_time += base::Milliseconds(20);
    transfer_handler_->OnTouchEnd(event_time);

    testing::Mock::VerifyAndClearExpectations(mock_);
    testing::Mock::VerifyAndClearExpectations(
        input_transfer_handler_client_.get());
  }
}

TEST_F(InputTransferHandlerTest, DownTimeAfterEventTimeBrowserHandlesSequence) {
  base::HistogramTester histogram_tester;
  base::TimeTicks event_time = base::TimeTicks::Now() - base::Milliseconds(60);
  base::TimeTicks down_time = event_time + base::Milliseconds(8);
  ui::MotionEventAndroidJava down_event = GetMotionEventAndroid(
      ui::MotionEvent::Action::DOWN, event_time, down_time, finger_pointer_);

  EXPECT_CALL(*mock_, MaybeTransferInputToViz(_)).Times(0);
  EXPECT_FALSE(transfer_handler_->OnTouchEvent(down_event));
  histogram_tester.ExpectUniqueSample(
      InputTransferHandlerAndroid::kTransferInputToVizResultHistogram,
      TransferInputToVizResult::kDownTimeAfterEventTime, 1);
}

TEST_F(InputTransferHandlerTest,
       DownTimeAfterEventTimeButActiveTouchSequenceOnViz) {
  base::TimeTicks event_time = base::TimeTicks::Now() - base::Milliseconds(60);
  base::TimeTicks down_time = event_time;
  ui::MotionEventAndroidJava down_event_1 = GetMotionEventAndroid(
      ui::MotionEvent::Action::DOWN, event_time, down_time, finger_pointer_);
  event_time += base::Milliseconds(8);
  ui::MotionEventAndroidJava cancel_event_1 = GetMotionEventAndroid(
      ui::MotionEvent::Action::CANCEL, event_time, down_time, finger_pointer_);

  EXPECT_CALL(*mock_, MaybeTransferInputToViz(_))
      .WillOnce(Return(kSuccessfullyTransferred));
  EXPECT_TRUE(transfer_handler_->OnTouchEvent(down_event_1));
  EXPECT_TRUE(transfer_handler_->OnTouchEvent(cancel_event_1));

  // Touch end hasn't been received from Viz.
  event_time += base::Milliseconds(20);
  // Down time is later than the event time of input event.
  down_time = event_time + base::Milliseconds(8);
  ui::MotionEventAndroidJava down_event_2 = GetMotionEventAndroid(
      ui::MotionEvent::Action::DOWN, event_time, down_time, finger_pointer_);
  EXPECT_TRUE(transfer_handler_->OnTouchEvent(down_event_2));
}

}  // namespace content
