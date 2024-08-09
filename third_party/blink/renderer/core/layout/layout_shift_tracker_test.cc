// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/core/layout/layout_shift_tracker.h"

#include "third_party/blink/public/common/input/web_mouse_event.h"
#include "third_party/blink/renderer/core/dom/dom_token_list.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/visual_viewport.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/html/forms/html_select_element.h"
#include "third_party/blink/renderer/core/performance_entry_names.h"
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
      #j { position: relative; width: 300px; height: 100px; background: blue; }
    </style>
    <div id='j'></div>
  )HTML");
  GetElementById("j")->setAttribute(html_names::kStyleAttr,
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
  // (which invokes UpdateAllLifecyclePhasesExceptPaint through
  // accessibilityController.accessibleElementById).

  SetBodyInnerHTML(R"HTML(
    <style>
      .hide { display: none; }
      .tr { will-change: transform; }
      body { margin: 0; }
      div { height: 100px; background: blue; }
    </style>
    <div id="container">
      <div id="A">A</div>
      <div id="B" class="tr hide">B</div>
    </div>
  )HTML");

  GetElementById("B")->setAttribute(html_names::kClassAttr, AtomicString("tr"));
  GetFrameView().UpdateAllLifecyclePhasesExceptPaint(
      DocumentUpdateReason::kTest);
  GetElementById("A")->setAttribute(html_names::kClassAttr,
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
  GetDocument()
      .QuerySelector(AtomicString("circle"))
      ->setAttribute(svg_names::kCxAttr, AtomicString("100"));
  UpdateAllLifecyclePhasesForTest();
  EXPECT_FLOAT_EQ(0, GetLayoutShiftTracker().Score());
}

TEST_F(LayoutShiftTrackerTest, IgnoreAfterChangeEvent) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #j { position: relative; width: 300px; height: 100px; background: blue; }
    </style>
    <div id='j'></div>
    <select id="sel" onchange="shift()">
      <option value="0">0</option>
      <option value="1">1</option>
    </select>
  )HTML");
  auto* select = To<HTMLSelectElement>(GetElementById("sel"));
  DCHECK(select);
  select->Focus();
  select->SelectOptionByPopup(1);
  GetElementById("j")->setAttribute(html_names::kStyleAttr,
                                    AtomicString("top: 60px"));

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
      #j { position: relative; width: 300px; height: 100px; background: blue; }
    </style>
    <div id='j'></div>
  )HTML");

  Compositor().BeginFrame();
  test::RunPendingTasks();

  WebLocalFrameImpl& child_frame =
      To<WebLocalFrameImpl>(*MainFrame().FirstChild());

  Element* div =
      child_frame.GetFrame()->GetDocument()->getElementById(AtomicString("j"));
  div->setAttribute(html_names::kStyleAttr, AtomicString("top: 60px"));

  Compositor().BeginFrame();
  test::RunPendingTasks();

  // 300 * (100 + 60) * (60 / 400) / (default viewport size 800 * 600)
  LayoutShiftTracker& layout_shift_tracker =
      child_frame.GetFrameView()->GetLayoutShiftTracker();
  EXPECT_FLOAT_EQ(0.4 * (60.0 / 400.0), layout_shift_tracker.Score());
  EXPECT_FLOAT_EQ(0.1 * (60.0 / 400.0), layout_shift_tracker.WeightedScore());

  // Move subframe halfway outside the viewport.
  GetDocument()
      .getElementById(AtomicString("i"))
      ->setAttribute(html_names::kStyleAttr, AtomicString("left: 600px"));

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

TEST_F(LayoutShiftTrackerSimTest, ZoomLevelChange) {
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

  WebView().MainFrameViewWidget()->SetZoomLevelForTesting(1.0);

  Compositor().BeginFrame();
  test::RunPendingTasks();

  LayoutShiftTracker& layout_shift_tracker =
      MainFrame().GetFrameView()->GetLayoutShiftTracker();
  EXPECT_FLOAT_EQ(0.0, layout_shift_tracker.Score());
}

class LayoutShiftTrackerNavigationTest : public LayoutShiftTrackerSimTest {
 protected:
  void RunTest(bool is_browser_initiated);
};

void LayoutShiftTrackerNavigationTest::RunTest(bool is_browser_initiated) {
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
        position: absolute;
      }
    </style>
    <div id="box"></div>
    <script>
      box.addEventListener("mouseup", (e) => {
        window.location.hash = '#a';
        e.preventDefault();
      });
      window.addEventListener('hashchange', () => {
        const shouldShow = window.location.hash === '#a';
        if (shouldShow)
          box.style.top = "100px";
        else
          box.style.top = "0px";
      });
    </script>
  )HTML");

  Compositor().BeginFrame();
  test::RunPendingTasks();

  auto* main_frame = To<LocalFrame>(WebView().GetPage()->MainFrame());
  Persistent<HistoryItem> item1 =
      main_frame->Loader().GetDocumentLoader()->GetHistoryItem();

  WebMouseEvent event1(WebInputEvent::Type::kMouseDown, gfx::PointF(),
                       gfx::PointF(), WebPointerProperties::Button::kLeft, 0,
                       WebInputEvent::Modifiers::kLeftButtonDown,
                       base::TimeTicks::Now());
  WebMouseEvent event2(WebInputEvent::Type::kMouseUp, gfx::PointF(),
                       gfx::PointF(), WebPointerProperties::Button::kLeft, 1,
                       WebInputEvent::Modifiers::kLeftButtonDown,
                       base::TimeTicks::Now());

  // Coordinates inside #box.
  event1.SetPositionInWidget(50, 150);
  event2.SetPositionInWidget(50, 160);

  WebView().MainFrameWidget()->HandleInputEvent(
      WebCoalescedInputEvent(event1, ui::LatencyInfo()));
  WebView().MainFrameWidget()->HandleInputEvent(
      WebCoalescedInputEvent(event2, ui::LatencyInfo()));

  Compositor().BeginFrame();
  test::RunPendingTasks();
  LayoutShiftTracker& layout_shift_tracker =
      MainFrame().GetFrameView()->GetLayoutShiftTracker();
  layout_shift_tracker.ResetTimerForTesting();

  Persistent<HistoryItem> item2 =
      main_frame->Loader().GetDocumentLoader()->GetHistoryItem();

  main_frame->Loader().GetDocumentLoader()->CommitSameDocumentNavigation(
      item1->Url(), WebFrameLoadType::kBackForward, item1.Get(),
      ClientRedirectPolicy::kNotClientRedirect,
      /*has_transient_user_activation=*/false, /*initiator_origin=*/nullptr,
      /*is_synchronously_committed=*/false, /*source_element=*/nullptr,
      mojom::blink::TriggeringEventInfo::kNotFromEvent, is_browser_initiated,
      /*has_ua_visual_transition,=*/false,
      /*soft_navigation_heuristics_task_id=*/std::nullopt);

  Compositor().BeginFrame();
  test::RunPendingTasks();

  WindowPerformance& perf = *DOMWindowPerformance::performance(Window());
  auto entries =
      perf.getBufferedEntriesByType(performance_entry_names::kLayoutShift);
  EXPECT_EQ(1u, entries.size());
  LayoutShift* shift = static_cast<LayoutShift*>(entries.front().Get());
  // region fraction 50%, distance fraction 1/8
  const double expected_shift_value = 0.5 * 0.125;
  const double expected_cls_score =
      is_browser_initiated ? 0 : expected_shift_value;

  // Set hadRecentInput to be true for browser initiated history navigation,
  // and the layout shift score will be 0.
  EXPECT_EQ(is_browser_initiated, shift->hadRecentInput());
  EXPECT_FLOAT_EQ(expected_shift_value, shift->value());
  EXPECT_FLOAT_EQ(expected_cls_score, layout_shift_tracker.Score());
}

TEST_F(LayoutShiftTrackerNavigationTest,
       BrowserInitiatedSameDocumentHistoryNavigation) {
  RunTest(true /* is_browser_initiated */);
}

TEST_F(LayoutShiftTrackerNavigationTest,
       RendererInitiatedSameDocumentHistoryNavigation) {
  RunTest(false /* is_browser_initiated */);
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

  EXPECT_EQ(0u,
            perf.getBufferedEntriesByType(performance_entry_names::kLayoutShift)
                .size());
  EXPECT_FLOAT_EQ(0.0, tracker.Score());

  WebView().MainFrameWidget()->HandleInputEvent(
      WebCoalescedInputEvent(event2, ui::LatencyInfo()));

  // region fraction 50%, distance fraction 1/8
  const double expected_shift = 0.5 * 0.125;

  auto entries =
      perf.getBufferedEntriesByType(performance_entry_names::kLayoutShift);
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

TEST_F(LayoutShiftTrackerSimTest, MouseMoveDraggingAction) {
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
        position: absolute;
      }
    </style>
    <div id="box"></div>
    <script>
      box.addEventListener("mousemove", (e) => {
        box.style.top = "50px";
        e.preventDefault();
      });
      box.addEventListener("mouseup", (e) => {
        box.style.top = "100px";
        e.preventDefault();
      });
    </script>
  )HTML");

  Compositor().BeginFrame();
  test::RunPendingTasks();

  WebMouseEvent event1(WebInputEvent::Type::kMouseDown, gfx::PointF(),
                       gfx::PointF(), WebPointerProperties::Button::kLeft, 0,
                       WebInputEvent::Modifiers::kLeftButtonDown,
                       base::TimeTicks::Now());
  WebMouseEvent event2(WebInputEvent::Type::kMouseMove, gfx::PointF(),
                       gfx::PointF(), WebPointerProperties::Button::kLeft, 1,
                       WebInputEvent::Modifiers::kLeftButtonDown,
                       base::TimeTicks::Now());
  WebMouseEvent event3(WebInputEvent::Type::kMouseUp, gfx::PointF(),
                       gfx::PointF(), WebPointerProperties::Button::kLeft, 1,
                       WebInputEvent::Modifiers::kLeftButtonDown,
                       base::TimeTicks::Now());

  // Coordinates inside #box.
  event1.SetPositionInWidget(50, 150);
  event2.SetPositionInWidget(50, 160);
  event3.SetPositionInWidget(50, 160);

  WebView().MainFrameWidget()->HandleInputEvent(
      WebCoalescedInputEvent(event1, ui::LatencyInfo()));

  WindowPerformance& perf = *DOMWindowPerformance::performance(Window());
  auto& tracker = MainFrame().GetFrameView()->GetLayoutShiftTracker();
  Compositor().BeginFrame();
  test::RunPendingTasks();

  EXPECT_EQ(0u,
            perf.getBufferedEntriesByType(performance_entry_names::kLayoutShift)
                .size());
  EXPECT_FLOAT_EQ(0.0, tracker.Score());

  tracker.ResetTimerForTesting();

  WebView().MainFrameWidget()->HandleInputEvent(
      WebCoalescedInputEvent(event2, ui::LatencyInfo()));

  Compositor().BeginFrame();
  test::RunPendingTasks();

  EXPECT_EQ(0u,
            perf.getBufferedEntriesByType(performance_entry_names::kLayoutShift)
                .size());
  EXPECT_FLOAT_EQ(0.0, tracker.Score());

  tracker.ResetTimerForTesting();

  WebView().MainFrameWidget()->HandleInputEvent(
      WebCoalescedInputEvent(event3, ui::LatencyInfo()));

  Compositor().BeginFrame();
  test::RunPendingTasks();

  auto entries =
      perf.getBufferedEntriesByType(performance_entry_names::kLayoutShift);
  EXPECT_EQ(2u, entries.size());
  LayoutShift* shift = static_cast<LayoutShift*>(entries.back().Get());

  EXPECT_TRUE(shift->hadRecentInput());
  EXPECT_GT(shift->value(), 0);
  EXPECT_FLOAT_EQ(0.0, tracker.Score());
}

TEST_F(LayoutShiftTrackerSimTest, TouchDraggingAction) {
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
        position: absolute;
      }
    </style>
    <div id="box"></div>
    <script>
      box.addEventListener("pointermove", (e) => {
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
  WebPointerEvent event2(WebInputEvent::Type::kPointerMove, pointer_properties,
                         5, 5);
  WebPointerEvent event3(WebInputEvent::Type::kPointerUp, pointer_properties, 5,
                         5);

  // Coordinates inside #box.
  event1.SetPositionInWidget(100, 160);
  event2.SetPositionInWidget(100, 180);
  event3.SetPositionInWidget(100, 180);

  WebView().MainFrameWidget()->HandleInputEvent(
      WebCoalescedInputEvent(event1, ui::LatencyInfo()));

  WindowPerformance& perf = *DOMWindowPerformance::performance(Window());
  auto& tracker = MainFrame().GetFrameView()->GetLayoutShiftTracker();

  EXPECT_EQ(0u,
            perf.getBufferedEntriesByType(performance_entry_names::kLayoutShift)
                .size());
  EXPECT_FLOAT_EQ(0.0, tracker.Score());

  WebView().MainFrameWidget()->HandleInputEvent(
      WebCoalescedInputEvent(event2, ui::LatencyInfo()));

  // Executes the BeginMainFrame processing steps and calls ReportShift in
  // LayoutShiftTracker to get the latest layout shift score.
  Compositor().BeginFrame();
  test::RunPendingTasks();

  EXPECT_EQ(0u,
            perf.getBufferedEntriesByType(performance_entry_names::kLayoutShift)
                .size());
  EXPECT_FLOAT_EQ(0.0, tracker.Score());

  WebView().MainFrameWidget()->HandleInputEvent(
      WebCoalescedInputEvent(event3, ui::LatencyInfo()));

  // region fraction 50%, distance fraction 1/8
  const double expected_shift = 0.5 * 0.125;

  auto entries =
      perf.getBufferedEntriesByType(performance_entry_names::kLayoutShift);
  EXPECT_EQ(1u, entries.size());
  LayoutShift* shift = static_cast<LayoutShift*>(entries.back().Get());

  EXPECT_TRUE(shift->hadRecentInput());
  EXPECT_FLOAT_EQ(expected_shift, shift->value());
  EXPECT_FLOAT_EQ(0.0, tracker.Score());
}

TEST_F(LayoutShiftTrackerSimTest, TouchScrollingAction) {
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
        position: absolute;
      }
    </style>
    <div id="box"></div>
    <script>
      box.addEventListener("pointermove", (e) => {
        box.style.top = e.clientY;
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
  WebPointerEvent event2(WebInputEvent::Type::kPointerMove, pointer_properties,
                         5, 5);
  WebPointerEvent event3(WebInputEvent::Type::kPointerCancel,
                         pointer_properties, 5, 5);
  WebPointerEvent event4(WebInputEvent::Type::kPointerMove, pointer_properties,
                         5, 5);

  // Coordinates inside #box.
  event1.SetPositionInWidget(80, 90);
  event2.SetPositionInWidget(80, 100);
  event3.SetPositionInWidget(80, 100);
  event4.SetPositionInWidget(80, 150);

  WebView().MainFrameWidget()->HandleInputEvent(
      WebCoalescedInputEvent(event1, ui::LatencyInfo()));

  WindowPerformance& perf = *DOMWindowPerformance::performance(Window());
  auto& tracker = MainFrame().GetFrameView()->GetLayoutShiftTracker();

  EXPECT_EQ(0u,
            perf.getBufferedEntriesByType(performance_entry_names::kLayoutShift)
                .size());
  EXPECT_FLOAT_EQ(0.0, tracker.Score());

  WebView().MainFrameWidget()->HandleInputEvent(
      WebCoalescedInputEvent(event2, ui::LatencyInfo()));

  // Executes the BeginMainFrame processing steps and calls ReportShift in
  // LayoutShiftTracker to get the latest layout shift score.
  Compositor().BeginFrame();
  test::RunPendingTasks();

  EXPECT_EQ(0u,
            perf.getBufferedEntriesByType(performance_entry_names::kLayoutShift)
                .size());
  EXPECT_FLOAT_EQ(0, tracker.Score());

  WebView().MainFrameWidget()->HandleInputEvent(
      WebCoalescedInputEvent(event3, ui::LatencyInfo()));

  // region fraction 50%, distance fraction 1/8
  const double expected_shift = 0.5 * 0.125;
  auto entries =
      perf.getBufferedEntriesByType(performance_entry_names::kLayoutShift);
  EXPECT_EQ(1u, entries.size());
  LayoutShift* shift = static_cast<LayoutShift*>(entries.back().Get());

  // For touch scroll, hasRecentInput is false, and the layout shift score is
  // reported when a PointerCancel event is received.
  EXPECT_FALSE(shift->hadRecentInput());
  EXPECT_FLOAT_EQ(expected_shift, shift->value());
  EXPECT_FLOAT_EQ(expected_shift, tracker.Score());

  WebView().MainFrameWidget()->HandleInputEvent(
      WebCoalescedInputEvent(event4, ui::LatencyInfo()));

  // Executes the BeginMainFrame processing steps and calls ReportShift in
  // LayoutShiftTracker to get the latest layout shift score.
  Compositor().BeginFrame();
  test::RunPendingTasks();

  entries =
      perf.getBufferedEntriesByType(performance_entry_names::kLayoutShift);
  EXPECT_EQ(2u, entries.size());
  shift = static_cast<LayoutShift*>(entries.back().Get());

  EXPECT_FALSE(shift->hadRecentInput());
  EXPECT_GT(shift->value(), 0);
  EXPECT_GT(tracker.Score(), expected_shift);
}

TEST_F(LayoutShiftTrackerSimTest, MultiplePointerDownUps) {
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
        position: absolute;
      }
    </style>
    <div id="box"></div>
    <script>
      box.addEventListener("pointerup", (e) => {
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
  WebPointerEvent event2(WebInputEvent::Type::kPointerDown, pointer_properties,
                         5, 5);
  WebPointerEvent event3(WebInputEvent::Type::kPointerUp, pointer_properties, 5,
                         5);
  WebPointerEvent event4(WebInputEvent::Type::kPointerUp, pointer_properties, 5,
                         5);

  // Coordinates inside #box.
  event1.SetPositionInWidget(90, 110);
  event2.SetPositionInWidget(90, 110);
  event3.SetPositionInWidget(90, 110);
  event4.SetPositionInWidget(90, 110);

  WebView().MainFrameWidget()->HandleInputEvent(
      WebCoalescedInputEvent(event1, ui::LatencyInfo()));

  WindowPerformance& perf = *DOMWindowPerformance::performance(Window());
  auto& tracker = MainFrame().GetFrameView()->GetLayoutShiftTracker();

  EXPECT_EQ(0u,
            perf.getBufferedEntriesByType(performance_entry_names::kLayoutShift)
                .size());
  EXPECT_FLOAT_EQ(0.0, tracker.Score());

  WebView().MainFrameWidget()->HandleInputEvent(
      WebCoalescedInputEvent(event2, ui::LatencyInfo()));

  EXPECT_EQ(0u,
            perf.getBufferedEntriesByType(performance_entry_names::kLayoutShift)
                .size());
  EXPECT_FLOAT_EQ(0, tracker.Score());

  WebView().MainFrameWidget()->HandleInputEvent(
      WebCoalescedInputEvent(event3, ui::LatencyInfo()));

  // Executes the BeginMainFrame processing steps and calls ReportShift in
  // LayoutShiftTracker to get the latest layout shift score.
  Compositor().BeginFrame();
  test::RunPendingTasks();

  EXPECT_EQ(0u,
            perf.getBufferedEntriesByType(performance_entry_names::kLayoutShift)
                .size());
  EXPECT_FLOAT_EQ(0, tracker.Score());

  WebView().MainFrameWidget()->HandleInputEvent(
      WebCoalescedInputEvent(event4, ui::LatencyInfo()));

  // region fraction 50%, distance fraction 1/8
  const double expected_shift = 0.5 * 0.125;
  auto entries =
      perf.getBufferedEntriesByType(performance_entry_names::kLayoutShift);
  EXPECT_EQ(1u, entries.size());
  LayoutShift* shift = static_cast<LayoutShift*>(entries.back().Get());

  EXPECT_TRUE(shift->hadRecentInput());
  EXPECT_FLOAT_EQ(expected_shift, shift->value());
  EXPECT_FLOAT_EQ(0.0, tracker.Score());
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

  Element* element = GetElementById("outer");
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

  Element* drop = GetElementById("drop");
  drop->removeAttribute(html_names::kStyleAttr);
  UpdateAllLifecyclePhasesForTest();

  drop->setAttribute(html_names::kStyleAttr, AtomicString("display: none"));
  UpdateAllLifecyclePhasesForTest();

  EXPECT_FLOAT_EQ(0, GetLayoutShiftTracker().Score());

  Element* comp = GetElementById("comp");
  comp->setAttribute(html_names::kClassAttr, AtomicString("sh"));
  drop->removeAttribute(html_names::kStyleAttr);
  UpdateAllLifecyclePhasesForTest();

  // old rect (240 * 120) / (800 * 600) = 0.06
  // new rect, 50% clipped by viewport (240 * 60) / (800 * 600) = 0.03
  // final score 0.06 + 0.03 = 0.09 * (490 move distance / 800)
  EXPECT_FLOAT_EQ(0.09 * (490.0 / 800.0), GetLayoutShiftTracker().Score());
}

TEST_F(LayoutShiftTrackerTest, ContentVisibilityAutoFirstPaint) {
  SetBodyInnerHTML(R"HTML(
    <style>
      .auto {
        content-visibility: auto;
        contain-intrinsic-size: 1px;
        width: 100px;
      }
    </style>
    <div id=target class=auto>
      <div style="width: 100px; height: 100px; background: blue"></div>
    </div>
  )HTML");
  auto* target = To<LayoutBox>(GetLayoutObjectByElementId("target"));

  // Because it's on-screen on the first frame, #target renders at size
  // 100x100 on the first frame, via a synchronous second layout, and there is
  // no CLS impact.
  EXPECT_FLOAT_EQ(0, GetLayoutShiftTracker().Score());
  EXPECT_EQ(PhysicalSize(100, 100), target->Size());
}

TEST_F(LayoutShiftTrackerTest,
       ContentVisibilityAutoOffscreenAfterScrollFirstPaint) {
  SetBodyInnerHTML(R"HTML(
    <style>
      .auto {
        content-visibility: auto;
        contain-intrinsic-size: 1px;
        width: 100px;
      }
    </style>
    <div id=target class=auto style="position: relative; top: 100000px">
      <div style="width: 100px; height: 100px; background: blue"></div>
    </div>
  )HTML");
  auto* target = To<LayoutBox>(GetLayoutObjectByElementId("target"));
  // #target starts offsceen, which doesn't count for CLS.
  EXPECT_FLOAT_EQ(0, GetLayoutShiftTracker().Score());
  EXPECT_EQ(PhysicalSize(100, 1), target->Size());

  // In the next frame, we scroll it onto the screen, but it still doesn't
  // count for CLS, and its subtree is not yet unskipped, because the
  // intersection observation takes effect on the subsequent frame.
  GetDocument().domWindow()->scrollTo(0, 100000);
  UpdateAllLifecyclePhasesForTest();
  EXPECT_FLOAT_EQ(0, GetLayoutShiftTracker().Score());
  EXPECT_EQ(PhysicalSize(100, 1), target->Size());

  // Now the subtree is unskipped, and #target renders at size 100x100.
  // Nevertheless, there is no impact on CLS.
  UpdateAllLifecyclePhasesForTest();
  // Target's LayoutObject gets re-attached.
  target = To<LayoutBox>(GetLayoutObjectByElementId("target"));
  EXPECT_FLOAT_EQ(0, GetLayoutShiftTracker().Score());
  EXPECT_EQ(PhysicalSize(100, 100), target->Size());
}

TEST_F(LayoutShiftTrackerTest, ContentVisibilityHiddenFirstPaint) {
  SetBodyInnerHTML(R"HTML(
    <style>
      .auto {
        content-visibility: hidden;
        contain-intrinsic-size: 1px;
        width: 100px;
      }
    </style>
    <div id=target class=auto>
      <div style="width: 100px; height: 100px; background: blue"></div>
    </div>
  )HTML");
  auto* target = To<LayoutBox>(GetLayoutObjectByElementId("target"));

  // Skipped subtrees don't cause CLS impact.
  EXPECT_FLOAT_EQ(0, GetLayoutShiftTracker().Score());
  EXPECT_EQ(PhysicalSize(100, 1), target->Size());
}

TEST_F(LayoutShiftTrackerTest, ContentVisibilityAutoResize) {
  SetBodyInnerHTML(R"HTML(
    <style>
      .auto {
        content-visibility: auto;
        contain-intrinsic-size: 10px 3000px;
        width: 100px;
      }
      .contained {
        height: 100px;
        background: blue;
      }
    </style>
    <div class=auto><div class=contained></div></div>
    <div class=auto id=target><div class=contained></div></div>
  )HTML");

  // Skipped subtrees don't cause CLS impact.
  UpdateAllLifecyclePhasesForTest();
  auto* target = To<LayoutBox>(GetLayoutObjectByElementId("target"));
  EXPECT_FLOAT_EQ(0, GetLayoutShiftTracker().Score());
  EXPECT_EQ(PhysicalSize(100, 100), target->Size());
}

TEST_F(LayoutShiftTrackerTest,
       ContentVisibilityAutoOnscreenAndOffscreenAfterScrollFirstPaint) {
  SetBodyInnerHTML(R"HTML(
    <style>
      .auto {
        content-visibility: auto;
        contain-intrinsic-size: 1px;
        width: 100px;
      }
    </style>
    <div id=onscreen class=auto>
      <div style="width: 100px; height: 100px; background: blue"></div>
    </div>
    <div id=offscreen class=auto style="position: relative; top: 100000px">
      <div style="width: 100px; height: 100px; background: blue"></div>
    </div>
  )HTML");
  auto* offscreen = To<LayoutBox>(GetLayoutObjectByElementId("offscreen"));
  auto* onscreen = To<LayoutBox>(GetLayoutObjectByElementId("onscreen"));

  // #offscreen starts offsceen, which doesn't count for CLS.
  EXPECT_FLOAT_EQ(0, GetLayoutShiftTracker().Score());
  EXPECT_EQ(PhysicalSize(100, 1), offscreen->Size());
  EXPECT_EQ(PhysicalSize(100, 100), onscreen->Size());

  // In the next frame, we scroll it onto the screen, but it still doesn't
  // count for CLS, and its subtree is not yet unskipped, because the
  // intersection observation takes effect on the subsequent frame.
  GetDocument().domWindow()->scrollTo(0, 100000 + 100);
  UpdateAllLifecyclePhasesForTest();
  EXPECT_FLOAT_EQ(0, GetLayoutShiftTracker().Score());
  EXPECT_EQ(PhysicalSize(100, 1), offscreen->Size());
  EXPECT_EQ(PhysicalSize(100, 100), onscreen->Size());

  // Now the subtree is unskipped, and #offscreen renders at size 100x100.
  // Nevertheless, there is no impact on CLS.
  UpdateAllLifecyclePhasesForTest();
  offscreen = To<LayoutBox>(GetLayoutObjectByElementId("offscreen"));
  onscreen = To<LayoutBox>(GetLayoutObjectByElementId("onscreen"));

  // Target's LayoutObject gets re-attached.
  offscreen = To<LayoutBox>(GetLayoutObjectByElementId("offscreen"));
  EXPECT_FLOAT_EQ(0, GetLayoutShiftTracker().Score());
  EXPECT_EQ(PhysicalSize(100, 100), offscreen->Size());
  // Because content-visibility: auto implies contain-intrinsic-size auto, the
  // size stays at 100x100.
  EXPECT_EQ(PhysicalSize(100, 100), onscreen->Size());

  // Move |offscreen| (which is visible and unlocked now), for which we should
  // report layout shift.
  To<Element>(offscreen->GetNode())
      ->setAttribute(html_names::kStyleAttr,
                     AtomicString("position: relative; top: 100100px"));
  UpdateAllLifecyclePhasesForTest();
  auto score = GetLayoutShiftTracker().Score();
  EXPECT_GT(score, 0);

  // Now scroll the element back off-screen.
  GetDocument().domWindow()->scrollTo(0, 0);
  UpdateAllLifecyclePhasesForTest();
  EXPECT_FLOAT_EQ(score, GetLayoutShiftTracker().Score());
  EXPECT_EQ(PhysicalSize(100, 100), offscreen->Size());
  EXPECT_EQ(PhysicalSize(100, 100), onscreen->Size());

  // In the subsequent frame, #offscreen becomes locked and changes its
  // layout size (and vice-versa for #onscreen).
  UpdateAllLifecyclePhasesForTest();
  offscreen = To<LayoutBox>(GetLayoutObjectByElementId("offscreen"));
  onscreen = To<LayoutBox>(GetLayoutObjectByElementId("onscreen"));

  EXPECT_FLOAT_EQ(score, GetLayoutShiftTracker().Score());
  EXPECT_EQ(PhysicalSize(100, 100), offscreen->Size());
  EXPECT_EQ(PhysicalSize(100, 100), onscreen->Size());
}

TEST_F(LayoutShiftTrackerTest, NestedFixedPos) {
  SetBodyInnerHTML(R"HTML(
    <div id=parent style="position: fixed; top: 0; left: -100%; width: 100%">
      <div id=target style="position: fixed; top: 0; width: 100%; height: 100%;
                            left: 0"; background: blue></div>
    </div>
    <div style="height: 5000px"></div>
  </div>
  )HTML");

  auto* target = To<LayoutBox>(GetLayoutObjectByElementId("target"));
  EXPECT_FLOAT_EQ(0, GetLayoutShiftTracker().Score());

  // Test that repaint of #target does not record a layout shift.
  target->SetNeedsPaintPropertyUpdate();
  target->SetSubtreeShouldDoFullPaintInvalidation();
  UpdateAllLifecyclePhasesForTest();
  EXPECT_FLOAT_EQ(0, GetLayoutShiftTracker().Score());
}

TEST_F(LayoutShiftTrackerTest, ClipByVisualViewport) {
  SetHtmlInnerHTML(R"HTML(
    <meta name="viewport" content="width=200, initial-scale=2">
    <style>
      #target {
        position: absolute;
        top: 0;
        left: 150px;
        width: 200px;
        height: 200px;
        background: blue;
      }
    </style>
    <div id=target></div>
  )HTML");

  GetDocument().GetPage()->GetVisualViewport().SetSize(gfx::Size(200, 500));
  GetDocument().GetPage()->GetVisualViewport().SetLocation(gfx::PointF(0, 100));
  UpdateAllLifecyclePhasesForTest();
  // The visual viewport.
  EXPECT_EQ(gfx::Rect(0, 100, 200, 500),
            GetDocument().View()->GetScrollableArea()->VisibleContentRect());
  // The layout viewport .
  EXPECT_EQ(gfx::Rect(0, 0, 800, 600),
            GetDocument().View()->LayoutViewport()->VisibleContentRect());
  EXPECT_FLOAT_EQ(0, GetLayoutShiftTracker().Score());

  GetElementById("target")->setAttribute(html_names::kStyleAttr,
                                         AtomicString("top: 100px"));
  UpdateAllLifecyclePhasesForTest();
  // 50.0: visible width
  // 100.0 + 100.0: visible height + vertical shift
  // 200.0 * 500.0: visual viewport area
  // 100.0 / 500.0: shift distance fraction
  EXPECT_FLOAT_EQ(50.0 * (100.0 + 100.0) / (200.0 * 500.0) * (100.0 / 500.0),
                  GetLayoutShiftTracker().Score());
}

TEST_F(LayoutShiftTrackerTest, ScrollThenCauseScrollAnchoring) {
  SetBodyInnerHTML(R"HTML(
    <style>
      .big {
        width: 100px;
        height: 500px;
        background: blue;
      }
      .small {
        width: 100px;
        height: 100px;
        background: green;
      }
    </style>
    <div class=big id=target></div>
    <div class=big></div>
    <div class=big></div>
    <div class=big></div>
    <div class=big></div>
    <div class=big></div>
  )HTML");
  auto* target_element = GetElementById("target");

  // Scroll the window which accumulates a scroll in the layout shift tracker.
  GetDocument().domWindow()->scrollBy(0, 1000);
  UpdateAllLifecyclePhasesForTest();

  EXPECT_FLOAT_EQ(0, GetLayoutShiftTracker().Score());

  target_element->classList().Remove(AtomicString("big"));
  target_element->classList().Add(AtomicString("small"));
  UpdateAllLifecyclePhasesForTest();

  EXPECT_FLOAT_EQ(0, GetLayoutShiftTracker().Score());

  target_element->classList().Remove(AtomicString("small"));
  target_element->classList().Add(AtomicString("big"));
  UpdateAllLifecyclePhasesForTest();

  EXPECT_FLOAT_EQ(0, GetLayoutShiftTracker().Score());
}

TEST_F(LayoutShiftTrackerTest, NeedsToTrack) {
  SetBodyInnerHTML(R"HTML(
    <style>* { width: 50px; height: 50px; }</style>
    <div id="tiny" style="width: 0.3px; height: 0.3px; background: blue"></div>
    <div id="sticky" style="background: blue; position: sticky"></div>

    <!-- block with decoration -->
    <div id="scroll" style="overflow: scroll"></div>
    <div id="background" style="background: blue"></div>
    <div id="border" style="border: 1px solid black"></div>
    <div id="outline" style="outline: 1px solid black"></div>
    <div id="shadow" style="box-shadow: 2px 2px black"></div>

    <!-- block with block children, some invisible -->
    <div id="hidden-parent">
      <div id="hidden" style="background: blue; visibility: hidden">
        <div id="visible-under-hidden"
             style="background:blue; visibility: visible"></div>
      </div>
    </div>

    <!-- block with inline children, some invisible -->
    <div id="empty-parent">
      <div id="empty"></div>
    </div>
    <div id="text-block">Text</div>
    <br id="br">

    <svg id="svg">
      <rect id="svg-rect" width="10" height="10" fill="green">
    </svg>

    <!-- replaced, special blocks, etc. -->
    <video id="video"></video>
    <img id="img">
    <textarea id="textarea">Text</textarea>
    <input id="text-input" type="text">
    <input id="file" type="file">
    <input id="radio" type="radio">
    <progress id="progress"></progress>
    <ul>
      <li id="li"></li>
    </ul>
    <hr id="hr">
  )HTML");

  const auto& tracker = GetLayoutShiftTracker();
  EXPECT_FALSE(tracker.NeedsToTrack(GetLayoutView()));
  EXPECT_FALSE(tracker.NeedsToTrack(*GetLayoutObjectByElementId("tiny")));
  EXPECT_FALSE(tracker.NeedsToTrack(*GetLayoutObjectByElementId("sticky")));

  // Blocks with decorations.
  EXPECT_TRUE(tracker.NeedsToTrack(*GetLayoutObjectByElementId("scroll")));
  EXPECT_TRUE(tracker.NeedsToTrack(*GetLayoutObjectByElementId("background")));
  EXPECT_TRUE(tracker.NeedsToTrack(*GetLayoutObjectByElementId("border")));
  EXPECT_TRUE(tracker.NeedsToTrack(*GetLayoutObjectByElementId("outline")));
  EXPECT_TRUE(tracker.NeedsToTrack(*GetLayoutObjectByElementId("shadow")));

  // Blocks with block children, some invisible. We don't check descendants for
  // visibility. Just assume there are visible descendants.
  EXPECT_TRUE(
      tracker.NeedsToTrack(*GetLayoutObjectByElementId("empty-parent")));
  EXPECT_FALSE(tracker.NeedsToTrack(*GetLayoutObjectByElementId("empty")));
  EXPECT_TRUE(
      tracker.NeedsToTrack(*GetLayoutObjectByElementId("hidden-parent")));
  EXPECT_FALSE(tracker.NeedsToTrack(*GetLayoutObjectByElementId("hidden")));
  EXPECT_TRUE(tracker.NeedsToTrack(
      *GetLayoutObjectByElementId("visible-under-hidden")));

  // Blocks with inline children, some invisible. We don't check descendants for
  // visibility. Just assume there are visible descendants.
  auto* text_block = To<LayoutBlock>(GetLayoutObjectByElementId("text-block"));
  EXPECT_TRUE(tracker.NeedsToTrack(*text_block));
  // No ContainingBlockScope.
  EXPECT_FALSE(tracker.NeedsToTrack(*text_block->FirstChild()));
  {
    LayoutShiftTracker::ContainingBlockScope scope(
        PhysicalSize(1, 2), PhysicalSize(2, 3), PhysicalRect(1, 2, 3, 4),
        PhysicalRect(2, 3, 4, 5));
    EXPECT_TRUE(tracker.NeedsToTrack(*text_block->FirstChild()));
  }
  auto* br = GetLayoutObjectByElementId("br");
  EXPECT_FALSE(tracker.NeedsToTrack(*br));
  EXPECT_TRUE(br->Parent()->IsAnonymous());
  EXPECT_FALSE(tracker.NeedsToTrack(*br->Parent()));

  EXPECT_TRUE(tracker.NeedsToTrack(*GetLayoutObjectByElementId("svg")));
  // We don't track SVG children.
  EXPECT_FALSE(tracker.NeedsToTrack(*GetLayoutObjectByElementId("svg-rect")));

  // Replaced, special blocks, etc.
  EXPECT_TRUE(tracker.NeedsToTrack(*GetLayoutObjectByElementId("video")));
  EXPECT_TRUE(tracker.NeedsToTrack(*GetLayoutObjectByElementId("img")));
  EXPECT_TRUE(tracker.NeedsToTrack(*GetLayoutObjectByElementId("textarea")));
  EXPECT_TRUE(tracker.NeedsToTrack(*GetLayoutObjectByElementId("text-input")));
  EXPECT_TRUE(tracker.NeedsToTrack(*GetLayoutObjectByElementId("file")));
  EXPECT_TRUE(tracker.NeedsToTrack(*GetLayoutObjectByElementId("radio")));
  EXPECT_TRUE(tracker.NeedsToTrack(*GetLayoutObjectByElementId("progress")));
  EXPECT_TRUE(tracker.NeedsToTrack(*GetLayoutObjectByElementId("li")));
  EXPECT_TRUE(tracker.NeedsToTrack(*GetLayoutObjectByElementId("hr")));
}

TEST_F(LayoutShiftTrackerTest, AnimatingTransformCreatesLayoutShiftRoot) {
  SetBodyInnerHTML(R"HTML(
    <style>
      @keyframes move {
        to { translate: 10px; }
      }
      #animation {
        animation: move 10s infinite;
        position: absolute;
        width: 0;
        height: 0;
        top: 0;
      }
      #child {
        position: relative;
        width: 200px;
        height: 200px;
        background: blue;
      }
    </style>
    <div id="animation">
      <div id="child"></div>
    </div>
  )HTML");

  EXPECT_FLOAT_EQ(0, GetLayoutShiftTracker().Score());

  GetElementById("animation")
      ->setAttribute(html_names::kStyleAttr, AtomicString("top: 400px"));
  // `animation` creates a layout shift root, so `child`'s shift doesn't
  // include the shift of `animation`. The 2px shift is below the threshold of
  // reporting a layout shift.
  GetElementById("child")->setAttribute(html_names::kStyleAttr,
                                        AtomicString("top: 2px"));
  UpdateAllLifecyclePhasesForTest();
  EXPECT_FLOAT_EQ(0, GetLayoutShiftTracker().Score());
}

}  // namespace blink
