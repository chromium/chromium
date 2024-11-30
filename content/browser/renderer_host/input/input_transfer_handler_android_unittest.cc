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
#include "ui/events/velocity_tracker/motion_event_generic.h"

namespace content {

using ::testing::_;
using ::testing::Return;

namespace {

class FakeInputTransferHandlerClient final
    : public InputTransferHandlerAndroidClient {
  gpu::SurfaceHandle GetRootSurfaceHandle() override {
    return gpu::kNullSurfaceHandle;
  }
};

class MockJniDelegate : public InputTransferHandlerAndroid::JniDelegate {
 public:
  ~MockJniDelegate() override = default;

  MOCK_METHOD((bool), MaybeTransferInputToViz, (int), (override));
};

}  // namespace

class InputTransferHandlerTest : public testing::Test {
 public:
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
  }

  void TearDown() override {
    transfer_handler_.reset();
    input_transfer_handler_client_.reset();
  }

 protected:
  std::unique_ptr<InputTransferHandlerAndroid> transfer_handler_;
  raw_ptr<MockJniDelegate> mock_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<FakeInputTransferHandlerClient>
      input_transfer_handler_client_;
};

TEST_F(InputTransferHandlerTest, ConsumeEventsIfSequenceTransferred) {
  base::TimeTicks event_time = base::TimeTicks::Now();
  ui::MotionEventGeneric down_event(ui::MotionEvent::Action::DOWN, event_time,
                                    ui::PointerProperties());

  EXPECT_CALL(*mock_, MaybeTransferInputToViz(_)).WillOnce(Return(true));
  EXPECT_TRUE(transfer_handler_->OnTouchEvent(down_event));

  ui::MotionEventGeneric move_event(ui::MotionEvent::Action::MOVE, event_time,
                                    ui::PointerProperties());
  EXPECT_TRUE(transfer_handler_->OnTouchEvent(move_event));
  EXPECT_TRUE(transfer_handler_->OnTouchEvent(move_event));

  ui::MotionEventGeneric cancel_event(ui::MotionEvent::Action::CANCEL,
                                      event_time, ui::PointerProperties());
  EXPECT_TRUE(transfer_handler_->OnTouchEvent(cancel_event));

  EXPECT_CALL(*mock_, MaybeTransferInputToViz(_)).WillOnce(Return(false));
  // New events shouldn't be consumed due the expectation set in line above.
  EXPECT_FALSE(transfer_handler_->OnTouchEvent(down_event));
  EXPECT_FALSE(transfer_handler_->OnTouchEvent(move_event));
}

TEST_F(InputTransferHandlerTest, EmitsTouchMovesSeenAfterTransferHistogram) {
  base::TimeTicks event_time = base::TimeTicks::Now();
  ui::MotionEventGeneric down_event(ui::MotionEvent::Action::DOWN, event_time,
                                    ui::PointerProperties());
  ui::MotionEventGeneric move_event(ui::MotionEvent::Action::MOVE, event_time,
                                    ui::PointerProperties());
  ui::MotionEventGeneric cancel_event(ui::MotionEvent::Action::CANCEL,
                                      event_time, ui::PointerProperties());

  for (int touch_moves_seen = 0; touch_moves_seen <= 2; touch_moves_seen++) {
    base::HistogramTester histogram_tester;
    EXPECT_CALL(*mock_, MaybeTransferInputToViz(_)).WillOnce(Return(true));
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
  ui::MotionEventGeneric down_event(ui::MotionEvent::Action::DOWN, event_time,
                                    ui::PointerProperties());
  ui::MotionEventGeneric move_event(ui::MotionEvent::Action::MOVE, event_time,
                                    ui::PointerProperties());
  ui::MotionEventGeneric cancel_event(ui::MotionEvent::Action::CANCEL,
                                      event_time, ui::PointerProperties());

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
    EXPECT_CALL(*mock_, MaybeTransferInputToViz(_)).WillOnce(Return(true));
    EXPECT_TRUE(transfer_handler_->OnTouchEvent(down_event));

    ui::MotionEventGeneric event(event_action, event_time,
                                 ui::PointerProperties());
    EXPECT_TRUE(transfer_handler_->OnTouchEvent(event));
    histogram_tester.ExpectUniqueSample(
        InputTransferHandlerAndroid::kEventsAfterTransferHistogram,
        expected_histogram_sample, 1);

    EXPECT_TRUE(transfer_handler_->OnTouchEvent(cancel_event));
  }
}

TEST_F(InputTransferHandlerTest, DoNotConsumeEventsIfSequenceNotTransferred) {
  base::TimeTicks event_time = base::TimeTicks::Now();
  ui::MotionEventGeneric down_event(ui::MotionEvent::Action::DOWN, event_time,
                                    ui::PointerProperties());

  EXPECT_CALL(*mock_, MaybeTransferInputToViz(_)).WillOnce(Return(false));
  EXPECT_FALSE(transfer_handler_->OnTouchEvent(down_event));

  ui::MotionEventGeneric move_event(ui::MotionEvent::Action::MOVE, event_time,
                                    ui::PointerProperties());
  EXPECT_FALSE(transfer_handler_->OnTouchEvent(move_event));
  EXPECT_FALSE(transfer_handler_->OnTouchEvent(move_event));

  ui::MotionEventGeneric cancel_event(ui::MotionEvent::Action::CANCEL,
                                      event_time, ui::PointerProperties());
  EXPECT_FALSE(transfer_handler_->OnTouchEvent(cancel_event));
}

}  // namespace content
