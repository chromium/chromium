// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/capture/mouse_cursor_overlay_controller_unittest.h"

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "content/browser/media/capture/mouse_cursor_overlay_controller.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "content/test/test_web_contents.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features_generated.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"

#if BUILDFLAG(IS_ANDROID)
#include "ui/android/event_forwarder.h"
#include "ui/android/view_android.h"
#include "ui/events/android/motion_event_android.h"
#elif defined(USE_AURA)
#include "ui/aura/window.h"
#include "ui/events/test/event_generator.h"
#endif

namespace content {

using testing::_;
using testing::Mock;

MockOverlay::MockOverlay() = default;
MockOverlay::~MockOverlay() = default;

MouseCursorOverlayControllerTestBase::MouseCursorOverlayControllerTestBase()
    : RenderViewHostTestHarness(
          base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
  scoped_feature_list_.InitWithFeatures({blink::features::kCapturedMouseEvents},
                                        {});
}

MouseCursorOverlayControllerTestBase::~MouseCursorOverlayControllerTestBase() =
    default;

void MouseCursorOverlayControllerTestBase::RunRestrictsToWebContentsTest() {
  MouseCursorOverlayController controller;

  auto target_web_contents =
      TestWebContents::Create(browser_context(), nullptr);

  const gfx::Rect target_bounds(10, 40, 50, 50);
  SetupCaptureTarget(target_web_contents.get(), target_bounds);

  controller.SetTargetView(GetTargetView(), target_web_contents.get());

  auto overlay_ptr = std::make_unique<MockOverlay>();
  MockOverlay* overlay = overlay_ptr.get();

  controller.Start(std::move(overlay_ptr),
                   base::SingleThreadTaskRunner::GetCurrentDefault());

  InitializeEventGenerator();

  // Inside bounds.
  gfx::Point mouse_pos(20, 45);
  gfx::Point expected_pos =
      GetExpectedCapturedPosition(mouse_pos, target_bounds);
  EXPECT_CALL(*overlay, OnCapturedMouseEvent(expected_pos)).Times(1);
  SendMouseMove(mouse_pos);
  task_environment()->FastForwardBy(base::Seconds(1));
  Mock::VerifyAndClearExpectations(overlay);

  // Outside bounds.
  gfx::Point mouse_pos_outside(60, 60);
  EXPECT_CALL(*overlay, OnCapturedMouseEvent(
                            MouseCursorOverlayController::kOutsideSurface))
      .Times(1);
  SendMouseMove(mouse_pos_outside);
  task_environment()->FastForwardBy(base::Seconds(1));
  Mock::VerifyAndClearExpectations(overlay);

  controller.SetTargetView(gfx::NativeView(), nullptr);
  controller.Stop();
}

#if BUILDFLAG(IS_ANDROID)

class MouseCursorOverlayControllerAndroidTest
    : public MouseCursorOverlayControllerTestBase {
 protected:
  void SetupCaptureTarget(WebContents* target_web_contents,
                          const gfx::Rect& bounds) override {
    target_web_contents->GetNativeView()->SetLayoutForTesting(
        bounds.x(), bounds.y(), bounds.width(), bounds.height());

    parent_view_ = std::make_unique<ui::ViewAndroid>(
        ui::ViewAndroid::LayoutType::kMatchParent);
    parent_view_->GetEventForwarder();

    child_view_ = std::make_unique<ui::ViewAndroid>(
        ui::ViewAndroid::LayoutType::kMatchParent);
    parent_view_->AddChild(child_view_.get());
  }

  gfx::NativeView GetTargetView() override { return child_view_.get(); }

  void InitializeEventGenerator() override {
    event_forwarder_ = parent_view_->event_forwarder();
    ASSERT_TRUE(event_forwarder_);
  }

  void SendMouseMove(const gfx::Point& position_in_parent) override {
    ui::MotionEventAndroid event = CreateAndroidMouseEvent(
        ui::MotionEventAndroid::GetAndroidAction(ui::MotionEvent::Action::DOWN),
        position_in_parent.x(), position_in_parent.y());
    for (ui::EventForwarder::Observer& obs :
         event_forwarder_->GetObserversForTesting()) {
      obs.OnMouseEvent(event);
    }
  }

  gfx::Point GetExpectedCapturedPosition(
      const gfx::Point& position_in_parent,
      const gfx::Rect& target_bounds) override {
    return position_in_parent;
  }

 private:
  ui::MotionEventAndroid CreateAndroidMouseEvent(int android_action,
                                                 float x,
                                                 float y) {
    ui::MotionEventAndroid::Pointer p(
        /*id=*/0, x, y, /*touch_major_pixels=*/0, /*touch_minor_pixels=*/0,
        /*pressure=*/0, /*orientation_rad=*/0, /*tilt_rad=*/0,
        ui::MotionEventAndroid::GetAndroidToolType(
            ui::MotionEvent::ToolType::MOUSE));
    return ui::MotionEventAndroid(
        /*pix_to_dip=*/1.0f, /*ticks_x=*/0.0f, /*ticks_y=*/0.0f,
        /*tick_multiplier=*/0.0f, base::TimeTicks::Now(),
        base::TimeTicks::Now(), base::TimeTicks::Now(), android_action,
        /*pointer_count=*/1,
        /*history_size=*/0, /*action_index=*/0, /*android_action_button=*/0,
        /*android_gesture_classification=*/0, /*android_button_state=*/0,
        /*android_meta_state=*/0, /*raw_offset_x_pixels=*/0.0f,
        /*raw_offset_y_pixels=*/0.0f, /*for_touch_handle=*/false, &p,
        /*pointer1=*/nullptr, /*source=*/nullptr);
  }

  std::unique_ptr<ui::ViewAndroid> parent_view_;
  std::unique_ptr<ui::ViewAndroid> child_view_;
  raw_ptr<ui::EventForwarder> event_forwarder_ = nullptr;
};

TEST_F(MouseCursorOverlayControllerAndroidTest, RestrictsToWebContents) {
  RunRestrictsToWebContentsTest();
}

#elif defined(USE_AURA)

class MouseCursorOverlayControllerAuraTest
    : public MouseCursorOverlayControllerTestBase {
 protected:
  void SetupCaptureTarget(WebContents* target_web_contents,
                          const gfx::Rect& bounds) override {
    target_web_contents->GetNativeView()->SetBounds(bounds);

    // Add child
    web_contents()->GetNativeView()->AddChild(
        target_web_contents->GetNativeView());
    web_contents()->GetNativeView()->SetBounds(gfx::Rect(0, 0, 100, 100));
    target_web_contents->GetNativeView()->Show();

    // Add the main view to the root window so it can receive events
    root_window()->AddChild(web_contents()->GetNativeView());
    web_contents()->GetNativeView()->Show();
  }

  gfx::NativeView GetTargetView() override { return root_window(); }

  void InitializeEventGenerator() override {
    generator_ = std::make_unique<ui::test::EventGenerator>(root_window());
  }

  void SendMouseMove(const gfx::Point& position_in_parent) override {
    generator_->MoveMouseTo(
        web_contents()->GetNativeView()->GetBoundsInScreen().origin() +
        position_in_parent.OffsetFromOrigin());
  }

  gfx::Point GetExpectedCapturedPosition(
      const gfx::Point& position_in_parent,
      const gfx::Rect& target_bounds) override {
    return position_in_parent - target_bounds.OffsetFromOrigin();
  }

 private:
  std::unique_ptr<ui::test::EventGenerator> generator_;
};

TEST_F(MouseCursorOverlayControllerAuraTest, RestrictsToWebContents) {
  RunRestrictsToWebContentsTest();
}

#endif

}  // namespace content
