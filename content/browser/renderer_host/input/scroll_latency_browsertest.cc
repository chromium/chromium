// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_trace_processor.h"
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
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/hit_test_region_observer.h"
#include "content/shell/browser/shell.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/input/synthetic_web_input_event_builders.h"
#include "third_party/blink/public/common/switches.h"
#include "ui/base/ui_base_features.h"
#include "ui/native_theme/native_theme_features.h"

#if BUILDFLAG(IS_MAC)
#include "ui/base/test/scoped_preferred_scroller_style_mac.h"
#endif

namespace {

const char kDataURL[] =
    "data:text/html;charset=utf-8,"
    "<!DOCTYPE html>"
    "<html>"
    "<head>"
    "<title>Scroll latency histograms browsertests.</title>"
    "<style>"
    "body {"
    "  height:9000px;"
    "  overscroll-behavior:none;"
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

  ScrollLatencyBrowserTest(const ScrollLatencyBrowserTest&) = delete;
  ScrollLatencyBrowserTest& operator=(const ScrollLatencyBrowserTest&) = delete;

  ~ScrollLatencyBrowserTest() override {}

  RenderWidgetHostImpl* GetWidgetHost() {
    return RenderWidgetHostImpl::From(shell()
                                          ->web_contents()
                                          ->GetPrimaryMainFrame()
                                          ->GetRenderViewHost()
                                          ->GetWidget());
  }

  // TODO(tdresser): Find a way to avoid sleeping like this. See
  // crbug.com/405282 for details.
  void GiveItSomeTime() {
    base::RunLoop run_loop;
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(), base::Milliseconds(10));
    run_loop.Run();
  }

  void OnSyntheticGestureCompleted(SyntheticGesture::Result result) {
    EXPECT_EQ(SyntheticGesture::GESTURE_FINISHED, result);
    run_loop_->Quit();
  }

 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    ContentBrowserTest::SetUpCommandLine(command_line);
    // Set the scroll animation duration to a large number so that
    // we ensure secondary GestureScrollUpdates update the animation
    // instead of starting a new one.
    command_line->AppendSwitchASCII(
        cc::switches::kCCScrollAnimationDurationForTesting, "10000000");
  }

  void LoadURL() {
    const GURL data_url(kDataURL);
    EXPECT_TRUE(NavigateToURL(shell(), data_url));

    RenderWidgetHostImpl* host = GetWidgetHost();

    HitTestRegionObserver observer(host->GetFrameSinkId());

    // Wait for the hit test data to be ready
    observer.WaitForHitTestData();
  }

  // Generate the gestures associated with kNumWheelScrolls ticks,
  // scrolling by |distance|. This will perform a smooth scroll on platforms
  // which support it.
  void DoSmoothWheelScroll(const gfx::Vector2d& distance) {
    blink::WebGestureEvent event =
        blink::SyntheticWebGestureEventBuilder::BuildScrollBegin(
            distance.x(), -distance.y(), blink::WebGestureDevice::kTouchpad, 1);
    event.data.scroll_begin.delta_hint_units =
        ui::ScrollGranularity::kScrollByPixel;
    GetWidgetHost()->ForwardGestureEvent(event);

    const uint32_t kNumWheelScrolls = 2;
    for (uint32_t i = 0; i < kNumWheelScrolls; i++) {
      // Install a VisualStateCallback and wait for the callback in response
      // to each GestureScrollUpdate before sending the next GSU. This will
      // ensure the events are not coalesced (resulting in fewer end-to-end
      // latency histograms being logged).
      // We must install a callback for each gesture since they are one-shot
      // callbacks.
      shell()->web_contents()->GetPrimaryMainFrame()->InsertVisualStateCallback(
          base::BindOnce(&ScrollLatencyBrowserTest::InvokeVisualStateCallback,
                         base::Unretained(this)));

      blink::WebGestureEvent event2 =
          blink::SyntheticWebGestureEventBuilder::BuildScrollUpdate(
              distance.x(), -distance.y(), 0,
              blink::WebGestureDevice::kTouchpad);
      event2.data.scroll_update.delta_units =
          ui::ScrollGranularity::kScrollByPixel;
      GetWidgetHost()->ForwardGestureEvent(event2);

      while (visual_state_callback_count_ <= i) {
        // TODO: There's currently no way to block until a GPU swap
        // completes. Until then we need to spin and wait. See
        // crbug.com/897520 for more details.
        GiveItSomeTime();
      }
    }
  }

  void InvokeVisualStateCallback(bool result) {
    EXPECT_TRUE(result);
    visual_state_callback_count_++;
  }

  void RunMultipleWheelScroll() {
    DoSmoothWheelScroll(gfx::Vector2d(0, 100));
    // We expect to see one ScrollBegin and two ScrollUpdate swap values.
    while (!VerifyRecordedSamplesForHistogram(
        1, "Event.Latency.ScrollBegin.Wheel.TimeToScrollUpdateSwapBegin4")) {
      GiveItSomeTime();
      FetchHistogramsFromChildProcesses();
    }

    while (!VerifyRecordedSamplesForHistogram(
        1, "Event.Latency.ScrollUpdate.Wheel.TimeToScrollUpdateSwapBegin4")) {
      GiveItSomeTime();
      FetchHistogramsFromChildProcesses();
    }
  }

  // Returns true if the given histogram has recorded the expected number of
  // samples.
  [[nodiscard]] bool VerifyRecordedSamplesForHistogram(
      const size_t num_samples,
      const std::string& histogram_name) const {
    return num_samples == GetSampleCountForHistogram(histogram_name);
  }

  size_t GetSampleCountForHistogram(const std::string& histogram_name) const {
    return histogram_tester_.GetAllSamples(histogram_name).size();
  }

  std::unique_ptr<base::RunLoop> run_loop_;

 protected:
  base::HistogramTester histogram_tester_;
  uint32_t visual_state_callback_count_ = 0;
};

// Disabled due to flakiness https://crbug.com/1163246.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || \
    BUILDFLAG(IS_ANDROID) ||                                         \
    (BUILDFLAG(IS_CHROMEOS) && defined(ADDRESS_SANITIZER))
#define MAYBE_MultipleWheelScroll DISABLED_MultipleWheelScroll
#else
#define MAYBE_MultipleWheelScroll MultipleWheelScroll
#endif

// Perform a smooth wheel scroll, and verify that our end-to-end wheel latency
// metrics are recorded. See crbug.com/599910 for details.
IN_PROC_BROWSER_TEST_F(ScrollLatencyBrowserTest, MAYBE_MultipleWheelScroll) {
  LoadURL();
  RunMultipleWheelScroll();
}

// Do an upward touch scroll, and verify that no scroll metrics is recorded when
// the scroll event is ignored.
IN_PROC_BROWSER_TEST_F(ScrollLatencyBrowserTest,
                       ScrollLatencyNotRecordedIfGSUIgnored) {
  LoadURL();
  auto scroll_update_watcher = std::make_unique<InputMsgWatcher>(
      GetWidgetHost(), blink::WebInputEvent::Type::kGestureScrollUpdate);

  // Try to scroll upward, the GSU(s) will get ignored since the scroller is at
  // its extent.
  SyntheticSmoothScrollGestureParams params;
  params.gesture_source_type = content::mojom::GestureSourceType::kTouchInput;
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
  EXPECT_EQ(blink::mojom::InputEventResultState::kNoConsumerExists,
            scroll_update_watcher->GetAckStateWaitIfNecessary());

  // Wait for one frame and then verify that the scroll metrics are not
  // recorded.
  std::unique_ptr<RenderFrameSubmissionObserver> frame_observer =
      std::make_unique<RenderFrameSubmissionObserver>(
          GetWidgetHost()->render_frame_metadata_provider());
  frame_observer->WaitForAnyFrameSubmission();

  FetchHistogramsFromChildProcesses();
  EXPECT_TRUE(VerifyRecordedSamplesForHistogram(
      0, "Event.Latency.ScrollBegin.Touch.TimeToScrollUpdateSwapBegin4"));
  EXPECT_TRUE(VerifyRecordedSamplesForHistogram(
      0, "EventLatency.GestureScrollUpdate.TotalLatency"));
}

#if BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)
// A basic smoke test verifying that key scroll-related events are recorded
// during scrolling. This test performs a simple scroll and expects to see three
// EventLatency events with the correct types.
IN_PROC_BROWSER_TEST_F(ScrollLatencyBrowserTest, ScrollingEventLatencyTrace) {
  LoadURL();
  base::test::TestTraceProcessor ttp;
  ttp.StartTrace("input.scrolling");
  DoSmoothWheelScroll(gfx::Vector2d(0, 100));
  while (!VerifyRecordedSamplesForHistogram(
      1, "EventLatency.GestureScrollUpdate.TotalLatency2")) {
    GiveItSomeTime();
    FetchHistogramsFromChildProcesses();
  }
  absl::Status status = ttp.StopAndParseTrace();
  ASSERT_TRUE(status.ok()) << status.message();
  std::string query =
      R"(
      SELECT EXTRACT_ARG(arg_set_id, 'event_latency.event_type') AS type
      FROM slice
      WHERE name = 'EventLatency'
      )";
  auto result = ttp.RunQuery(query);
  ASSERT_TRUE(result.has_value()) << result.error();
  EXPECT_THAT(result.value(),
              ::testing::ElementsAre(
                  std::vector<std::string>{"type"},
                  std::vector<std::string>{"GESTURE_SCROLL_BEGIN"},
                  std::vector<std::string>{"FIRST_GESTURE_SCROLL_UPDATE"},
                  std::vector<std::string>{"GESTURE_SCROLL_UPDATE"}));
}
#endif  // BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)

class ScrollLatencyScrollbarBrowserTest : public ScrollLatencyBrowserTest {
 public:
  ScrollLatencyScrollbarBrowserTest() = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ScrollLatencyBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(::switches::kDisableSmoothScrolling);

    // The following features need to be disabled:
    // - kOverlayScrollbar since overlay scrollbars are not hit-testable (thus
    // input is not routed to scrollbars).
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {}, {features::kOverlayScrollbar});
  }

  ~ScrollLatencyScrollbarBrowserTest() override = default;

 private:
#if BUILDFLAG(IS_MAC)
  // Native scrollbars on Mac are overlay scrollbars. Hence they need to be
  // disabled.
  ui::test::ScopedPreferredScrollerStyle scroller_style_override{false};
#endif

 protected:
  void RunScrollbarThumbDragLatencyTest() {
    // See above comment in RunScrollbarButtonLatencyTest for why this test
    // doesn't run on Android.
#if !BUILDFLAG(IS_ANDROID)
    // Click on the scrollbar thumb and drag it twice to induce a compositor
    // thread scrollbar ScrollBegin and ScrollUpdate.
    gfx::PointF scrollbar_thumb(795, 30);
    blink::WebMouseEvent mouse_down =
        blink::SyntheticWebMouseEventBuilder::Build(
            blink::WebInputEvent::Type::kMouseDown, scrollbar_thumb.x(),
            scrollbar_thumb.y(), 0);
    mouse_down.button = blink::WebMouseEvent::Button::kLeft;
    mouse_down.SetTimeStamp(base::TimeTicks::Now());
    GetWidgetHost()->ForwardMouseEvent(mouse_down);

    // This is to avoid a race condition where a mousemove is processed before
    // the renderer has had a chance to set up the scroll state (like the
    // scroll_node etc). This happens due to the fact that when the renderer
    // gets a mousedown, it is first "queued" as a GSB. At this point, the
    // scroll node is not yet set up. Now, if a mousemove is sent from the
    // browser proc before a frame is generated, it gets dispatched immediately
    // and this can lead to nullptr derefernces.
    RunUntilInputProcessed(GetWidgetHost());

    blink::WebMouseEvent mouse_move =
        blink::SyntheticWebMouseEventBuilder::Build(
            blink::WebInputEvent::Type::kMouseMove, scrollbar_thumb.x(),
            scrollbar_thumb.y() + 10, 0);
    mouse_move.button = blink::WebMouseEvent::Button::kLeft;
    mouse_move.SetTimeStamp(base::TimeTicks::Now());
    GetWidgetHost()->ForwardMouseEvent(mouse_move);
    RunUntilInputProcessed(GetWidgetHost());

    mouse_move.SetPositionInWidget(scrollbar_thumb.x(),
                                   scrollbar_thumb.y() + 20);
    mouse_move.SetPositionInScreen(scrollbar_thumb.x(),
                                   scrollbar_thumb.y() + 20);
    GetWidgetHost()->ForwardMouseEvent(mouse_move);
    RunUntilInputProcessed(GetWidgetHost());

    blink::WebMouseEvent mouse_up = blink::SyntheticWebMouseEventBuilder::Build(
        blink::WebInputEvent::Type::kMouseUp, scrollbar_thumb.x(),
        scrollbar_thumb.y() + 20, 0);
    mouse_up.button = blink::WebMouseEvent::Button::kLeft;
    mouse_up.SetTimeStamp(base::TimeTicks::Now());
    GetWidgetHost()->ForwardMouseEvent(mouse_up);

    RunUntilInputProcessed(GetWidgetHost());

    FetchHistogramsFromChildProcesses();
    EXPECT_GT(
        GetSampleCountForHistogram(
            "Event.Latency.ScrollBegin.Scrollbar.TimeToScrollUpdateSwapBegin4"),
        0u);
#endif  // !BUILDFLAG(IS_ANDROID)
  }

  base::test::ScopedFeatureList scoped_feature_list_;
};

// Crashes on Mac ASAN.  https://crbug.com/1188553
// TODO(crbug.com/1188553): Flaky on Linux Wayland CI/CQ builders.
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
#define MAYBE_ScrollbarThumbDragLatency DISABLED_ScrollbarThumbDragLatency
#else
#define MAYBE_ScrollbarThumbDragLatency ScrollbarThumbDragLatency
#endif
IN_PROC_BROWSER_TEST_F(ScrollLatencyScrollbarBrowserTest,
                       MAYBE_ScrollbarThumbDragLatency) {
  LoadURL();

  RunScrollbarThumbDragLatencyTest();
}

}  // namespace content
