// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/render_widget_host_view_android.h"

#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_timeouts.h"
#include "base/test/test_trace_processor.h"
#include "components/input/features.h"
#include "components/input/utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/events/android/motion_event_android_factory.h"
#include "ui/events/android/motion_event_android_java.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/motionevent_jni_headers/MotionEvent_jni.h"

namespace content {

using ::testing::_;
using ::testing::Return;

namespace {

class MockJniDelegate : public InputTransferHandlerAndroid::JniDelegate {
 public:
  ~MockJniDelegate() override = default;

  MOCK_METHOD((int), MaybeTransferInputToViz, (int), (override));
  MOCK_METHOD((int), TransferInputToViz, (int), (override));
};

}  // namespace

class RenderWidgetHostViewAndroidBrowserTest : public ContentBrowserTest {};

class InputOnVizBrowserTest : public RenderWidgetHostViewAndroidBrowserTest {
 public:
  InputOnVizBrowserTest() {
    scoped_feature_list_.InitWithFeatureState(input::features::kInputOnViz,
                                              /* enabled= */ true);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(InputOnVizBrowserTest, TransfersStateOnTouchDown) {
  base::test::TestTraceProcessor ttp;
  ttp.StartTrace("input");
  RenderFrameSubmissionObserver render_frame_submission_observer(
      shell()->web_contents());

  EXPECT_TRUE(NavigateToURL(
      shell(), GURL("data:text/html,<!doctype html>"
                    "<body style='background-color: magenta;'></body>")));
  if (render_frame_submission_observer.render_frame_count() == 0) {
    render_frame_submission_observer.WaitForAnyFrameSubmission();
  }

  auto* view = static_cast<RenderWidgetHostViewAndroid*>(
      shell()->web_contents()->GetRenderWidgetHostView());
  ASSERT_NE(view, nullptr);
  auto* input_transfer_handler = view->GetInputTransferHandlerForTesting();
  ASSERT_EQ(!!input_transfer_handler,
            input::InputUtils::IsTransferInputToVizSupported());
  if (!input_transfer_handler) {
    return;
  }

  auto jni_delegate = std::unique_ptr<InputTransferHandlerAndroid::JniDelegate>(
      new MockJniDelegate());
  MockJniDelegate* mock_jni = static_cast<MockJniDelegate*>(jni_delegate.get());
  input_transfer_handler->set_jni_delegate_for_testing(std::move(jni_delegate));

  gfx::Point point(/*x=*/100, /*y=*/100);
  int tool_type = static_cast<int>(ui::MotionEvent::ToolType::FINGER);
  ui::MotionEventAndroid::Pointer p(0, point.x(), point.y(), 10, 0, 0, 0, 0,
                                    tool_type);
  JNIEnv* env = base::android::AttachCurrentThread();
  auto event_time = ui::EventTimeForNow();
  auto down_time_ms =
      base::TimeTicks::FromUptimeMillis(event_time.ToUptimeMillis());
  auto action = ui::MotionEvent::Action::DOWN;

  base::android::ScopedJavaLocalRef<jobject> obj =
      JNI_MotionEvent::Java_MotionEvent_obtain(
          env, /*downTime=*/0, /*eventTime=*/0, /*action=*/0, /*x=*/0, /*y=*/0,
          /*metaState=*/0);
  auto touch = ui::MotionEventAndroidFactory::CreateFromJava(
      env, obj,
      /*pix_to_dip=*/1.f,
      /*ticks_x=*/0,
      /*ticks_y=*/0,
      /*tick_multiplier=*/0,
      /*oldest_event_time=*/event_time,
      /*latest_event_time=*/event_time,
      /*down_time_ms=*/down_time_ms,
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
      /*pointer0=*/&p,
      /*pointer1=*/nullptr,
      /*is_latest_event_time_resampled=*/false);

  int successfully_transferred =
      static_cast<int>(TransferInputToVizResult::kSuccessfullyTransferred);
  EXPECT_CALL(*mock_jni, MaybeTransferInputToViz(_))
      .WillOnce(Return(successfully_transferred));
  view->OnTouchEvent(*touch);

  absl::Status status = ttp.StopAndParseTrace();
  EXPECT_TRUE(status.ok()) << status.message();

  std::string query = R"(SELECT COUNT(*) as cnt
                     FROM slice
                     WHERE slice.name =
                       'RenderWidgetHostViewAndroid::StateOnTouchTransfer')";
  auto result = ttp.RunQuery(query);

  EXPECT_TRUE(result.has_value());

  // `result.value()` would look something like this: {{"cnt"}, {"<num>"}.
  EXPECT_EQ(result.value().size(), 2u);
  EXPECT_EQ(result.value()[1].size(), 1u);
  const std::string slice_count = result.value()[1][0];
  const std::string expected_count =
      input::InputUtils::IsTransferInputToVizSupported() ? "1" : "0";
  EXPECT_EQ(slice_count, expected_count);
}

class RenderWidgetHostViewAndroidFluidResizeBrowserTest
    : public RenderWidgetHostViewAndroidBrowserTest {
 public:
  RenderWidgetHostViewAndroidFluidResizeBrowserTest() {
    feature_list_.InitAndEnableFeature(features::kFluidResize);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(RenderWidgetHostViewAndroidFluidResizeBrowserTest,
                       ResizeDefersSynchronizationToNextFrame) {
  RenderFrameSubmissionObserver render_frame_submission_observer(
      shell()->web_contents());
  EXPECT_TRUE(NavigateToURL(
      shell(), GURL("data:text/html,<!doctype html>"
                    "<body style='background-color: magenta;'></body>")));
  if (render_frame_submission_observer.render_frame_count() == 0) {
    render_frame_submission_observer.WaitForAnyFrameSubmission();
  }

  auto* view = static_cast<RenderWidgetHostViewAndroid*>(
      shell()->web_contents()->GetRenderWidgetHostView());
  ASSERT_NE(view, nullptr);
  // A single call to Animate() may not be sufficient to settle visual
  // properties. Animate() triggers a synchronization with the renderer, and the
  // renderer's response can, in turn, trigger further visual property changes
  // in the browser, re-setting `visual_properties_update_pending_`. The loop
  // ensures this cycle is complete and the state is stable before the test
  // proceeds. On each iteration, it waits for a new frame and validates that
  // one was produced to ensure progress is being made.
  auto last_rfm = render_frame_submission_observer.LastRenderFrameMetadata();
  while (view->visual_properties_update_pending_) {
    view->Animate(base::TimeTicks::Now());
    render_frame_submission_observer.WaitForAnyFrameSubmission();
    if (view->visual_properties_update_pending_) {
      CHECK(last_rfm.local_surface_id !=
            render_frame_submission_observer.LastRenderFrameMetadata()
                .local_surface_id);
      last_rfm = render_frame_submission_observer.LastRenderFrameMetadata();
    }
  }

  EXPECT_FALSE(view->visual_properties_update_pending_);

  const gfx::Size current_size_dip = view->GetViewBounds().size();
  const float scale = view->GetDeviceScaleFactor();
  const gfx::Size current_size_px =
      gfx::ScaleToCeiledSize(current_size_dip, scale);
  const gfx::Size new_size(current_size_px.width() / 2,
                           current_size_px.height() / 2);
  // Ensure we're actually resizing.
  ASSERT_NE(new_size, current_size_px);
  view->screen_state_change_handler_.OnPhysicalBackingSizeChanged(new_size, 0);

  if (view->using_browser_compositor_) {
    EXPECT_TRUE(view->visual_properties_update_pending_);
    // Confirmed visual properties update is pending. We now wait for the
    // renderer to submit a frame acknowledging the resize.
    RenderFrameSubmissionObserver frame_observer(shell()->web_contents());
    frame_observer.WaitForAnyFrameSubmission();

    // The renderer has submitted a frame, but the browser's animation tick
    // that clears the pending flag might not have run yet. To avoid a race
    // condition, we manually call Animate() to process the update.
    view->Animate(base::TimeTicks::Now());

    EXPECT_FALSE(view->visual_properties_update_pending_);
  } else {
    // Confirmed no pending visual properties update.
    EXPECT_FALSE(view->visual_properties_update_pending_);
  }
}

}  // namespace content
