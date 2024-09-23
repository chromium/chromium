// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <tuple>
#include <utility>

#include "base/auto_reset.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/input/render_widget_host_input_event_router.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_base.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/input/synthetic_smooth_scroll_gesture.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/hit_test_region_observer.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "third_party/blink/public/common/input/synthetic_web_input_event_builders.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "ui/events/event_switches.h"
#include "ui/latency/latency_info.h"

using blink::WebInputEvent;

namespace {

const char kJankyPageURL[] =
    "data:text/html;charset=utf-8,"
    "<!DOCTYPE html>"
    "<meta name='viewport' content='width=device-width'/>"
    "<style>"
    "html, body {"
    "  margin: 0;"
    "}"
    ".spacer { height: 10000px; }"
    "</style>"
    "<div class=spacer></div>"
    "<script>"
    "  function jank(millis)"
    "  {"
    "    var end = performance.now() + millis;"
    "    while(performance.now() < end) {};"
    "    window.mouseMoveCount = 0;"
    "    window.touchMoveCount = 0;"
    "  }"
    "  window.mouseMoveCount = 0;"
    "  window.touchMoveCount = 0;"
    "  document.addEventListener('mousemove', function(e) {"
    "    window.mouseMoveCount++;"
    "    window.lastMouseMoveEvent = e;"
    "  });"
    "  document.addEventListener('touchmove', function (e) {"
    "    window.touchMoveCount++;"
    "    window.lastTouchMoveEvent = e;"
    "  }, {passive: true});"
    "  document.addEventListener('click', function(e) { jank(500); });"
    "  document.title='ready';"
    "</script>";

}  // namespace

namespace content {

class MainThreadEventQueueBrowserTest : public ContentBrowserTest {
 public:
  MainThreadEventQueueBrowserTest() {}

  MainThreadEventQueueBrowserTest(const MainThreadEventQueueBrowserTest&) =
      delete;
  MainThreadEventQueueBrowserTest& operator=(
      const MainThreadEventQueueBrowserTest&) = delete;

  ~MainThreadEventQueueBrowserTest() override {}

  RenderWidgetHostImpl* GetWidgetHost() {
    return RenderWidgetHostImpl::From(shell()
                                          ->web_contents()
                                          ->GetPrimaryMainFrame()
                                          ->GetRenderViewHost()
                                          ->GetWidget());
  }

  void OnSyntheticGestureCompleted(SyntheticGesture::Result result) {
    EXPECT_EQ(SyntheticGesture::GESTURE_FINISHED, result);
  }

 protected:
  void LoadURL(const char* page_data) {
    const GURL data_url(page_data);
    EXPECT_TRUE(NavigateToURL(shell(), data_url));

    RenderWidgetHostImpl* host = GetWidgetHost();
    host->GetView()->SetSize(gfx::Size(400, 400));

    std::u16string ready_title(u"ready");
    TitleWatcher watcher(shell()->web_contents(), ready_title);
    std::ignore = watcher.WaitAndGetTitle();

    HitTestRegionObserver observer(host->GetFrameSinkId());
    observer.WaitForHitTestData();
  }

  void DoMouseMove() {
    // Send a click event to cause some jankiness. This is done via a click
    // event as ExecJs is synchronous.
    SimulateMouseClick(shell()->web_contents(), 0,
                       blink::WebPointerProperties::Button::kLeft);
    auto input_msg_watcher = std::make_unique<InputMsgWatcher>(
        GetWidgetHost(), blink::WebInputEvent::Type::kMouseMove);
    GetWidgetHost()->ForwardMouseEvent(
        blink::SyntheticWebMouseEventBuilder::Build(
            blink::WebInputEvent::Type::kMouseMove, 10, 10, 0));
    GetWidgetHost()->ForwardMouseEvent(
        blink::SyntheticWebMouseEventBuilder::Build(
            blink::WebInputEvent::Type::kMouseMove, 15, 15, 0));
    GetWidgetHost()->ForwardMouseEvent(
        blink::SyntheticWebMouseEventBuilder::Build(
            blink::WebInputEvent::Type::kMouseMove, 20, 25, 0));

    // Runs until we get the InputMsgAck callback.
    EXPECT_EQ(blink::mojom::InputEventResultState::kConsumed,
              input_msg_watcher->WaitForAck());
    EXPECT_EQ(blink::mojom::InputEventResultSource::kMainThread,
              static_cast<blink::mojom::InputEventResultSource>(
                  input_msg_watcher->last_event_ack_source()));

    int mouse_move_count = 0;
    while (mouse_move_count <= 0)
      mouse_move_count = EvalJs(shell(), "window.mouseMoveCount").ExtractInt();
    EXPECT_EQ(1, mouse_move_count);

    EXPECT_EQ(20, EvalJs(shell(), "window.lastMouseMoveEvent.x"));
    EXPECT_EQ(25, EvalJs(shell(), "window.lastMouseMoveEvent.y"));
  }

  void DoTouchMove() {
    blink::SyntheticWebTouchEvent events[4];
    events[0].PressPoint(10, 10);
    events[1].PressPoint(10, 10);
    events[1].MovePoint(0, 20, 20);
    events[2].PressPoint(10, 10);
    events[2].MovePoint(0, 30, 30);
    events[3].PressPoint(10, 10);
    events[3].MovePoint(0, 35, 40);

    // Send a click event to cause some jankiness. This is done via a click
    // event as ExecJs is synchronous.
    SimulateMouseClick(shell()->web_contents(), 0,
                       blink::WebPointerProperties::Button::kLeft);
    auto input_msg_watcher = std::make_unique<InputMsgWatcher>(
        GetWidgetHost(), blink::WebInputEvent::Type::kTouchMove);

    auto* root_view = GetWidgetHost()->GetView();
    auto* input_event_router =
        GetWidgetHost()->delegate()->GetInputEventRouter();
    for (auto& event : events)
      input_event_router->RouteTouchEvent(root_view, &event, ui::LatencyInfo());

    // Runs until we get the InputMsgAck callback.
    EXPECT_EQ(blink::mojom::InputEventResultState::kSetNonBlocking,
              input_msg_watcher->WaitForAck());
    EXPECT_EQ(blink::mojom::InputEventResultSource::kCompositorThread,
              static_cast<blink::mojom::InputEventResultSource>(
                  input_msg_watcher->last_event_ack_source()));

    int touch_move_count = 0;
    while (touch_move_count <= 0)
      touch_move_count = EvalJs(shell(), "window.touchMoveCount").ExtractInt();
    EXPECT_EQ(1, touch_move_count);

    EXPECT_EQ(35,
              EvalJs(shell(), "window.lastTouchMoveEvent.touches[0].pageX"));
    EXPECT_EQ(40,
              EvalJs(shell(), "window.lastTouchMoveEvent.touches[0].pageY"));
  }
};

// Disabled due to flaky test results on Windows (https://crbug.com/805666) and
// Linux (https://crbug.com/1406591).
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX)
#define MAYBE_MouseMove DISABLED_MouseMove
#else
#define MAYBE_MouseMove MouseMove
#endif
IN_PROC_BROWSER_TEST_F(MainThreadEventQueueBrowserTest, MAYBE_MouseMove) {
  LoadURL(kJankyPageURL);
  DoMouseMove();
}

// Disabled on MacOS because it doesn't support touch input.
#if BUILDFLAG(IS_CHROMEOS_LACROS) || BUILDFLAG(IS_MAC)
#define MAYBE_TouchMove DISABLED_TouchMove
#else
#define MAYBE_TouchMove TouchMove
#endif
IN_PROC_BROWSER_TEST_F(MainThreadEventQueueBrowserTest, MAYBE_TouchMove) {
  LoadURL(kJankyPageURL);
  DoTouchMove();
}

}  // namespace content
