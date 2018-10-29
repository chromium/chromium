// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "content/browser/renderer_host/input/synthetic_gesture.h"
#include "content/browser/renderer_host/input/synthetic_gesture_controller.h"
#include "content/browser/renderer_host/input/synthetic_gesture_target.h"
#include "content/browser/renderer_host/input/synthetic_smooth_scroll_gesture.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/input/synthetic_gesture_params.h"
#include "content/common/input/synthetic_smooth_scroll_gesture_params.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"

namespace {

const char kDataURL[] =
    "data:text/html;charset=utf-8,"
    "<!DOCTYPE html>"
    "<html>"
    "<head>"
    "<title>Scroll latency histograms browsertests.</title>"
    "<script src=\"../../resources/testharness.js\"></script>"
    "<script src=\"../../resources/testharnessreport.js\"></script>"
    "<style>"
    "body {"
    "  height:3000px;"
    "}"
    "</style>"
    "</head>"
    "<body>"
    "<div id='spinner'>Spinning</div>"
    "</body>"
    "<script>"
    "var degree = 0;"
    "function spin() {"
    "degree = degree + 3;"
    "if (degree >= 360)"
    "degree -= 360;"
    "document.getElementById('spinner').style['transform'] = "
    "'rotate(' + degree + 'deg)';"
    "requestAnimationFrame(spin);"
    "}"
    "spin();"
    "</script>"
    "</html>";

}  // namespace

namespace content {

class ScrollLatencyBrowserTest : public ContentBrowserTest {
 public:
  ScrollLatencyBrowserTest() {}
  ~ScrollLatencyBrowserTest() override {}

  RenderWidgetHostImpl* GetWidgetHost() {
    return RenderWidgetHostImpl::From(
        shell()->web_contents()->GetRenderViewHost()->GetWidget());
  }

  // TODO(tdresser): Find a way to avoid sleeping like this. See
  // crbug.com/405282 for details.
  void GiveItSomeTime() {
    base::RunLoop run_loop;
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(),
        base::TimeDelta::FromMillisecondsD(10));
    run_loop.Run();
  }

  void WaitAFrame() {
    while (!GetWidgetHost()->RequestRepaintForTesting())
      GiveItSomeTime();
    frame_observer_->Wait();
  }

  void OnSyntheticGestureCompleted(SyntheticGesture::Result result) {
    EXPECT_EQ(SyntheticGesture::GESTURE_FINISHED, result);
    run_loop_->Quit();
  }

 protected:
  void LoadURL() {
    const GURL data_url(kDataURL);
    NavigateToURL(shell(), data_url);

    RenderWidgetHostImpl* host = GetWidgetHost();
    host->GetView()->SetSize(gfx::Size(400, 400));

    frame_observer_ = std::make_unique<MainThreadFrameObserver>(
        shell()->web_contents()->GetRenderViewHost()->GetWidget());

    // Wait a frame to make sure the page has renderered.
    WaitAFrame();
    frame_observer_.reset();
  }

  // Generate a single wheel tick, scrolling by |distance|. This will perform a
  // smooth scroll on platforms which support it.
  void DoSmoothWheelScroll(const gfx::Vector2d& distance) {
    blink::WebGestureEvent event =
        SyntheticWebGestureEventBuilder::BuildScrollBegin(
            distance.x(), -distance.y(),
            blink::WebGestureDevice::kWebGestureDeviceTouchpad, 1);
    event.data.scroll_begin.delta_hint_units =
        blink::WebGestureEvent::ScrollUnits::kPixels;
    GetWidgetHost()->ForwardGestureEvent(event);

    blink::WebGestureEvent event2 =
        SyntheticWebGestureEventBuilder::BuildScrollUpdate(
            distance.x(), -distance.y(), 0,
            blink::WebGestureDevice::kWebGestureDeviceTouchpad);
    event2.data.scroll_update.delta_units =
        blink::WebGestureEvent::ScrollUnits::kPixels;
    GetWidgetHost()->ForwardGestureEvent(event2);
  }

  // Returns true if the given histogram has recorded the expected number of
  // samples.
  bool VerifyRecordedSamplesForHistogram(
      const size_t num_samples,
      const std::string& histogram_name) const {
    return num_samples ==
           histogram_tester_.GetAllSamples(histogram_name).size();
  }

  std::unique_ptr<base::RunLoop> run_loop_;

 private:
  base::HistogramTester histogram_tester_;
  std::unique_ptr<MainThreadFrameObserver> frame_observer_;

  DISALLOW_COPY_AND_ASSIGN(ScrollLatencyBrowserTest);
};

// Perform a smooth wheel scroll, and verify that our end-to-end wheel latency
// metric is recorded. See crbug.com/599910 for details.
IN_PROC_BROWSER_TEST_F(ScrollLatencyBrowserTest, SmoothWheelScroll) {
  LoadURL();

  DoSmoothWheelScroll(gfx::Vector2d(0, 100));
  while (!VerifyRecordedSamplesForHistogram(
      1, "Event.Latency.ScrollBegin.Wheel.TimeToScrollUpdateSwapBegin2")) {
    GiveItSomeTime();
    FetchHistogramsFromChildProcesses();
  }
}

// Do an upward wheel scroll, and verify that no scroll metrics is recorded when
// the scroll event is ignored.
IN_PROC_BROWSER_TEST_F(ScrollLatencyBrowserTest,
                       ScrollLatencyNotRecordedIfGSUIgnored) {
  LoadURL();
  auto scroll_update_watcher = std::make_unique<InputMsgWatcher>(
      GetWidgetHost(), blink::WebInputEvent::kGestureScrollUpdate);

  // Try to scroll upward, the GSU(s) will get ignored since the scroller is at
  // its extent.
  SyntheticSmoothScrollGestureParams params;
  params.gesture_source_type = SyntheticGestureParams::TOUCH_INPUT;
  params.anchor = gfx::PointF(10, 10);
  params.distances.push_back(gfx::Vector2d(0, 60));

  run_loop_ = std::make_unique<base::RunLoop>();

  std::unique_ptr<SyntheticSmoothScrollGesture> gesture(
      new SyntheticSmoothScrollGesture(params));
  GetWidgetHost()->QueueSyntheticGesture(
      std::move(gesture),
      base::BindOnce(&ScrollLatencyBrowserTest::OnSyntheticGestureCompleted,
                     base::Unretained(this)));

  // Runs until we get the OnSyntheticGestureCompleted callback and verify that
  // the first GSU event is ignored.
  run_loop_->Run();
  EXPECT_EQ(INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS,
            scroll_update_watcher->GetAckStateWaitIfNecessary());

  // Wait for one frame and then verify that the scroll metrics are not
  // recorded.
  std::unique_ptr<RenderFrameSubmissionObserver> frame_observer =
      std::make_unique<RenderFrameSubmissionObserver>(
          GetWidgetHost()->render_frame_metadata_provider());
  frame_observer->WaitForAnyFrameSubmission();
  FetchHistogramsFromChildProcesses();
  EXPECT_TRUE(VerifyRecordedSamplesForHistogram(
      0, "Event.Latency.ScrollBegin.Touch.BrowserNotifiedToBeforeGpuSwap2"));
  EXPECT_TRUE(VerifyRecordedSamplesForHistogram(
      0, "Event.Latency.ScrollBegin.Touch.GpuSwap2"));
  EXPECT_TRUE(VerifyRecordedSamplesForHistogram(
      0, "Event.Latency.ScrollBegin.Touch.HandledToRendererSwap2_Impl"));
  EXPECT_TRUE(VerifyRecordedSamplesForHistogram(
      0, "Event.Latency.ScrollBegin.Touch.RendererSwapToBrowserNotified2"));
  EXPECT_TRUE(VerifyRecordedSamplesForHistogram(
      0, "Event.Latency.ScrollBegin.Touch.TimeToHandled2_Impl"));
  EXPECT_TRUE(VerifyRecordedSamplesForHistogram(
      0, "Event.Latency.ScrollBegin.Touch.TimeToScrollUpdateSwapBegin2"));
}

}  // namespace content
