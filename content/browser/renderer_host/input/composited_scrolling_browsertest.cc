// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "cc/base/features.h"
#include "content/browser/renderer_host/input/synthetic_gesture.h"
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
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "third_party/blink/public/common/switches.h"
#include "ui/gfx/geometry/angle_conversions.h"

namespace {

const char kCompositedScrollingDataURL[] =
    "data:text/html;charset=utf-8,"
    "<!DOCTYPE html>"
    "<meta name='viewport' content='width=device-width'/>"
    "<style>"
    "%23scroller {"
    "  width:500px;"
    "  height:500px;"
    "  overflow:scroll;"
    "  transform: rotateX(-30deg);"
    "}"

    "%23content {"
    "  background-color:red;"
    "  width:1000px;"
    "  height:1000px;"
    "}"
    "</style>"
    "<div id='scroller'>"
    "  <div id='content'>"
    "  </div>"
    "</div>"
    "<script>"
    "  document.title='ready';"
    "</script>";

}  // namespace

namespace content {


class CompositedScrollingBrowserTest : public ContentBrowserTest {
 public:
  CompositedScrollingBrowserTest() {
    // Disable scroll resampling because this is checking scroll distance.
    scoped_feature_list_.InitAndDisableFeature(
        blink::features::kResamplingScrollEvents);
  }

  ~CompositedScrollingBrowserTest() override = default;

  void SetUpCommandLine(base::CommandLine* cmd) override {
    cmd->AppendSwitch(blink::switches::kEnablePreferCompositingToLCDText);
  }

  RenderWidgetHostImpl* GetWidgetHost() {
    return RenderWidgetHostImpl::From(
        shell()->web_contents()->GetRenderViewHost()->GetWidget());
  }

  void OnSyntheticGestureCompleted(SyntheticGesture::Result result) {
    EXPECT_EQ(SyntheticGesture::GESTURE_FINISHED, result);
    runner_->Quit();
  }

 protected:
  void LoadURL(const char* url) {
    const GURL data_url(url);
    EXPECT_TRUE(NavigateToURL(shell(), data_url));

    RenderWidgetHostImpl* host = GetWidgetHost();
    HitTestRegionObserver observer(GetWidgetHost()->GetFrameSinkId());
    host->GetView()->SetSize(gfx::Size(400, 400));

    base::string16 ready_title(base::ASCIIToUTF16("ready"));
    TitleWatcher watcher(shell()->web_contents(), ready_title);
    ignore_result(watcher.WaitAndGetTitle());

    // Wait for the hit test data to be ready after initiating URL loading
    // before returning
    observer.WaitForHitTestData();
  }

  // ContentBrowserTest:
  int ExecuteScriptAndExtractInt(const std::string& script) {
    int value = 0;
    EXPECT_TRUE(content::ExecuteScriptAndExtractInt(
        shell(), "domAutomationController.send(" + script + ")", &value));
    return value;
  }

  double ExecuteScriptAndExtractDouble(const std::string& script) {
    double value = 0;
    EXPECT_TRUE(content::ExecuteScriptAndExtractDouble(
        shell(), "domAutomationController.send(" + script + ")", &value));
    return value;
  }

  double GetScrollTop() {
    return ExecuteScriptAndExtractDouble(
        "document.getElementById(\"scroller\").scrollTop");
  }

  // Generate touch events for a synthetic scroll from |point| for |distance|.
  // Returns the distance scrolled.
  double DoScroll(SyntheticGestureParams::GestureSourceType type,
                  const gfx::Point& point,
                  const gfx::Vector2d& distance) {
    SyntheticSmoothScrollGestureParams params;
    params.gesture_source_type = type;
    params.anchor = gfx::PointF(point);
    params.distances.push_back(-distance);

    runner_ = new MessageLoopRunner();

    std::unique_ptr<SyntheticSmoothScrollGesture> gesture(
        new SyntheticSmoothScrollGesture(params));
    GetWidgetHost()->QueueSyntheticGesture(
        std::move(gesture),
        base::BindOnce(
            &CompositedScrollingBrowserTest::OnSyntheticGestureCompleted,
            base::Unretained(this)));

    // Runs until we get the OnSyntheticGestureCompleted callback
    runner_->Run();
    runner_ = nullptr;

    return GetScrollTop();
  }

  double DoTouchScroll(const gfx::Point& point, const gfx::Vector2d& distance) {
    return DoScroll(SyntheticGestureParams::TOUCH_INPUT, point, distance);
  }

  double DoWheelScroll(const gfx::Point& point, const gfx::Vector2d& distance) {
    return DoScroll(SyntheticGestureParams::MOUSE_INPUT, point, distance);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  scoped_refptr<MessageLoopRunner> runner_;

  DISALLOW_COPY_AND_ASSIGN(CompositedScrollingBrowserTest);
};

// Verify transforming a scroller doesn't prevent it from scrolling. See
// crbug.com/543655 for a case where this was broken.
// Disabled on MacOS because it doesn't support touch input.
// Disabled on Android due to flakiness, see https://crbug.com/376668.
// Flaky on Windows: crbug.com/804009
#if defined(OS_MAC) || defined(OS_ANDROID) || defined(OS_WIN)
#define MAYBE_Scroll3DTransformedScroller DISABLED_Scroll3DTransformedScroller
#else
#define MAYBE_Scroll3DTransformedScroller Scroll3DTransformedScroller
#endif
IN_PROC_BROWSER_TEST_F(CompositedScrollingBrowserTest,
                       MAYBE_Scroll3DTransformedScroller) {
  LoadURL(kCompositedScrollingDataURL);
  ASSERT_EQ(0, GetScrollTop());

  double scroll_distance =
      DoTouchScroll(gfx::Point(50, 150), gfx::Vector2d(0, 100));
  // The scroll distance is increased due to the rotation of the scroller.
  EXPECT_NEAR(100 / std::cos(gfx::DegToRad(30.f)), scroll_distance, 1.f);
}

class CompositedScrollingMetricTest : public CompositedScrollingBrowserTest,
                                      public testing::WithParamInterface<bool> {
 public:
  CompositedScrollingMetricTest() = default;
  ~CompositedScrollingMetricTest() override = default;

  void SetUpCommandLine(base::CommandLine* cmd) override {
    const bool enable_composited_scrolling = GetParam();
    if (enable_composited_scrolling)
      cmd->AppendSwitch(blink::switches::kEnablePreferCompositingToLCDText);
    else
      cmd->AppendSwitch(blink::switches::kDisableThreadedScrolling);
  }

  bool CompositingEnabled() { return GetParam(); }

  const char* kWheelHistogramName = "Renderer4.ScrollingThread.Wheel";
  const char* kTouchHistogramName = "Renderer4.ScrollingThread.Touch";

  // These values are defined in input_handler_proxy.cc and match the enum in
  // histograms.xml.
  enum ScrollingThreadStatus {
    kScrollingOnCompositor = 0,
    kScrollingOnCompositorBlockedOnMain = 1,
    kScrollingOnMain = 2,
    kMaxValue = kScrollingOnMain,
  };
};

INSTANTIATE_TEST_SUITE_P(All,
                         CompositedScrollingMetricTest,
                         ::testing::Bool());

// Tests the functionality of the histogram tracking composited vs main thread
// scrolls.
IN_PROC_BROWSER_TEST_P(CompositedScrollingMetricTest,
                       RecordCorrectScrollingThread) {
  LoadURL(R"HTML(
    data:text/html;charset=utf-8,
    <!DOCTYPE html>
    <style>
      %23scroller {
        width: 300px;
        height: 300px;
        overflow: auto;
      }
      %23space {
        width: 1000px;
        height: 1000px;
      }
    </style>
    <div id='scroller'>
      <div id='space'></div>
    </div>
    <script>
      document.title = 'ready';
    </script>
  )HTML");

  base::HistogramTester histograms;

  DoTouchScroll(gfx::Point(50, 50), gfx::Vector2d(0, 100));
  DoTouchScroll(gfx::Point(50, 50), gfx::Vector2d(0, -100));

  DoWheelScroll(gfx::Point(50, 50), gfx::Vector2d(0, 100));

  content::FetchHistogramsFromChildProcesses();

  base::HistogramBase::Sample expected_bucket =
      CompositingEnabled() ? kScrollingOnCompositor : kScrollingOnMain;
  if (base::FeatureList::IsEnabled(::features::kScrollUnification)) {
    // TODO: crbug.com/1082590
    // After ScrollUnification all scrolls happen on the compositor thread
    // but some will still force blocking on main thread
    expected_bucket = kScrollingOnCompositor;
  }

  histograms.ExpectUniqueSample(kTouchHistogramName, expected_bucket, 2);
  histograms.ExpectUniqueSample(kWheelHistogramName, expected_bucket, 1);
}

// Tests the composited vs main thread scrolling histogram in the presence of
// blocking event handlers.
IN_PROC_BROWSER_TEST_P(CompositedScrollingMetricTest, BlockingEventHandlers) {
  LoadURL(R"HTML(
    data:text/html;charset=utf-8,
    <!DOCTYPE html>
    <style>
      %23scroller {
        width: 300px;
        height: 300px;
        overflow: auto;
      }
      %23space {
        width: 1000px;
        height: 1000px;
      }
    </style>
    <div id='scroller'>
      <div id='space'></div>
    </div>
    <script>
      const scroller = document.getElementById('scroller');
      scroller.addEventListener('wheel', (e) => { }, {passive: false});
      scroller.addEventListener('touchstart', (e) => { }, {passive: false});
      onload = () => {
        document.title = 'ready';
      }
    </script>
  )HTML");

  base::HistogramTester histograms;

  DoTouchScroll(gfx::Point(50, 50), gfx::Vector2d(0, 100));
  DoTouchScroll(gfx::Point(50, 50), gfx::Vector2d(0, -100));

  DoWheelScroll(gfx::Point(50, 50), gfx::Vector2d(0, 100));

  content::FetchHistogramsFromChildProcesses();

  base::HistogramBase::Sample expected_bucket =
      CompositingEnabled() ? kScrollingOnCompositorBlockedOnMain
                           : kScrollingOnMain;
  if (base::FeatureList::IsEnabled(::features::kScrollUnification)) {
    // TODO: crbug.com/1082590
    // After ScrollUnification all scrolls happen on the compositor thread
    // but some will still force blocking on main thread
    expected_bucket = kScrollingOnCompositorBlockedOnMain;
  }

  histograms.ExpectUniqueSample(kTouchHistogramName, expected_bucket, 2);
  histograms.ExpectUniqueSample(kWheelHistogramName, expected_bucket, 1);
}

// Tests the composited vs main thread scrolling histogram in the presence of
// passive event handlers. These should behave the same as the case without any
// event handlers at all.
IN_PROC_BROWSER_TEST_P(CompositedScrollingMetricTest, PassiveEventHandlers) {
  LoadURL(R"HTML(
    data:text/html;charset=utf-8,
    <!DOCTYPE html>
    <style>
      %23scroller {
        width: 300px;
        height: 300px;
        overflow: auto;
      }
      %23space {
        width: 1000px;
        height: 1000px;
      }
    </style>
    <div id='scroller'>
      <div id='space'></div>
    </div>
    <script>
      const scroller = document.getElementById('scroller');
      scroller.addEventListener('wheel', (e) => { }, {passive: true});
      scroller.addEventListener('touchstart', (e) => { }, {passive: true});
      onload = () => {
        document.title = 'ready';
      }
    </script>
  )HTML");

  base::HistogramTester histograms;

  DoTouchScroll(gfx::Point(50, 50), gfx::Vector2d(0, 100));
  DoTouchScroll(gfx::Point(50, 50), gfx::Vector2d(0, -100));

  DoWheelScroll(gfx::Point(50, 50), gfx::Vector2d(0, 100));

  content::FetchHistogramsFromChildProcesses();

  base::HistogramBase::Sample expected_bucket =
      CompositingEnabled() ? kScrollingOnCompositor : kScrollingOnMain;
  if (base::FeatureList::IsEnabled(::features::kScrollUnification)) {
    // TODO: crbug.com/1082590
    // After ScrollUnification all scrolls happen on the compositor thread
    // but some will still force blocking on main thread
    expected_bucket = kScrollingOnCompositor;
  }

  histograms.ExpectUniqueSample(kTouchHistogramName, expected_bucket, 2);
  histograms.ExpectUniqueSample(kWheelHistogramName, expected_bucket, 1);
}

}  // namespace content
