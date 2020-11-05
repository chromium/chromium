// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/layout_shift_tracker.h"

#include "third_party/blink/public/common/input/web_mouse_event.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/svg_names.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"
#include "third_party/blink/renderer/core/timing/dom_window_performance.h"
#include "third_party/blink/renderer/core/timing/layout_shift.h"
#include "third_party/blink/renderer/core/timing/window_performance.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"

namespace blink {

class LayoutShiftTrackerTest : public RenderingTest {
 protected:
  void SetUp() override {
    EnableCompositing();
    RenderingTest::SetUp();
  }
  LocalFrameView& GetFrameView() { return *GetFrame().View(); }
  LayoutShiftTracker& GetLayoutShiftTracker() {
    return GetFrameView().GetLayoutShiftTracker();
  }

  void SimulateInput() {
    GetLayoutShiftTracker().NotifyInput(WebMouseEvent(
        WebInputEvent::Type::kMouseDown, gfx::PointF(), gfx::PointF(),
        WebPointerProperties::Button::kLeft, 0,
        WebInputEvent::Modifiers::kLeftButtonDown, base::TimeTicks::Now()));
  }
};

TEST_F(LayoutShiftTrackerTest, IgnoreAfterInput) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #j { position: relative; width: 300px; height: 100px; }
    </style>
    <div id='j'></div>
  )HTML");
  GetDocument().getElementById("j")->setAttribute(html_names::kStyleAttr,
                                                  AtomicString("top: 60px"));
  SimulateInput();
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(0.0, GetLayoutShiftTracker().Score());
  EXPECT_TRUE(GetLayoutShiftTracker().ObservedInputOrScroll());
  EXPECT_TRUE(GetLayoutShiftTracker()
                  .MostRecentInputTimestamp()
                  .since_origin()
                  .InSecondsF() > 0.0);
}

TEST_F(LayoutShiftTrackerTest, CompositedShiftBeforeFirstPaint) {
  // Tests that we don't crash if a new layer shifts during a second compositing
  // update before prepaint sets up property tree state.  See crbug.com/881735
  // (which invokes UpdateLifecycleToCompositingCleanPlusScrolling through
  // accessibilityController.accessibleElementById).

  SetBodyInnerHTML(R"HTML(
    <style>
      .hide { display: none; }
      .tr { will-change: transform; }
      body { margin: 0; }
      div { height: 100px; }
    </style>
    <div id="container">
      <div id="A">A</div>
      <div id="B" class="tr hide">B</div>
    </div>
  )HTML");

  GetDocument().getElementById("B")->setAttribute(html_names::kClassAttr,
                                                  AtomicString("tr"));
  GetFrameView().UpdateLifecycleToCompositingCleanPlusScrolling(
      DocumentUpdateReason::kTest);
  GetDocument().getElementById("A")->setAttribute(html_names::kClassAttr,
                                                  AtomicString("hide"));
  UpdateAllLifecyclePhasesForTest();
}

TEST_F(LayoutShiftTrackerTest, IgnoreSVG) {
  SetBodyInnerHTML(R"HTML(
    <svg>
      <circle cx="50" cy="50" r="40"
              stroke="black" stroke-width="3" fill="red" />
    </svg>
  )HTML");
  GetDocument().QuerySelector("circle")->setAttribute(svg_names::kCxAttr,
                                                      AtomicString("100"));
  UpdateAllLifecyclePhasesForTest();
  EXPECT_FLOAT_EQ(0, GetLayoutShiftTracker().Score());
}

class LayoutShiftTrackerSimTest : public SimTest {
 protected:
  void SetUp() override {
    SimTest::SetUp();
    WebView().MainFrameViewWidget()->Resize(gfx::Size(800, 600));
  }
};

TEST_F(LayoutShiftTrackerSimTest, SubframeWeighting) {
  // TODO(crbug.com/943668): Test OOPIF path.
  SimRequest main_resource("https://example.com/", "text/html");
  SimRequest child_resource("https://example.com/sub.html", "text/html");

  LoadURL("https://example.com/");
  main_resource.Complete(R"HTML(
    <style> #i { border: 0; position: absolute; left: 0; top: 0; } </style>
    <iframe id=i width=400 height=300 src='sub.html'></iframe>
  )HTML");

  Compositor().BeginFrame();
  test::RunPendingTasks();

  child_resource.Complete(R"HTML(
    <style>
      #j { position: relative; width: 300px; height: 100px; }
    </style>
    <div id='j'></div>
  )HTML");

  Compositor().BeginFrame();
  test::RunPendingTasks();

  WebLocalFrameImpl& child_frame =
      To<WebLocalFrameImpl>(*MainFrame().FirstChild());

  Element* div = child_frame.GetFrame()->GetDocument()->getElementById("j");
  div->setAttribute(html_names::kStyleAttr, AtomicString("top: 60px"));

  Compositor().BeginFrame();
  test::RunPendingTasks();

  // 300 * (100 + 60) * (60 / 400) / (default viewport size 800 * 600)
  LayoutShiftTracker& layout_shift_tracker =
      child_frame.GetFrameView()->GetLayoutShiftTracker();
  EXPECT_FLOAT_EQ(0.4 * (60.0 / 400.0), layout_shift_tracker.Score());
  EXPECT_FLOAT_EQ(0.1 * (60.0 / 400.0), layout_shift_tracker.WeightedScore());

  // Move subframe halfway outside the viewport.
  GetDocument().getElementById("i")->setAttribute(html_names::kStyleAttr,
                                                  AtomicString("left: 600px"));

  div->removeAttribute(html_names::kStyleAttr);

  Compositor().BeginFrame();
  test::RunPendingTasks();

  EXPECT_FLOAT_EQ(0.8 * (60.0 / 400.0), layout_shift_tracker.Score());
  EXPECT_FLOAT_EQ(0.15 * (60.0 / 400.0), layout_shift_tracker.WeightedScore());
}

TEST_F(LayoutShiftTrackerSimTest, ViewportSizeChange) {
  SimRequest main_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");
  main_resource.Complete(R"HTML(
    <style>
      body { margin: 0; }
      .square {
        display: inline-block;
        position: relative;
        width: 300px;
        height: 300px;
        background:yellow;
      }
    </style>
    <div class='square'></div>
    <div class='square'></div>
  )HTML");

  Compositor().BeginFrame();
  test::RunPendingTasks();

  // Resize the viewport, making it 400px wide. This should cause the second div
  // to change position during block layout flow. Since it was the result of a
  // viewport size change, this position change should not affect the score.
  WebView().MainFrameViewWidget()->Resize(gfx::Size(400, 600));

  Compositor().BeginFrame();
  test::RunPendingTasks();

  LayoutShiftTracker& layout_shift_tracker =
      MainFrame().GetFrameView()->GetLayoutShiftTracker();
  EXPECT_FLOAT_EQ(0.0, layout_shift_tracker.Score());
}

class LayoutShiftTrackerPointerdownTest : public LayoutShiftTrackerSimTest {
 protected:
  void RunTest(WebInputEvent::Type completion_type, bool expect_exclusion);
};

void LayoutShiftTrackerPointerdownTest::RunTest(
    WebInputEvent::Type completion_type,
    bool expect_exclusion) {
  SimRequest main_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");
  main_resource.Complete(R"HTML(
    <style>
      body { margin: 0; height: 1500px; }
      #box {
        left: 0px;
        top: 0px;
        width: 400px;
        height: 600px;
        background: yellow;
        position: relative;
      }
    </style>
    <div id="box"></div>
    <script>
      box.addEventListener("pointerdown", (e) => {
        box.style.top = "100px";
        e.preventDefault();
      });
    </script>
  )HTML");

  Compositor().BeginFrame();
  test::RunPendingTasks();

  WebPointerProperties pointer_properties = WebPointerProperties(
      1 /* PointerId */, WebPointerProperties::PointerType::kTouch,
      WebPointerProperties::Button::kLeft);

  WebPointerEvent event1(WebInputEvent::Type::kPointerDown, pointer_properties,
                         5, 5);
  WebPointerEvent event2(completion_type, pointer_properties, 5, 5);

  // Coordinates inside #box.
  event1.SetPositionInWidget(50, 150);
  event2.SetPositionInWidget(50, 160);

  WebView().MainFrameWidget()->HandleInputEvent(
      WebCoalescedInputEvent(event1, ui::LatencyInfo()));

  Compositor().BeginFrame();
  test::RunPendingTasks();

  WindowPerformance& perf = *DOMWindowPerformance::performance(Window());
  auto& tracker = MainFrame().GetFrameView()->GetLayoutShiftTracker();

  EXPECT_EQ(0u, perf.getBufferedEntriesByType("layout-shift").size());
  EXPECT_FLOAT_EQ(0.0, tracker.Score());

  WebView().MainFrameWidget()->HandleInputEvent(
      WebCoalescedInputEvent(event2, ui::LatencyInfo()));

  // region fraction 50%, distance fraction 1/8
  const double expected_shift = 0.5 * 0.125;

  auto entries = perf.getBufferedEntriesByType("layout-shift");
  EXPECT_EQ(1u, entries.size());
  LayoutShift* shift = static_cast<LayoutShift*>(entries.front().Get());

  EXPECT_EQ(expect_exclusion, shift->hadRecentInput());
  EXPECT_FLOAT_EQ(expected_shift, shift->value());
  EXPECT_FLOAT_EQ(expect_exclusion ? 0.0 : expected_shift, tracker.Score());
}

TEST_F(LayoutShiftTrackerPointerdownTest, PointerdownBecomesTap) {
  RunTest(WebInputEvent::Type::kPointerUp, true /* expect_exclusion */);
}

TEST_F(LayoutShiftTrackerPointerdownTest, PointerdownCancelled) {
  RunTest(WebInputEvent::Type::kPointerCancel, false /* expect_exclusion */);
}

TEST_F(LayoutShiftTrackerPointerdownTest, PointerdownBecomesScroll) {
  RunTest(WebInputEvent::Type::kPointerCausedUaAction,
          false /* expect_exclusion */);
}

TEST_F(LayoutShiftTrackerTest, StableCompositingChanges) {
  SetBodyInnerHTML(R"HTML(
    <style>
      body { margin: 0; }
      #outer {
        margin-left: 50px;
        margin-top: 50px;
        width: 200px;
        height: 200px;
        background: #dde;
      }
      .tr {
        will-change: transform;
      }
      .pl {
        position: relative;
        z-index: 0;
        left: 0;
        top: 0;
      }
      #inner {
        display: inline-block;
        width: 100px;
        height: 100px;
        background: #666;
        margin-left: 50px;
        margin-top: 50px;
      }
    </style>
    <div id=outer><div id=inner></div></div>
  )HTML");

  Element* element = GetDocument().getElementById("outer");
  size_t state = 0;
  auto advance = [this, element, &state]() -> bool {
    //
    // Test each of the following transitions:
    // - add/remove a PaintLayer
    // - add/remove a cc::Layer when there is already a PaintLayer
    // - add/remove a cc::Layer and a PaintLayer together

    static const char* states[] = {"", "pl", "pl tr", "pl", "", "tr", ""};
    element->setAttribute(html_names::kClassAttr, AtomicString(states[state]));
    UpdateAllLifecyclePhasesForTest();
    return ++state < sizeof states / sizeof *states;
  };
  while (advance()) {
  }
  EXPECT_FLOAT_EQ(0, GetLayoutShiftTracker().Score());
}

TEST_F(LayoutShiftTrackerTest, CompositedOverflowExpansion) {
  SetBodyInnerHTML(R"HTML(
    <style>

    html { will-change: transform; }
    body { height: 2000px; margin: 0; }
    #drop {
      position: absolute;
      width: 1px;
      height: 1px;
      left: -10000px;
      top: -1000px;
    }
    .pl {
      position: relative;
      background: #ddd;
      z-index: 0;
      width: 290px;
      height: 170px;
      left: 25px;
      top: 25px;
    }
    #comp {
      position: relative;
      width: 240px;
      height: 120px;
      background: #efe;
      will-change: transform;
      z-index: 0;
      left: 25px;
      top: 25px;
    }
    .sh {
      top: 515px !important;
    }

    </style>
    <div class="pl">
      <div id="comp"></div>
    </div>
    <div id="drop" style="display: none"></div>
  )HTML");

  Element* drop = GetDocument().getElementById("drop");
  drop->removeAttribute(html_names::kStyleAttr);
  UpdateAllLifecyclePhasesForTest();

  drop->setAttribute(html_names::kStyleAttr, AtomicString("display: none"));
  UpdateAllLifecyclePhasesForTest();

  EXPECT_FLOAT_EQ(0, GetLayoutShiftTracker().Score());

  Element* comp = GetDocument().getElementById("comp");
  comp->setAttribute(html_names::kClassAttr, AtomicString("sh"));
  drop->removeAttribute(html_names::kStyleAttr);
  UpdateAllLifecyclePhasesForTest();

  // old rect (240 * 120) / (800 * 600) = 0.06
  // new rect, 50% clipped by viewport (240 * 60) / (800 * 600) = 0.03
  // final score 0.06 + 0.03 = 0.09 * (490 move distance / 800)
  EXPECT_FLOAT_EQ(0.09 * (490.0 / 800.0), GetLayoutShiftTracker().Score());
}

}  // namespace blink
