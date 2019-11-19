// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/direct_manipulation_helper_win.h"

#include "base/test/scoped_feature_list.h"
#include "base/win/windows_version.h"
#include "content/browser/renderer_host/direct_manipulation_test_helper_win.h"
#include "content/browser/renderer_host/legacy_render_widget_host_win.h"
#include "content/browser/renderer_host/render_widget_host_view_aura.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/ui_base_features.h"
#include "ui/base/win/window_event_target.h"
#include "ui/events/event_rewriter.h"
#include "ui/events/event_source.h"
#include "url/gurl.h"

namespace content {

class DirectManipulationBrowserTestBase : public ContentBrowserTest {
 public:
  DirectManipulationBrowserTestBase() {}

  LegacyRenderWidgetHostHWND* GetLegacyRenderWidgetHostHWND() {
    RenderWidgetHostViewAura* rwhva = static_cast<RenderWidgetHostViewAura*>(
        shell()->web_contents()->GetRenderWidgetHostView());
    return rwhva->legacy_render_widget_host_HWND_;
  }

  ui::WindowEventTarget* GetWindowEventTarget() {
    LegacyRenderWidgetHostHWND* lrwhh = GetLegacyRenderWidgetHostHWND();

    return lrwhh->GetWindowEventTarget(lrwhh->GetParent());
  }

  void SetDirectManipulationInteraction(
      DIRECTMANIPULATION_INTERACTION_TYPE type) {
    LegacyRenderWidgetHostHWND* lrwhh = GetLegacyRenderWidgetHostHWND();

    lrwhh->direct_manipulation_helper_->event_handler_->OnInteraction(nullptr,
                                                                      type);
  }

  bool HasAnimationObserver(LegacyRenderWidgetHostHWND* lrwhh) {
    return lrwhh->direct_manipulation_helper_->compositor_
        ->HasAnimationObserver(lrwhh->direct_manipulation_helper_.get());
  }

  void StartNewSequence() {
    LegacyRenderWidgetHostHWND* lrwhh = GetLegacyRenderWidgetHostHWND();

    lrwhh->direct_manipulation_helper_->event_handler_->OnViewportStatusChanged(
        lrwhh->direct_manipulation_helper_->viewport_.Get(),
        DIRECTMANIPULATION_READY, DIRECTMANIPULATION_RUNNING);
  }

  void UpdateContents(MockDirectManipulationContent* content) {
    LegacyRenderWidgetHostHWND* lrwhh = GetLegacyRenderWidgetHostHWND();
    lrwhh->direct_manipulation_helper_->event_handler_->OnContentUpdated(
        lrwhh->direct_manipulation_helper_->viewport_.Get(), content);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(DirectManipulationBrowserTestBase);
};

class DirectManipulationBrowserTest : public DirectManipulationBrowserTestBase,
                                      public testing::WithParamInterface<bool> {
 public:
  DirectManipulationBrowserTest() {
    if (GetParam()) {
      scoped_feature_list_.InitWithFeatures(
          {features::kPrecisionTouchpad,
           features::kPrecisionTouchpadScrollPhase},
          {});
    } else {
      scoped_feature_list_.InitWithFeatures(
          {features::kPrecisionTouchpad},
          {features::kPrecisionTouchpadScrollPhase});
    }
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;

  DISALLOW_COPY_AND_ASSIGN(DirectManipulationBrowserTest);
};

INSTANTIATE_TEST_SUITE_P(WithScrollEventPhase,
                         DirectManipulationBrowserTest,
                         testing::Bool());

// Ensure the AnimationObserver is only created after direct manipulation
// interaction begin and destroyed after direct manipulation interaction end.
IN_PROC_BROWSER_TEST_P(DirectManipulationBrowserTest,
                       ObserverDuringInteraction) {
  if (base::win::GetVersion() < base::win::Version::WIN10)
    return;

  EXPECT_TRUE(NavigateToURL(shell(), GURL(url::kAboutBlankURL)));

  LegacyRenderWidgetHostHWND* lrwhh = GetLegacyRenderWidgetHostHWND();
  ASSERT_TRUE(lrwhh);

  // The observer should not be created before it is needed.
  EXPECT_FALSE(HasAnimationObserver(lrwhh));

  // Begin direct manipulation interaction.
  SetDirectManipulationInteraction(DIRECTMANIPULATION_INTERACTION_BEGIN);
  // AnimationObserver should be added after direct manipulation interaction
  // begin.
  EXPECT_TRUE(HasAnimationObserver(lrwhh));

  // End direct manipulation interaction.
  SetDirectManipulationInteraction(DIRECTMANIPULATION_INTERACTION_END);

  // The animation observer should be removed.
  EXPECT_FALSE(HasAnimationObserver(lrwhh));
}

// EventLogger is to observe the events sent from WindowEventTarget (the root
// window).
class EventLogger : public ui::EventRewriter {
 public:
  EventLogger() {}
  ~EventLogger() override {}

  std::unique_ptr<ui::Event> ReleaseLastEvent() {
    return std::move(last_event_);
  }

 private:
  // ui::EventRewriter
  ui::EventDispatchDetails RewriteEvent(
      const ui::Event& event,
      const Continuation continuation) override {
    DCHECK(!last_event_);
    last_event_ = ui::Event::Clone(event);
    return SendEvent(continuation, &event);
  }

  std::unique_ptr<ui::Event> last_event_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(EventLogger);
};

// Check DirectManipulation events convert to ui::event correctly.
IN_PROC_BROWSER_TEST_P(DirectManipulationBrowserTest, EventConvert) {
  if (base::win::GetVersion() < base::win::Version::WIN10)
    return;

  EXPECT_TRUE(NavigateToURL(shell(), GURL(url::kAboutBlankURL)));

  LegacyRenderWidgetHostHWND* lrwhh = GetLegacyRenderWidgetHostHWND();
  ASSERT_TRUE(lrwhh);

  HWND hwnd =
      shell()->window()->GetRootWindow()->GetHost()->GetAcceleratedWidget();

  ui::EventSource* dwthw = static_cast<ui::EventSource*>(
      aura::WindowTreeHost::GetForAcceleratedWidget(hwnd));
  EventLogger event_logger;
  dwthw->AddEventRewriter(&event_logger);

  ui::WindowEventTarget* target = GetWindowEventTarget();

  {
    target->ApplyPanGestureScroll(1, 2);
    std::unique_ptr<ui::Event> event = event_logger.ReleaseLastEvent();
    ASSERT_TRUE(event);

    if (GetParam()) {
      EXPECT_EQ(ui::ET_SCROLL, event->type());
      ui::ScrollEvent* scroll_event = event->AsScrollEvent();
      EXPECT_EQ(1, scroll_event->x_offset());
      EXPECT_EQ(2, scroll_event->y_offset());
      EXPECT_EQ(ui::EventMomentumPhase::NONE, scroll_event->momentum_phase());
      EXPECT_EQ(ui::ScrollEventPhase::kUpdate,
                scroll_event->scroll_event_phase());
    } else {
      EXPECT_EQ(ui::ET_MOUSEWHEEL, event->type());
      ui::MouseWheelEvent* wheel_event = event->AsMouseWheelEvent();
      EXPECT_EQ(1, wheel_event->x_offset());
      EXPECT_EQ(2, wheel_event->y_offset());
      EXPECT_TRUE(wheel_event->flags() & ui::EF_PRECISION_SCROLLING_DELTA);
    }
  }

  {
    target->ApplyPanGestureFling(1, 2);
    std::unique_ptr<ui::Event> event = event_logger.ReleaseLastEvent();
    ASSERT_TRUE(event);

    if (GetParam()) {
      EXPECT_EQ(ui::ET_SCROLL, event->type());
      ui::ScrollEvent* scroll_event = event->AsScrollEvent();
      EXPECT_EQ(1, scroll_event->x_offset());
      EXPECT_EQ(2, scroll_event->y_offset());
      EXPECT_EQ(ui::EventMomentumPhase::INERTIAL_UPDATE,
                scroll_event->momentum_phase());
      EXPECT_EQ(ui::ScrollEventPhase::kNone,
                scroll_event->scroll_event_phase());
    } else {
      EXPECT_EQ(ui::ET_MOUSEWHEEL, event->type());
      ui::MouseWheelEvent* wheel_event = event->AsMouseWheelEvent();
      EXPECT_EQ(1, wheel_event->x_offset());
      EXPECT_EQ(2, wheel_event->y_offset());
      EXPECT_TRUE(wheel_event->flags() & ui::EF_PRECISION_SCROLLING_DELTA);
    }
  }

  {
    target->ApplyPanGestureScrollBegin(1, 2);
    std::unique_ptr<ui::Event> event = event_logger.ReleaseLastEvent();

    if (GetParam()) {
      ASSERT_TRUE(event);
      EXPECT_EQ(ui::ET_SCROLL, event->type());
      ui::ScrollEvent* scroll_event = event->AsScrollEvent();
      EXPECT_EQ(1, scroll_event->x_offset());
      EXPECT_EQ(2, scroll_event->y_offset());
      EXPECT_EQ(ui::EventMomentumPhase::NONE, scroll_event->momentum_phase());
      EXPECT_EQ(ui::ScrollEventPhase::kBegan,
                scroll_event->scroll_event_phase());
    } else {
      EXPECT_EQ(ui::ET_MOUSEWHEEL, event->type());
      ui::MouseWheelEvent* wheel_event = event->AsMouseWheelEvent();
      EXPECT_EQ(1, wheel_event->x_offset());
      EXPECT_EQ(2, wheel_event->y_offset());
      EXPECT_TRUE(wheel_event->flags() & ui::EF_PRECISION_SCROLLING_DELTA);
    }
  }

  {
    target->ApplyPanGestureScrollEnd(true);
    std::unique_ptr<ui::Event> event = event_logger.ReleaseLastEvent();

    if (GetParam()) {
      ASSERT_TRUE(event);
      EXPECT_EQ(ui::ET_SCROLL, event->type());
      ui::ScrollEvent* scroll_event = event->AsScrollEvent();
      EXPECT_EQ(0, scroll_event->x_offset());
      EXPECT_EQ(0, scroll_event->y_offset());
      EXPECT_EQ(ui::EventMomentumPhase::BLOCKED,
                scroll_event->momentum_phase());
      EXPECT_EQ(ui::ScrollEventPhase::kEnd, scroll_event->scroll_event_phase());
    } else {
      ASSERT_FALSE(event);
    }
  }

  {
    target->ApplyPanGestureFlingBegin();
    std::unique_ptr<ui::Event> event = event_logger.ReleaseLastEvent();

    if (GetParam()) {
      ASSERT_TRUE(event);
      EXPECT_EQ(ui::ET_SCROLL, event->type());
      ui::ScrollEvent* scroll_event = event->AsScrollEvent();
      EXPECT_EQ(0, scroll_event->x_offset());
      EXPECT_EQ(0, scroll_event->y_offset());
      EXPECT_EQ(ui::EventMomentumPhase::BEGAN, scroll_event->momentum_phase());
      EXPECT_EQ(ui::ScrollEventPhase::kNone,
                scroll_event->scroll_event_phase());
    } else {
      ASSERT_FALSE(event);
    }
  }

  {
    target->ApplyPanGestureFlingEnd();
    std::unique_ptr<ui::Event> event = event_logger.ReleaseLastEvent();

    if (GetParam()) {
      ASSERT_TRUE(event);
      EXPECT_EQ(ui::ET_SCROLL, event->type());
      ui::ScrollEvent* scroll_event = event->AsScrollEvent();
      EXPECT_EQ(0, scroll_event->x_offset());
      EXPECT_EQ(0, scroll_event->y_offset());
      EXPECT_EQ(ui::EventMomentumPhase::END, scroll_event->momentum_phase());
      EXPECT_EQ(ui::ScrollEventPhase::kNone,
                scroll_event->scroll_event_phase());
    } else {
      ASSERT_FALSE(event);
    }
  }

  {
    target->ApplyPinchZoomBegin();
    std::unique_ptr<ui::Event> event = event_logger.ReleaseLastEvent();
    ASSERT_TRUE(event);
    EXPECT_EQ(ui::ET_GESTURE_PINCH_BEGIN, event->type());
    ui::GestureEvent* gesture_event = event->AsGestureEvent();
    EXPECT_EQ(ui::GestureDeviceType::DEVICE_TOUCHPAD,
              gesture_event->details().device_type());
  }

  {
    target->ApplyPinchZoomScale(1.1f);
    std::unique_ptr<ui::Event> event = event_logger.ReleaseLastEvent();
    ASSERT_TRUE(event);
    EXPECT_EQ(ui::ET_GESTURE_PINCH_UPDATE, event->type());
    ui::GestureEvent* gesture_event = event->AsGestureEvent();
    EXPECT_EQ(ui::GestureDeviceType::DEVICE_TOUCHPAD,
              gesture_event->details().device_type());
    EXPECT_EQ(1.1f, gesture_event->details().scale());
  }

  {
    target->ApplyPinchZoomEnd();
    std::unique_ptr<ui::Event> event = event_logger.ReleaseLastEvent();
    ASSERT_TRUE(event);
    EXPECT_EQ(ui::ET_GESTURE_PINCH_END, event->type());
    ui::GestureEvent* gesture_event = event->AsGestureEvent();
    EXPECT_EQ(ui::GestureDeviceType::DEVICE_TOUCHPAD,
              gesture_event->details().device_type());
  }

  dwthw->RemoveEventRewriter(&event_logger);
}

class PrecisionTouchpadBrowserTest : public DirectManipulationBrowserTestBase {
 public:
  PrecisionTouchpadBrowserTest() {
    content_ = Microsoft::WRL::Make<MockDirectManipulationContent>();
  }

  void UpdateContents(float scale, float scroll_x, float scroll_y) {
    content_->SetContentTransform(scale, scroll_x, scroll_y);
    DirectManipulationBrowserTestBase::UpdateContents(content_.Get());
  }

  void UseCenterPointAsMockCursorPosition(WebContentsImpl* web_contents) {
    SetMockCursorPositionForTesting(
        web_contents, web_contents->GetContainerBounds().CenterPoint());
  }

 private:
  Microsoft::WRL::ComPtr<MockDirectManipulationContent> content_;

  DISALLOW_COPY_AND_ASSIGN(PrecisionTouchpadBrowserTest);
};

// Confirm that preventDefault correctly prevents pinch zoom on precision
// touchpad.
IN_PROC_BROWSER_TEST_F(PrecisionTouchpadBrowserTest, PreventDefaultPinchZoom) {
  if (base::win::GetVersion() < base::win::Version::WIN10)
    return;

  ASSERT_TRUE(NavigateToURL(shell(), GURL(R"HTML(data:text/html,<!DOCTYPE html>
        <html>
          Hello, world
        </html>)HTML")));

  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  RenderWidgetHostImpl* rwhi = web_contents->GetRenderWidgetHostWithPageFocus();

  // Wait for a frame to be produced or else the test will sometimes try to
  // zoom too early and be unable to, causing flakiness.
  MainThreadFrameObserver observer(
      web_contents->GetRenderViewHost()->GetWidget());
  observer.Wait();

  UseCenterPointAsMockCursorPosition(web_contents);

  EXPECT_EQ(1, EvalJs(web_contents, "window.visualViewport.scale"));

  // Initial amount to try zooming by.
  const int kInitialZoom = 2;

  // Used to confirm the gesture is ACK'd before checking the zoom state. The
  // ACK result itself isn't relevant in this test.
  auto input_msg_watcher = std::make_unique<InputMsgWatcher>(
      web_contents->GetRenderViewHost()->GetWidget(),
      blink::WebInputEvent::kGesturePinchUpdate);

  // First, test a standard zoom.
  UpdateContents(kInitialZoom, 0, 0);
  EXPECT_TRUE(input_msg_watcher->WaitForAck());
  RunUntilInputProcessed(rwhi);

  EXPECT_EQ(kInitialZoom, EvalJs(web_contents, "window.visualViewport.scale"));

  // In order for the end event to be fired and state reset, a new sequence has
  // to be started. This simulates lifting fingers off of the touch pad.
  StartNewSequence();

  // Now add the preventDefault to confirm zooming does not happen.
  EXPECT_TRUE(ExecJs(web_contents,
                     R"(var handler = function (e) {e.preventDefault(); };
      document.addEventListener('wheel', handler, {passive: false}); )"));
  RunUntilInputProcessed(rwhi);

  // Arbitrary zoom amount chosen here to make the test fail if it does zoom.
  UpdateContents(3.5, 0, 0);
  EXPECT_TRUE(input_msg_watcher->WaitForAck());
  RunUntilInputProcessed(rwhi);

  EXPECT_EQ(kInitialZoom, EvalJs(web_contents, "window.visualViewport.scale"));

  // Confirm a zoom back out to 1 works as expected.
  StartNewSequence();
  RunUntilInputProcessed(rwhi);

  EXPECT_TRUE(ExecJs(
      web_contents,
      R"(document.removeEventListener('wheel', handler, {passive: false}); )"));

  const float kEndZoom = 0.5;

  UpdateContents(kEndZoom, 0, 0);
  EXPECT_TRUE(input_msg_watcher->WaitForAck());
  RunUntilInputProcessed(rwhi);

  EXPECT_EQ(static_cast<int>(kInitialZoom * kEndZoom),
            EvalJs(web_contents, "window.visualViewport.scale"));
}

// Confirm that preventDefault correctly prevents scrolling on precision
// touchpad.
IN_PROC_BROWSER_TEST_F(PrecisionTouchpadBrowserTest, PreventDefaultScroll) {
  if (base::win::GetVersion() < base::win::Version::WIN10)
    return;

  ASSERT_TRUE(NavigateToURL(shell(), GURL(R"HTML(data:text/html,<!DOCTYPE html>
    <html>
      <body style='height:2000px; width:2000px;'>
        Hello, world
      </body>
    </html>)HTML")));

  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  RenderWidgetHostImpl* rwhi = web_contents->GetRenderWidgetHostWithPageFocus();

  // Wait for a frame to be produced or else the test will sometimes try to
  // scroll too early and be unable to, causing flakiness.
  MainThreadFrameObserver observer(
      web_contents->GetRenderViewHost()->GetWidget());
  observer.Wait();

  UseCenterPointAsMockCursorPosition(web_contents);

  EXPECT_EQ(0, EvalJs(web_contents, "document.documentElement.scrollLeft"));
  EXPECT_EQ(0, EvalJs(web_contents, "document.documentElement.scrollTop"));

  // The distance to try scrolling. Note that scrolling down or right is
  // considered the negative direction, so this value will be negated when
  // passed to UpdateContents.
  const int kInitialScrollDistance = 200;

  // Used to confirm the gesture is ACK'd before checking the scroll state. The
  // ACK result itself isn't relevant in this test.
  auto input_msg_watcher = std::make_unique<InputMsgWatcher>(
      web_contents->GetRenderViewHost()->GetWidget(),
      blink::WebInputEvent::kMouseWheel);

  // First, test scrolling vertically
  UpdateContents(1, 0, -kInitialScrollDistance);
  EXPECT_TRUE(input_msg_watcher->WaitForAck());
  RunUntilInputProcessed(rwhi);

  EXPECT_EQ(0, EvalJs(web_contents, "document.documentElement.scrollLeft"));
  EXPECT_EQ(kInitialScrollDistance,
            EvalJs(web_contents, "document.documentElement.scrollTop"));

  // Then, horizontally. Note that a new sequence is not starting between these,
  // which is why the y value remains the same.
  UpdateContents(1, -kInitialScrollDistance, -kInitialScrollDistance);
  RunUntilInputProcessed(rwhi);

  EXPECT_EQ(kInitialScrollDistance,
            EvalJs(web_contents, "document.documentElement.scrollLeft"));
  EXPECT_EQ(kInitialScrollDistance,
            EvalJs(web_contents, "document.documentElement.scrollTop"));

  // In order for the end event to be fired and state reset, a new sequence has
  // to be started. This simulates lifting fingers off of the touch pad.
  StartNewSequence();

  // Now add the preventDefault to confirm scrolling does not happen.
  EXPECT_TRUE(ExecJs(web_contents,
                     R"(var handler = function (e) {e.preventDefault(); };
      document.addEventListener('wheel', handler, {passive: false}); )"));
  RunUntilInputProcessed(rwhi);

  // Updating with arbitrarily chosen numbers that should make it obvious where
  // values are coming from when this test fails.
  UpdateContents(1, 354, 291);
  EXPECT_TRUE(input_msg_watcher->WaitForAck());
  RunUntilInputProcessed(rwhi);

  EXPECT_EQ(kInitialScrollDistance,
            EvalJs(web_contents, "document.documentElement.scrollLeft"));
  EXPECT_EQ(kInitialScrollDistance,
            EvalJs(web_contents, "document.documentElement.scrollTop"));

  // Confirm a scroll back towards the origin works after removing the event
  // listener.
  StartNewSequence();

  EXPECT_TRUE(ExecJs(
      web_contents,
      R"(document.removeEventListener('wheel', handler, {passive: false}); )"));

  // Values arbitrarily chosen so to confirm that scrolling up and left works
  // without going all the way to the origin.
  const int kScrollXDistance = 120;
  const int kScrollYDistance = 150;
  UpdateContents(1, kScrollXDistance, kScrollYDistance);
  EXPECT_TRUE(input_msg_watcher->WaitForAck());
  RunUntilInputProcessed(rwhi);

  EXPECT_EQ(kInitialScrollDistance - kScrollXDistance,
            EvalJs(web_contents, "document.documentElement.scrollLeft"));
  EXPECT_EQ(kInitialScrollDistance - kScrollYDistance,
            EvalJs(web_contents, "document.documentElement.scrollTop"));
}

}  // namespace content
