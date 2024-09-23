// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string_view>

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "build/build_config.h"
#include "components/input/render_widget_host_input_event_router.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/hit_test_region_observer.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/shell/browser/shell.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/input/synthetic_web_input_event_builders.h"
#include "third_party/blink/public/common/input/web_gesture_event.h"

namespace content {
namespace {

class AnchorElementInteractionBrowserTest : public ContentBrowserTest {
 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(
        switches::kEnableExperimentalWebPlatformFeatures);
  }

  void SetUpOnMainThread() override {
    embedded_test_server()->RegisterRequestMonitor(base::BindLambdaForTesting(
        [this](const net::test_server::HttpRequest& request) {
          if (next_request_callback_) {
            std::move(next_request_callback_).Run(request);
          }
        }));
    ASSERT_TRUE(test_server_handle_ =
                    embedded_test_server()->StartAndReturnHandle());
    ContentBrowserTest::SetUpOnMainThread();
  }

  void TearDownOnMainThread() override {
    ASSERT_TRUE(embedded_test_server()->ShutdownAndWaitUntilComplete());
  }

  RenderWidgetHostImpl* GetWidgetHost() const {
    return RenderWidgetHostImpl::From(
        shell()->web_contents()->GetPrimaryMainFrame()->GetRenderWidgetHost());
  }

  net::test_server::HttpRequest AwaitNextRequest() {
    base::RunLoop run_loop;
    net::test_server::HttpRequest next_request;
    next_request_callback_ = base::BindLambdaForTesting(
        [&](const net::test_server::HttpRequest& request) {
          next_request = request;
          run_loop.Quit();
        });
    run_loop.Run();
    return next_request;
  }

  net::test_server::EmbeddedTestServerHandle test_server_handle_;
  base::OnceCallback<void(const net::test_server::HttpRequest&)>
      next_request_callback_;
};

struct TestScriptOptions {
  gfx::Rect link_area;
  std::string_view eagerness = "conservative";
};

std::string MakeTestScript(const TestScriptOptions& options = {}) {
  return JsReplace(R"(
    let a = document.createElement('a');
    a.href = 'title2.html';
    Object.assign(a.style, {
      display: 'block',
      position: 'absolute',
      left: $1 + 'px',
      top: $2 + 'px',
      width: $3 + 'px',
      height: $4 + 'px',
    });
    document.body.appendChild(a);
    let script = document.createElement('script');
    script.type = 'speculationrules';
    script.textContent = JSON.stringify({prefetch: [
      {source: 'document', eagerness: $5}
    ]});
    document.head.appendChild(script);
    document.head.appendChild(Object.assign(
      document.createElement('meta'),
      {name: 'viewport', content: 'width=device-width, initial-scale=1'}));
    // Double-RAF. We need to ensure layout and style are clean (so document
    // rules are current).
    new Promise(resolve => {
      requestAnimationFrame(() => requestAnimationFrame(() => resolve()));
    });
  )",
                   options.link_area.x(), options.link_area.y(),
                   options.link_area.width(), options.link_area.height(),
                   options.eagerness);
}

// End-to-end test that document rules can cause prefetch on mouse down.
IN_PROC_BROWSER_TEST_F(AnchorElementInteractionBrowserTest, MouseDownPrefetch) {
  ASSERT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL("/title1.html")));
  ASSERT_TRUE(ExecJs(shell()->web_contents(),
                     MakeTestScript({.link_area = {0, 0, 100, 100}})));

  auto* widget = GetWidgetHost();
  MainThreadFrameObserver(widget).Wait();
  blink::WebMouseEvent mouse_events[] = {
      blink::SyntheticWebMouseEventBuilder::Build(
          blink::WebInputEvent::Type::kMouseDown, 50, 50, 0),
      blink::SyntheticWebMouseEventBuilder::Build(
          blink::WebInputEvent::Type::kMouseUp, 50, 50, 0),
  };
  for (auto& event : mouse_events) {
    event.button = blink::WebMouseEvent::Button::kLeft;
    event.click_count = 1;
  }

  widget->ForwardMouseEvent(mouse_events[0]);
  net::test_server::HttpRequest prefetch_request = AwaitNextRequest();
  EXPECT_EQ(prefetch_request.relative_url, "/title2.html");
  EXPECT_EQ(prefetch_request.headers["sec-purpose"], "prefetch");

  TestNavigationObserver navigation_observer(shell()->web_contents());
  widget->ForwardMouseEvent(mouse_events[1]);
  navigation_observer.Wait();
  EXPECT_EQ(
      "navigational-prefetch",
      EvalJs(shell()->web_contents(),
             "performance.getEntriesByType('navigation')[0].deliveryType"));
}

// End-to-end test that document rules can cause prefetch on mouse hover.
IN_PROC_BROWSER_TEST_F(AnchorElementInteractionBrowserTest,
                       MouseHoverPrefetch) {
  ASSERT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL("/title1.html")));
  ASSERT_TRUE(ExecJs(shell()->web_contents(),
                     MakeTestScript({.link_area = {0, 0, 100, 100},
                                     .eagerness = "moderate"})));

  auto* widget = GetWidgetHost();
  MainThreadFrameObserver(widget).Wait();

  SimulateMouseEvent(shell()->web_contents(),
                     blink::WebInputEvent::Type::kMouseMove, {50, 50});
  net::test_server::HttpRequest prefetch_request = AwaitNextRequest();
  EXPECT_EQ(prefetch_request.relative_url, "/title2.html");
  EXPECT_EQ(prefetch_request.headers["sec-purpose"], "prefetch");

  TestNavigationObserver navigation_observer(shell()->web_contents());
  SimulateMouseClickAt(shell()->web_contents(), 0,
                       blink::WebMouseEvent::Button::kLeft, {50, 50});
  navigation_observer.Wait();
  EXPECT_EQ(
      "navigational-prefetch",
      EvalJs(shell()->web_contents(),
             "performance.getEntriesByType('navigation')[0].deliveryType"));
}

// Touch events are not supported on macOS.
#if !BUILDFLAG(IS_MAC)

// End-to-end test that document rules can cause prefetch on touch down.
IN_PROC_BROWSER_TEST_F(AnchorElementInteractionBrowserTest, TouchDownPrefetch) {
  ASSERT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL("/title1.html")));
  ASSERT_TRUE(ExecJs(shell()->web_contents(),
                     MakeTestScript({.link_area = {0, 0, 100, 100}})));

  auto* widget = GetWidgetHost();
  MainThreadFrameObserver(widget).Wait();
  auto* view = widget->GetView();
  auto* router = widget->delegate()->GetInputEventRouter();
  blink::SyntheticWebTouchEvent touch_event;

  touch_event.PressPoint(50, 50);
  router->RouteTouchEvent(view, &touch_event, ui::LatencyInfo());
  net::test_server::HttpRequest prefetch_request = AwaitNextRequest();
  EXPECT_EQ(prefetch_request.relative_url, "/title2.html");
  EXPECT_EQ(prefetch_request.headers["sec-purpose"], "prefetch");

  // The synthetic click originates from the gesture recognizer's tap gesture,
  // not the touch end.
  TestNavigationObserver navigation_observer(shell()->web_contents());
  SimulateTapDownAt(shell()->web_contents(), {50, 50});
  touch_event.ReleasePoint(0);
  router->RouteTouchEvent(view, &touch_event, ui::LatencyInfo());
  SimulateTapAt(shell()->web_contents(), {50, 50});
  navigation_observer.Wait();
  EXPECT_EQ(
      "navigational-prefetch",
      EvalJs(shell()->web_contents(),
             "performance.getEntriesByType('navigation')[0].deliveryType"));
}

#endif  // !BUILDFLAG(IS_MAC)

}  // namespace
}  // namespace content
