// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <tuple>

#include "base/feature_list.h"
#include "build/build_config.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "third_party/blink/public/common/input/synthetic_web_input_event_builders.h"
#include "ui/events/base_event_utils.h"

namespace {

const std::string kAutoscrollDataURL = R"HTML(
  <!DOCTYPE html>
  <meta name='viewport' content='width=device-width'/>
  <style>
  html, body {
    margin: 0;
  }
  .spacer { height: 10000px; }
  </style>
  <div class=spacer></div>
  <script>
    document.title='ready';
  </script>)HTML";
}  // namespace

namespace content {

// Waits for the ack of a gesture scroll event with the given type and returns
// the acked event.
class GestureScrollEventWatcher : public RenderWidgetHost::InputEventObserver {
 public:
  GestureScrollEventWatcher(RenderWidgetHost* rwh,
                            blink::WebInputEvent::Type event_type)
      : rwh_(static_cast<RenderWidgetHostImpl*>(rwh)->GetWeakPtr()),
        event_type_(event_type) {
    rwh->AddInputEventObserver(this);
    Reset();
  }
  ~GestureScrollEventWatcher() override {
    if (rwh_)
      rwh_->RemoveInputEventObserver(this);
  }

  void OnInputEventAck(blink::mojom::InputEventResultSource,
                       blink::mojom::InputEventResultState,
                       const blink::WebInputEvent& event) override {
    if (event.GetType() != event_type_)
      return;
    if (run_loop_)
      run_loop_->Quit();

    blink::WebGestureEvent acked_gesture_event =
        *static_cast<const blink::WebGestureEvent*>(&event);
    acked_gesture_event_ =
        std::make_unique<blink::WebGestureEvent>(acked_gesture_event);
  }

  void Wait() {
    if (acked_gesture_event_)
      return;
    DCHECK(run_loop_);
    run_loop_->Run();
  }

  void Reset() {
    acked_gesture_event_.reset();
    run_loop_ = std::make_unique<base::RunLoop>();
  }

  const blink::WebGestureEvent* AckedGestureEvent() const {
    return acked_gesture_event_.get();
  }

 private:
  base::WeakPtr<RenderWidgetHostImpl> rwh_;
  blink::WebInputEvent::Type event_type_;
  std::unique_ptr<base::RunLoop> run_loop_;
  std::unique_ptr<blink::WebGestureEvent> acked_gesture_event_;
};

class AutoscrollBrowserTest : public ContentBrowserTest {
 public:
  AutoscrollBrowserTest() {}

  AutoscrollBrowserTest(const AutoscrollBrowserTest&) = delete;
  AutoscrollBrowserTest& operator=(const AutoscrollBrowserTest&) = delete;

  ~AutoscrollBrowserTest() override {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(switches::kEnableBlinkFeatures,
                                    "MiddleClickAutoscroll");
  }

 protected:
  RenderWidgetHostImpl* GetWidgetHost() {
    return RenderWidgetHostImpl::From(shell()
                                          ->web_contents()
                                          ->GetPrimaryMainFrame()
                                          ->GetRenderViewHost()
                                          ->GetWidget());
  }

  void LoadURL(const std::string& page_data) {
    const GURL data_url("data:text/html," + page_data);
    EXPECT_TRUE(NavigateToURL(shell(), data_url));

    RenderWidgetHostImpl* host = GetWidgetHost();
    host->GetView()->SetSize(gfx::Size(400, 400));

    std::u16string ready_title(u"ready");
    TitleWatcher watcher(shell()->web_contents(), ready_title);
    std::ignore = watcher.WaitAndGetTitle();

    MainThreadFrameObserver main_thread_sync(host);
    main_thread_sync.Wait();
  }

  // Force redraw and wait for a compositor commit for the given number of
  // times.
  void WaitForCommitFrames(int num_repeat) {
    RenderFrameSubmissionObserver observer(
        GetWidgetHost()->render_frame_metadata_provider());
    for (int i = 0; i < num_repeat; i++) {
      GetWidgetHost()->RequestForceRedraw(i);
      observer.WaitForAnyFrameSubmission();
    }
  }

  void SimulateMiddleClick(int x, int y, int modifiers) {
    bool is_autoscroll_in_progress = GetWidgetHost()->IsAutoscrollInProgress();

    // Simulate and send middle click mouse down.
    blink::WebMouseEvent down_event =
        blink::SyntheticWebMouseEventBuilder::Build(
            blink::WebInputEvent::Type::kMouseDown, x, y, modifiers);
    down_event.button = blink::WebMouseEvent::Button::kMiddle;
    down_event.SetTimeStamp(ui::EventTimeForNow());
    down_event.SetPositionInScreen(x, y);
    GetWidgetHost()->ForwardMouseEvent(down_event);

    // Simulate and send middle click mouse up.
    blink::WebMouseEvent up_event = blink::SyntheticWebMouseEventBuilder::Build(
        blink::WebInputEvent::Type::kMouseUp, x, y, modifiers);
    up_event.button = blink::WebMouseEvent::Button::kMiddle;
    up_event.SetTimeStamp(ui::EventTimeForNow());
    up_event.SetPositionInScreen(x, y);
    GetWidgetHost()->ForwardMouseEvent(up_event);

    // Wait till the IPC messages arrive and IsAutoscrollInProgress() toggles.
    while (GetWidgetHost()->IsAutoscrollInProgress() ==
           is_autoscroll_in_progress) {
      WaitForCommitFrames(1);
    }
  }

  void WaitForScroll(RenderFrameSubmissionObserver& observer) {
    gfx::PointF default_scroll_offset;
    while (observer.LastRenderFrameMetadata()
               .root_scroll_offset.value_or(default_scroll_offset)
               .y() <= 0) {
      observer.WaitForMetadataChange();
    }
  }
};

// We don't plan on supporting middle click autoscroll on Android.
// See https://crbug.com/686223 We similarly don't plan on supporting
// this for iOS.
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
IN_PROC_BROWSER_TEST_F(AutoscrollBrowserTest, AutoscrollFling) {
  LoadURL(kAutoscrollDataURL);

  // Start autoscroll with middle click.
  auto scroll_begin_watcher = std::make_unique<GestureScrollEventWatcher>(
      GetWidgetHost(), blink::WebInputEvent::Type::kGestureScrollBegin);
  SimulateMiddleClick(10, 10, blink::WebInputEvent::kNoModifiers);

  // The page should start scrolling with mouse move.
  RenderFrameSubmissionObserver observer(
      GetWidgetHost()->render_frame_metadata_provider());
  blink::WebMouseEvent move_event = blink::SyntheticWebMouseEventBuilder::Build(
      blink::WebInputEvent::Type::kMouseMove, 50, 50,
      blink::WebInputEvent::kNoModifiers);
  move_event.SetTimeStamp(ui::EventTimeForNow());
  move_event.SetPositionInScreen(50, 50);
  GetWidgetHost()->ForwardMouseEvent(move_event);
  scroll_begin_watcher->Wait();
  WaitForScroll(observer);
}

// Tests that the GSB sent in the beginning of a middle click autoscroll has
// none-zero delta hints.
IN_PROC_BROWSER_TEST_F(AutoscrollBrowserTest, AutoscrollFlingGSBDeltaHints) {
  LoadURL(kAutoscrollDataURL);

  // Start autoscroll with middle click.
  auto scroll_begin_watcher = std::make_unique<GestureScrollEventWatcher>(
      GetWidgetHost(), blink::WebInputEvent::Type::kGestureScrollBegin);
  SimulateMiddleClick(10, 10, blink::WebInputEvent::kNoModifiers);

  // A GSB will be sent on first mouse move.
  blink::WebMouseEvent move_event = blink::SyntheticWebMouseEventBuilder::Build(
      blink::WebInputEvent::Type::kMouseMove, 50, 50,
      blink::WebInputEvent::kNoModifiers);
  move_event.SetTimeStamp(ui::EventTimeForNow());
  move_event.SetPositionInScreen(50, 50);
  GetWidgetHost()->ForwardMouseEvent(move_event);
  scroll_begin_watcher->Wait();
  const blink::WebGestureEvent* acked_scroll_begin =
      scroll_begin_watcher->AckedGestureEvent();
  DCHECK(acked_scroll_begin);
  DCHECK(acked_scroll_begin->data.scroll_begin.delta_x_hint ||
         acked_scroll_begin->data.scroll_begin.delta_y_hint);
}

// Tests that the GSU and GSE events generated from the autoscroll fling have
// non-zero positions in widget.
IN_PROC_BROWSER_TEST_F(AutoscrollBrowserTest, GSUGSEValidPositionInWidget) {
  LoadURL(kAutoscrollDataURL);

  // Start autoscroll with middle click.
  auto scroll_update_watcher = std::make_unique<GestureScrollEventWatcher>(
      GetWidgetHost(), blink::WebInputEvent::Type::kGestureScrollUpdate);
  SimulateMiddleClick(10, 10, blink::WebInputEvent::kNoModifiers);

  // Check that the generated GSU has non-zero position in widget.
  blink::WebMouseEvent move_event = blink::SyntheticWebMouseEventBuilder::Build(
      blink::WebInputEvent::Type::kMouseMove, 50, 50,
      blink::WebInputEvent::kNoModifiers);
  move_event.SetTimeStamp(ui::EventTimeForNow());
  move_event.SetPositionInScreen(50, 50);
  GetWidgetHost()->ForwardMouseEvent(move_event);
  scroll_update_watcher->Wait();
  const blink::WebGestureEvent* acked_scroll_update =
      scroll_update_watcher->AckedGestureEvent();
  DCHECK(acked_scroll_update);
  DCHECK(acked_scroll_update->PositionInWidget() != gfx::PointF());

  // End autoscroll and check that the GSE generated from autoscroll fling
  // cancelation has non-zero position in widget.
  auto scroll_end_watcher = std::make_unique<GestureScrollEventWatcher>(
      GetWidgetHost(), blink::WebInputEvent::Type::kGestureScrollEnd);
  SimulateMiddleClick(50, 50, blink::WebInputEvent::kNoModifiers);
  scroll_end_watcher->Wait();
  const blink::WebGestureEvent* acked_scroll_end =
      scroll_end_watcher->AckedGestureEvent();
  DCHECK(acked_scroll_end);
  DCHECK(acked_scroll_end->PositionInWidget() != gfx::PointF());
}

// Checks that wheel scrolling works after autoscroll cancelation.
IN_PROC_BROWSER_TEST_F(AutoscrollBrowserTest,
                       WheelScrollingWorksAfterAutoscrollCancel) {
  LoadURL(kAutoscrollDataURL);

  // Start autoscroll with middle click.
  SimulateMiddleClick(10, 10, blink::WebInputEvent::kNoModifiers);

  // Without moving the mouse cancel the autoscroll fling with another click.
  SimulateMiddleClick(10, 10, blink::WebInputEvent::kNoModifiers);

  // The mouse wheel scrolling must work after autoscroll cancellation.
  RenderFrameSubmissionObserver observer(
      GetWidgetHost()->render_frame_metadata_provider());
  blink::WebMouseWheelEvent wheel_event =
      blink::SyntheticWebMouseWheelEventBuilder::Build(
          10, 10, 0, -53, 0, ui::ScrollGranularity::kScrollByPrecisePixel);
  wheel_event.phase = blink::WebMouseWheelEvent::kPhaseBegan;
  GetWidgetHost()->ForwardWheelEvent(wheel_event);
  WaitForScroll(observer);
}

// Checks that wheel scrolling does not work once the cursor has entered the
// autoscroll mode.
IN_PROC_BROWSER_TEST_F(AutoscrollBrowserTest,
                       WheelScrollingDoesNotWorkInAutoscrollMode) {
  LoadURL(kAutoscrollDataURL);

  // Start autoscroll with middle click.
  SimulateMiddleClick(10, 10, blink::WebInputEvent::kNoModifiers);

  // Without moving the mouse, start wheel scrolling.
  RenderFrameSubmissionObserver observer(
      GetWidgetHost()->render_frame_metadata_provider());
  blink::WebMouseWheelEvent wheel_event =
      blink::SyntheticWebMouseWheelEventBuilder::Build(
          10, 10, 0, -53, 0, ui::ScrollGranularity::kScrollByPrecisePixel);
  wheel_event.phase = blink::WebMouseWheelEvent::kPhaseBegan;
  GetWidgetHost()->ForwardWheelEvent(wheel_event);

  // Wait for 4 commits, then verify that the page has not scrolled.
  WaitForCommitFrames(4);
  gfx::PointF default_scroll_offset;
  DCHECK_EQ(observer.LastRenderFrameMetadata()
                .root_scroll_offset.value_or(default_scroll_offset)
                .y(),
            0);
}

// Checks that autoscrolling still works after changing the scroll direction
// when the element is fully scrolled.
IN_PROC_BROWSER_TEST_F(AutoscrollBrowserTest,
                       AutoscrollDirectionChangeAfterFullyScrolled) {
  LoadURL(kAutoscrollDataURL);

  // Start autoscroll with middle click.
  auto scroll_begin_watcher = std::make_unique<GestureScrollEventWatcher>(
      GetWidgetHost(), blink::WebInputEvent::Type::kGestureScrollBegin);
  SimulateMiddleClick(100, 100, blink::WebInputEvent::kNoModifiers);

  // Move the mouse up, no scrolling happens since the page is at its extent.
  auto scroll_update_watcher = std::make_unique<InputMsgWatcher>(
      GetWidgetHost(), blink::WebInputEvent::Type::kGestureScrollUpdate);
  blink::WebMouseEvent move_up = blink::SyntheticWebMouseEventBuilder::Build(
      blink::WebInputEvent::Type::kMouseMove, 20, 20,
      blink::WebInputEvent::kNoModifiers);
  move_up.SetTimeStamp(ui::EventTimeForNow());
  move_up.SetPositionInScreen(20, 20);
  GetWidgetHost()->ForwardMouseEvent(move_up);
  scroll_begin_watcher->Wait();
  EXPECT_EQ(blink::mojom::InputEventResultState::kNoConsumerExists,
            scroll_update_watcher->WaitForAck());

  // Wait for 10 commits before changing the scroll direction.
  WaitForCommitFrames(10);

  // Now move the mouse down and wait for the page to scroll. The test will
  // timeout if autoscrolling does not work after direction change.
  RenderFrameSubmissionObserver observer(
      GetWidgetHost()->render_frame_metadata_provider());
  blink::WebMouseEvent move_down = blink::SyntheticWebMouseEventBuilder::Build(
      blink::WebInputEvent::Type::kMouseMove, 180, 180,
      blink::WebInputEvent::kNoModifiers);
  move_down.SetTimeStamp(ui::EventTimeForNow());
  move_down.SetPositionInScreen(180, 180);
  GetWidgetHost()->ForwardMouseEvent(move_down);
  WaitForScroll(observer);
}
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

}  // namespace content
