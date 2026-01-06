// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/input/input_transfer_handler_android.h"

#include <atomic>
#include <utility>
#include <vector>

#include "base/android/jni_android.h"
#include "base/memory/raw_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "components/input/features.h"
#include "components/input/utils.h"
#include "components/viz/common/input/viz_touch_state.h"
#include "content/public/test/test_renderer_host.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/input/web_touch_event.h"
#include "ui/events/android/motion_event_android_factory.h"
#include "ui/events/android/motion_event_android_java.h"
#include "ui/events/motionevent_jni_headers/MotionEvent_jni.h"
#include "ui/events/velocity_tracker/motion_event_generic.h"

namespace content {

using ::testing::_;
using ::testing::DoAll;
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

class FakeInputTransferHandlerAndroid : public InputTransferHandlerAndroid {
 public:
  explicit FakeInputTransferHandlerAndroid(
      InputTransferHandlerAndroidClient* client)
      : InputTransferHandlerAndroid(client) {}

  bool IsTouchSequencePotentiallyActiveOnViz() const override {
    if (!base::FeatureList::IsEnabled(input::features::kInputOnViz)) {
      return false;
    }
    return test_viz_touch_state_.is_sequence_active.load(
        std::memory_order_acquire);
  }

  void SetIsTouchSequencePotentiallyActiveOnViz(bool active) {
    test_viz_touch_state_.is_sequence_active.store(active,
                                                   std::memory_order_release);
  }

  const viz::VizTouchState* GetVizTouchState() const override {
    return &test_viz_touch_state_;
  }

  viz::VizTouchState* GetTestVizTouchState() { return &test_viz_touch_state_; }

 private:
  viz::VizTouchState test_viz_touch_state_;
};

std::unique_ptr<ui::MotionEventAndroid> GetMotionEventAndroid(
    ui::MotionEvent::Action action,
    base::TimeTicks event_time,
    base::TimeTicks down_time,
    const ui::MotionEventAndroid::Pointer& pointer) {
  float pix_to_dip = 1.0;

  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jobject> obj =
      JNI_MotionEvent::Java_MotionEvent_obtain(
          env, /*downTime=*/0, /*eventTime=*/0, /*action=*/0, /*x=*/0, /*y=*/0,
          /*metaState=*/0);

  return ui::MotionEventAndroidFactory::CreateFromJava(
      env, obj, pix_to_dip,
      /*ticks_x=*/0.f,
      /*ticks_y=*/0.f,
      /*tick_multiplier=*/0.f,
      /*oldest_event_time=*/event_time,
      /*latest_event_time=*/event_time,
      /*down_time_ms=*/down_time,
      /*android_action=*/ui::MotionEventAndroid::GetAndroidAction(action),
      /*pointer_count=*/1,
      /*history_size=*/0,
      /*action_index=*/0,
      /*android_action_button=*/0,
      /*android_gesture_classification=*/0,
      /*android_button_state=*/0,
      /*raw_offset_x_pixels=*/0,
      /*raw_offset_y_pixels=*/0,
      /*for_touch_handle=*/false,
      /*pointer0=*/&pointer,
      /*pointer1=*/nullptr,
      /*is_latest_event_time_resampled=*/false);
}

}  // namespace

// TODO(crbug.com/397939487): Add helper methods to generate test input
// to help simplifying test logic and use mock time source.
class InputTransferHandlerTest : public RenderViewHostTestHarness {
 public:
  explicit InputTransferHandlerTest(bool init_feature = true)
      : finger_pointer_(0, 0, 0, 0, 0, 0, 0, 0, 0),
        init_feature_(init_feature) {}

  void SetUp() override {
    RenderViewHostTestHarness::SetUp();
    if (init_feature_) {
      scoped_feature_list_.InitAndEnableFeature(input::features::kInputOnViz);
    }

    if (!input::InputUtils::IsTransferInputToVizSupported()) {
      GTEST_SKIP()
          << "The class is only used when transfer input to viz is supported.";
    }

    input_transfer_handler_client_ =
        std::make_unique<FakeInputTransferHandlerClient>();
    transfer_handler_ = std::make_unique<FakeInputTransferHandlerAndroid>(
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
    RenderViewHostTestHarness::TearDown();
  }

 protected:
  std::unique_ptr<FakeInputTransferHandlerAndroid> transfer_handler_;
  raw_ptr<MockJniDelegate> mock_;
  std::unique_ptr<FakeInputTransferHandlerClient>
      input_transfer_handler_client_;
  ui::MotionEventAndroid::Pointer finger_pointer_;
  base::test::ScopedFeatureList scoped_feature_list_;

 private:
  bool init_feature_;
};

TEST_F(InputTransferHandlerTest, ConsumeEventsIfSequenceTransferred) {
  base::TimeTicks event_time = base::TimeTicks::Now();

  auto down_event = GetMotionEventAndroid(
      ui::MotionEvent::Action::DOWN, event_time, event_time, finger_pointer_);

  EXPECT_CALL(*mock_, MaybeTransferInputToViz(_)).WillOnce([&]() {
    transfer_handler_->SetIsTouchSequencePotentiallyActiveOnViz(true);
    return kSuccessfullyTransferred;
  });
  EXPECT_CALL(*input_transfer_handler_client_,
              SendStateOnTouchTransfer(_, /*browser_would_have_handled=*/false))
      .Times(1);
  EXPECT_TRUE(transfer_handler_->OnTouchEvent(*down_event));

  auto move_event = GetMotionEventAndroid(
      ui::MotionEvent::Action::MOVE, event_time, event_time, finger_pointer_);
  EXPECT_TRUE(transfer_handler_->OnTouchEvent(*move_event));
  EXPECT_TRUE(transfer_handler_->OnTouchEvent(*move_event));

  auto cancel_event = GetMotionEventAndroid(
      ui::MotionEvent::Action::CANCEL, event_time, event_time, finger_pointer_);
  EXPECT_TRUE(transfer_handler_->OnTouchEvent(*cancel_event));

  transfer_handler_->SetIsTouchSequencePotentiallyActiveOnViz(false);

  EXPECT_CALL(*mock_, MaybeTransferInputToViz(_))
      .WillOnce(Return(kFailureTransferring));
  // New events shouldn't be consumed due the expectation set in line above.
  EXPECT_FALSE(transfer_handler_->OnTouchEvent(*down_event));
  EXPECT_FALSE(transfer_handler_->OnTouchEvent(*move_event));
}

TEST_F(InputTransferHandlerTest, SequenceTransferredBackFromViz) {
  base::HistogramTester histogram_tester;

  base::TimeTicks event_time = base::TimeTicks::Now();
  base::TimeTicks down_time = event_time;
  // Sequence 1.
  auto down_event = GetMotionEventAndroid(
      ui::MotionEvent::Action::DOWN, event_time, down_time, finger_pointer_);
  auto cancel_event = GetMotionEventAndroid(
      ui::MotionEvent::Action::CANCEL, event_time, down_time, finger_pointer_);

  // Assume that a touch sequence is potentially active on Viz.
  transfer_handler_->SetIsTouchSequencePotentiallyActiveOnViz(true);

  EXPECT_CALL(*mock_, MaybeTransferInputToViz(_))
      .WillOnce(
          Return(static_cast<int>(TransferInputToVizResult::kImeIsActive)));
  EXPECT_CALL(*input_transfer_handler_client_,
              SendStateOnTouchTransfer(_, /*browser_would_have_handled=*/true))
      .Times(1);
  EXPECT_TRUE(transfer_handler_->OnTouchEvent(*down_event));
  EXPECT_TRUE(transfer_handler_->OnTouchEvent(*cancel_event));

  // Simulate Viz transferring this sequence back by setting the shared memory.
  transfer_handler_->GetTestVizTouchState()
      ->last_transferred_back_down_time_ms.store(
          down_event->GetRawDownTime().ToUptimeMillis(),
          std::memory_order_release);

  event_time += base::Milliseconds(8);
  // Transferred back Sequence1 from Viz.
  auto down_event2 = GetMotionEventAndroid(
      ui::MotionEvent::Action::DOWN, event_time, down_time, finger_pointer_);

  EXPECT_CALL(*mock_, MaybeTransferInputToViz(_)).Times(0);
  // Transferred back sequence handled on browser.
  EXPECT_FALSE(transfer_handler_->OnTouchEvent(*down_event));

  histogram_tester.ExpectBucketCount(
      InputTransferHandlerAndroid::kTransferInputToVizResultHistogram,
      TransferInputToVizResult::kSequenceTransferredBackFromViz, 1);
}

TEST_F(InputTransferHandlerTest, EmitsTouchMovesSeenAfterTransferHistogram) {
  base::TimeTicks event_time = base::TimeTicks::Now();
  auto down_event = GetMotionEventAndroid(
      ui::MotionEvent::Action::DOWN, event_time, event_time, finger_pointer_);
  auto move_event = GetMotionEventAndroid(
      ui::MotionEvent::Action::MOVE, event_time, event_time, finger_pointer_);
  auto cancel_event = GetMotionEventAndroid(
      ui::MotionEvent::Action::CANCEL, event_time, event_time, finger_pointer_);

  for (int touch_moves_seen = 0; touch_moves_seen <= 2; touch_moves_seen++) {
    base::HistogramTester histogram_tester;
    EXPECT_CALL(*mock_, MaybeTransferInputToViz(_))
        .WillOnce(Return(kSuccessfullyTransferred));
    EXPECT_TRUE(transfer_handler_->OnTouchEvent(*down_event));
    for (int ind = 1; ind <= touch_moves_seen; ind++) {
      EXPECT_TRUE(transfer_handler_->OnTouchEvent(*move_event));
    }
    EXPECT_TRUE(transfer_handler_->OnTouchEvent(*cancel_event));
    histogram_tester.ExpectUniqueSample(
        InputTransferHandlerAndroid::kTouchMovesSeenHistogram, touch_moves_seen,
        1);
  }
}

TEST_F(InputTransferHandlerTest, EmitsEventsAfterTransferHistogram) {
  base::TimeTicks event_time = base::TimeTicks::Now();
  auto down_event = GetMotionEventAndroid(
      ui::MotionEvent::Action::DOWN, event_time, event_time, finger_pointer_);
  auto move_event = GetMotionEventAndroid(
      ui::MotionEvent::Action::MOVE, event_time, event_time, finger_pointer_);
  auto cancel_event = GetMotionEventAndroid(
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
    EXPECT_TRUE(transfer_handler_->OnTouchEvent(*down_event));

    auto event = GetMotionEventAndroid(event_action, event_time, event_time,
                                       finger_pointer_);
    EXPECT_TRUE(transfer_handler_->OnTouchEvent(*event));
    histogram_tester.ExpectUniqueSample(
        InputTransferHandlerAndroid::kEventsAfterTransferHistogram,
        expected_histogram_sample, 1);

    EXPECT_TRUE(transfer_handler_->OnTouchEvent(*cancel_event));
  }
}

TEST_F(InputTransferHandlerTest, DoNotConsumeEventsIfSequenceNotTransferred) {
  base::TimeTicks event_time = base::TimeTicks::Now();
  auto down_event = GetMotionEventAndroid(
      ui::MotionEvent::Action::DOWN, event_time, event_time, finger_pointer_);

  EXPECT_CALL(*mock_, MaybeTransferInputToViz(_))
      .WillOnce(Return(kFailureTransferring));
  EXPECT_FALSE(transfer_handler_->OnTouchEvent(*down_event));

  auto move_event = GetMotionEventAndroid(
      ui::MotionEvent::Action::MOVE, event_time, event_time, finger_pointer_);
  EXPECT_FALSE(transfer_handler_->OnTouchEvent(*move_event));
  EXPECT_FALSE(transfer_handler_->OnTouchEvent(*move_event));

  auto cancel_event = GetMotionEventAndroid(
      ui::MotionEvent::Action::CANCEL, event_time, event_time, finger_pointer_);
  EXPECT_FALSE(transfer_handler_->OnTouchEvent(*cancel_event));
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
    auto down_event =
        GetMotionEventAndroid(ui::MotionEvent::Action::DOWN, event_time,
                              event_time, non_finger_pointer);
    auto up_event =
        GetMotionEventAndroid(ui::MotionEvent::Action::UP, event_time,
                              event_time, non_finger_pointer);
    EXPECT_CALL(*mock_, MaybeTransferInputToViz(_)).Times(0);
    EXPECT_FALSE(transfer_handler_->OnTouchEvent(*down_event));
    EXPECT_FALSE(transfer_handler_->OnTouchEvent(*up_event));
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
    auto down_event = GetMotionEventAndroid(ui::MotionEvent::Action::DOWN,
                                            event_time, event_time, pointer);
    auto up_event = GetMotionEventAndroid(ui::MotionEvent::Action::CANCEL,
                                          event_time, event_time, pointer);
    if (transfer_result !=
        static_cast<int>(TransferInputToVizResult::kNonFingerToolType)) {
      EXPECT_CALL(*mock_, MaybeTransferInputToViz(_))
          .WillOnce(Return(transfer_result));
    }
    transfer_handler_->OnTouchEvent(*down_event);
    transfer_handler_->OnTouchEvent(*up_event);

    histogram_tester.ExpectUniqueSample(
        InputTransferHandlerAndroid::kTransferInputToVizResultHistogram,
        static_cast<TransferInputToVizResult>(transfer_result), 1);
  }
}

TEST_F(InputTransferHandlerTest, RetryTransfer) {
  const std::vector<TransferInputToVizResult> browser_handling_cases = {
      TransferInputToVizResult::kSelectionHandlesActive,
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
    auto down_event = GetMotionEventAndroid(
        ui::MotionEvent::Action::DOWN, event_time, down_time, finger_pointer_);
    event_time += base::Milliseconds(8);
    auto cancel_event =
        GetMotionEventAndroid(ui::MotionEvent::Action::CANCEL, event_time,
                              down_time, finger_pointer_);

    EXPECT_CALL(*mock_, MaybeTransferInputToViz(_)).WillOnce([&]() {
      transfer_handler_->SetIsTouchSequencePotentiallyActiveOnViz(true);
      return kSuccessfullyTransferred;
    });
    EXPECT_CALL(
        *input_transfer_handler_client_,
        SendStateOnTouchTransfer(_, /*browser_would_have_handled=*/false))
        .Times(1);
    EXPECT_TRUE(transfer_handler_->OnTouchEvent(*down_event));
    EXPECT_TRUE(transfer_handler_->OnTouchEvent(*cancel_event));

    testing::Mock::VerifyAndClearExpectations(mock_);
    testing::Mock::VerifyAndClearExpectations(
        input_transfer_handler_client_.get());

    event_time += base::Milliseconds(20);
    down_time = event_time;
    auto down_event_2 = GetMotionEventAndroid(
        ui::MotionEvent::Action::DOWN, event_time, down_time, finger_pointer_);
    event_time += base::Milliseconds(8);
    auto cancel_event_2 =
        GetMotionEventAndroid(ui::MotionEvent::Action::CANCEL, event_time,
                              down_time, finger_pointer_);

    bool should_retransfer =
        std::find(browser_handling_cases.begin(), browser_handling_cases.end(),
                  static_cast<TransferInputToVizResult>(transfer_result)) !=
        browser_handling_cases.end();
    EXPECT_CALL(*mock_, MaybeTransferInputToViz(_))
        .WillOnce(Return(static_cast<int>(transfer_result)));
    if (should_retransfer) {
      EXPECT_CALL(*mock_, TransferInputToViz(_)).WillOnce([&]() {
        transfer_handler_->SetIsTouchSequencePotentiallyActiveOnViz(true);
        return kSuccessfullyTransferred;
      });
      // Expect state is sent with browser_would_have_handled set to true.
      EXPECT_CALL(
          *input_transfer_handler_client_,
          SendStateOnTouchTransfer(_, /*browser_would_have_handled=*/true))
          .Times(1);
    }

    EXPECT_TRUE(transfer_handler_->OnTouchEvent(*down_event_2));
    EXPECT_TRUE(transfer_handler_->OnTouchEvent(*cancel_event_2));

    // Now Viz sees a touch end.
    transfer_handler_->SetIsTouchSequencePotentiallyActiveOnViz(false);

    testing::Mock::VerifyAndClearExpectations(mock_);
    testing::Mock::VerifyAndClearExpectations(
        input_transfer_handler_client_.get());
  }
}

TEST_F(InputTransferHandlerTest,
       DropFailedTransferSequenceWhileVizHandlesInput) {
  base::TimeTicks event_time = base::TimeTicks::Now() - base::Milliseconds(60);
  base::TimeTicks down_time = event_time;
  auto down_event_1 = GetMotionEventAndroid(
      ui::MotionEvent::Action::DOWN, event_time, down_time, finger_pointer_);
  event_time += base::Milliseconds(8);
  auto cancel_event_1 = GetMotionEventAndroid(
      ui::MotionEvent::Action::CANCEL, event_time, down_time, finger_pointer_);

  EXPECT_CALL(*mock_, MaybeTransferInputToViz(_)).WillOnce([&]() {
    transfer_handler_->SetIsTouchSequencePotentiallyActiveOnViz(true);
    return kSuccessfullyTransferred;
  });
  EXPECT_TRUE(transfer_handler_->OnTouchEvent(*down_event_1));
  EXPECT_TRUE(transfer_handler_->OnTouchEvent(*cancel_event_1));

  event_time += base::Milliseconds(20);
  down_time = event_time;
  auto down_event_2 = GetMotionEventAndroid(
      ui::MotionEvent::Action::DOWN, event_time, down_time, finger_pointer_);
  event_time += base::Milliseconds(8);
  auto move_event_2 = GetMotionEventAndroid(
      ui::MotionEvent::Action::MOVE, event_time, down_time, finger_pointer_);
  event_time += base::Milliseconds(8);
  auto cancel_event_2 = GetMotionEventAndroid(
      ui::MotionEvent::Action::CANCEL, event_time, down_time, finger_pointer_);
  // The next sequence fails to transfer.
  EXPECT_CALL(*mock_, MaybeTransferInputToViz(_))
      .WillOnce(Return(kFailureTransferring));

  // We haven't received a notification from Viz for TouchEnd yet.
  // The new sequence should be dropped, instead of Browser and Viz
  // potentially handling the seequence at same time.
  base::HistogramTester histogram_tester;
  EXPECT_TRUE(transfer_handler_->OnTouchEvent(*down_event_2));
  EXPECT_TRUE(transfer_handler_->OnTouchEvent(*move_event_2));
  EXPECT_TRUE(transfer_handler_->OnTouchEvent(*cancel_event_2));
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
  auto down_event_1 = GetMotionEventAndroid(
      ui::MotionEvent::Action::DOWN, event_time, down_time, finger_pointer_);
  event_time += base::Milliseconds(8);
  auto cancel_event_1 = GetMotionEventAndroid(
      ui::MotionEvent::Action::CANCEL, event_time, down_time, finger_pointer_);

  EXPECT_CALL(*mock_, MaybeTransferInputToViz(_)).WillOnce([&]() {
    transfer_handler_->SetIsTouchSequencePotentiallyActiveOnViz(true);
    return kSuccessfullyTransferred;
  });
  EXPECT_TRUE(transfer_handler_->OnTouchEvent(*down_event_1));
  EXPECT_TRUE(transfer_handler_->OnTouchEvent(*cancel_event_1));

  transfer_handler_->SetIsTouchSequencePotentiallyActiveOnViz(false);

  event_time += base::Milliseconds(20);
  down_time = event_time;
  auto down_event_2 = GetMotionEventAndroid(
      ui::MotionEvent::Action::DOWN, event_time, down_time, finger_pointer_);
  event_time += base::Milliseconds(8);
  auto move_event_2 = GetMotionEventAndroid(
      ui::MotionEvent::Action::MOVE, event_time, down_time, finger_pointer_);
  event_time += base::Milliseconds(8);
  auto cancel_event_2 = GetMotionEventAndroid(
      ui::MotionEvent::Action::CANCEL, event_time, down_time, finger_pointer_);
  // The next sequence fails to transfer.
  EXPECT_CALL(*mock_, MaybeTransferInputToViz(_))
      .WillOnce(Return(kFailureTransferring));

  EXPECT_FALSE(transfer_handler_->OnTouchEvent(*down_event_2));
  EXPECT_FALSE(transfer_handler_->OnTouchEvent(*move_event_2));
  EXPECT_FALSE(transfer_handler_->OnTouchEvent(*cancel_event_2));
}

TEST_F(InputTransferHandlerTest, DoNotRetryTransferIfNoActiveSequence) {
  const std::vector<TransferInputToVizResult> browser_handling_cases = {
      TransferInputToVizResult::kSelectionHandlesActive,
      TransferInputToVizResult::kImeIsActive,
      TransferInputToVizResult::kRequestedByEmbedder,
      TransferInputToVizResult::kMultipleBrowserWindowsOpen};
  // Use large enough offset(2000ms) here such that the event times don't go in
  // future, as more events are synthesized in loop below. Event time gets
  // incremented by 8ms below, every time an event is synthesized.
  base::TimeTicks event_time =
      base::TimeTicks::Now() - base::Milliseconds(2000);
  for (int transfer_result = 0;
       transfer_result <= static_cast<int>(TransferInputToVizResult::kMaxValue);
       transfer_result++) {
    event_time += base::Milliseconds(8);
    base::TimeTicks down_time = event_time;
    auto down_event = GetMotionEventAndroid(
        ui::MotionEvent::Action::DOWN, event_time, down_time, finger_pointer_);
    event_time += base::Milliseconds(8);
    auto cancel_event =
        GetMotionEventAndroid(ui::MotionEvent::Action::CANCEL, event_time,
                              down_time, finger_pointer_);

    EXPECT_CALL(*mock_, MaybeTransferInputToViz(_)).WillOnce([&]() {
      transfer_handler_->SetIsTouchSequencePotentiallyActiveOnViz(true);
      return kSuccessfullyTransferred;
    });
    EXPECT_CALL(
        *input_transfer_handler_client_,
        SendStateOnTouchTransfer(_, /*browser_would_have_handled=*/false))
        .Times(1);
    EXPECT_TRUE(transfer_handler_->OnTouchEvent(*down_event));
    EXPECT_TRUE(transfer_handler_->OnTouchEvent(*cancel_event));

    testing::Mock::VerifyAndClearExpectations(mock_);
    testing::Mock::VerifyAndClearExpectations(
        input_transfer_handler_client_.get());

    // Reset the state since we've seen a TouchEnd on Viz.
    transfer_handler_->SetIsTouchSequencePotentiallyActiveOnViz(false);

    event_time += base::Milliseconds(20);
    down_time = event_time;
    auto down_event_2 = GetMotionEventAndroid(
        ui::MotionEvent::Action::DOWN, event_time, down_time, finger_pointer_);
    event_time += base::Milliseconds(8);
    auto cancel_event_2 =
        GetMotionEventAndroid(ui::MotionEvent::Action::CANCEL, event_time,
                              down_time, finger_pointer_);

    // Do not attempt to retransfer.
    EXPECT_CALL(*mock_, MaybeTransferInputToViz(_))
        .WillOnce(Return(static_cast<int>(transfer_result)));
    EXPECT_CALL(*mock_, TransferInputToViz(_)).Times(0);
    if (transfer_result == kSuccessfullyTransferred) {
      // Sequence successfully transferred to Viz, update the state.
      transfer_handler_->SetIsTouchSequencePotentiallyActiveOnViz(true);
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
    EXPECT_EQ(transfer_handler_->OnTouchEvent(*down_event_2), consume_sequence);
    EXPECT_EQ(transfer_handler_->OnTouchEvent(*cancel_event_2),
              consume_sequence);

    transfer_handler_->SetIsTouchSequencePotentiallyActiveOnViz(false);

    testing::Mock::VerifyAndClearExpectations(mock_);
    testing::Mock::VerifyAndClearExpectations(
        input_transfer_handler_client_.get());
  }
}

TEST_F(InputTransferHandlerTest, AcceptsNewSequenceAfterBrowserCancel) {
  base::TimeTicks event_time =
      base::TimeTicks::Now() - base::Milliseconds(1000);
  auto down_event = GetMotionEventAndroid(
      ui::MotionEvent::Action::DOWN, event_time, event_time, finger_pointer_);

  // Start a sequence and transfer it.
  EXPECT_CALL(*mock_, MaybeTransferInputToViz(_)).WillOnce([&]() {
    return kSuccessfullyTransferred;
  });
  EXPECT_CALL(*input_transfer_handler_client_,
              SendStateOnTouchTransfer(_, /*browser_would_have_handled=*/false))
      .Times(1);
  EXPECT_TRUE(transfer_handler_->OnTouchEvent(*down_event));

  event_time += base::Milliseconds(8);
  // Verify that subsequent events are consumed (transferred state).
  auto move_event = GetMotionEventAndroid(
      ui::MotionEvent::Action::MOVE, event_time, event_time, finger_pointer_);
  EXPECT_TRUE(transfer_handler_->OnTouchEvent(*move_event));

  event_time += base::Milliseconds(8);
  // Simulate receiving a TouchCancel from the Browser process (e.g. timeout).
  // This should reset the handler state.
  blink::WebTouchEvent touch_cancel(blink::WebInputEvent::Type::kTouchCancel,
                                    blink::WebInputEvent::kNoModifiers,
                                    event_time);
  transfer_handler_->GetInputObserver().OnInputEvent(
      *rvh()->GetWidget(), touch_cancel, input::InputEventSource::kBrowser);

  // Now start a NEW sequence. It should be processed normally (attempt
  // transfer), not dropped or auto-consumed as part of the old sequence.
  // Advance time enough to be distinct, and ensure event time > down time to
  // avoid negative delta checks.
  event_time += base::Milliseconds(16);
  auto down_event_2 = GetMotionEventAndroid(
      ui::MotionEvent::Action::DOWN, event_time, event_time, finger_pointer_);

  EXPECT_CALL(*mock_, MaybeTransferInputToViz(_)).WillOnce([&]() {
    return kSuccessfullyTransferred;
  });
  EXPECT_CALL(*input_transfer_handler_client_,
              SendStateOnTouchTransfer(_, /*browser_would_have_handled=*/false))
      .Times(1);

  // If reset worked, this returns true because it's a new successful transfer,
  // NOT because it's "consuming events until cancel" from the old sequence.
  // The Mock expectation above proves we entered the transfer logic again.
  EXPECT_TRUE(transfer_handler_->OnTouchEvent(*down_event_2));
}

TEST_F(InputTransferHandlerTest, DoNotResetOnVizCancel) {
  base::TimeTicks event_time = base::TimeTicks::Now();
  auto down_event = GetMotionEventAndroid(
      ui::MotionEvent::Action::DOWN, event_time, event_time, finger_pointer_);

  // Start a sequence and transfer it.
  EXPECT_CALL(*mock_, MaybeTransferInputToViz(_)).WillOnce([&]() {
    transfer_handler_->SetIsTouchSequencePotentiallyActiveOnViz(true);
    return kSuccessfullyTransferred;
  });
  EXPECT_TRUE(transfer_handler_->OnTouchEvent(*down_event));

  event_time += base::Milliseconds(8);
  // Simulate receiving a TouchCancel from the VIZ process.
  // This should NOT reset the handler state (it waits for local cancel).
  blink::WebTouchEvent touch_cancel(blink::WebInputEvent::Type::kTouchCancel,
                                    blink::WebInputEvent::kNoModifiers,
                                    event_time);
  transfer_handler_->GetInputObserver().OnInputEvent(
      *rvh()->GetWidget(), touch_cancel, input::InputEventSource::kViz);

  event_time += base::Milliseconds(8);
  // New events should still be consumed as part of the transferred sequence
  // (or dropped if we consider the sequence dead, but the key is we didn't
  // reset). Actually, if we didn't reset, we are still in
  // kConsumeEventsUntilCancel.
  auto move_event = GetMotionEventAndroid(
      ui::MotionEvent::Action::MOVE, event_time, event_time, finger_pointer_);
  EXPECT_TRUE(transfer_handler_->OnTouchEvent(*move_event));
}

class DownTimeAfterEventTimeTest
    : public InputTransferHandlerTest,
      public ::testing::WithParamInterface<std::string> {
 public:
  DownTimeAfterEventTimeTest()
      : InputTransferHandlerTest(/* init_feature= */ false) {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        input::features::kInputOnViz,
        {{input::features::kTransferSequencesWithAbnormalDownTime.name,
          GetParam()}});
  }
};

TEST_P(DownTimeAfterEventTimeTest, TransferBasic) {
  base::HistogramTester histogram_tester;
  base::TimeTicks event_time = base::TimeTicks::Now() - base::Milliseconds(60);
  base::TimeTicks down_time = event_time + base::Milliseconds(8);
  auto abnormal_down_event = GetMotionEventAndroid(
      ui::MotionEvent::Action::DOWN, event_time, down_time, finger_pointer_);

  const bool expected_transfer = GetParam() == "true";
  if (expected_transfer) {
    EXPECT_CALL(*mock_, MaybeTransferInputToViz(_)).WillOnce([&]() {
      transfer_handler_->SetIsTouchSequencePotentiallyActiveOnViz(true);
      return kSuccessfullyTransferred;
    });
  } else {
    EXPECT_CALL(*mock_, MaybeTransferInputToViz(_)).Times(0);
  }
  EXPECT_EQ(transfer_handler_->OnTouchEvent(*abnormal_down_event),
            expected_transfer);
}

TEST_P(DownTimeAfterEventTimeTest, TransferWhileActiveTouchSequenceOnViz) {
  base::TimeTicks event_time = base::TimeTicks::Now() - base::Milliseconds(60);
  base::TimeTicks down_time = event_time;
  auto down_event_1 = GetMotionEventAndroid(
      ui::MotionEvent::Action::DOWN, event_time, down_time, finger_pointer_);
  event_time += base::Milliseconds(8);
  auto cancel_event_1 = GetMotionEventAndroid(
      ui::MotionEvent::Action::CANCEL, event_time, down_time, finger_pointer_);

  EXPECT_CALL(*mock_, MaybeTransferInputToViz(_)).WillOnce([&]() {
    transfer_handler_->SetIsTouchSequencePotentiallyActiveOnViz(true);
    return kSuccessfullyTransferred;
  });
  EXPECT_TRUE(transfer_handler_->OnTouchEvent(*down_event_1));
  EXPECT_TRUE(transfer_handler_->OnTouchEvent(*cancel_event_1));

  // Touch end hasn't been received from Viz.
  event_time += base::Milliseconds(20);
  // Down time is later than the event time of input event.
  down_time = event_time + base::Milliseconds(8);
  auto down_event_2 = GetMotionEventAndroid(
      ui::MotionEvent::Action::DOWN, event_time, down_time, finger_pointer_);
  const bool expected_transfer = GetParam() == "true";
  if (expected_transfer) {
    EXPECT_CALL(*mock_, MaybeTransferInputToViz(_)).WillOnce([&]() {
      transfer_handler_->SetIsTouchSequencePotentiallyActiveOnViz(true);
      return kSuccessfullyTransferred;
    });
  } else {
    EXPECT_CALL(*mock_, MaybeTransferInputToViz(_)).Times(0);
  }
  // In both cases InputTransferHandler consumes the event. Just that the
  // sequence is dropped when `kTransferSequencesWithAbnormalDownTime` is false.
  EXPECT_TRUE(transfer_handler_->OnTouchEvent(*down_event_2));
}

TEST_P(DownTimeAfterEventTimeTest, TouchEndEventTimeIsLessThanDownTime) {
  base::TimeTicks event_time = base::TimeTicks::Now() - base::Milliseconds(60);
  // Down time is later than the event time of input event.
  base::TimeTicks down_time = event_time + base::Milliseconds(8);

  auto down_event = GetMotionEventAndroid(
      ui::MotionEvent::Action::DOWN, event_time, down_time, finger_pointer_);

  const bool expected_transfer = GetParam() == "true";
  if (expected_transfer) {
    EXPECT_CALL(*mock_, MaybeTransferInputToViz(_)).WillOnce([&]() {
      transfer_handler_->SetIsTouchSequencePotentiallyActiveOnViz(true);
      return kSuccessfullyTransferred;
    });
  } else {
    EXPECT_CALL(*mock_, MaybeTransferInputToViz(_)).Times(0);
  }

  EXPECT_EQ(transfer_handler_->OnTouchEvent(*down_event), expected_transfer);

  if (!expected_transfer) {
    return;
  }

  // After a successful transfer, the test should simulate that Viz is now
  // handling the touch sequence.
  transfer_handler_->SetIsTouchSequencePotentiallyActiveOnViz(true);
  EXPECT_TRUE(transfer_handler_->IsTouchSequencePotentiallyActiveOnViz());

  auto cancel_event = GetMotionEventAndroid(
      ui::MotionEvent::Action::CANCEL, event_time, down_time, finger_pointer_);
  EXPECT_TRUE(transfer_handler_->OnTouchEvent(*cancel_event));

  EXPECT_TRUE(transfer_handler_->IsTouchSequencePotentiallyActiveOnViz());

  // Simulate the touch sequence ending on Viz.
  transfer_handler_->SetIsTouchSequencePotentiallyActiveOnViz(false);
  EXPECT_FALSE(transfer_handler_->IsTouchSequencePotentiallyActiveOnViz());
}

INSTANTIATE_TEST_SUITE_P(DownTimeAfterEventTimeTest,
                         DownTimeAfterEventTimeTest,
                         ::testing::Values("false", "true"));

}  // namespace content
