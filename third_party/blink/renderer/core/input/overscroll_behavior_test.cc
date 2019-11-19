// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
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

  void SetInnerOverscrollBehavior(EOverscrollBehavior, EOverscrollBehavior);

  void ScrollBegin(double hint_x, double hint_y);
  void ScrollUpdate(double x, double y);
  void ScrollEnd();

  void Scroll(double x, double y);
};

void OverscrollBehaviorTest::SetUp() {
  SimTest::SetUp();
  v8::HandleScope HandleScope(v8::Isolate::GetCurrent());
  WebView().MainFrameWidget()->Resize(WebSize(400, 400));
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <div id='outer' style='height: 300px; width: 300px; overflow:
    scroll;'>
      <div id='inner' style='height: 500px; width: 500px; overflow:
    scroll;'>
        <div id='content' style='height: 700px; width: 700px;'>
    </div></div></div>
  )HTML");

  Compositor().BeginFrame();

  Element* outer = GetDocument().getElementById("outer");
  Element* inner = GetDocument().getElementById("inner");

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

void OverscrollBehaviorTest::SetInnerOverscrollBehavior(EOverscrollBehavior x,
                                                        EOverscrollBehavior y) {
  Element* inner = GetDocument().getElementById("inner");
  scoped_refptr<ComputedStyle> modified_style =
      ComputedStyle::Clone(*inner->GetComputedStyle());
  modified_style->SetOverscrollBehaviorX(x);
  modified_style->SetOverscrollBehaviorY(y);
  inner->GetLayoutObject()->SetModifiedStyleOutsideStyleRecalc(
      std::move(modified_style), LayoutObject::ApplyStyleChanges::kNo);
}

void OverscrollBehaviorTest::ScrollBegin(double hint_x, double hint_y) {
  WebGestureEvent event(WebInputEvent::kGestureScrollBegin,
                        WebInputEvent::kNoModifiers, base::TimeTicks::Now(),
                        WebGestureDevice::kTouchscreen);
  event.SetPositionInWidget(WebFloatPoint(20, 20));
  event.SetPositionInScreen(WebFloatPoint(20, 20));
  event.data.scroll_begin.delta_x_hint = -hint_x;
  event.data.scroll_begin.delta_y_hint = -hint_y;
  event.data.scroll_begin.pointer_count = 1;
  event.SetFrameScale(1);
  GetDocument().GetFrame()->GetEventHandler().HandleGestureScrollEvent(event);
}

void OverscrollBehaviorTest::ScrollUpdate(double delta_x, double delta_y) {
  WebGestureEvent event(WebInputEvent::kGestureScrollUpdate,
                        WebInputEvent::kNoModifiers, base::TimeTicks::Now(),
                        WebGestureDevice::kTouchscreen);
  event.SetPositionInWidget(WebFloatPoint(20, 20));
  event.SetPositionInScreen(WebFloatPoint(20, 20));
  event.data.scroll_update.delta_x = -delta_x;
  event.data.scroll_update.delta_y = -delta_y;
  event.SetFrameScale(1);
  GetDocument().GetFrame()->GetEventHandler().HandleGestureScrollEvent(event);
}

void OverscrollBehaviorTest::ScrollEnd() {
  WebGestureEvent event(WebInputEvent::kGestureScrollEnd,
                        WebInputEvent::kNoModifiers, base::TimeTicks::Now(),
                        WebGestureDevice::kTouchscreen);
  event.SetPositionInWidget(WebFloatPoint(20, 20));
  event.SetPositionInScreen(WebFloatPoint(20, 20));
  GetDocument().GetFrame()->GetEventHandler().HandleGestureScrollEvent(event);
}

void OverscrollBehaviorTest::Scroll(double x, double y) {
  ScrollBegin(x, y);
  ScrollUpdate(x, y);
  ScrollEnd();
}

TEST_F(OverscrollBehaviorTest, AutoAllowsPropagation) {
  SetInnerOverscrollBehavior(EOverscrollBehavior::kAuto,
                             EOverscrollBehavior::kAuto);
  Scroll(-100.0, -100.0);
  Element* outer = GetDocument().getElementById("outer");
  ASSERT_EQ(outer->scrollLeft(), 100);
  ASSERT_EQ(outer->scrollTop(), 100);
}

TEST_F(OverscrollBehaviorTest, ContainOnXPreventsPropagationsOnX) {
  SetInnerOverscrollBehavior(EOverscrollBehavior::kContain,
                             EOverscrollBehavior::kAuto);
  Scroll(-100, 0.0);
  Element* outer = GetDocument().getElementById("outer");
  ASSERT_EQ(outer->scrollLeft(), 200);
  ASSERT_EQ(outer->scrollTop(), 200);
}

TEST_F(OverscrollBehaviorTest, ContainOnXAllowsPropagationsOnY) {
  SetInnerOverscrollBehavior(EOverscrollBehavior::kContain,
                             EOverscrollBehavior::kAuto);
  Scroll(0.0, -100.0);
  Element* outer = GetDocument().getElementById("outer");
  ASSERT_EQ(outer->scrollLeft(), 200);
  ASSERT_EQ(outer->scrollTop(), 100);
}

TEST_F(OverscrollBehaviorTest, ContainOnXPreventsDiagonalPropagations) {
  SetInnerOverscrollBehavior(EOverscrollBehavior::kContain,
                             EOverscrollBehavior::kAuto);
  Scroll(-100.0, -100.0);
  Element* outer = GetDocument().getElementById("outer");
  ASSERT_EQ(outer->scrollLeft(), 200);
  ASSERT_EQ(outer->scrollTop(), 200);
}

TEST_F(OverscrollBehaviorTest, ContainOnYPreventsPropagationsOnY) {
  SetInnerOverscrollBehavior(EOverscrollBehavior::kAuto,
                             EOverscrollBehavior::kContain);
  Scroll(0.0, -100.0);
  Element* outer = GetDocument().getElementById("outer");
  ASSERT_EQ(outer->scrollLeft(), 200);
  ASSERT_EQ(outer->scrollTop(), 200);
}

TEST_F(OverscrollBehaviorTest, ContainOnYAllowsPropagationsOnX) {
  SetInnerOverscrollBehavior(EOverscrollBehavior::kAuto,
                             EOverscrollBehavior::kContain);
  Scroll(-100.0, 0.0);
  Element* outer = GetDocument().getElementById("outer");
  ASSERT_EQ(outer->scrollLeft(), 100);
  ASSERT_EQ(outer->scrollTop(), 200);
}

TEST_F(OverscrollBehaviorTest, ContainOnYPreventsDiagonalPropagations) {
  SetInnerOverscrollBehavior(EOverscrollBehavior::kAuto,
                             EOverscrollBehavior::kContain);
  Scroll(-100.0, -100.0);
  Element* outer = GetDocument().getElementById("outer");
  ASSERT_EQ(outer->scrollLeft(), 200);
  ASSERT_EQ(outer->scrollTop(), 200);
}

TEST_F(OverscrollBehaviorTest, LatchToTheElementPreventedByOverscrollBehavior) {
  SetInnerOverscrollBehavior(EOverscrollBehavior::kNone,
                             EOverscrollBehavior::kNone);
  ScrollBegin(-100, 0);
  ScrollUpdate(-100, 0);
  ScrollUpdate(100, 0);
  ScrollUpdate(0, -100);
  ScrollUpdate(0, 100);
  ScrollEnd();

  Element* inner = GetDocument().getElementById("inner");
  ASSERT_EQ(inner->scrollLeft(), 100);
  ASSERT_EQ(inner->scrollTop(), 100);
}

}  // namespace blink
