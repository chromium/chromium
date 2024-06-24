// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/auto_reset.h"
#include "base/command_line.h"
#include "base/location.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "components/input/render_widget_host_input_event_router.h"
#include "content/browser/gpu/compositor_util.h"
#include "content/browser/renderer_host/render_widget_host_delegate.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/hit_test_region_observer.h"
#include "content/shell/browser/shell.h"
#include "third_party/blink/public/common/input/synthetic_web_input_event_builders.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "ui/latency/latency_info.h"

using blink::WebInputEvent;

namespace {

const char kTouchEventDataURL[] =
    "data:text/html;charset=utf-8,"
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
    "<head>"
    "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">"
    "</head>"
#endif
    "<body onload='setup();'>"
    "  <div id='first'></div><div id='second'></div><div id='third'></div>"
    "</body>"
    "<style>"
    " div {"
    "    position: absolute;"
    "    width: 100px;"
    "    height: 100px;"
    "    -webkit-transform: translate3d(0, 0, 0);"
    " }"
    "  %23first {"
    "    top: 0px;"
    "    left: 0px;"
    "    background-color: green;"
    "  }"
    "  %23second {"
    "    top: 0px;"
    "    left: 110px;"
    "    background-color: blue;"
    "  }"
    "  %23third {"
    "    top: 110px;"
    "    left: 0px;"
    "    background-color: yellow;"
    "  }"
    "</style>"
    "<script>"
    "  function setup() {"
    "    second.ontouchstart = () => {};"
    "    third.ontouchstart = e => e.preventDefault();"
    "  }"
    "</script>";

}  // namespace

namespace content {

class TouchInputBrowserTest : public ContentBrowserTest {
 public:
  TouchInputBrowserTest() {}
  ~TouchInputBrowserTest() override {}

  RenderWidgetHostImpl* GetWidgetHost() {
    return RenderWidgetHostImpl::From(shell()
                                          ->web_contents()
                                          ->GetPrimaryMainFrame()
                                          ->GetRenderViewHost()
                                          ->GetWidget());
  }

  std::unique_ptr<InputMsgWatcher> AddFilter(blink::WebInputEvent::Type type) {
    return std::make_unique<InputMsgWatcher>(GetWidgetHost(), type);
  }

 protected:
  void SendTouchEvent(blink::SyntheticWebTouchEvent* event) {
    auto* root_view = GetWidgetHost()->GetView();
    auto* input_event_router =
        GetWidgetHost()->delegate()->GetInputEventRouter();
    input_event_router->RouteTouchEvent(root_view, event, ui::LatencyInfo());

    event->ResetPoints();
  }
  void LoadURL() {
    const GURL data_url(kTouchEventDataURL);
    EXPECT_TRUE(NavigateToURL(shell(), data_url));

    RenderWidgetHostImpl* host = GetWidgetHost();
    // Wait to confirm a frame was generated from the navigation.
    RenderFrameSubmissionObserver frame_observer(
        host->render_frame_metadata_provider());
    frame_observer.WaitForMetadataChange();

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
    // On non mobile profiles, set a size for the view, and wait for a new frame
    // to be generated at that size. On Android and iOS the size is specified in
    // kTouchEventDataURL.
    host->GetView()->SetSize(gfx::Size(400, 400));
    frame_observer.WaitForAnyFrameSubmission();
#endif

    HitTestRegionObserver observer(host->GetFrameSinkId());
    observer.WaitForHitTestData();
  }

  void SetUpCommandLine(base::CommandLine* cmd) override {
    cmd->AppendSwitchASCII(switches::kTouchEventFeatureDetection,
                           switches::kTouchEventFeatureDetectionEnabled);
  }
};

IN_PROC_BROWSER_TEST_F(TouchInputBrowserTest, TouchNoHandler) {
  LoadURL();
  blink::SyntheticWebTouchEvent touch;

  // A press on |first| should be acked with NO_CONSUMER_EXISTS since there is
  // no touch-start handler on it.
  touch.PressPoint(25, 25);
  auto filter = AddFilter(WebInputEvent::Type::kTouchStart);
  SendTouchEvent(&touch);

  EXPECT_EQ(blink::mojom::InputEventResultState::kNoConsumerExists,
            filter->WaitForAck());

  // The same is true for release because there is no touch-end handler.
  filter = AddFilter(WebInputEvent::Type::kTouchEnd);
  touch.ReleasePoint(0);
  SendTouchEvent(&touch);
  EXPECT_EQ(blink::mojom::InputEventResultState::kNotConsumed,
            filter->WaitForAck());
}

IN_PROC_BROWSER_TEST_F(TouchInputBrowserTest, TouchStartNoConsume) {
  LoadURL();
  blink::SyntheticWebTouchEvent touch;

  // Press on |second| should be acked with NOT_CONSUMED since there is a
  // touch-start handler on |second|, but it doesn't consume the event.
  touch.PressPoint(125, 25);
  auto filter = AddFilter(WebInputEvent::Type::kTouchStart);
  SendTouchEvent(&touch);
  EXPECT_EQ(blink::mojom::InputEventResultState::kNotConsumed,
            filter->WaitForAck());

  // Even though there is no touch-end handler there, the touch-end is still
  // dispatched for state consistency in downstream event-path.  That event
  // should be acked with NOT_CONSUMED.
  filter = AddFilter(WebInputEvent::Type::kTouchEnd);
  touch.ReleasePoint(0);
  SendTouchEvent(&touch);
  EXPECT_EQ(blink::mojom::InputEventResultState::kNotConsumed,
            filter->WaitForAck());
}

IN_PROC_BROWSER_TEST_F(TouchInputBrowserTest, TouchStartConsume) {
  LoadURL();
  blink::SyntheticWebTouchEvent touch;

  // Press on |third| should be acked with CONSUMED since the touch-start
  // handler there consumes the event.
  touch.PressPoint(25, 125);
  auto filter = AddFilter(WebInputEvent::Type::kTouchStart);
  SendTouchEvent(&touch);
  EXPECT_EQ(blink::mojom::InputEventResultState::kConsumed,
            filter->WaitForAck());

  // Even though there is no touch-end handler there, the touch-end is still
  // dispatched for state consistency in downstream event-path.  That event
  // should be acked with NOT_CONSUMED.
  touch.ReleasePoint(0);
  filter = AddFilter(WebInputEvent::Type::kTouchEnd);
  SendTouchEvent(&touch);
  EXPECT_EQ(blink::mojom::InputEventResultState::kNotConsumed,
            filter->WaitForAck());
}

IN_PROC_BROWSER_TEST_F(TouchInputBrowserTest, MultiPointTouchPress) {
  LoadURL();
  blink::SyntheticWebTouchEvent touch;

  // Press on |first|, which sould be acked with NO_CONSUMER_EXISTS. Then press
  // on |third|. That point should be acked with CONSUMED.
  touch.PressPoint(25, 25);
  auto filter = AddFilter(WebInputEvent::Type::kTouchStart);
  SendTouchEvent(&touch);
  EXPECT_EQ(blink::mojom::InputEventResultState::kNoConsumerExists,
            filter->WaitForAck());

  touch.PressPoint(25, 125);
  filter = AddFilter(WebInputEvent::Type::kTouchStart);
  SendTouchEvent(&touch);
  EXPECT_EQ(blink::mojom::InputEventResultState::kConsumed,
            filter->WaitForAck());
}

}  // namespace content
