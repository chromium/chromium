// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <tuple>

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "cc/base/switches.h"
#include "cc/input/scroll_utils.h"
#include "content/browser/renderer_host/input/synthetic_gesture.h"
#include "content/browser/renderer_host/input/synthetic_gesture_controller.h"
#include "content/browser/renderer_host/input/synthetic_gesture_target.h"
#include "content/browser/renderer_host/input/synthetic_smooth_scroll_gesture.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/input/synthetic_gesture_params.h"
#include "content/common/input/synthetic_smooth_scroll_gesture_params.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/hit_test_region_observer.h"
#include "content/shell/browser/shell.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/switches.h"
#include "ui/base/ui_base_features.h"
#include "ui/native_theme/native_theme_features.h"

namespace {

constexpr int kIntermediateScrollOffset = 25;

const std::string kOverflowScrollDataURL = R"HTML(
    data:text/html;charset=utf-8,
    <!DOCTYPE html>
    <meta name='viewport' content='width=device-width, minimum-scale=1'>
    <style>
      %23container {
        width: 200px;
        height: 200px;
        overflow: scroll;
      }
      %23content {
        width: 7500px;
        height: 7500px;
        background-color: blue;
      }
    </style>
    <div id="container">
      <div id="content"></div>
    </div>
    <script>
      var element = document.getElementById('container');
      window.onload = function() {
        document.title='ready';
      }
    </script>
    )HTML";

const std::string kMainFrameScrollDataURL = R"HTML(
    data:text/html;charset=utf-8,
    <!DOCTYPE html>
    <meta name='viewport' content='width=device-width, minimum-scale=1'>
    <style>
      %23scrollableDiv {
        width: 500px;
        height: 10000px;
        background-color: blue;
      }
    </style>
    <div id='scrollableDiv'></div>
    <script>
      window.onload = function() {
        document.title='ready';
      }
    </script>
    )HTML";

const std::string kSubframeScrollDataURL = R"HTML(
    data:text/html;charset=utf-8,
    <!DOCTYPE html>
    <meta name='viewport' content='width=device-width, minimum-scale=1'>
    <style>
      %23subframe {
        width: 200px;
        height: 200px;
      }
    </style>
    <body onload="document.title='ready'">
    <iframe id='subframe' srcdoc="
      <style>
        %23content {
          width: 7500px;
          height: 7500px;
          background-color: blue;
        }
      </style>
      <div id='content'></div>">
    </iframe>
    </body>
    <script>
      var subframe = document.getElementById('subframe');
    </script>
    )HTML";

const std::string kMirroredScrollersDataURL = R"HTML(
    data:text/html;charset=utf-8,<!DOCTYPE html>
    <meta name=viewport content="width=device-width, minimum-scale=1">
    <style>
      body, p { margin: 0 }
      .s { overflow: scroll; border: 1px solid black;
           width: 400px; height: 300px }
      .sp { height: 1200px; width: 900px }
    </style>
    <div id=s1 class=s><p class=sp>SCROLLER</p></div>
    <div id=s2 class=s><p class=sp>MIRROR</p></div>
    <script>
      s1.onscroll = () => { s2.scrollTo(0, s1.scrollTop) }
      onload = () => { document.title = "ready" }
    </script>
    )HTML";

}  // namespace

namespace content {

// This test is to verify that in-progress smooth scrolls stops when
// interrupted by an instant scroll, another smooth scroll, a touch scroll, or
// a mouse wheel scroll on an overflow:scroll element, main frame and subframe.
class ScrollBehaviorBrowserTest : public ContentBrowserTest,
                                  public testing::WithParamInterface<bool> {
 public:
  explicit ScrollBehaviorBrowserTest(
      const absl::optional<bool> enable_percent_based_scrolling = absl::nullopt)
      : disable_threaded_scrolling_(GetParam()) {
    if (enable_percent_based_scrolling.has_value() &&
        *enable_percent_based_scrolling) {
      scoped_feature_list.InitAndEnableFeature(
          features::kWindowsScrollingPersonality);
    } else {
      scoped_feature_list.InitAndDisableFeature(
          features::kWindowsScrollingPersonality);
    }
  }

  ScrollBehaviorBrowserTest(const ScrollBehaviorBrowserTest&) = delete;
  ScrollBehaviorBrowserTest& operator=(const ScrollBehaviorBrowserTest&) =
      delete;

  ~ScrollBehaviorBrowserTest() override = default;

  RenderWidgetHostImpl* GetWidgetHost() {
    return RenderWidgetHostImpl::From(shell()
                                          ->web_contents()
                                          ->GetPrimaryMainFrame()
                                          ->GetRenderViewHost()
                                          ->GetWidget());
  }

  void OnSyntheticGestureCompleted(SyntheticGesture::Result result) {
    EXPECT_EQ(SyntheticGesture::GESTURE_FINISHED, result);
    run_loop_->Quit();
  }

 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    ContentBrowserTest::SetUpCommandLine(command_line);
    if (disable_threaded_scrolling_) {
      command_line->AppendSwitch(blink::switches::kDisableThreadedScrolling);
    }
    // Set the scroll animation duration to 1 second (artificially slow) to make
    // it likely that the second scroll interrupts the first scroll's animation.
    //
    // NOTE: It is likely but NOT guaranteed that interruption will occur. If
    // interruption occurs, tests should verify that the behavior is correct.
    // But if interruption does not occur, tests should be written to pass.
    //
    // We also do not want the animation to be TOO slow - both to be kind to the
    // bots and to avoid false positives from the "value holds" checks.
    //
    command_line->AppendSwitchASCII(
        cc::switches::kCCScrollAnimationDurationForTesting, "1");
  }

  void LoadURL(const std::string page_url) {
    const GURL data_url(page_url);
    EXPECT_TRUE(NavigateToURL(shell(), data_url));

    RenderWidgetHostImpl* host = GetWidgetHost();
    host->GetView()->SetSize(gfx::Size(400, 400));

    std::u16string ready_title(u"ready");
    TitleWatcher watcher(shell()->web_contents(), ready_title);
    std::ignore = watcher.WaitAndGetTitle();

    HitTestRegionObserver observer(host->GetFrameSinkId());
    // Wait for the hit test data to be ready
    observer.WaitForHitTestData();
  }

  gfx::SizeF GetViewportSize() {
    return gfx::SizeF(
        EvalJs(shell(), "window.visualViewport.width").ExtractDouble(),
        EvalJs(shell(), "window.visualViewport.height").ExtractDouble());
  }

  gfx::SizeF GetContainerSize(const std::string& container_expr) {
    return gfx::SizeF(
        EvalJs(shell(), container_expr + ".clientWidth").ExtractDouble(),
        EvalJs(shell(), container_expr + ".clientHeight").ExtractDouble());
  }

  WebContentsImpl* web_contents() const {
    return static_cast<WebContentsImpl*>(shell()->web_contents());
  }

  // The scroll delta values are in the viewport direction. Positive
  // scroll_delta_y means scroll down, positive scroll_delta_x means scroll
  // right.
  void SimulateScroll(content::mojom::GestureSourceType gesture_source_type,
                      int scroll_delta_x,
                      int scroll_delta_y,
                      const std::string& container_expr,
                      bool blocking = true) {
    auto scroll_update_watcher = std::make_unique<InputMsgWatcher>(
        GetWidgetHost(), blink::WebInputEvent::Type::kGestureScrollEnd);

    // This speed affects only the rate at which the requested scroll delta is
    // sent from the synthetic gesture controller, and doesn't affect the speed
    // of the animation in the renderer.
    constexpr int kSpeedInstant = 400000;
    SyntheticSmoothScrollGestureParams params;
    params.gesture_source_type = gesture_source_type;
    params.anchor = gfx::PointF(50, 50);
    params.speed_in_pixels_s = kSpeedInstant;
    if (features::IsPercentBasedScrollingEnabled()) {
      params.distances.push_back(
          cc::ScrollUtils::ResolvePixelScrollToPercentageForTesting(
              gfx::Vector2dF(-scroll_delta_x, -scroll_delta_y),
              GetContainerSize(container_expr), GetViewportSize()));
      params.granularity = ui::ScrollGranularity::kScrollByPercentage;
    } else {
      params.distances.push_back(
          gfx::Vector2d(-scroll_delta_x, -scroll_delta_y));
      params.granularity = ui::ScrollGranularity::kScrollByPixel;
    }
    run_loop_ = std::make_unique<base::RunLoop>();

    auto gesture = std::make_unique<SyntheticSmoothScrollGesture>(params);
    GetWidgetHost()->QueueSyntheticGesture(
        std::move(gesture),
        base::BindOnce(&ScrollBehaviorBrowserTest::OnSyntheticGestureCompleted,
                       base::Unretained(this)));
    if (blocking)
      run_loop_->Run();
  }

  void WaitForScrollToStart(const std::string& script) {
    // When the first smooth scroll starts and scroll to 5 pixels, we will
    // send the second scroll to interrupt the current smooth scroll.
    constexpr int kExpectedScrollTop = 5;
    MainThreadFrameObserver frame_observer(GetWidgetHost());
    while (EvalJs(shell(), script).ExtractDouble() < kExpectedScrollTop) {
      frame_observer.Wait();
    }
  }

  void WaitUntilLessThan(const std::string& script,
                         double starting_scroll_top) {
    // For the scroll interruption, we want to make sure that the first smooth
    // scroll animation stops right away, and the second scroll starts.
    MainThreadFrameObserver frame_observer(GetWidgetHost());
    double current = EvalJs(shell(), script).ExtractDouble();

    // If the animation doesn't reverse within this number of pixels we fail the
    // test.
    constexpr int kThreshold = 20;
    while (current >= starting_scroll_top) {
      ASSERT_LT(current, starting_scroll_top + kThreshold);
      frame_observer.Wait();
      current = EvalJs(shell(), script).ExtractDouble();
    }
  }

  void ValueHoldsAt(const std::string& scroll_top_script, double scroll_top) {
    // This function checks that the scroll top value holds at the given value
    // for 10 frames.
    MainThreadFrameObserver frame_observer(GetWidgetHost());
    int frame_count = 10;
    while (frame_count > 0) {
      ASSERT_EQ(EvalJs(shell(), scroll_top_script).ExtractDouble(), scroll_top);
      frame_observer.Wait();
      frame_count--;
    }
  }

  double WaitForScrollToEnd(const std::string& script) {
    MainThreadFrameObserver frame_observer(GetWidgetHost());
    int frame_count = 0;
    double scroll_top = -1;
    while (true) {
      double new_scroll_top = EvalJs(shell(), script).ExtractDouble();
      if (new_scroll_top == scroll_top) {
        frame_count++;
        // Return when the scroll top value holds steady for 10 frames.
        if (frame_count == 10)
          return scroll_top;
      } else {
        // Scroll top value changed; reset counter.
        frame_count = 0;
        scroll_top = new_scroll_top;
      }
      frame_observer.Wait();
    }
  }

  void RunTestInstantScriptScrollAdjustsSmoothWheelScroll();
  void RunTestSmoothWheelScrollCompletesWithScriptedMirror();

  base::test::ScopedFeatureList scoped_feature_list;
  std::unique_ptr<base::RunLoop> run_loop_;
  bool disable_threaded_scrolling_ = false;
};

class ScrollBehaviorBrowserTestWithPercentBasedScrolling
    : public ScrollBehaviorBrowserTest {
 public:
  ScrollBehaviorBrowserTestWithPercentBasedScrolling()
      : ScrollBehaviorBrowserTest(absl::optional<bool>(true)) {}
};

INSTANTIATE_TEST_SUITE_P(All, ScrollBehaviorBrowserTest, ::testing::Bool());
INSTANTIATE_TEST_SUITE_P(All,
                         ScrollBehaviorBrowserTestWithPercentBasedScrolling,
                         ::testing::Values(true));

// This tests that a in-progress smooth scroll on an overflow:scroll element
// stops when interrupted by an instant scroll.
IN_PROC_BROWSER_TEST_P(ScrollBehaviorBrowserTest,
                       InstantScriptScrollAbortsSmoothScriptScroll) {
  // TODO(crbug.com/1133492): the last animation is committed after we set the
  // scrollTop even when we cancel the animation, so the final scrollTop value
  // is not 0, we need to fix it.
  if (!disable_threaded_scrolling_)
    return;

  LoadURL(kOverflowScrollDataURL);

  EXPECT_TRUE(ExecJs(shell()->web_contents(),
                     "element.scrollTo({top: 100, behavior: 'smooth'});"));

  std::string scroll_top_script = "element.scrollTop";
  WaitForScrollToStart(scroll_top_script);

  double scroll_top = EvalJs(shell(), scroll_top_script).ExtractDouble();
  ASSERT_GT(scroll_top, 0);

  // When interrupted by an instant scroll, the in-progress smooth scrolls stop.
  EXPECT_TRUE(ExecJs(shell()->web_contents(), "element.scrollTop = 0;"));

  // Instant scroll does not cause animation, it scroll to 0 right away.
  ValueHoldsAt(scroll_top_script, 0);
}

IN_PROC_BROWSER_TEST_P(ScrollBehaviorBrowserTestWithPercentBasedScrolling,
                       InstantScriptScrollAdjustsSmoothWheelScroll) {
  RunTestInstantScriptScrollAdjustsSmoothWheelScroll();
}

IN_PROC_BROWSER_TEST_P(ScrollBehaviorBrowserTest,
                       InstantScriptScrollAdjustsSmoothWheelScroll) {
  RunTestInstantScriptScrollAdjustsSmoothWheelScroll();
}

// This tests that a in-progress smooth wheel scroll on a scrollable element is
// adjusted (without cancellation) when interrupted by an instant script scroll.
void ScrollBehaviorBrowserTest::
    RunTestInstantScriptScrollAdjustsSmoothWheelScroll() {
  LoadURL(kOverflowScrollDataURL);
  SimulateScroll(content::mojom::GestureSourceType::kMouseInput, 0, 100,
                 "element",
                 /* blocking */ false);
  WaitForScrollToStart("element.scrollTop");
  EXPECT_TRUE(ExecJs(shell()->web_contents(), "element.scrollBy(0, -5);"));
  EXPECT_NEAR(WaitForScrollToEnd("element.scrollTop"), 95, 1);
}

IN_PROC_BROWSER_TEST_P(ScrollBehaviorBrowserTestWithPercentBasedScrolling,
                       SmoothWheelScrollCompletesWithScriptedMirror) {
  RunTestSmoothWheelScrollCompletesWithScriptedMirror();
}

IN_PROC_BROWSER_TEST_P(ScrollBehaviorBrowserTest,
                       SmoothWheelScrollCompletesWithScriptedMirror) {
  RunTestSmoothWheelScrollCompletesWithScriptedMirror();
}

// This tests that a smooth wheel scroll is not interrupted when script syncs
// a separate scroller in the onscroll handler. (This was the root cause of
// crbug.com/1248388 affecting CodeMirror as described in crbug.com/1264266.)
void ScrollBehaviorBrowserTest::
    RunTestSmoothWheelScrollCompletesWithScriptedMirror() {
  LoadURL(kMirroredScrollersDataURL);
  SimulateScroll(content::mojom::GestureSourceType::kMouseInput, 0, 200, "s1",
                 /* blocking */ false);
  WaitForScrollToStart("s1.scrollTop");
  EXPECT_NEAR(WaitForScrollToEnd("s1.scrollTop"), 200, 1);
  EXPECT_NEAR(WaitForScrollToEnd("s2.scrollTop"), 200, 1);
}

// This tests that a in-progress smooth scroll on an overflow:scroll element
// stops when interrupted by another smooth scroll.
IN_PROC_BROWSER_TEST_P(ScrollBehaviorBrowserTest,
                       OneSmoothScriptScrollAbortsAnother_Element) {
  LoadURL(kOverflowScrollDataURL);

  EXPECT_TRUE(ExecJs(shell()->web_contents(),
                     "element.scrollTo({top: 100, behavior: 'smooth'});"));

  std::string scroll_top_script = "element.scrollTop";
  WaitForScrollToStart(scroll_top_script);

  double scroll_top = EvalJs(shell(), scroll_top_script).ExtractDouble();
  ASSERT_GT(scroll_top, 0);

  // When interrupted by a smooth scroll, the in-progress smooth scrolls stop.
  EXPECT_TRUE(ExecJs(shell()->web_contents(),
                     "element.scrollTo({top: 0, behavior: 'smooth'});"));

  WaitUntilLessThan(scroll_top_script, scroll_top);
  double new_scroll_top = EvalJs(shell(), scroll_top_script).ExtractDouble();
  EXPECT_LT(new_scroll_top, scroll_top);
}

// This tests that a in-progress smooth scroll on an overflow:scroll element
// stops when interrupted by a touch scroll.
// Currently only pre-Scroll-Unification main-thread input-handling gets this
// right (crbug.com/1116647#c5).
IN_PROC_BROWSER_TEST_P(ScrollBehaviorBrowserTest,
                       DISABLED_TouchScrollAbortsSmoothScriptScroll) {
  // TODO(crbug.com/1116647): compositing scroll should be able to cancel a
  // running programmatic scroll.
  if (!disable_threaded_scrolling_)
    return;

  LoadURL(kOverflowScrollDataURL);

  EXPECT_TRUE(ExecJs(shell()->web_contents(),
                     "element.scrollTo({top: 100, behavior: 'smooth'});"));

  std::string scroll_top_script = "element.scrollTop";
  WaitForScrollToStart(scroll_top_script);

  double scroll_top = EvalJs(shell(), scroll_top_script).ExtractDouble();
  ASSERT_GT(scroll_top, 0);
  ASSERT_LT(scroll_top, kIntermediateScrollOffset);

  // When interrupted by a touch scroll, the in-progress smooth scrolls stop.
  SimulateScroll(content::mojom::GestureSourceType::kTouchInput, 0, -100,
                 "element");

  // The touch scroll should cause scroll to 0 and cancel the animation, so
  // make sure the value stays at 0.
  ValueHoldsAt(scroll_top_script, 0);
}

// This tests that a in-progress smooth scroll on an overflow:scroll element
// stops when interrupted by a mouse wheel scroll.
// Flaky, mainly on Mac, but also on other slower builders/testers:
// https://crbug.com/1175392
IN_PROC_BROWSER_TEST_P(ScrollBehaviorBrowserTest,
                       DISABLED_WheelScrollAbortsSmoothScriptScroll) {
  // TODO(crbug.com/1116647): compositing scroll should be able to cancel a
  // running programmatic scroll.
  if (!disable_threaded_scrolling_)
    return;

  LoadURL(kOverflowScrollDataURL);

  EXPECT_TRUE(ExecJs(shell()->web_contents(),
                     "element.scrollTo({top: 100, behavior: 'smooth'});"));

  std::string scroll_top_script = "element.scrollTop";
  WaitForScrollToStart(scroll_top_script);

  double scroll_top = EvalJs(shell(), scroll_top_script).ExtractDouble();
  ASSERT_GT(scroll_top, 0);
  ASSERT_LT(scroll_top, kIntermediateScrollOffset);

  // When interrupted by a wheel scroll, the in-progress smooth scrolls stop.
  SimulateScroll(content::mojom::GestureSourceType::kMouseInput, 0, -30,
                 "element");

  // Smooth scrolling is disabled for wheel scroll on Mac.
  // https://crbug.com/574283.
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_ANDROID)
  ValueHoldsAt(scroll_top_script, 0);
#else
  WaitUntilLessThan(scroll_top_script, scroll_top);
  double new_scroll_top = EvalJs(shell(), scroll_top_script).ExtractDouble();
  EXPECT_LT(new_scroll_top, scroll_top);
  EXPECT_GT(new_scroll_top, 0);
#endif
}

// This tests that a in-progress smooth scroll on the main frame stops when
// interrupted by another smooth scroll.
IN_PROC_BROWSER_TEST_P(ScrollBehaviorBrowserTest,
                       OneSmoothScriptScrollAbortsAnother_Document) {
  LoadURL(kMainFrameScrollDataURL);

  EXPECT_TRUE(ExecJs(shell()->web_contents(),
                     "window.scrollTo({top: 100, behavior: 'smooth'});"));

  std::string scroll_top_script = "document.scrollingElement.scrollTop";
  WaitForScrollToStart(scroll_top_script);

  double scroll_top = EvalJs(shell(), scroll_top_script).ExtractDouble();
  ASSERT_GT(scroll_top, 0);

  // When interrupted by a smooth scroll, the in-progress smooth scrolls stop.
  EXPECT_TRUE(ExecJs(shell()->web_contents(),
                     "window.scrollTo({top: 0, behavior: 'smooth'});"));

  WaitUntilLessThan(scroll_top_script, scroll_top);
  double new_scroll_top = EvalJs(shell(), scroll_top_script).ExtractDouble();
  EXPECT_LT(new_scroll_top, scroll_top);
}

// This tests that a in-progress smooth scroll on a subframe stops when
// interrupted by another smooth scroll.
// Flaky on multiple platforms: crbug.com/1306980
IN_PROC_BROWSER_TEST_P(ScrollBehaviorBrowserTest,
                       DISABLED_OneSmoothScriptScrollAbortsAnother_Subframe) {
  LoadURL(kSubframeScrollDataURL);

  EXPECT_TRUE(ExecJs(
      shell()->web_contents(),
      "subframe.contentWindow.scrollTo({top: 100, behavior: 'smooth'});"));

  std::string scroll_top_script =
      "subframe.contentDocument.scrollingElement.scrollTop";
  WaitForScrollToStart(scroll_top_script);

  double scroll_top = EvalJs(shell(), scroll_top_script).ExtractDouble();
  ASSERT_GT(scroll_top, 0);

  // When interrupted by a smooth scroll, the in-progress smooth scrolls stop.
  EXPECT_TRUE(
      ExecJs(shell()->web_contents(),
             "subframe.contentWindow.scrollTo({top: 0, behavior: 'smooth'});"));

  WaitUntilLessThan(scroll_top_script, scroll_top);
  double new_scroll_top = EvalJs(shell(), scroll_top_script).ExtractDouble();
  EXPECT_LT(new_scroll_top, scroll_top);
}

}  // namespace content
