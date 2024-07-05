// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/scroll/scroll_animator.h"

#include "base/test/bind.h"
#include "cc/base/features.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/web/web_script_source.h"
#include "third_party/blink/renderer/core/css/css_style_declaration.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/root_frame_viewport.h"
#include "third_party/blink/renderer/core/frame/visual_viewport.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/geometry/dom_rect.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/paint/paint_and_raster_invalidation_test.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/scroll/scroll_animator_base.h"
#include "third_party/blink/renderer/core/scroll/scrollable_area.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"
#include "third_party/blink/renderer/platform/testing/paint_test_configurations.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"

namespace blink {

namespace {
double ScrollAnimationDuration() {
  return ::features::IsImpulseScrollAnimationEnabled() ? 1.5 : 0.5;
}
}  // namespace

class FractionalScrollSimTest : public SimTest, public PaintTestConfigurations {
 public:
  FractionalScrollSimTest() : fractional_scroll_offsets_for_test_(true) {}

 private:
  ScopedFractionalScrollOffsetsForTest fractional_scroll_offsets_for_test_;
};

INSTANTIATE_PAINT_TEST_SUITE_P(FractionalScrollSimTest);

TEST_P(FractionalScrollSimTest, GetBoundingClientRectAtFractional) {
  WebView().MainFrameViewWidget()->Resize(gfx::Size(800, 600));
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
      body, html {
        margin: 0;
        height: 2000px;
        width: 2000px;
      }
      div {
        position: absolute;
        left: 800px;
        top: 600px;
        width: 100px;
        height: 100px;
      }
    </style>
    <body>
      <div id="target"></div>
    </body>
  )HTML");
  Compositor().BeginFrame();

  // Scroll on the layout viewport.
  GetDocument().View()->GetScrollableArea()->SetScrollOffset(
      ScrollOffset(700.5f, 500.6f), mojom::blink::ScrollType::kProgrammatic,
      mojom::blink::ScrollBehavior::kInstant);

  Compositor().BeginFrame();

  Element* target = GetDocument().getElementById(AtomicString("target"));
  DOMRect* rect = target->GetBoundingClientRect();
  const float kOneLayoutUnit = 1.f / LayoutUnit::kFixedPointDenominator;
  EXPECT_NEAR(LayoutUnit(800.f - 700.5f), rect->left(), kOneLayoutUnit);
  EXPECT_NEAR(LayoutUnit(600.f - 500.6f), rect->top(), kOneLayoutUnit);
}

TEST_P(FractionalScrollSimTest, NoRepaintOnScrollFromSubpixel) {
  WebView().MainFrameViewWidget()->Resize(gfx::Size(800, 600));
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
      body {
        height: 4000px;
      }

      #container {
        will-change:transform;
        margin-top: 300px;
      }

      #child {
        height: 100px;
        width: 100px;
        transform: translateY(-0.5px);
        background-color: coral;
      }

      #fixed {
        position: fixed;
        top: 0;
        width: 100px;
        height: 20px;
        background-color: dodgerblue
      }
    </style>

    <!-- This test no longer tests the code path when the test was added
         because the fixed element no longer triggers compositing update on
         scroll. We still keep this test to ensure no paint invalidation. -->
    <div id="fixed"></div>
    <div id="container">
        <div id="child">Child</div>
    </div>
  )HTML");
  Compositor().BeginFrame();

  GetDocument().View()->SetTracksRasterInvalidations(true);

  // Scroll on the layout viewport.
  GetDocument().View()->GetScrollableArea()->SetScrollOffset(
      ScrollOffset(0.f, 100.5f), mojom::blink::ScrollType::kProgrammatic,
      mojom::blink::ScrollBehavior::kInstant);

  Compositor().BeginFrame();
  EXPECT_FALSE(
      GetRasterInvalidationTracking(*GetDocument().View(), 0, "container")
          ->HasInvalidations());
  GetDocument().View()->SetTracksRasterInvalidations(false);
}

// Verifies that the sticky constraints are correctly computed when the scroll
// offset is fractional. Ensures any kind of layout unit snapping is
// consistent.
TEST_P(FractionalScrollSimTest, StickyDoesntOscillate) {
  WebView().MainFrameWidget()->Resize(gfx::Size(800, 600));
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
      #sticky {
        position: sticky; top: 0; width: 100px; height: 100px;
      }
      body {
        margin: 0;
        height: 300vh;
      }
      #padding {
        height: 8px;
        width: 100%;
      }
    </style>
    <div id='padding'></div>
    <div id='sticky'></div>
  )HTML");
  Compositor().BeginFrame();

  const float kOneLayoutUnitF = LayoutUnit::Epsilon();
  Element* sticky = GetDocument().getElementById(AtomicString("sticky"));

  // Try sub-layout-unit scroll offsets. The sticky box shouldn't move.
  for (int i = 0; i < 3; ++i) {
    GetDocument().View()->GetScrollableArea()->ScrollBy(
        ScrollOffset(0.f, kOneLayoutUnitF / 4.f),
        mojom::blink::ScrollType::kProgrammatic);
    Compositor().BeginFrame();
    EXPECT_EQ(8, sticky->GetBoundingClientRect()->top());
  }

  // This offset is specifically chosen since it doesn't land on a LayoutUnit
  // boundary and reproduced https://crbug.com/1010961.
  GetDocument().View()->GetScrollableArea()->SetScrollOffset(
      ScrollOffset(0.f, 98.8675308f), mojom::blink::ScrollType::kProgrammatic,
      mojom::blink::ScrollBehavior::kInstant);
  Compositor().BeginFrame();
  EXPECT_EQ(0, sticky->GetBoundingClientRect()->top());

  // Incrementally scroll from here, making sure the sticky position remains
  // fixed.
  for (int i = 0; i < 4; ++i) {
    GetDocument().View()->GetScrollableArea()->ScrollBy(
        ScrollOffset(0.f, kOneLayoutUnitF / 3.f),
        mojom::blink::ScrollType::kProgrammatic);
    Compositor().BeginFrame();
    EXPECT_EQ(0, sticky->GetBoundingClientRect()->top());
  }
}

class ScrollAnimatorSimTest : public SimTest, public PaintTestConfigurations {};

INSTANTIATE_PAINT_TEST_SUITE_P(ScrollAnimatorSimTest);

// Test that the callback of user scroll will be executed when the animation
// finishes at ScrollAnimator::TickAnimation for root frame user scroll at the
// layout viewport.
TEST_P(ScrollAnimatorSimTest, TestRootFrameLayoutViewportUserScrollCallBack) {
  GetDocument().GetFrame()->GetSettings()->SetScrollAnimatorEnabled(true);
  WebView().MainFrameViewWidget()->Resize(gfx::Size(800, 500));
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
      body, html {
        margin: 0;
        height: 500vh;
      }
    </style>
    <body>
    </body>
  )HTML");
  Compositor().BeginFrame();

  WebView().MainFrameWidget()->SetFocus(true);
  WebView().SetIsActive(true);

  // Scroll on the layout viewport.
  bool finished = false;
  GetDocument().View()->GetScrollableArea()->UserScroll(
      ui::ScrollGranularity::kScrollByLine, ScrollOffset(100, 300),
      ScrollableArea::ScrollCallback(base::BindLambdaForTesting(
          [&](ScrollableArea::ScrollCompletionMode) { finished = true; })));
  // Sync time with ScrollAnimator.
  Compositor().ResetLastFrameTime();

  Compositor().BeginFrame();
  ASSERT_FALSE(finished);

  // The callback is executed when the animation finishes at
  // ScrollAnimator::TickAnimation.
  Compositor().BeginFrame();
  Compositor().BeginFrame(ScrollAnimationDuration());
  ASSERT_TRUE(finished);
}

// Test that the callback of user scroll will be executed when the animation
// finishes at ScrollAnimator::TickAnimation for root frame user scroll at the
// visual viewport.
TEST_P(ScrollAnimatorSimTest, TestRootFrameVisualViewporUserScrollCallBack) {
  GetDocument().GetFrame()->GetSettings()->SetScrollAnimatorEnabled(true);
  WebView().MainFrameViewWidget()->Resize(gfx::Size(800, 500));
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
      body, html {
        margin: 0;
        height: 500vh;
      }
    </style>
    <body>
    </body>
  )HTML");
  Compositor().BeginFrame();

  WebView().MainFrameWidget()->SetFocus(true);
  WebView().SetIsActive(true);
  WebView().SetPageScaleFactor(2);

  // Scroll on the visual viewport.
  bool finished = false;
  GetDocument().View()->GetScrollableArea()->UserScroll(
      ui::ScrollGranularity::kScrollByLine, ScrollOffset(100, 300),
      ScrollableArea::ScrollCallback(base::BindLambdaForTesting(
          [&](ScrollableArea::ScrollCompletionMode) { finished = true; })));
  // Sync time with ScrollAnimator.
  Compositor().ResetLastFrameTime();

  Compositor().BeginFrame();
  ASSERT_FALSE(finished);

  // The callback is executed when the animation finishes at
  // ScrollAnimator::TickAnimation.
  Compositor().BeginFrame();
  Compositor().BeginFrame(ScrollAnimationDuration());
  ASSERT_TRUE(finished);
}

// Test that the callback of user scroll will be executed when the animation
// finishes at ScrollAnimator::TickAnimation for root frame user scroll at both
// the layout and visual viewport.
TEST_P(ScrollAnimatorSimTest, TestRootFrameBothViewportsUserScrollCallBack) {
  GetDocument().GetFrame()->GetSettings()->SetScrollAnimatorEnabled(true);
  WebView().MainFrameViewWidget()->Resize(gfx::Size(800, 500));
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
      body, html {
        margin: 0;
        height: 500vh;
      }
    </style>
    <body>
    </body>
  )HTML");
  Compositor().BeginFrame();

  WebView().MainFrameWidget()->SetFocus(true);
  WebView().SetIsActive(true);
  WebView().SetPageScaleFactor(2);

  // Scroll on both the layout and visual viewports.
  bool finished = false;
  GetDocument().View()->GetScrollableArea()->UserScroll(
      ui::ScrollGranularity::kScrollByLine, ScrollOffset(0, 1000),
      ScrollableArea::ScrollCallback(base::BindLambdaForTesting(
          [&](ScrollableArea::ScrollCompletionMode) { finished = true; })));
  // Sync time with ScrollAnimator.
  Compositor().ResetLastFrameTime();

  Compositor().BeginFrame();
  ASSERT_FALSE(finished);

  // The callback is executed when the animation finishes at
  // ScrollAnimator::TickAnimation.
  Compositor().BeginFrame();
  Compositor().BeginFrame(ScrollAnimationDuration());
  ASSERT_TRUE(finished);
}

// Test that the callback of user scroll will be executed when the animation
// finishes at ScrollAnimator::TickAnimation for div user scroll.
TEST_P(ScrollAnimatorSimTest, TestDivUserScrollCallBack) {
  GetDocument().GetSettings()->SetScrollAnimatorEnabled(true);
  WebView().MainFrameViewWidget()->Resize(gfx::Size(800, 500));
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
      #scroller {
        width: 100px;
        height: 100px;
        overflow: auto;
      }
      #overflow {
        height: 500px;
        width: 500px;
      }
    </style>
    <div id="scroller">
      <div id="overflow"></div>
    </div>
  )HTML");
  Compositor().BeginFrame();

  WebView().MainFrameWidget()->SetFocus(true);
  WebView().SetIsActive(true);

  Element* scroller = GetDocument().getElementById(AtomicString("scroller"));

  bool finished = false;
  PaintLayerScrollableArea* scrollable_area =
      To<LayoutBox>(scroller->GetLayoutObject())->GetScrollableArea();
  scrollable_area->UserScroll(
      ui::ScrollGranularity::kScrollByLine, ScrollOffset(0, 100),
      ScrollableArea::ScrollCallback(base::BindLambdaForTesting(
          [&](ScrollableArea::ScrollCompletionMode) { finished = true; })));
  // Sync time with ScrollAnimator.
  Compositor().ResetLastFrameTime();

  Compositor().BeginFrame();
  ASSERT_FALSE(finished);

  // The callback is executed when the animation finishes at
  // ScrollAnimator::TickAnimation.
  Compositor().BeginFrame(ScrollAnimationDuration());
  ASSERT_TRUE(finished);
}

// Test that the callback of user scroll will be executed in
// ScrollAnimatorBase::UserScroll when animation is disabled.
TEST_P(ScrollAnimatorSimTest, TestUserScrollCallBackAnimatorDisabled) {
  GetDocument().GetFrame()->GetSettings()->SetScrollAnimatorEnabled(false);
  WebView().MainFrameViewWidget()->Resize(gfx::Size(800, 500));
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
      body, html {
        margin: 0;
        height: 500vh;
      }
    </style>
    <body>
    </body>
  )HTML");
  Compositor().BeginFrame();

  WebView().MainFrameWidget()->SetFocus(true);
  WebView().SetIsActive(true);

  bool finished = false;
  GetDocument().View()->GetScrollableArea()->UserScroll(
      ui::ScrollGranularity::kScrollByLine, ScrollOffset(0, 300),
      ScrollableArea::ScrollCallback(base::BindLambdaForTesting(
          [&](ScrollableArea::ScrollCompletionMode) { finished = true; })));
  // Sync time with ScrollAnimator.
  Compositor().ResetLastFrameTime();

  Compositor().BeginFrame();
  ASSERT_TRUE(finished);
}

// Test that the callback of user scroll will be executed when the animation is
// canceled because performing a programmatic scroll in the middle of a user
// scroll will cancel the animation.
TEST_P(ScrollAnimatorSimTest, TestRootFrameUserScrollCallBackCancelAnimation) {
  GetDocument().GetFrame()->GetSettings()->SetScrollAnimatorEnabled(true);
  WebView().MainFrameViewWidget()->Resize(gfx::Size(800, 500));
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
      body, html {
        margin: 0;
        height: 500vh;
      }
    </style>
    <body>
    </body>
  )HTML");
  Compositor().BeginFrame();

  WebView().MainFrameWidget()->SetFocus(true);
  WebView().SetIsActive(true);

  // Scroll on the layout viewport.
  bool finished = false;
  GetDocument().View()->GetScrollableArea()->UserScroll(
      ui::ScrollGranularity::kScrollByLine, ScrollOffset(100, 300),
      ScrollableArea::ScrollCallback(base::BindLambdaForTesting(
          [&](ScrollableArea::ScrollCompletionMode) { finished = true; })));
  // Sync time with ScrollAnimator.
  Compositor().ResetLastFrameTime();

  Compositor().BeginFrame();
  ASSERT_FALSE(finished);

  // Programmatic scroll will cancel the current user scroll animation and the
  // callback will be executed.
  GetDocument().View()->GetScrollableArea()->SetScrollOffset(
      ScrollOffset(0, 300), mojom::blink::ScrollType::kProgrammatic,
      mojom::blink::ScrollBehavior::kSmooth, ScrollableArea::ScrollCallback());
  Compositor().BeginFrame();
  ASSERT_TRUE(finished);
}

class ScrollInfacesUseCounterSimTest : public SimTest,
                                       public PaintTestConfigurations {
 public:
  // Reload the page, set direction and writing-mode, then check the initial
  // useCounted status.
  void Reset(const String& direction, const String& writing_mode) {
    SimRequest request("https://example.com/test.html", "text/html");
    LoadURL("https://example.com/test.html");
    request.Complete(R"HTML(
            <!DOCTYPE html>
            <style>
              #scroller {
                width: 100px;
                height: 100px;
                overflow: scroll;
              }
              #content {
                width: 300;
                height: 300;
              }
            </style>
            <div id="scroller"><div id="content"></div></div>
        )HTML");
    auto& document = GetDocument();
    auto* style = document.getElementById(AtomicString("scroller"))->style();
    style->setProperty(&Window(), "direction", direction, String(),
                       ASSERT_NO_EXCEPTION);
    style->setProperty(&Window(), "writing-mode", writing_mode, String(),
                       ASSERT_NO_EXCEPTION);
    Compositor().BeginFrame();
    EXPECT_FALSE(document.IsUseCounted(
        WebFeature::
            kElementWithLeftwardOrUpwardOverflowDirection_ScrollLeftOrTop));
    EXPECT_FALSE(document.IsUseCounted(
        WebFeature::
            kElementWithLeftwardOrUpwardOverflowDirection_ScrollLeftOrTopSetPositive));
  }

  // Check if Element.scrollLeft/Top could trigger useCounter as expected.
  void CheckScrollLeftOrTop(const String& command, bool exppected_use_counted) {
    String scroll_command =
        "document.querySelector('#scroller')." + command + ";";
    MainFrame().ExecuteScriptAndReturnValue(WebScriptSource(scroll_command));
    auto& document = GetDocument();
    EXPECT_EQ(
        exppected_use_counted,
        document.IsUseCounted(
            WebFeature::
                kElementWithLeftwardOrUpwardOverflowDirection_ScrollLeftOrTop));
    EXPECT_FALSE(document.IsUseCounted(
        WebFeature::
            kElementWithLeftwardOrUpwardOverflowDirection_ScrollLeftOrTopSetPositive));
  }

  // Check if Element.setScrollLeft/Top could trigger useCounter as expected.
  void CheckSetScrollLeftOrTop(const String& command,
                               bool exppected_use_counted) {
    String scroll_command =
        "document.querySelector('#scroller')." + command + " = -1;";
    MainFrame().ExecuteScriptAndReturnValue(WebScriptSource(scroll_command));
    auto& document = GetDocument();
    EXPECT_EQ(
        exppected_use_counted,
        document.IsUseCounted(
            WebFeature::
                kElementWithLeftwardOrUpwardOverflowDirection_ScrollLeftOrTop));
    EXPECT_FALSE(document.IsUseCounted(
        WebFeature::
            kElementWithLeftwardOrUpwardOverflowDirection_ScrollLeftOrTopSetPositive));
    scroll_command = "document.querySelector('#scroller')." + command + " = 1;";
    MainFrame().ExecuteScriptAndReturnValue(WebScriptSource(scroll_command));
    EXPECT_EQ(
        exppected_use_counted,
        document.IsUseCounted(
            WebFeature::
                kElementWithLeftwardOrUpwardOverflowDirection_ScrollLeftOrTopSetPositive));
  }

  // Check if Element.scrollTo/scroll could trigger useCounter as expected.
  void CheckScrollTo(const String& command, bool exppected_use_counted) {
    String scroll_command =
        "document.querySelector('#scroller')." + command + "(-1, -1);";
    MainFrame().ExecuteScriptAndReturnValue(WebScriptSource(scroll_command));
    auto& document = GetDocument();
    EXPECT_EQ(
        exppected_use_counted,
        document.IsUseCounted(
            WebFeature::
                kElementWithLeftwardOrUpwardOverflowDirection_ScrollLeftOrTop));
    EXPECT_FALSE(document.IsUseCounted(
        WebFeature::
            kElementWithLeftwardOrUpwardOverflowDirection_ScrollLeftOrTopSetPositive));
    scroll_command =
        "document.querySelector('#scroller')." + command + "(1, 1);";
    MainFrame().ExecuteScriptAndReturnValue(WebScriptSource(scroll_command));
    EXPECT_EQ(
        exppected_use_counted,
        document.IsUseCounted(
            WebFeature::
                kElementWithLeftwardOrUpwardOverflowDirection_ScrollLeftOrTopSetPositive));
  }
};

INSTANTIATE_PAINT_TEST_SUITE_P(ScrollInfacesUseCounterSimTest);

struct TestCase {
  String direction;
  String writingMode;
  bool scrollLeftUseCounted;
  bool scrollTopUseCounted;
};

TEST_P(ScrollInfacesUseCounterSimTest, ScrollTestAll) {
  v8::HandleScope handle_scope(
      WebView().GetPage()->GetAgentGroupScheduler().Isolate());
  WebView().MainFrameViewWidget()->Resize(gfx::Size(800, 600));
  const Vector<TestCase> test_cases = {
      {"ltr", "horizontal-tb", false, false},
      {"rtl", "horizontal-tb", true, false},
      {"ltr", "vertical-lr", false, false},
      {"rtl", "vertical-lr", false, true},
      {"ltr", "vertical-rl", true, false},
      {"rtl", "vertical-rl", true, true},
  };

  for (const TestCase& test_case : test_cases) {
    Reset(test_case.direction, test_case.writingMode);
    CheckScrollLeftOrTop("scrollLeft", test_case.scrollLeftUseCounted);

    Reset(test_case.direction, test_case.writingMode);
    CheckSetScrollLeftOrTop("scrollLeft", test_case.scrollLeftUseCounted);

    Reset(test_case.direction, test_case.writingMode);
    CheckScrollLeftOrTop("scrollTop", test_case.scrollTopUseCounted);

    Reset(test_case.direction, test_case.writingMode);
    CheckSetScrollLeftOrTop("scrollTop", test_case.scrollTopUseCounted);

    bool expectedScrollUseCounted =
        test_case.scrollLeftUseCounted || test_case.scrollTopUseCounted;
    Reset(test_case.direction, test_case.writingMode);
    CheckScrollTo("scrollTo", expectedScrollUseCounted);

    Reset(test_case.direction, test_case.writingMode);
    CheckScrollTo("scroll", expectedScrollUseCounted);

    Reset(test_case.direction, test_case.writingMode);
    CheckScrollTo("scrollBy", false);
  }
}

class ScrollPositionsInNonDefaultWritingModeSimTest
    : public SimTest,
      public PaintTestConfigurations {};

INSTANTIATE_PAINT_TEST_SUITE_P(ScrollPositionsInNonDefaultWritingModeSimTest);

// Verify that scrollIntoView() does not trigger the use counter
// kElementWithLeftwardOrUpwardOverflowDirection_ScrollLeftOrTopSetPositive
// and can be used to feature detect the convention of scroll coordinates.
TEST_P(ScrollPositionsInNonDefaultWritingModeSimTest,
       ScrollIntoViewAndCounters) {
  SimRequest main_resource("https://example.com/", "text/html");
  SimRequest child_frame_resource("https://example.com/subframe.html",
                                  "text/html");
  LoadURL("https://example.com/");
  // Load a page that performs feature detection of scroll behavior by relying
  // on scrollIntoView().
  main_resource.Complete(
      R"HTML(
        <body>
             <div style="direction: rtl; position: fixed; left: 0; top: 0; overflow: hidden; width: 1px; height: 1px;"><div style="width: 2px; height: 1px;"><div style="display: inline-block; width: 1px;"></div><div style="display: inline-block; width: 1px;"></div></div></div>
             <script>
               var scroller = document.body.firstElementChild;
               scroller.firstElementChild.children[0].scrollIntoView();
               var right = scroller.scrollLeft;
               scroller.firstElementChild.children[1].scrollIntoView();
               var left = scroller.scrollLeft;
               if (left < right)
                   console.log("decreasing");
               if (left < 0)
                   console.log("nonpositive");
             </script>
        </body>)HTML");
  Compositor().BeginFrame();
  test::RunPendingTasks();
  // Per the CSSOM specification, the standard behavior is:
  // - decreasing coordinates when scrolling leftward.
  // - nonpositive coordinates for leftward scroller.
  EXPECT_TRUE(ConsoleMessages().Contains("decreasing"));
  EXPECT_TRUE(ConsoleMessages().Contains("nonpositive"));
  // Reading scrollLeft triggers the first counter:
  EXPECT_TRUE(GetDocument().IsUseCounted(
      WebFeature::
          kElementWithLeftwardOrUpwardOverflowDirection_ScrollLeftOrTop));
  // However, calling scrollIntoView() should not trigger the second counter:
  EXPECT_FALSE(GetDocument().IsUseCounted(
      WebFeature::
          kElementWithLeftwardOrUpwardOverflowDirection_ScrollLeftOrTopSetPositive));
}

}  // namespace blink
