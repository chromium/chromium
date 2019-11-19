// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/page/autoscroll_controller.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/css_value_keywords.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/input/event_handler.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"

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
  WebView().MainFrameWidget()->Resize(WebSize(800, 600));
  WebView().SetBaseBackgroundColorOverride(SK_ColorTRANSPARENT);
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

  Element* scrollable = document.getElementById("scrollable");
  DCHECK(scrollable);
  DCHECK(scrollable->GetLayoutObject());

  WebMouseEvent event(WebInputEvent::kMouseDown, WebFloatPoint(5, 5),
                      WebFloatPoint(5, 5), WebPointerProperties::Button::kLeft,
                      0, WebInputEvent::Modifiers::kLeftButtonDown,
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
  AutoscrollController& controller = GetAutoscrollController();
  LocalFrame* frame = GetDocument().GetFrame();

  EXPECT_FALSE(controller.IsAutoscrolling());

  controller.StartMiddleClickAutoscroll(frame, FloatPoint(), FloatPoint(),
                                        false, false);

  EXPECT_TRUE(controller.IsAutoscrolling());

  WebMouseEvent mouse_leave_event(WebInputEvent::kMouseLeave,
                                  WebInputEvent::kNoModifiers,
                                  base::TimeTicks::Now());
  mouse_leave_event.SetFrameScale(1);

  frame->GetEventHandler().HandleMouseLeaveEvent(mouse_leave_event);

  EXPECT_TRUE(controller.IsAutoscrolling());
}

}  // namespace blink
