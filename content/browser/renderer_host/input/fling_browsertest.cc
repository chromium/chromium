// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "content/browser/renderer_host/input/synthetic_smooth_scroll_gesture.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/hit_test_region_observer.h"
#include "content/shell/browser/shell.h"
#include "content/test/content_browser_test_utils_internal.h"
#include "net/dns/mock_host_resolver.h"
#include "third_party/blink/public/platform/web_input_event.h"
#include "ui/events/base_event_utils.h"

using blink::WebInputEvent;

namespace {

const std::string kBrowserFlingDataURL = R"HTML(
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

const std::string kTouchActionFilterDataURL = R"HTML(
  <!DOCTYPE html>
  <meta name='viewport' content='width=device-width'/>
  <style>
    body {
      height: 10000px;
      touch-action: pan-y;
    }
  </style>
  <script>
    document.title='ready';
  </script>)HTML";
}  // namespace

namespace content {

class BrowserSideFlingBrowserTest : public ContentBrowserTest {
 public:
  BrowserSideFlingBrowserTest() {}
  ~BrowserSideFlingBrowserTest() override {}

  void OnSyntheticGestureCompleted(SyntheticGesture::Result result) {
    EXPECT_EQ(SyntheticGesture::GESTURE_FINISHED, result);
    run_loop_->Quit();
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    SetupCrossSiteRedirector(embedded_test_server());
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    IsolateAllSitesForTesting(command_line);
  }

 protected:
  RenderWidgetHostImpl* GetWidgetHost() {
    return RenderWidgetHostImpl::From(
        shell()->web_contents()->GetRenderViewHost()->GetWidget());
  }

  void SynchronizeThreads() {
    MainThreadFrameObserver observer(GetWidgetHost());
    observer.Wait();
  }

  void LoadURL(const std::string& page_data) {
    const GURL data_url("data:text/html," + page_data);
    NavigateToURL(shell(), data_url);

    RenderWidgetHostImpl* host = GetWidgetHost();
    host->GetView()->SetSize(gfx::Size(400, 400));

    base::string16 ready_title(base::ASCIIToUTF16("ready"));
    TitleWatcher watcher(shell()->web_contents(), ready_title);
    ignore_result(watcher.WaitAndGetTitle());
    SynchronizeThreads();
  }

  void LoadPageWithOOPIF() {
    // navigate main frame to URL.
    GURL main_url(embedded_test_server()->GetURL(
        "a.com", "/frame_tree/scrollable_page_with_positioned_frame.html"));
    EXPECT_TRUE(NavigateToURL(shell(), main_url));

    FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                              ->GetFrameTree()
                              ->root();
    ASSERT_EQ(1U, root->child_count());

    // Navigate oopif to URL.
    FrameTreeNode* iframe_node = root->child_at(0);
    GURL iframe_url(embedded_test_server()->GetURL("b.com", "/tall_page.html"));
    {
      RenderFrameDeletedObserver deleted_observer(
          iframe_node->current_frame_host());
      NavigateFrameToURL(iframe_node, iframe_url);
      deleted_observer.WaitUntilDeleted();
    }

    WaitForHitTestDataOrChildSurfaceReady(iframe_node->current_frame_host());
    FrameTreeVisualizer visualizer;
    ASSERT_EQ(
        " Site A ------------ proxies for B\n"
        "   +--Site B ------- proxies for A\n"
        "Where A = http://a.com/\n"
        "      B = http://b.com/",
        visualizer.DepictFrameTree(root));

    root_view_ = static_cast<RenderWidgetHostViewBase*>(
        root->current_frame_host()->GetRenderWidgetHost()->GetView());
    child_view_ = static_cast<RenderWidgetHostViewBase*>(
        iframe_node->current_frame_host()->GetRenderWidgetHost()->GetView());
  }

  void SimulateTouchscreenFling(
      RenderWidgetHostImpl* render_widget_host,
      RenderWidgetHostImpl* parent_render_widget_host = nullptr,
      const gfx::Vector2dF& fling_velocity = gfx::Vector2dF(0.f, -2000.f)) {
    DCHECK(render_widget_host);
    // Send a GSB to start scrolling sequence. In case of scroll bubbling wait
    // for the parent to receive the GSB before sending the GFS.
    auto input_msg_watcher =
        parent_render_widget_host
            ? std::make_unique<InputMsgWatcher>(
                  parent_render_widget_host,
                  blink::WebInputEvent::kGestureScrollBegin)
            : std::make_unique<InputMsgWatcher>(
                  render_widget_host,
                  blink::WebInputEvent::kGestureScrollBegin);
    blink::WebGestureEvent gesture_scroll_begin(
        blink::WebGestureEvent::kGestureScrollBegin,
        blink::WebInputEvent::kNoModifiers, ui::EventTimeForNow());
    gesture_scroll_begin.SetSourceDevice(blink::kWebGestureDeviceTouchscreen);
    gesture_scroll_begin.data.scroll_begin.delta_hint_units =
        blink::WebGestureEvent::ScrollUnits::kPrecisePixels;
    gesture_scroll_begin.data.scroll_begin.delta_x_hint = fling_velocity.x();
    gesture_scroll_begin.data.scroll_begin.delta_y_hint = fling_velocity.y();
    const gfx::PointF scroll_location_in_widget(1, 1);
    const gfx::PointF scroll_location_in_root =
        child_view_ ? child_view_->TransformPointToRootCoordSpaceF(
                          scroll_location_in_widget)
                    : scroll_location_in_widget;
    const gfx::PointF scroll_location_in_screen =
        child_view_ ? scroll_location_in_root +
                          root_view_->GetViewBounds().OffsetFromOrigin()
                    : scroll_location_in_widget;
    gesture_scroll_begin.SetPositionInWidget(scroll_location_in_widget);
    gesture_scroll_begin.SetPositionInScreen(scroll_location_in_screen);
    render_widget_host->ForwardGestureEvent(gesture_scroll_begin);
    input_msg_watcher->GetAckStateWaitIfNecessary();

    //  Send a GFS.
    blink::WebGestureEvent gesture_fling_start(
        blink::WebGestureEvent::kGestureFlingStart,
        blink::WebInputEvent::kNoModifiers, ui::EventTimeForNow());
    gesture_fling_start.SetSourceDevice(blink::kWebGestureDeviceTouchscreen);
    gesture_fling_start.data.fling_start.velocity_x = fling_velocity.x();
    gesture_fling_start.data.fling_start.velocity_y = fling_velocity.y();
    gesture_fling_start.SetPositionInWidget(scroll_location_in_widget);
    gesture_fling_start.SetPositionInScreen(scroll_location_in_screen);
    render_widget_host->ForwardGestureEvent(gesture_fling_start);
  }

  void SimulateTouchpadFling(
      RenderWidgetHostImpl* render_widget_host,
      RenderWidgetHostImpl* parent_render_widget_host = nullptr,
      const gfx::Vector2dF& fling_velocity = gfx::Vector2dF(0.f, -2000.f)) {
    DCHECK(render_widget_host);
    // Send a wheel event to start scrolling sequence. In case of scroll
    // bubbling wait for the parent to receive the GSB before sending the GFS.
    auto input_msg_watcher =
        parent_render_widget_host
            ? std::make_unique<InputMsgWatcher>(
                  parent_render_widget_host,
                  blink::WebInputEvent::kGestureScrollBegin)
            : std::make_unique<InputMsgWatcher>(
                  render_widget_host,
                  blink::WebInputEvent::kGestureScrollBegin);
    blink::WebMouseWheelEvent wheel_event =
        SyntheticWebMouseWheelEventBuilder::Build(
            10, 10, fling_velocity.x() / 1000, fling_velocity.y() / 1000, 0,
            true);
    wheel_event.phase = blink::WebMouseWheelEvent::kPhaseBegan;
    const gfx::PointF position_in_widget(1, 1);
    const gfx::PointF position_in_root =
        child_view_
            ? child_view_->TransformPointToRootCoordSpaceF(position_in_widget)
            : position_in_widget;
    const gfx::PointF position_in_screen =
        child_view_
            ? position_in_root + root_view_->GetViewBounds().OffsetFromOrigin()
            : position_in_widget;
    wheel_event.SetPositionInWidget(position_in_widget);
    wheel_event.SetPositionInScreen(position_in_screen);
    render_widget_host->ForwardWheelEvent(wheel_event);
    input_msg_watcher->GetAckStateWaitIfNecessary();

    //  Send a GFS.
    blink::WebGestureEvent gesture_fling_start(
        blink::WebGestureEvent::kGestureFlingStart,
        blink::WebInputEvent::kNoModifiers, ui::EventTimeForNow());
    gesture_fling_start.SetSourceDevice(blink::kWebGestureDeviceTouchpad);
    gesture_fling_start.data.fling_start.velocity_x = fling_velocity.x();
    gesture_fling_start.data.fling_start.velocity_y = fling_velocity.y();
    gesture_fling_start.SetPositionInWidget(position_in_widget);
    gesture_fling_start.SetPositionInScreen(position_in_screen);
    render_widget_host->ForwardGestureEvent(gesture_fling_start);
  }

  void WaitForScroll() {
    RenderFrameSubmissionObserver observer(
        GetWidgetHost()->render_frame_metadata_provider());
    gfx::Vector2dF default_scroll_offset;
    // scrollTop > 0 is not enough since the first progressFling is called from
    // FlingController::ProcessGestureFlingStart. Wait for scrollTop to exceed
    // 100 pixels to make sure that ProgressFling has been called through
    // FlingScheduler at least once.
    while (observer.LastRenderFrameMetadata()
               .root_scroll_offset.value_or(default_scroll_offset)
               .y() <= 100) {
      observer.WaitForMetadataChange();
    }
  }

  void GiveItSomeTime(int64_t time_delta_ms = 10) {
    base::RunLoop run_loop;
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(),
        base::TimeDelta::FromMilliseconds(time_delta_ms));
    run_loop.Run();
  }

  void WaitForFrameScroll(FrameTreeNode* frame_node,
                          int target_scroll_offset = 100,
                          bool upward = false) {
    DCHECK(frame_node);
    double scroll_top =
        EvalJs(frame_node->current_frame_host(), "window.scrollY")
            .ExtractDouble();
    // scrollTop > 0 is not enough since the first progressFling is called from
    // FlingController::ProcessGestureFlingStart. Wait for scrollTop to reach
    // target_scroll_offset to make sure that ProgressFling has been called
    // through FlingScheduler at least once.
    while ((upward && scroll_top > target_scroll_offset) ||
           (!upward && scroll_top < target_scroll_offset)) {
      GiveItSomeTime();
      scroll_top = EvalJs(frame_node->current_frame_host(), "window.scrollY")
                       .ExtractDouble();
    }
  }

  FrameTreeNode* GetRootNode() {
    return static_cast<WebContentsImpl*>(shell()->web_contents())
        ->GetFrameTree()
        ->root();
  }

  FrameTreeNode* GetChildNode() {
    FrameTreeNode* root = GetRootNode();
    return root->child_at(0);
  }

  std::unique_ptr<base::RunLoop> run_loop_;
  RenderWidgetHostViewBase* child_view_ = nullptr;
  RenderWidgetHostViewBase* root_view_ = nullptr;

 private:
  DISALLOW_COPY_AND_ASSIGN(BrowserSideFlingBrowserTest);
};

// On Mac we don't have any touchscreen/touchpad fling events (GFS/GFC).
// Instead, the OS keeps sending wheel events when the user lifts their fingers
// from touchpad.
#if !defined(OS_MACOSX)
IN_PROC_BROWSER_TEST_F(BrowserSideFlingBrowserTest, TouchscreenFling) {
  LoadURL(kBrowserFlingDataURL);
  SimulateTouchscreenFling(GetWidgetHost());
  WaitForScroll();
}
IN_PROC_BROWSER_TEST_F(BrowserSideFlingBrowserTest, TouchpadFling) {
  LoadURL(kBrowserFlingDataURL);
  SimulateTouchpadFling(GetWidgetHost());
  WaitForScroll();
}

// Tests that flinging does not continue after navigating to a page that uses
// the same renderer.
IN_PROC_BROWSER_TEST_F(BrowserSideFlingBrowserTest,
                       FlingingStopsAfterNavigation) {
  GURL first_url(embedded_test_server()->GetURL(
      "b.a.com", "/scrollable_page_with_iframe.html"));
  EXPECT_TRUE(NavigateToURL(shell(), first_url));
  SynchronizeThreads();
  SimulateTouchscreenFling(GetWidgetHost());
  WaitForScroll();

  // Navigate to a second page with the same domain.
  GURL second_url(
      embedded_test_server()->GetURL("a.com", "/scrollable_page.html"));
  NavigateToURL(shell(), second_url);
  SynchronizeThreads();

  // Wait for 100ms. Then check that the second page has not scrolled.
  GiveItSomeTime(100);
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetFrameTree()
                            ->root();
  EXPECT_EQ(
      0, EvalJs(root->current_frame_host(), "window.scrollY").ExtractDouble());
}

IN_PROC_BROWSER_TEST_F(BrowserSideFlingBrowserTest, TouchscreenFlingInOOPIF) {
  LoadPageWithOOPIF();
  SimulateTouchscreenFling(child_view_->host());
  WaitForFrameScroll(GetChildNode());
}
IN_PROC_BROWSER_TEST_F(BrowserSideFlingBrowserTest, TouchpadFlingInOOPIF) {
  LoadPageWithOOPIF();
  SimulateTouchpadFling(child_view_->host());
  WaitForFrameScroll(GetChildNode());
}
IN_PROC_BROWSER_TEST_F(BrowserSideFlingBrowserTest,
                       TouchscreenFlingBubblesFromOOPIF) {
  LoadPageWithOOPIF();
  // Scroll the parent down so that it is scrollable upward.
  EXPECT_TRUE(
      ExecJs(GetRootNode()->current_frame_host(), "window.scrollTo(0, 20)"));
  // We expect to have window.scrollY == 20 after scrolling but with zoom for
  // dsf enabled on android we get window.scrollY == 19 (see
  // https://crbug.com/891860).
  WaitForFrameScroll(GetRootNode(), 19);
  SynchronizeThreads();

  // Fling and wait for the parent to scroll upward.
  gfx::Vector2d fling_velocity(0, 2000);
  SimulateTouchscreenFling(child_view_->host(), GetWidgetHost(),
                           fling_velocity);
  WaitForFrameScroll(GetRootNode(), 15, true /* upward */);
}

// Touchpad fling only happens on ChromeOS.
#if defined(CHROMEOS)
IN_PROC_BROWSER_TEST_F(BrowserSideFlingBrowserTest,
                       TouchpadFlingBubblesFromOOPIF) {
  LoadPageWithOOPIF();
  // Scroll the parent down so that it is scrollable upward.
  EXPECT_TRUE(
      ExecJs(GetRootNode()->current_frame_host(), "window.scrollTo(0, 20)"));
  // We expect to have window.scrollY == 20 after scrolling but with zoom for
  // dsf enabled on android we get window.scrollY == 19 (see
  // https://crbug.com/891860).
  WaitForFrameScroll(GetRootNode(), 19);
  SynchronizeThreads();

  // Fling and wait for the parent to scroll upward.
  gfx::Vector2d fling_velocity(0, 2000);
  SimulateTouchpadFling(child_view_->host(), GetWidgetHost(), fling_velocity);
  WaitForFrameScroll(GetRootNode(), 15, true /* upward */);
}
#endif  // defined(CHROMEOS)

IN_PROC_BROWSER_TEST_F(BrowserSideFlingBrowserTest, GFCGetsBubbledFromOOPIF) {
  LoadPageWithOOPIF();
  // Scroll the parent down so that it is scrollable upward.
  EXPECT_TRUE(
      ExecJs(GetRootNode()->current_frame_host(), "window.scrollTo(0, 20)"));
  // We expect to have window.scrollY == 20 after scrolling but with zoom for
  // dsf enabled on android we get window.scrollY == 19 (see
  // https://crbug.com/891860).
  WaitForFrameScroll(GetRootNode(), 19);
  SynchronizeThreads();

  // Fling and wait for the parent to scroll upward.
  gfx::Vector2d fling_velocity(0, 2000);
  SimulateTouchscreenFling(child_view_->host(), GetWidgetHost(),
                           fling_velocity);
  WaitForFrameScroll(GetRootNode(), 15, true /* upward */);

  // Send a GFC to the child and wait for it to get bubbled.
  auto input_msg_watcher = std::make_unique<InputMsgWatcher>(
      GetWidgetHost(), blink::WebInputEvent::kGestureFlingCancel);
  blink::WebGestureEvent gesture_fling_cancel(
      blink::WebGestureEvent::kGestureFlingCancel,
      blink::WebInputEvent::kNoModifiers, ui::EventTimeForNow());
  gesture_fling_cancel.SetSourceDevice(blink::kWebGestureDeviceTouchscreen);

  const gfx::PointF location_in_widget(1, 1);
  const gfx::PointF location_in_root =
      child_view_->TransformPointToRootCoordSpaceF(location_in_widget);
  const gfx::PointF location_in_screen =
      location_in_root + root_view_->GetViewBounds().OffsetFromOrigin();
  gesture_fling_cancel.SetPositionInWidget(location_in_widget);
  gesture_fling_cancel.SetPositionInScreen(location_in_screen);
  child_view_->host()->ForwardGestureEvent(gesture_fling_cancel);
  input_msg_watcher->GetAckStateWaitIfNecessary();
}

// Flaky, see https://crbug.com/850455
#define MAYBE_ScrollEndGeneratedForFilteredFling \
  DISABLED_ScrollEndGeneratedForFilteredFling
IN_PROC_BROWSER_TEST_F(BrowserSideFlingBrowserTest,
                       MAYBE_ScrollEndGeneratedForFilteredFling) {
  LoadURL(kTouchActionFilterDataURL);

  // Necessary for checking the ACK source of the sent events. The events are
  // filtered when the Browser is the source.
  auto scroll_begin_watcher = std::make_unique<InputMsgWatcher>(
      GetWidgetHost(), blink::WebInputEvent::kGestureScrollBegin);
  auto fling_start_watcher = std::make_unique<InputMsgWatcher>(
      GetWidgetHost(), blink::WebInputEvent::kGestureFlingStart);
  auto scroll_end_watcher = std::make_unique<InputMsgWatcher>(
      GetWidgetHost(), blink::WebInputEvent::kGestureScrollEnd);

  // Do a horizontal touchscreen scroll followed by a fling. The GFS must get
  // filtered since the GSB is filtered.
  SyntheticSmoothScrollGestureParams params;
  params.gesture_source_type = SyntheticGestureParams::TOUCH_INPUT;
  params.anchor = gfx::PointF(10, 10);
  params.distances.push_back(gfx::Vector2d(-60, 0));
  params.prevent_fling = false;

  run_loop_ = std::make_unique<base::RunLoop>();

  std::unique_ptr<SyntheticSmoothScrollGesture> gesture(
      new SyntheticSmoothScrollGesture(params));
  GetWidgetHost()->QueueSyntheticGesture(
      std::move(gesture),
      base::BindOnce(&BrowserSideFlingBrowserTest::OnSyntheticGestureCompleted,
                     base::Unretained(this)));

  // Runs until we get the OnSyntheticGestureCompleted callback.
  run_loop_->Run();

  scroll_begin_watcher->GetAckStateWaitIfNecessary();
  EXPECT_EQ(InputEventAckSource::BROWSER,
            scroll_begin_watcher->last_event_ack_source());

  fling_start_watcher->GetAckStateWaitIfNecessary();
  EXPECT_EQ(InputEventAckSource::BROWSER,
            fling_start_watcher->last_event_ack_source());

  // Since the GFS is filtered. the input_router_impl will generate and forward
  // a GSE to make sure that the scrolling sequence and the touch action filter
  // state get reset properly. The generated GSE will also get filtered since
  // its equivalent GSB is filtered. The test will timeout if the GSE is not
  // generated.
  scroll_end_watcher->GetAckStateWaitIfNecessary();
  EXPECT_EQ(InputEventAckSource::BROWSER,
            scroll_end_watcher->last_event_ack_source());
}
#endif  // !defined(OS_MACOSX)

}  // namespace content
