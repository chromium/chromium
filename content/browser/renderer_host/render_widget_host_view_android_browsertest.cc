// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/render_widget_host_view_android.h"

#include "base/test/scoped_feature_list.h"
#include "base/test/test_timeouts.h"
#include "base/test/test_trace_processor.h"
#include "components/input/features.h"
#include "components/input/utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/events/android/motion_event_android_java.h"
#include "ui/events/base_event_utils.h"

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
  auto time_ns = (ui::EventTimeForNow() - base::TimeTicks()).InNanoseconds();
  auto action = ui::MotionEvent::Action::DOWN;
  ui::MotionEventAndroidJava touch(
      env, nullptr, 1.f, 0, 0, 0, base::TimeTicks::FromJavaNanoTime(time_ns),
      ui::MotionEventAndroid::GetAndroidAction(action), 1, 0, 0, 0, 0, 0, 0, 0,
      0, 0, false, &p, nullptr);

  int successfully_transferred =
      static_cast<int>(TransferInputToVizResult::kSuccessfullyTransferred);
  EXPECT_CALL(*mock_jni, MaybeTransferInputToViz(_))
      .WillOnce(Return(successfully_transferred));
  view->OnTouchEvent(touch);

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

}  // namespace content
