// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/input/event_handler.h"
#include "third_party/blink/renderer/core/input/scroll_manager.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/testing/sim/sim_compositor.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"

namespace blink {

class OverscrollBehaviorTest : public SimTest {
 protected:
  void SetUp() override;

  void SetInnerOverscrollBehavior(String, String);

  void ScrollBegin(double hint_x, double hint_y);
  void ScrollUpdate(double x, double y);
  void ScrollEnd();

  void Scroll(double x, double y);
};

void OverscrollBehaviorTest::SetUp() {
  SimTest::SetUp();
  v8::HandleScope HandleScope(
      WebView().GetPage()->GetAgentGroupScheduler().Isolate());
  ResizeView(gfx::Size(400, 400));
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <style>
      #outer { height: 300px; width: 300px; overflow: scroll; }
      #inner { height: 500px; width: 500px; overflow: scroll; }
    </style>
    <div id='outer'>
      <div id='inner'>
        <div id='content' style='height: 700px; width: 700px;'>
        </div>
      </div>
    </div>
  )HTML");

  Compositor().BeginFrame();

  Element* outer = GetDocument().getElementById(AtomicString("outer"));
  Element* inner = GetDocument().getElementById(AtomicString("inner"));

  // Scrolls the outer element to its bottom-right extent, and makes sure the
  // inner element is at its top-left extent. So that if the scroll is up and
  // left, the inner element doesn't scroll, and we are able to check if the
  // scroll is propagated to the outer element.
  outer->setScrollLeft(200);
  outer->setScrollTop(200);
  ASSERT_EQ(outer->scrollLeft(), 200);
  ASSERT_EQ(outer->scrollTop(), 200);
  ASSERT_EQ(inner->scrollLeft(), 0);
  ASSERT_EQ(inner->scrollTop(), 0);
}

void OverscrollBehaviorTest::SetInnerOverscrollBehavior(String x, String y) {
  GetDocument()
      .getElementById(AtomicString("inner"))
      ->setAttribute(html_names::kStyleAttr,
                     AtomicString(String::Format(
                         "overscroll-behavior-x: %s; overscroll-behavior-y: %s",
                         x.Utf8().c_str(), y.Utf8().c_str())));
}

void OverscrollBehaviorTest::ScrollBegin(double hint_x, double hint_y) {
  WebGestureEvent event(WebInputEvent::Type::kGestureScrollBegin,
                        WebInputEvent::kNoModifiers, base::TimeTicks::Now(),
                        WebGestureDevice::kTouchscreen);
  event.SetPositionInWidget(gfx::PointF(20, 20));
  event.SetPositionInScreen(gfx::PointF(20, 20));
  event.data.scroll_begin.delta_x_hint = -hint_x;
  event.data.scroll_begin.delta_y_hint = -hint_y;
  event.data.scroll_begin.pointer_count = 1;
  event.SetFrameScale(1);
  GetWebFrameWidget().DispatchThroughCcInputHandler(event);
}

void OverscrollBehaviorTest::ScrollUpdate(double delta_x, double delta_y) {
  WebGestureEvent event(WebInputEvent::Type::kGestureScrollUpdate,
                        WebInputEvent::kNoModifiers, base::TimeTicks::Now(),
                        WebGestureDevice::kTouchscreen);
  event.SetPositionInWidget(gfx::PointF(20, 20));
  event.SetPositionInScreen(gfx::PointF(20, 20));
  event.data.scroll_update.delta_x = -delta_x;
  event.data.scroll_update.delta_y = -delta_y;
  event.SetFrameScale(1);
  GetWebFrameWidget().DispatchThroughCcInputHandler(event);
}

void OverscrollBehaviorTest::ScrollEnd() {
  WebGestureEvent event(WebInputEvent::Type::kGestureScrollEnd,
                        WebInputEvent::kNoModifiers, base::TimeTicks::Now(),
                        WebGestureDevice::kTouchscreen);
  event.SetPositionInWidget(gfx::PointF(20, 20));
  event.SetPositionInScreen(gfx::PointF(20, 20));
  GetWebFrameWidget().DispatchThroughCcInputHandler(event);
}

void OverscrollBehaviorTest::Scroll(double x, double y) {
  // Commits property tree state, so cc sees updated overscroll-behavior.
  Compositor().BeginFrame();

  ScrollBegin(x, y);
  ScrollUpdate(x, y);
  ScrollEnd();

  // Applies viewport deltas, so main sees the new scroll offset.
  Compositor().BeginFrame();
}

TEST_F(OverscrollBehaviorTest, AutoAllowsPropagation) {
  SetInnerOverscrollBehavior("auto", "auto");
  Scroll(-100.0, -100.0);
  Element* outer = GetDocument().getElementById(AtomicString("outer"));
  ASSERT_EQ(outer->scrollLeft(), 100);
  ASSERT_EQ(outer->scrollTop(), 100);
}

TEST_F(OverscrollBehaviorTest, ContainOnXPreventsPropagationsOnX) {
  SetInnerOverscrollBehavior("contain", "auto");
  Scroll(-100, 0.0);
  Element* outer = GetDocument().getElementById(AtomicString("outer"));
  ASSERT_EQ(outer->scrollLeft(), 200);
  ASSERT_EQ(outer->scrollTop(), 200);
}

TEST_F(OverscrollBehaviorTest, ContainOnXAllowsPropagationsOnY) {
  SetInnerOverscrollBehavior("contain", "auto");
  Scroll(0.0, -100.0);
  Element* outer = GetDocument().getElementById(AtomicString("outer"));
  ASSERT_EQ(outer->scrollLeft(), 200);
  ASSERT_EQ(outer->scrollTop(), 100);
}

TEST_F(OverscrollBehaviorTest, ContainOnXPreventsDiagonalPropagations) {
  SetInnerOverscrollBehavior("contain", "auto");
  Scroll(-100.0, -100.0);
  Element* outer = GetDocument().getElementById(AtomicString("outer"));
  ASSERT_EQ(outer->scrollLeft(), 200);
  ASSERT_EQ(outer->scrollTop(), 200);
}

TEST_F(OverscrollBehaviorTest, ContainOnYPreventsPropagationsOnY) {
  SetInnerOverscrollBehavior("auto", "contain");
  Scroll(0.0, -100.0);
  Element* outer = GetDocument().getElementById(AtomicString("outer"));
  ASSERT_EQ(outer->scrollLeft(), 200);
  ASSERT_EQ(outer->scrollTop(), 200);
}

TEST_F(OverscrollBehaviorTest, ContainOnYAllowsPropagationsOnX) {
  SetInnerOverscrollBehavior("auto", "contain");
  Scroll(-100.0, 0.0);
  Element* outer = GetDocument().getElementById(AtomicString("outer"));
  ASSERT_EQ(outer->scrollLeft(), 100);
  ASSERT_EQ(outer->scrollTop(), 200);
}

TEST_F(OverscrollBehaviorTest, ContainOnYPreventsDiagonalPropagations) {
  SetInnerOverscrollBehavior("auto", "contain");
  Scroll(-100.0, -100.0);
  Element* outer = GetDocument().getElementById(AtomicString("outer"));
  ASSERT_EQ(outer->scrollLeft(), 200);
  ASSERT_EQ(outer->scrollTop(), 200);
}

TEST_F(OverscrollBehaviorTest, LatchToTheElementPreventedByOverscrollBehavior) {
  SetInnerOverscrollBehavior("none", "none");
  Compositor().BeginFrame();
  ScrollBegin(-100, 0);

  // Always call BeginFrame between updates to force the last update to be
  // handled via InputHandlerProxy::DeliverInputForBeginFrame.  This avoids
  // interference from event coalescing in CompositorThreadEventQueue::Queue.
  //
  // Note: this test also requires ScrollPredictor to be disabled; that happens
  // via TestWebFrameWidget::AllowsScrollResampling.
  //
  ScrollUpdate(-100, 0);
  Compositor().BeginFrame();
  ScrollUpdate(100, 0);
  Compositor().BeginFrame();
  ScrollUpdate(0, -100);
  Compositor().BeginFrame();
  ScrollUpdate(0, 100);
  Compositor().BeginFrame();

  ScrollEnd();
  Compositor().BeginFrame();

  Element* inner = GetDocument().getElementById(AtomicString("inner"));
  ASSERT_EQ(inner->scrollLeft(), 100);
  ASSERT_EQ(inner->scrollTop(), 100);
}

}  // namespace blink
