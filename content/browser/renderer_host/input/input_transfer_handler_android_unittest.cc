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
              (const ui::MotionEvent&),
              (override));
};

class MockJniDelegate : public InputTransferHandlerAndroid::JniDelegate {
 public:
  ~MockJniDelegate() override = default;

  MOCK_METHOD((int), MaybeTransferInputToViz, (int, float), (override));
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
      0, 0, 0, 0, 0, 0, false, &pointer, nullptr);
}

}  // namespace

class InputTransferHandlerTest : public testing::Test {
 public:
  InputTransferHandlerTest() : finger_pointer_(0, 0, 0, 0, 0, 0, 0, 0) {}

  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(input::features::kInputOnViz);

    if (!input::IsTransferInputToVizSupported()) {
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

  EXPECT_CALL(*mock_, MaybeTransferInputToViz(_, _))
      .WillOnce(Return(kSuccessfullyTransferred));
  EXPECT_CALL(*input_transfer_handler_client_, SendStateOnTouchTransfer(_))
      .Times(1);
  EXPECT_TRUE(transfer_handler_->OnTouchEvent(down_event));

  ui::MotionEventAndroidJava move_event = GetMotionEventAndroid(
      ui::MotionEvent::Action::MOVE, event_time, event_time, finger_pointer_);
  EXPECT_TRUE(transfer_handler_->OnTouchEvent(move_event));
  EXPECT_TRUE(transfer_handler_->OnTouchEvent(move_event));

  ui::MotionEventAndroidJava cancel_event = GetMotionEventAndroid(
      ui::MotionEvent::Action::CANCEL, event_time, event_time, finger_pointer_);
  EXPECT_TRUE(transfer_handler_->OnTouchEvent(cancel_event));

  EXPECT_CALL(*mock_, MaybeTransferInputToViz(_, _))
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
    EXPECT_CALL(*mock_, MaybeTransferInputToViz(_, _))
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
    EXPECT_CALL(*mock_, MaybeTransferInputToViz(_, _))
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

  EXPECT_CALL(*mock_, MaybeTransferInputToViz(_, _))
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
    ui::MotionEventAndroid::Pointer non_finger_pointer(0, 0, 0, 0, 0, 0, 0, 0);
    non_finger_pointer.tool_type = tool_type;
    ui::MotionEventAndroidJava down_event =
        GetMotionEventAndroid(ui::MotionEvent::Action::DOWN, event_time,
                              event_time, non_finger_pointer);
    ui::MotionEventAndroidJava up_event =
        GetMotionEventAndroid(ui::MotionEvent::Action::UP, event_time,
                              event_time, non_finger_pointer);
    EXPECT_CALL(*mock_, MaybeTransferInputToViz(_, _)).Times(0);
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
      EXPECT_CALL(*mock_, MaybeTransferInputToViz(_, _))
          .WillOnce(Return(transfer_result));
    }
    transfer_handler_->OnTouchEvent(down_event);
    transfer_handler_->OnTouchEvent(up_event);

    histogram_tester.ExpectUniqueSample(
        InputTransferHandlerAndroid::kTransferInputToVizResultHistogram,
        static_cast<TransferInputToVizResult>(transfer_result), 1);
  }
}

}  // namespace content
