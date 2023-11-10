// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/base/features.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/input/event_handler.h"
#include "third_party/blink/renderer/core/input/scroll_manager.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/testing/sim/sim_compositor.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"

namespace blink {

class ScrollSnapTest : public SimTest {
 protected:
  void SetUpForDiv();
  // The following x, y, hint_x, hint_y, delta_x, delta_y are represents
  // the pointer/finger's location on touch screen.
  void GestureScroll(double x,
                     double y,
                     double delta_x,
                     double delta_y,
                     bool composited = false);
  void ScrollBegin(double x, double y, double hint_x, double hint_y);
  void ScrollUpdate(double x,
                    double y,
                    double delta_x,
                    double delta_y,
                    bool is_in_inertial_phase = false);
  void ScrollEnd(double x, double y, bool is_in_inertial_phase = false);
  void SetInitialScrollOffset(double x, double y);
};

void ScrollSnapTest::SetUpForDiv() {
  v8::HandleScope HandleScope(
      WebView().GetPage()->GetAgentGroupScheduler().Isolate());
  WebView().MainFrameViewWidget()->Resize(gfx::Size(400, 400));
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
    #scroller {
      width: 140px;
      height: 160px;
      overflow: scroll;
      scroll-snap-type: both mandatory;
      padding: 0px;
    }
    #container {
      margin: 0px;
      padding: 0px;
      width: 500px;
      height: 500px;
    }
    #area {
      position: relative;
      left: 200px;
      top: 200px;
      width: 100px;
      height: 100px;
      scroll-snap-align: start;
    }
    </style>
    <div id='scroller'>
      <div id='container'>
        <div id='area'></div>
      </div>
    </div>
  )HTML");

  Compositor().BeginFrame();
}

void ScrollSnapTest::GestureScroll(double x,
                                   double y,
                                   double delta_x,
                                   double delta_y,
                                   bool composited) {
  ScrollBegin(x, y, delta_x, delta_y);
  ScrollUpdate(x, y, delta_x, delta_y);
  ScrollEnd(x + delta_x, y + delta_y);

  // Wait for animation to finish.
  // Pass raster = true to reach LayerTreeHostImpl::UpdateAnimationState,
  // which will set start time and transition to KeyframeModel::RUNNING.
  Compositor().BeginFrame(0.016, true);
  Compositor().BeginFrame(0.3);
}

void ScrollSnapTest::ScrollBegin(double x,
                                 double y,
                                 double hint_x,
                                 double hint_y) {
  WebGestureEvent event(WebInputEvent::Type::kGestureScrollBegin,
                        WebInputEvent::kNoModifiers, base::TimeTicks::Now(),
                        WebGestureDevice::kTouchscreen);
  event.SetPositionInWidget(gfx::PointF(x, y));
  event.SetPositionInScreen(gfx::PointF(x, y));
  event.data.scroll_begin.delta_x_hint = hint_x;
  event.data.scroll_begin.delta_y_hint = hint_y;
  event.data.scroll_begin.pointer_count = 1;
  event.SetFrameScale(1);
  GetWebFrameWidget().DispatchThroughCcInputHandler(event);
}

void ScrollSnapTest::ScrollUpdate(double x,
                                  double y,
                                  double delta_x,
                                  double delta_y,
                                  bool is_in_inertial_phase) {
  WebGestureEvent event(WebInputEvent::Type::kGestureScrollUpdate,
                        WebInputEvent::kNoModifiers, base::TimeTicks::Now(),
                        WebGestureDevice::kTouchscreen);
  event.SetPositionInWidget(gfx::PointF(x, y));
  event.SetPositionInScreen(gfx::PointF(x, y));
  event.data.scroll_update.delta_x = delta_x;
  event.data.scroll_update.delta_y = delta_y;
  if (is_in_inertial_phase) {
    event.data.scroll_update.inertial_phase =
        WebGestureEvent::InertialPhaseState::kMomentum;
    event.SetTimeStamp(Compositor().LastFrameTime());
  }
  event.SetFrameScale(1);
  GetWebFrameWidget().DispatchThroughCcInputHandler(event);
}

void ScrollSnapTest::ScrollEnd(double x, double y, bool is_in_inertial_phase) {
  WebGestureEvent event(WebInputEvent::Type::kGestureScrollEnd,
                        WebInputEvent::kNoModifiers, base::TimeTicks::Now(),
                        WebGestureDevice::kTouchscreen);
  event.SetPositionInWidget(gfx::PointF(x, y));
  event.SetPositionInScreen(gfx::PointF(x, y));
  event.data.scroll_end.inertial_phase =
      is_in_inertial_phase ? WebGestureEvent::InertialPhaseState::kMomentum
                           : WebGestureEvent::InertialPhaseState::kNonMomentum;
  GetWebFrameWidget().DispatchThroughCcInputHandler(event);
}

void ScrollSnapTest::SetInitialScrollOffset(double x, double y) {
  Element* scroller = GetDocument().getElementById(AtomicString("scroller"));
  scroller->GetLayoutBoxForScrolling()
      ->GetScrollableArea()
      ->ScrollToAbsolutePosition(gfx::PointF(x, y),
                                 mojom::blink::ScrollBehavior::kAuto);
  ASSERT_EQ(scroller->scrollLeft(), x);
  ASSERT_EQ(scroller->scrollTop(), y);
}

TEST_F(ScrollSnapTest, ScrollSnapOnX) {
  SetUpForDiv();
  SetInitialScrollOffset(50, 150);
  Compositor().BeginFrame();

  GestureScroll(100, 100, -50, 0);

  Element* scroller = GetDocument().getElementById(AtomicString("scroller"));
  // Snaps to align the area at start.
  ASSERT_EQ(scroller->scrollLeft(), 200);
  // An x-locked scroll ignores snap points on y.
  ASSERT_EQ(scroller->scrollTop(), 150);
}

TEST_F(ScrollSnapTest, ScrollSnapOnY) {
  SetUpForDiv();
  SetInitialScrollOffset(150, 50);
  Compositor().BeginFrame();

  GestureScroll(100, 100, 0, -50);

  Element* scroller = GetDocument().getElementById(AtomicString("scroller"));
  // A y-locked scroll ignores snap points on x.
  ASSERT_EQ(scroller->scrollLeft(), 150);
  // Snaps to align the area at start.
  ASSERT_EQ(scroller->scrollTop(), 200);
}

TEST_F(ScrollSnapTest, ScrollSnapOnBoth) {
  SetUpForDiv();
  SetInitialScrollOffset(50, 50);
  Compositor().BeginFrame();

  GestureScroll(100, 100, -50, -50);

  Element* scroller = GetDocument().getElementById(AtomicString("scroller"));
  // A scroll gesture that has move in both x and y would snap on both axes.
  ASSERT_EQ(scroller->scrollLeft(), 200);
  ASSERT_EQ(scroller->scrollTop(), 200);
}

TEST_F(ScrollSnapTest, AnimateFlingToArriveAtSnapPoint) {
  SetUpForDiv();
  // Vertically align with the area.
  SetInitialScrollOffset(0, 200);
  Compositor().BeginFrame();

  Element* scroller = GetDocument().getElementById(AtomicString("scroller"));
  ASSERT_EQ(scroller->scrollLeft(), 0);
  ASSERT_EQ(scroller->scrollTop(), 200);

  ScrollBegin(100, 100, -5, 0);
  // Starts with a non-inertial GSU.
  ScrollUpdate(100, 100, -5, 0);
  Compositor().BeginFrame();

  // Fling with an inertial GSU.
  ScrollUpdate(95, 100, -5, 0, true);
  ScrollEnd(90, 100);

  // Animate halfway through the fling.
  Compositor().BeginFrame(0.25);
  ASSERT_GT(scroller->scrollLeft(), 150);
  ASSERT_LT(scroller->scrollLeft(), 180);
  ASSERT_EQ(scroller->scrollTop(), 200);
  // Finish the animation.
  Compositor().BeginFrame(0.6);

  ASSERT_EQ(scroller->scrollLeft(), 200);
  ASSERT_EQ(scroller->scrollTop(), 200);
}

TEST_F(ScrollSnapTest, SnapWhenBodyViewportDefining) {
  v8::HandleScope HandleScope(
      WebView().GetPage()->GetAgentGroupScheduler().Isolate());
  WebView().MainFrameViewWidget()->Resize(gfx::Size(300, 300));
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
    html {
      scroll-snap-type: both mandatory;
    }
    body {
      overflow: scroll;
      height: 300px;
      width: 300px;
      margin: 0px;
    }
    #container {
      margin: 0px;
      padding: 0px;
      width: 500px;
      height: 500px;
    }
    #initial-area {
      position: relative;
      left: 0px;
      top: 0px;
      width: 100px;
      height: 100px;
      scroll-snap-align: start;
    }
    #area {
      position: relative;
      left: 200px;
      top: 200px;
      width: 100px;
      height: 100px;
      scroll-snap-align: start;
    }
    </style>
    <div id='container'>
      <div id='initial-area'></div>
      <div id='area'></div>
    </div>
  )HTML");
  Compositor().BeginFrame();

  // The scroller snaps to the snap area that is closest to the origin (0,0) on
  // the initial layout.
  ASSERT_EQ(Window().scrollX(), 0);
  ASSERT_EQ(Window().scrollY(), 0);

  // The scroll delta needs to be large enough such that the closer snap area
  // will be the one at (200,200).
  // i.e. distance((200,200), (110,110)) <  distance((0,0), (110,110))
  GestureScroll(100, 100, -110, -110, true);

  // Sanity check that body is the viewport defining element
  ASSERT_EQ(GetDocument().body(), GetDocument().ViewportDefiningElement());

  // When body is viewport defining and overflows then any snap points on the
  // body element will be captured by layout view as the snap container.
  ASSERT_EQ(Window().scrollX(), 200);
  ASSERT_EQ(Window().scrollY(), 200);
}

TEST_F(ScrollSnapTest, SnapWhenHtmlViewportDefining) {
  v8::HandleScope HandleScope(
      WebView().GetPage()->GetAgentGroupScheduler().Isolate());
  WebView().MainFrameViewWidget()->Resize(gfx::Size(300, 300));
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
    :root {
      overflow: scroll;
      scroll-snap-type: both mandatory;
      height: 300px;
      width: 300px;
    }
    body {
      margin: 0px;
    }
    #container {
      margin: 0px;
      padding: 0px;
      width: 500px;
      height: 500px;
    }
    #initial-area {
      position: relative;
      left: 0px;
      top: 0px;
      width: 100px;
      height: 100px;
      scroll-snap-align: start;
    }
    #area {
      position: relative;
      left: 200px;
      top: 200px;
      width: 100px;
      height: 100px;
      scroll-snap-align: start;
    }
    </style>
    <div id='container'>
      <div id='initial-area'></div>
      <div id='area'></div>
    </div>
  )HTML");
  Compositor().BeginFrame();

  // The scroller snaps to the snap area that is closest to the origin (0,0) on
  // the initial layout.
  ASSERT_EQ(Window().scrollX(), 0);
  ASSERT_EQ(Window().scrollY(), 0);

  // The scroll delta needs to be large enough such that the closer snap area
  // will be the one at (200,200).
  // i.e. distance((200,200), (110,110)) <  distance((0,0), (110,110))
  GestureScroll(100, 100, -110, -110, true);

  // Sanity check that document element is the viewport defining element
  ASSERT_EQ(GetDocument().documentElement(),
            GetDocument().ViewportDefiningElement());

  // When document is viewport defining and overflows then any snap ponts on the
  // document element will be captured by layout view as snap container.
  ASSERT_EQ(Window().scrollX(), 200);
  ASSERT_EQ(Window().scrollY(), 200);
}

TEST_F(ScrollSnapTest, SnapWhenBodyOverflowHtmlViewportDefining) {
  v8::HandleScope HandleScope(
      WebView().GetPage()->GetAgentGroupScheduler().Isolate());
  WebView().MainFrameViewWidget()->Resize(gfx::Size(300, 300));
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
    :root {
      overflow: scroll;
      height: 300px;
      width: 300px;
    }
    body {
      overflow: scroll;
      scroll-snap-type: both mandatory;
      height: 400px;
      width: 400px;
    }
    #container {
      margin: 0px;
      padding: 0px;
      width: 600px;
      height: 600px;
    }
    #initial-area {
      position: relative;
      left: 0px;
      top: 0px;
      width: 100px;
      height: 100px;
      scroll-snap-align: start;
    }
    #area {
      position: relative;
      left: 200px;
      top: 200px;
      width: 100px;
      height: 100px;
      scroll-snap-align: start;
    }
    </style>
    <div id='container'>
      <div id='initial-area'></div>
      <div id='area'></div>
    </div>
  )HTML");
  Compositor().BeginFrame();

  // The scroller snaps to the snap area that is closest to the origin (0,0) on
  // the initial layout.
  Element* body = GetDocument().body();
  ASSERT_EQ(body->scrollLeft(), 0);
  ASSERT_EQ(body->scrollTop(), 0);

  // The scroll delta needs to be large enough such that the closer snap area
  // will be the one at (200,200).
  // i.e. distance((200,200), (110,110)) <  distance((0,0), (110,110))
  GestureScroll(100, 100, -110, -110);

  // Sanity check that document element is the viewport defining element
  ASSERT_EQ(GetDocument().documentElement(),
            GetDocument().ViewportDefiningElement());

  // When body and document elements are both scrollable then body element
  // should capture snap points defined on it as opposed to layout view.
  ASSERT_EQ(body->scrollLeft(), 200);
  ASSERT_EQ(body->scrollTop(), 200);
}

TEST_F(ScrollSnapTest, ResizeDuringGesture) {
  ResizeView(gfx::Size(400, 400));
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
    ::-webkit-scrollbar { display: none; }
    html { scroll-snap-type: both mandatory; }
    body { margin: 0; width: 600px; height: 600px; }
    #a1 { position: absolute; left: 0; top: 0; background: blue;
          width: 100px; height: 100px; scroll-snap-align: start; }
    #a2 { position: absolute; left: 400px; top: 400px; background: blue;
          width: 100px; height: 100px; scroll-snap-align: end; }
    </style>
    <div id='a1'></div>
    <div id='a2'></div>
  )HTML");

  Compositor().BeginFrame();

  Element* viewport = GetDocument().scrollingElement();
  ASSERT_EQ(viewport->scrollLeft(), 0);
  ASSERT_EQ(viewport->scrollTop(), 0);

  ScrollBegin(10, 10, -75, -75);
  ScrollUpdate(10, 10, -75, -75);

  Compositor().BeginFrame();

  ASSERT_EQ(viewport->scrollLeft(), 75);
  ASSERT_EQ(viewport->scrollTop(), 75);

  ResizeView(gfx::Size(450, 450));
  Compositor().BeginFrame();

  // After mid-gesture resize, we should still be at 75.
  ASSERT_EQ(viewport->scrollLeft(), 75);
  ASSERT_EQ(viewport->scrollTop(), 75);

  ScrollEnd(10, 10);

  // The scrollend is deferred for the snap animation in cc::InputHandler; wait
  // for the animation to finish.  (We pss raster = true to ensure that we call
  // LayerTreeHostImpl::UpdateAnimationState, which will set start time and
  // transition to KeyframeModel::RUNNING.)
  Compositor().BeginFrame(0.016, true);
  Compositor().BeginFrame(0.3);

  // Once the snap animation is finished, we run a deferred SnapAfterLayout.
  ASSERT_EQ(viewport->scrollLeft(), 50);
  ASSERT_EQ(viewport->scrollTop(), 50);
}

}  // namespace blink
