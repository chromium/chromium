// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/page/autoscroll_controller.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/script_evaluation_result.h"
#include "third_party/blink/renderer/bindings/core/v8/script_function.h"
#include "third_party/blink/renderer/core/css_value_keywords.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/range.h"
#include "third_party/blink/renderer/core/editing/dom_selection.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/editing/selection_controller.h"
#include "third_party/blink/renderer/core/editing/selection_template.h"
#include "third_party/blink/renderer/core/input/event_handler.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/script/classic_script.h"
#include "third_party/blink/renderer/core/scroll/scroll_animator_base.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"
#include "third_party/blink/renderer/core/timing/dom_window_performance.h"

namespace blink {

class AutoscrollControllerTest : public SimTest {
 public:
  AutoscrollController& GetAutoscrollController() {
    return WebView().GetPage()->GetAutoscrollController();
  }
};

// Ensure Autoscroll not crash by layout called in UpdateSelectionForMouseDrag.
TEST_F(AutoscrollControllerTest,
       CrashWhenLayoutStopAnimationBeforeScheduleAnimation) {
  WebView().MainFrameViewWidget()->Resize(gfx::Size(800, 600));
  WebView().SetPageBaseBackgroundColor(SK_ColorTRANSPARENT);
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
      #scrollable {
        overflow: auto;
        width: 10px;
        height: 10px;
      }
    </style>
    <div id='scrollable'>
      <p id='p'>Some text here for selection autoscroll.</p>
      <p>Some text here for selection autoscroll.</p>
      <p>Some text here for selection autoscroll.</p>
      <p>Some text here for selection autoscroll.</p>
      <p>Some text here for selection autoscroll.</p>
      <p>Some text here for selection autoscroll.</p>
      <p>Some text here for selection autoscroll.</p>
      <p>Some text here for selection autoscroll.</p>
    </div>
  )HTML");

  Compositor().BeginFrame();

  AutoscrollController& controller = GetAutoscrollController();
  Document& document = GetDocument();

  Element* scrollable = document.getElementById(AtomicString("scrollable"));
  DCHECK(scrollable);
  DCHECK(scrollable->GetLayoutObject());

  WebMouseEvent event(WebInputEvent::Type::kMouseDown, gfx::PointF(5, 5),
                      gfx::PointF(5, 5), WebPointerProperties::Button::kLeft, 0,
                      WebInputEvent::Modifiers::kLeftButtonDown,
                      base::TimeTicks::Now());
  event.SetFrameScale(1);

  GetDocument().GetFrame()->GetEventHandler().HandleMousePressEvent(event);

  controller.StartAutoscrollForSelection(scrollable->GetLayoutObject());

  DCHECK(controller.IsAutoscrolling());

  // Hide scrollable here will cause UpdateSelectionForMouseDrag stop animation.
  scrollable->SetInlineStyleProperty(CSSPropertyID::kDisplay,
                                     CSSValueID::kNone);

  // BeginFrame will call AutoscrollController::Animate.
  Compositor().BeginFrame();

  EXPECT_FALSE(controller.IsAutoscrolling());
}

// Ensure that autoscrolling continues when the MouseLeave event is fired.
TEST_F(AutoscrollControllerTest, ContinueAutoscrollAfterMouseLeaveEvent) {
  WebView().MainFrameViewWidget()->Resize(gfx::Size(800, 600));
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
      #scrollable {
        width: 820px;
        height: 620px;
      }
    </style>
    <div id='scrollable'></div>
  )HTML");

  Compositor().BeginFrame();

  AutoscrollController& controller = GetAutoscrollController();

  EXPECT_FALSE(controller.IsAutoscrolling());

  LocalFrame* frame = GetDocument().GetFrame();
  Node* document_node = GetDocument().documentElement();
  controller.StartMiddleClickAutoscroll(
      frame, document_node->parentNode()->GetLayoutBox(), gfx::PointF(),
      gfx::PointF());

  EXPECT_TRUE(controller.IsAutoscrolling());

  WebMouseEvent mouse_leave_event(WebInputEvent::Type::kMouseLeave,
                                  WebInputEvent::kNoModifiers,
                                  base::TimeTicks::Now());
  mouse_leave_event.SetFrameScale(1);

  frame->GetEventHandler().HandleMouseLeaveEvent(mouse_leave_event);

  EXPECT_TRUE(controller.IsAutoscrolling());
}

// Ensure that autoscrolling stops when scrolling is no longer available.
TEST_F(AutoscrollControllerTest, StopAutoscrollOnResize) {
  WebView().MainFrameViewWidget()->Resize(gfx::Size(800, 600));
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
      #scrollable {
        width: 820px;
        height: 620px;
      }
    </style>
    <div id='scrollable'></div>
  )HTML");

  Compositor().BeginFrame();

  AutoscrollController& controller = GetAutoscrollController();

  EXPECT_FALSE(controller.IsAutoscrolling());

  LocalFrame* frame = GetDocument().GetFrame();
  controller.StartMiddleClickAutoscroll(frame, GetDocument().GetLayoutView(),
                                        gfx::PointF(), gfx::PointF());

  EXPECT_TRUE(controller.IsAutoscrolling());

  // Confirm that it correctly stops autoscrolling when scrolling is no longer
  // possible
  WebView().MainFrameViewWidget()->Resize(gfx::Size(840, 640));

  WebMouseEvent mouse_move_event(WebInputEvent::Type::kMouseMove,
                                 WebInputEvent::kNoModifiers,
                                 base::TimeTicks::Now());

  frame->GetEventHandler().HandleMouseMoveEvent(
      mouse_move_event, Vector<WebMouseEvent>(), Vector<WebMouseEvent>());

  EXPECT_FALSE(controller.IsAutoscrolling());

  // Confirm that autoscrolling doesn't restart when scrolling is available
  // again
  WebView().MainFrameViewWidget()->Resize(gfx::Size(800, 600));

  WebMouseEvent mouse_move_event2(WebInputEvent::Type::kMouseMove,
                                  WebInputEvent::kNoModifiers,
                                  base::TimeTicks::Now());

  frame->GetEventHandler().HandleMouseMoveEvent(
      mouse_move_event2, Vector<WebMouseEvent>(), Vector<WebMouseEvent>());

  EXPECT_FALSE(controller.IsAutoscrolling());

  // And finally confirm that autoscrolling can start again.
  controller.StartMiddleClickAutoscroll(frame, GetDocument().GetLayoutView(),
                                        gfx::PointF(), gfx::PointF());

  EXPECT_TRUE(controller.IsAutoscrolling());
}

// Ensure that middle click autoscroll is not propagated in a direction when
// propagation is not allowed.
TEST_F(AutoscrollControllerTest, AutoscrollIsNotPropagated) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <html>
      <head>
        <style>
          #scrollable {
            width: 820px;
            height: 620px;
            overflow: auto;
            overscroll-behavior: contain;
          }
          #inner {
            width: 2500px;
            background-color: aqua;
            height: 100px;
          }
        </style>
      </head>
      <body style='width: 3000px; height: 3000px;'>
        <div id="scrollable">
          <div id="inner"></div>
        </div>
      </body>
    </html>
  )HTML");

  Compositor().BeginFrame();

  AutoscrollController& controller = GetAutoscrollController();

  EXPECT_FALSE(controller.IsAutoscrolling());

  LocalFrame* frame = GetDocument().GetFrame();
  LayoutBox* scrollable =
      GetDocument().getElementById(AtomicString("scrollable"))->GetLayoutBox();

  controller.StartMiddleClickAutoscroll(
      frame, scrollable, gfx::PointF(15.0, 15.0), gfx::PointF(15.0, 15.0));

  EXPECT_TRUE(controller.IsAutoscrolling());
  EXPECT_TRUE(controller.horizontal_autoscroll_layout_box_);
  EXPECT_FALSE(controller.vertical_autoscroll_layout_box_);
}

// Ensure that middle click autoscroll is propagated in a direction when
// overscroll-behavior is set to auto for a that direction.
TEST_F(AutoscrollControllerTest, AutoscrollIsPropagatedInYDirection) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <html>
      <head>
        <style>
          #scrollable {
            width: 820px;
            height: 620px;
            overflow: auto;
            overscroll-behavior-x: contain;
          }
          #inner {
            width: 1000px;
            background-color: aqua;
            height: 100px;
          }
        </style>
      </head>
      <body style='width: 3000px; height: 3000px;'>
        <div id="scrollable">
          <div id="inner"></div>
        </div>
      </body>
    </html>
  )HTML");

  Compositor().BeginFrame();

  AutoscrollController& controller = GetAutoscrollController();

  EXPECT_FALSE(controller.IsAutoscrolling());

  LocalFrame* frame = GetDocument().GetFrame();
  LayoutBox* scrollable =
      GetDocument().getElementById(AtomicString("scrollable"))->GetLayoutBox();

  controller.StartMiddleClickAutoscroll(
      frame, scrollable, gfx::PointF(15.0, 15.0), gfx::PointF(15.0, 15.0));

  EXPECT_TRUE(controller.IsAutoscrolling());
  EXPECT_TRUE(controller.vertical_autoscroll_layout_box_);
  EXPECT_TRUE(controller.horizontal_autoscroll_layout_box_);
}

TEST_F(AutoscrollControllerTest, TextSelectionAutoScroll) {
  WebView().MainFrameViewWidget()->Resize(gfx::Size(800, 600));

  GetDocument().GetSettings()->SetScriptEnabled(true);
  Element* script = GetDocument().CreateRawElement(html_names::kScriptTag);
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <style>
      #targetDiv {
          width: 200px;
          height: 200px;
          overflow: scroll;
      }

      #innerDiv {
          width: 4000px;
          height: 4000px;
      }
    </style>
    <body style='margin:0'>
      <div id='targetDiv'>
      <div id='innerDiv'>
        <p>Test test test test test test...</p>
        <p>Test test test test test test...</p>
        <p>Test test test test test test...</p>
        <p>Test test test test test test...</p>
        <p>Test test test test test test...</p>
        <p>Test test test test test test...</p>
        <p>Test test test test test test...</p>
        <p>Test test test test test test...</p>
        <p>Test test test test test test...</p>
        <p>Test test test test test test...</p>
        <p>Test test test test test test...</p>
        <p>Test test test test test test...</p>
        <p>Test test test test test test...</p>
        <p>Test test test test test test...</p>
        <p>Test test test test test test...</p>
        <p>Test test test test test test...</p>
        <p>Test test test test test test...</p>
        <p>Test test test test test test...</p>
        <p>Test test test test test test...</p>
        <p>Test test test test test test...</p>
        <p>Test test test test test test...</p>
        <p>Test test test test test test...</p>
        <p>Test test test test test test...</p>
        <p>Test test test test test test...</p>
        <p>Test test test test test test...</p>
      </div>
      </div>
      <p id='log'></p>
  </body>
  )HTML");

  script->setInnerHTML(R"HTML(
    let eventCounts = {pointerdown: 0, scroll: 0};
    let target = document.getElementById('targetDiv');
    for (let evt in eventCounts) {
      target.addEventListener(evt, function(e) {
        eventCounts[e.type]++;
        let log = document.getElementById('log');
        log.innerText += " " + e.type;
       });
    }
  )HTML");
  GetDocument().body()->AppendChild(script);
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);

  Compositor().BeginFrame();

  AutoscrollController& controller = GetAutoscrollController();
  Document& document = GetDocument();

  Element* scrollable = document.getElementById(AtomicString("targetDiv"));
  DCHECK(scrollable);
  DCHECK(scrollable->GetLayoutObject());

  WebMouseEvent event(WebInputEvent::Type::kMouseDown, gfx::PointF(10, 10),
                      gfx::PointF(10, 10), WebPointerProperties::Button::kLeft,
                      1, WebInputEvent::Modifiers::kLeftButtonDown,
                      base::TimeTicks::Now());
  event.SetFrameScale(1);
  GetDocument().GetFrame()->GetEventHandler().HandleMousePressEvent(event);

  controller.StartAutoscrollForSelection(scrollable->GetLayoutObject());

  DCHECK(controller.IsAutoscrolling());

  WebMouseEvent mouse_event(
      WebInputEvent::Type::kMouseMove, gfx::PointF(50, 150),
      gfx::PointF(50, 150), WebPointerProperties::Button::kLeft, 1,
      WebInputEvent::Modifiers::kLeftButtonDown, base::TimeTicks::Now());
  GetDocument().GetFrame()->GetEventHandler().HandleMouseMoveEvent(
      mouse_event, Vector<WebMouseEvent>(), Vector<WebMouseEvent>());
  ScrollableArea* scrollable_area =
      scrollable->GetLayoutBox()->GetScrollableArea();

  controller.Animate();
  EXPECT_TRUE(controller.AutoscrollInProgress());

  Compositor().BeginFrame();

  mouse_event.SetPositionInWidget(100, 200);
  mouse_event.SetPositionInScreen(100, 200);
  GetDocument().GetFrame()->GetEventHandler().HandleMouseMoveEvent(
      mouse_event, Vector<WebMouseEvent>(), Vector<WebMouseEvent>());

  controller.Animate();
  LocalDOMWindow* current_window = GetDocument().GetFrame()->DomWindow();
  DCHECK(current_window);
  WindowPerformance* window_performance =
      DOMWindowPerformance::performance(*current_window);
  EXPECT_TRUE(controller.AutoscrollInProgress());
  EXPECT_GT(scrollable_area->GetScrollOffset().y(), 0);
  EXPECT_TRUE(window_performance->IsAutoscrollActive());

  Compositor().BeginFrame();
  EXPECT_GT(scrollable_area->GetScrollOffset().y(), 0);
  EXPECT_TRUE(window_performance->IsAutoscrollActive());

  mouse_event.SetType(blink::WebInputEvent::Type::kMouseUp);
  GetDocument().GetFrame()->GetEventHandler().HandleMouseReleaseEvent(
      mouse_event);

  Compositor().BeginFrame();
  EXPECT_FALSE(controller.AutoscrollInProgress());
  EXPECT_FALSE(window_performance->IsAutoscrollActive());

  WebElement element = GetDocument().getElementById(AtomicString("log"));
  EXPECT_EQ("pointerdown scroll", element.InnerHTML().Utf8());

  ASSERT_TRUE(
      GetDocument().GetFrame()->Selection().GetSelectionInDOMTree().IsRange());
  Range* range = CreateRange(EphemeralRange(
      GetDocument().GetFrame()->Selection().GetSelectionInDOMTree().Anchor(),
      GetDocument().GetFrame()->Selection().GetSelectionInDOMTree().Focus()));
  ASSERT_TRUE(range);
  EXPECT_GT(range->GetText().length(), 0u);
}

}  // namespace blink
