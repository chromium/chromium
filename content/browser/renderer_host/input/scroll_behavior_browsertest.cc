// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "cc/base/switches.h"
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

}  // namespace

namespace content {

// This test is to verify that in-progress smooth scrolls stops when
// interrupted by an instant scroll, another smooth scroll, a touch scroll, or
// a mouse wheel scroll on an overflow:scroll element, main frame and subframe.
class ScrollBehaviorBrowserTest : public ContentBrowserTest,
                                  public testing::WithParamInterface<bool> {
 public:
  ScrollBehaviorBrowserTest() : disable_threaded_scrolling_(GetParam()) {}

  ~ScrollBehaviorBrowserTest() override = default;

  RenderWidgetHostImpl* GetWidgetHost() {
    return RenderWidgetHostImpl::From(
        shell()->web_contents()->GetRenderViewHost()->GetWidget());
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
    // Set the scroll animation duration to 5 seconds so that we ensure
    // the second scroll happens before the scroll animation finishes.
    command_line->AppendSwitchASCII(
        cc::switches::kCCScrollAnimationDurationForTesting, "5");
  }

  void LoadURL(const std::string page_url) {
    const GURL data_url(page_url);
    EXPECT_TRUE(NavigateToURL(shell(), data_url));

    RenderWidgetHostImpl* host = GetWidgetHost();
    host->GetView()->SetSize(gfx::Size(400, 400));

    base::string16 ready_title(base::ASCIIToUTF16("ready"));
    TitleWatcher watcher(shell()->web_contents(), ready_title);
    ignore_result(watcher.WaitAndGetTitle());

    HitTestRegionObserver observer(host->GetFrameSinkId());
    // Wait for the hit test data to be ready
    observer.WaitForHitTestData();
  }

  double ExecuteScriptAndExtractDouble(const std::string& script) {
    double value = 0;
    EXPECT_TRUE(content::ExecuteScriptAndExtractDouble(
        shell(), "domAutomationController.send(" + script + ")", &value));
    return value;
  }

  WebContentsImpl* web_contents() const {
    return static_cast<WebContentsImpl*>(shell()->web_contents());
  }

  // The scroll delta values are in the viewport direction. Positive
  // scroll_delta_y means scroll down, positive scroll_delta_x means scroll
  // right.
  void SimulateScroll(
      SyntheticGestureParams::GestureSourceType gesture_source_type,
      int scroll_delta_x,
      int scroll_delta_y) {
    auto scroll_update_watcher = std::make_unique<InputMsgWatcher>(
        GetWidgetHost(), blink::WebInputEvent::Type::kGestureScrollEnd);

    // This speed affects only the rate at which the requested scroll delta is
    // sent from the synthetic gesture controller, and doesn't affect the speed
    // of the animation in the renderer.
    constexpr int kSpeedInstant = 400000;
    SyntheticSmoothScrollGestureParams params;
    params.gesture_source_type = gesture_source_type;
    params.anchor = gfx::PointF(50, 50);
    params.distances.push_back(gfx::Vector2d(-scroll_delta_x, -scroll_delta_y));
    params.speed_in_pixels_s = kSpeedInstant;
    params.granularity = ui::ScrollGranularity::kScrollByPixel;

    run_loop_ = std::make_unique<base::RunLoop>();

    auto gesture = std::make_unique<SyntheticSmoothScrollGesture>(params);
    GetWidgetHost()->QueueSyntheticGesture(
        std::move(gesture),
        base::BindOnce(&ScrollBehaviorBrowserTest::OnSyntheticGestureCompleted,
                       base::Unretained(this)));
    run_loop_->Run();
  }

  void WaitForScrollToStart(const std::string& script) {
    // When the first smooth scroll starts and scroll to 5 pixels, we will
    // send the second scroll to interrupt the current smooth scroll.
    constexpr int kExpectedScrollTop = 5;
    MainThreadFrameObserver frame_observer(
        shell()->web_contents()->GetRenderViewHost()->GetWidget());
    while (ExecuteScriptAndExtractDouble(script) < kExpectedScrollTop)
      frame_observer.Wait();
  }

  void WaitUntilLessThan(const std::string& script,
                         double starting_scroll_top) {
    // For the scroll interruption, we want to make sure that the first smooth
    // scroll animation stops right away, and the second scroll starts.
    MainThreadFrameObserver frame_observer(
        shell()->web_contents()->GetRenderViewHost()->GetWidget());
    double current = ExecuteScriptAndExtractDouble(script);

    // If the animation doesn't reverse within this number of pixels we fail the
    // test.
    constexpr int kThreshold = 20;
    while (current >= starting_scroll_top) {
      ASSERT_LT(current, starting_scroll_top + kThreshold);
      frame_observer.Wait();
      current = ExecuteScriptAndExtractDouble(script);
    }
  }

  void ValueHoldsAt(const std::string& scroll_top_script, double scroll_top) {
    // This function checks that the scroll top value holds at the given value
    // for 10 frames.
    MainThreadFrameObserver frame_observer(
        shell()->web_contents()->GetRenderViewHost()->GetWidget());
    int frame_count = 5;
    while (frame_count > 0) {
      ASSERT_EQ(ExecuteScriptAndExtractDouble(scroll_top_script), scroll_top);
      frame_observer.Wait();
      frame_count--;
    }
  }

  RenderViewHost* GetRenderViewHost() const {
    RenderViewHost* const rvh = shell()->web_contents()->GetRenderViewHost();
    CHECK(rvh);
    return rvh;
  }

  std::unique_ptr<base::RunLoop> run_loop_;
  bool disable_threaded_scrolling_ = false;

  DISALLOW_COPY_AND_ASSIGN(ScrollBehaviorBrowserTest);
};

INSTANTIATE_TEST_SUITE_P(All, ScrollBehaviorBrowserTest, ::testing::Bool());

// This tests that a in-progress smooth scroll on an overflow:scroll element
// stops when interrupted by an instant scroll.
IN_PROC_BROWSER_TEST_P(ScrollBehaviorBrowserTest,
                       OverflowScrollInterruptedByInstantScroll) {
  // TODO(crbug.com/1133492): the last animation is committed after we set the
  // scrollTop even when we cancel the animation, so the final scrollTop value
  // is not 0, we need to fix it.
  if (!disable_threaded_scrolling_)
    return;

  LoadURL(kOverflowScrollDataURL);

  EXPECT_TRUE(
      ExecuteScript(shell()->web_contents(),
                    "element.scrollTo({top: 100, behavior: 'smooth'});"));

  std::string scroll_top_script = "element.scrollTop";
  WaitForScrollToStart(scroll_top_script);

  double scroll_top = ExecuteScriptAndExtractDouble(scroll_top_script);
  ASSERT_GT(scroll_top, 0);
  ASSERT_LT(scroll_top, kIntermediateScrollOffset);

  // When interrupted by an instant scroll, the in-progress smooth scrolls stop.
  EXPECT_TRUE(ExecuteScript(shell()->web_contents(), "element.scrollTop = 0;"));

  // Instant scroll does not cause animation, it scroll to 0 right away.
  ValueHoldsAt(scroll_top_script, 0);
}

// This tests that a in-progress smooth scroll on an overflow:scroll element
// stops when interrupted by another smooth scroll.
IN_PROC_BROWSER_TEST_P(ScrollBehaviorBrowserTest,
                       OverflowScrollInterruptedBySmoothScroll) {
  LoadURL(kOverflowScrollDataURL);

  EXPECT_TRUE(
      ExecuteScript(shell()->web_contents(),
                    "element.scrollTo({top: 100, behavior: 'smooth'});"));

  std::string scroll_top_script = "element.scrollTop";
  WaitForScrollToStart(scroll_top_script);

  double scroll_top = ExecuteScriptAndExtractDouble(scroll_top_script);
  ASSERT_GT(scroll_top, 0);
  ASSERT_LT(scroll_top, kIntermediateScrollOffset);

  // When interrupted by a smooth scroll, the in-progress smooth scrolls stop.
  EXPECT_TRUE(ExecuteScript(shell()->web_contents(),
                            "element.scrollTo({top: 0, behavior: 'smooth'});"));

  WaitUntilLessThan(scroll_top_script, scroll_top);
  double new_scroll_top = ExecuteScriptAndExtractDouble(scroll_top_script);
  EXPECT_LT(new_scroll_top, scroll_top);
  EXPECT_GT(new_scroll_top, 0);
}

// This tests that a in-progress smooth scroll on an overflow:scroll element
// stops when interrupted by a touch scroll.
IN_PROC_BROWSER_TEST_P(ScrollBehaviorBrowserTest,
                       OverflowScrollInterruptedByTouchScroll) {
  // TODO(crbug.com/1116647): compositing scroll should be able to cancel a
  // running programmatic scroll.
  if (!disable_threaded_scrolling_)
    return;

  LoadURL(kOverflowScrollDataURL);

  EXPECT_TRUE(
      ExecuteScript(shell()->web_contents(),
                    "element.scrollTo({top: 100, behavior: 'smooth'});"));

  std::string scroll_top_script = "element.scrollTop";
  WaitForScrollToStart(scroll_top_script);

  double scroll_top = ExecuteScriptAndExtractDouble(scroll_top_script);
  ASSERT_GT(scroll_top, 0);
  ASSERT_LT(scroll_top, kIntermediateScrollOffset);

  // When interrupted by a touch scroll, the in-progress smooth scrolls stop.
  SimulateScroll(SyntheticGestureParams::TOUCH_INPUT, 0, -100);

  // The touch scroll should cause scroll to 0 and cancel the animation, so
  // make sure the value stays at 0.
  ValueHoldsAt(scroll_top_script, 0);
}

// This tests that a in-progress smooth scroll on an overflow:scroll element
// stops when interrupted by a mouse wheel scroll.
IN_PROC_BROWSER_TEST_P(ScrollBehaviorBrowserTest,
                       OverflowScrollInterruptedByWheelScroll) {
  // TODO(crbug.com/1116647): compositing scroll should be able to cancel a
  // running programmatic scroll.
  if (!disable_threaded_scrolling_)
    return;

  LoadURL(kOverflowScrollDataURL);

  EXPECT_TRUE(
      ExecuteScript(shell()->web_contents(),
                    "element.scrollTo({top: 100, behavior: 'smooth'});"));

  std::string scroll_top_script = "element.scrollTop";
  WaitForScrollToStart(scroll_top_script);

  double scroll_top = ExecuteScriptAndExtractDouble(scroll_top_script);
  ASSERT_GT(scroll_top, 0);
  ASSERT_LT(scroll_top, kIntermediateScrollOffset);

  // When interrupted by a wheel scroll, the in-progress smooth scrolls stop.
  SimulateScroll(SyntheticGestureParams::MOUSE_INPUT, 0, -30);

  // Smooth scrolling is disabled for wheel scroll on Mac.
  // https://crbug.com/574283.
#if defined(OS_MAC) || defined(OS_ANDROID)
  ValueHoldsAt(scroll_top_script, 0);
#else
  WaitUntilLessThan(scroll_top_script, scroll_top);
  double new_scroll_top = ExecuteScriptAndExtractDouble(scroll_top_script);
  EXPECT_LT(new_scroll_top, scroll_top);
  EXPECT_GT(new_scroll_top, 0);
#endif
}

// This tests that a in-progress smooth scroll on the main frame stops when
// interrupted by another smooth scroll.
IN_PROC_BROWSER_TEST_P(ScrollBehaviorBrowserTest,
                       MainFrameScrollInterruptedBySmoothScroll) {
  LoadURL(kMainFrameScrollDataURL);

  EXPECT_TRUE(
      ExecuteScript(shell()->web_contents(),
                    "window.scrollTo({top: 100, behavior: 'smooth'});"));

  std::string scroll_top_script = "document.scrollingElement.scrollTop";
  WaitForScrollToStart(scroll_top_script);

  double scroll_top = ExecuteScriptAndExtractDouble(scroll_top_script);
  ASSERT_GT(scroll_top, 0);
  ASSERT_LT(scroll_top, kIntermediateScrollOffset);

  // When interrupted by a smooth scroll, the in-progress smooth scrolls stop.
  EXPECT_TRUE(ExecuteScript(shell()->web_contents(),
                            "window.scrollTo({top: 0, behavior: 'smooth'});"));

  WaitUntilLessThan(scroll_top_script, scroll_top);
  double new_scroll_top = ExecuteScriptAndExtractDouble(scroll_top_script);
  EXPECT_LT(new_scroll_top, scroll_top);
  EXPECT_GT(new_scroll_top, 0);
}

// This tests that a in-progress smooth scroll on a subframe stops when
// interrupted by another smooth scroll.
IN_PROC_BROWSER_TEST_P(ScrollBehaviorBrowserTest,
                       SubframeScrollInterruptedBySmoothScroll) {
  LoadURL(kSubframeScrollDataURL);

  EXPECT_TRUE(ExecuteScript(
      shell()->web_contents(),
      "subframe.contentWindow.scrollTo({top: 100, behavior: 'smooth'});"));

  std::string scroll_top_script =
      "subframe.contentDocument.scrollingElement.scrollTop";
  WaitForScrollToStart(scroll_top_script);

  double scroll_top = ExecuteScriptAndExtractDouble(scroll_top_script);
  ASSERT_GT(scroll_top, 0);
  ASSERT_LT(scroll_top, kIntermediateScrollOffset);

  // When interrupted by a smooth scroll, the in-progress smooth scrolls stop.
  EXPECT_TRUE(ExecuteScript(
      shell()->web_contents(),
      "subframe.contentWindow.scrollTo({top: 0, behavior: 'smooth'});"));

  WaitUntilLessThan(scroll_top_script, scroll_top);
  double new_scroll_top = ExecuteScriptAndExtractDouble(scroll_top_script);
  EXPECT_LT(new_scroll_top, scroll_top);
  EXPECT_GT(new_scroll_top, 0);
}

}  // namespace content
