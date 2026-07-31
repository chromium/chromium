// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "build/build_config.h"
#include "cc/base/features.h"
#include "components/viz/common/frame_sinks/copy_output_result.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/renderer_host/unbounded_surface_window.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/input/synthetic_gesture_target.h"
#include "content/common/input/synthetic_pointer_driver.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/hit_test_region_observer.h"
#include "content/public/test/no_renderer_crashes_assertion.h"
#include "content/shell/browser/shell.h"
#include "content/test/content_browser_test_utils_internal.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "url/gurl.h"

#if defined(USE_AURA)
#include "ui/aura/window.h"
#include "ui/aura/window_tracker.h"
#endif

#if BUILDFLAG(IS_OZONE)
#include "ui/ozone/public/ozone_platform.h"
#endif

namespace content {

class UnboundedElementBrowserTestBase : public ContentBrowserTest {
 public:
  UnboundedElementBrowserTestBase() = default;
  ~UnboundedElementBrowserTestBase() override = default;

 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    ContentBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(
        switches::kTouchEventFeatureDetection,
        switches::kTouchEventFeatureDetectionEnabled);
  }

  void SetUpOnMainThread() override {
    ContentBrowserTest::SetUpOnMainThread();
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  void TearDownOnMainThread() override {
    if (base::FeatureList::IsEnabled(blink::features::kUnboundedElement)) {
      UnboundedSurfaceWindow* window =
          primary_main_frame_host()->GetUnboundedSurfaceWindow();
      if (window) {
        auto tracker = CreateDestructionTracker(*window);
        window->Dismiss();
        WaitForDestruction(std::move(tracker));
      }
    }
    ContentBrowserTest::TearDownOnMainThread();
  }

#if defined(USE_AURA)
  // Native widget destruction is async with Aura, hence we define helpers to
  // wait for destruction.
  std::unique_ptr<aura::WindowTracker> CreateDestructionTracker(
      UnboundedSurfaceWindow& window) {
    auto tracker = std::make_unique<aura::WindowTracker>();
    gfx::NativeWindow native_window = window.GetNativeWindow();
    if (!native_window) {
      return tracker;
    }
    tracker->Add(native_window);
#if BUILDFLAG(IS_WIN)
    // Explicitly wait for the top-level window to be destroyed on windows, to
    // avoid the test runner's leak detection check from failing.
    if (native_window->parent()) {
      tracker->Add(native_window->parent());
    }
#endif
    return tracker;
  }

  void WaitForDestruction(std::unique_ptr<aura::WindowTracker> tracker) {
    EXPECT_TRUE(
        base::test::RunUntil([&]() { return tracker->windows().empty(); }));
  }
#else
  // Stub implementations for non-Aura.
  int CreateDestructionTracker(UnboundedSurfaceWindow& window) { return 0; }
  void WaitForDestruction(int tracker) { return; }
#endif  // defined(USE_AURA)

  WebContentsImpl* web_contents() const {
    return static_cast<WebContentsImpl*>(shell()->web_contents());
  }

  RenderFrameHostImpl* primary_main_frame_host() {
    return web_contents()->GetPrimaryFrameTree().root()->current_frame_host();
  }

  void WaitForFrameReady() {
    WaitForHitTestData(primary_main_frame_host());
    MainThreadFrameObserver frame_observer(
        primary_main_frame_host()->GetRenderWidgetHost());
    frame_observer.Wait();
  }
};

class UnboundedElementBrowserTest : public UnboundedElementBrowserTestBase {
 public:
  UnboundedElementBrowserTest() = default;
  ~UnboundedElementBrowserTest() override = default;
  void SetUp() override {
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
    // TODO(crbug.com/508672616): Not yet implemented on Android/iOS.
    GTEST_SKIP();
#else
    feature_list_.InitWithFeatures(
        {blink::features::kUnboundedElement,
         blink::features::kUnboundedElementOnTheOpenWeb},
        {::features::kTreesInViz});
    UnboundedElementBrowserTestBase::SetUp();
#endif
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(UnboundedElementBrowserTest, ActivationPreconditions) {
  GURL url(embedded_test_server()->GetURL("/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));

  // Create an unbounded element via HTML snippet:
  std::string script = R"(
    document.body.innerHTML = `
      <meta name="viewport" content="width=device-width, initial-scale=1">
      <div id="target" unbounded></div>
    `;
    document.getElementById('target').showUnboundedElement().catch(e => e.name);
  )";
  // showUnboundedElement throws DOMException NotAllowedError without transient
  // user gesture.
  EXPECT_EQ("NotAllowedError", EvalJs(primary_main_frame_host(), script,
                                      EXECUTE_SCRIPT_NO_USER_GESTURE));
}

IN_PROC_BROWSER_TEST_F(UnboundedElementBrowserTest,
                       WebUIPrivilegedBypassesUserActivation) {
  GURL webui_url = GURL(std::string(kChromeUIScheme) + "://" +
                        std::string(kChromeUIGpuHost));
  EXPECT_TRUE(NavigateToURL(shell(), webui_url));

  // Use DOM APIs instead of innerHTML because WebUI pages enforce TrustedTypes
  // with a strict CSP ("trusted-types static-types;") that disallows creating
  // custom policies in test scripts.
  std::string script = R"(
    const meta = document.createElement('meta');
    meta.name = 'viewport';
    meta.content = 'width=device-width, initial-scale=1';
    document.head.appendChild(meta);

    const div = document.createElement('div');
    div.id = 'target';
    div.setAttribute('unbounded', '');
    div.style.width = '100px';
    div.style.height = '100px';
    document.body.appendChild(div);

    div.showUnboundedElement().then(() => "Success", e => e.name);
  )";
  // Since it's a privileged WebUI page, it should bypass the transient user
  // activation requirement.
  EXPECT_EQ("Success", EvalJs(primary_main_frame_host(), script,
                              EXECUTE_SCRIPT_NO_USER_GESTURE));
}

IN_PROC_BROWSER_TEST_F(UnboundedElementBrowserTest, AncestorClipping) {
  GURL url(embedded_test_server()->GetURL("/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));

  std::string setup_script = R"(
    document.body.innerHTML = `
      <meta name="viewport" content="width=device-width, initial-scale=1">
      <div id="container" style="width:50px; height:50px; overflow:hidden;
           position:relative;">
        <div id="child" style="width:100px; height:100px; position:absolute;
             top:0; left:0;" unbounded></div>
      </div>
    `;
    const child = document.getElementById('child');
    child.addEventListener('mousedown', () => { window.__clicked = true; });
    child.showUnboundedElement();
  )";
  EXPECT_TRUE(ExecJs(primary_main_frame_host(), setup_script));
  WaitForFrameReady();

  SimulateMouseClickAt(web_contents(), 0, blink::WebMouseEvent::Button::kLeft,
                       gfx::Point(75, 75));
  RunUntilInputProcessed(primary_main_frame_host()->GetRenderWidgetHost());

  EXPECT_TRUE(
      EvalJs(primary_main_frame_host(), "window.__clicked").ExtractBool());
}

IN_PROC_BROWSER_TEST_F(UnboundedElementBrowserTest, InputEventRouting) {
  GURL url(embedded_test_server()->GetURL("/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));

  std::string script = R"(
    document.body.innerHTML = `
      <meta name="viewport" content="width=device-width, initial-scale=1">
      <div id="child" style="width:100px; height:100px;" unbounded></div>
    `;
    const div = document.getElementById('child');
    div.addEventListener('mousemove', (e) => {
      window.__mouse_x = e.clientX;
      window.__mouse_y = e.clientY;
    });
    div.showUnboundedElement();
  )";
  EXPECT_TRUE(ExecJs(primary_main_frame_host(), script));
  WaitForFrameReady();

  SimulateMouseEvent(web_contents(), blink::WebInputEvent::Type::kMouseMove,
                     gfx::Point(50, 50));
  RunUntilInputProcessed(primary_main_frame_host()->GetRenderWidgetHost());

  EXPECT_EQ(50, EvalJs(primary_main_frame_host(), "window.__mouse_x"));
  EXPECT_EQ(50, EvalJs(primary_main_frame_host(), "window.__mouse_y"));
}

IN_PROC_BROWSER_TEST_F(UnboundedElementBrowserTest, InputEventRoutingTouch) {
  GURL url(embedded_test_server()->GetURL("/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));

  std::string script = R"(
    document.body.innerHTML = `
      <meta name="viewport" content="width=device-width, initial-scale=1">
      <div id="child" style="width:100px; height:100px;" unbounded></div>
    `;
    const div = document.getElementById('child');
    div.addEventListener('touchstart', (e) => {
      window.__touch_x = e.touches[0].clientX;
      window.__touch_y = e.touches[0].clientY;
    });
    div.showUnboundedElement();
  )";
  EXPECT_TRUE(ExecJs(primary_main_frame_host(), script));
  WaitForFrameReady();

  std::unique_ptr<SyntheticPointerDriver> synthetic_pointer_driver =
      SyntheticPointerDriver::Create(
          content::mojom::GestureSourceType::kTouchInput,
          /*from_devtools_debugger=*/true);
  RenderWidgetHostImpl* render_widget_host =
      primary_main_frame_host()->GetRenderWidgetHost();
  auto* root_view = render_widget_host->GetView()->GetRootView();
  std::unique_ptr<SyntheticGestureTarget> synthetic_gesture_target =
      root_view ? root_view->CreateSyntheticGestureTarget()
                : render_widget_host->GetView()->CreateSyntheticGestureTarget();

  synthetic_pointer_driver->Press(50, 50, 0,
                                  SyntheticPointerActionParams::Button::LEFT);
  synthetic_pointer_driver->DispatchEvent(synthetic_gesture_target.get(),
                                          base::TimeTicks::Now());

  RunUntilInputProcessed(render_widget_host);

  EXPECT_EQ(50, EvalJs(primary_main_frame_host(), "window.__touch_x"));
  EXPECT_EQ(50, EvalJs(primary_main_frame_host(), "window.__touch_y"));
}

IN_PROC_BROWSER_TEST_F(UnboundedElementBrowserTest, LightDismissEscKey) {
  GURL url(embedded_test_server()->GetURL("/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));

  std::string script = R"(
    document.body.innerHTML = `
      <meta name="viewport" content="width=device-width, initial-scale=1">
      <div id="target" style="width:50px; height:50px;" unbounded></div>
    `;
    document.getElementById('target').showUnboundedElement();
  )";
  ASSERT_TRUE(ExecJs(primary_main_frame_host(), script));
  WaitForFrameReady();

  std::string get_style =
      "getComputedStyle(document.getElementById('target')).visibility";
  EXPECT_EQ("visible", EvalJs(primary_main_frame_host(), get_style));

  UnboundedSurfaceWindow* window =
      primary_main_frame_host()->GetUnboundedSurfaceWindow();
  ASSERT_TRUE(window);
  auto tracker = CreateDestructionTracker(*window);

  SimulateKeyPress(web_contents(), ui::DomKey::ESCAPE, ui::DomCode::ESCAPE,
                   ui::VKEY_ESCAPE, false, false, false, false);
  RunUntilInputProcessed(primary_main_frame_host()->GetRenderWidgetHost());

  EXPECT_EQ("hidden", EvalJs(primary_main_frame_host(), get_style));

  WaitForDestruction(std::move(tracker));
}

IN_PROC_BROWSER_TEST_F(UnboundedElementBrowserTest, LightDismissClickOutside) {
  GURL url(embedded_test_server()->GetURL("/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));

  std::string script = R"(
    document.body.innerHTML = `
      <meta name="viewport" content="width=device-width, initial-scale=1">
      <div id="target" style="width:50px; height:50px;" unbounded></div>
    `;
    document.getElementById('target').showUnboundedElement();
  )";
  ASSERT_TRUE(ExecJs(primary_main_frame_host(), script));
  WaitForFrameReady();

  std::string get_style =
      "getComputedStyle(document.getElementById('target')).visibility";
  EXPECT_EQ("visible", EvalJs(primary_main_frame_host(), get_style));

  UnboundedSurfaceWindow* window =
      primary_main_frame_host()->GetUnboundedSurfaceWindow();
  ASSERT_TRUE(window);
  auto tracker = CreateDestructionTracker(*window);

  SimulateMouseClickAt(web_contents(), 0, blink::WebMouseEvent::Button::kLeft,
                       gfx::Point(300, 300));
  RunUntilInputProcessed(primary_main_frame_host()->GetRenderWidgetHost());
  EXPECT_EQ("hidden", EvalJs(primary_main_frame_host(), get_style));

  WaitForDestruction(std::move(tracker));
}

IN_PROC_BROWSER_TEST_F(UnboundedElementBrowserTest, PopoverInsideUnbounded) {
  GURL url(embedded_test_server()->GetURL("/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));

  std::string script = R"(
    document.body.innerHTML = `
      <meta name="viewport" content="width=device-width, initial-scale=1">
      <div id="child" style="width:100px; height:100px;" unbounded>
        <div id="popover" popover>Nested Popover</div>
      </div>
    `;
    document.getElementById('child').showUnboundedElement().then(() => {
      document.getElementById('popover').showPopover();
    });
  )";
  ASSERT_TRUE(ExecJs(primary_main_frame_host(), script));
  WaitForFrameReady();

  EXPECT_TRUE(EvalJs(primary_main_frame_host(),
                     "document.getElementById('popover')"
                     ".matches(':popover-open')")
                  .ExtractBool());
}

IN_PROC_BROWSER_TEST_F(UnboundedElementBrowserTest, CompositorPopupAllocation) {
  GURL url(embedded_test_server()->GetURL("/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));

  std::string script = R"(
    document.body.innerHTML = `
      <meta name="viewport" content="width=device-width, initial-scale=1">
      <div id="target" style="width:100px; height:100px;" unbounded></div>
    `;
    document.getElementById('target').showUnboundedElement();
  )";
  EXPECT_TRUE(ExecJs(primary_main_frame_host(), script));
  WaitForFrameReady();

  UnboundedSurfaceWindow* window =
      primary_main_frame_host()->GetUnboundedSurfaceWindow();
  ASSERT_TRUE(window);
  gfx::Rect bounds = window->GetBounds();
  EXPECT_EQ(100, bounds.width());
  EXPECT_EQ(100, bounds.height());
}

IN_PROC_BROWSER_TEST_F(UnboundedElementBrowserTest, VisualOverflowBounds) {
  GURL url(embedded_test_server()->GetURL("/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));

  std::string script = R"(
    document.body.innerHTML = `
      <meta name="viewport" content="width=device-width, initial-scale=1">
      <div id="target" style="width:100px; height:100px;
           filter:drop-shadow(50px 50px 0px green);" unbounded></div>
    `;
    document.getElementById('target').showUnboundedElement();
  )";
  EXPECT_TRUE(ExecJs(primary_main_frame_host(), script));
  WaitForFrameReady();

  UnboundedSurfaceWindow* window =
      primary_main_frame_host()->GetUnboundedSurfaceWindow();
  ASSERT_TRUE(window);
  gfx::Rect bounds = window->GetBounds();
  EXPECT_EQ(150, bounds.width());
  EXPECT_EQ(150, bounds.height());
}

IN_PROC_BROWSER_TEST_F(UnboundedElementBrowserTest,
                       RequestWithEmptyBoundsThrowsException) {
  GURL url(embedded_test_server()->GetURL("/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));

  // Execute script that calls showUnboundedElement on an element with empty
  // bounds and catches the exception name.
  std::string script = R"(
    document.body.innerHTML = `
      <meta name="viewport" content="width=device-width, initial-scale=1">
      <div id="target" style="width:0; height:0;" unbounded></div>
    `;
    document.getElementById('target').showUnboundedElement()
        .then(() => "Success", e => e.name);
  )";
  EXPECT_EQ("NotSupportedError", EvalJs(primary_main_frame_host(), script));
}

IN_PROC_BROWSER_TEST_F(UnboundedElementBrowserTest,
                       RequestWithoutAttributeThrowsException) {
  GURL url(embedded_test_server()->GetURL("/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));

  // Execute script that calls showUnboundedElement on an element
  // without the 'unbounded' attribute and catches the exception name.
  std::string script = R"(
    document.body.innerHTML = `
      <meta name="viewport" content="width=device-width, initial-scale=1">
      <div id="target" style="width:100px; height:100px;"></div>
    `;
    document.getElementById('target').showUnboundedElement()
        .then(() => "Success", e => e.name);
  )";
  EXPECT_EQ("InvalidStateError", EvalJs(primary_main_frame_host(), script));
}

class UnboundedElementHighDPIBrowserTest : public UnboundedElementBrowserTest {
 public:
  UnboundedElementHighDPIBrowserTest() = default;
  ~UnboundedElementHighDPIBrowserTest() override = default;

  void SetUp() override {
    EnablePixelOutput(2.0f);
    UnboundedElementBrowserTest::SetUp();
  }
};

IN_PROC_BROWSER_TEST_F(UnboundedElementHighDPIBrowserTest,
                       CompositorPopupAllocationHighDPI) {
#if BUILDFLAG(IS_OZONE)
  if (ui::OzonePlatform::RunningOnWaylandForTest()) {
    // TODO(crbug.com/523970924): Some Wayland compositors (such as Mutter)
    // configure native popup bounds in physical pixels under high-DPI scaling.
    GTEST_SKIP();
  }
#endif
  GURL url(embedded_test_server()->GetURL("/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));

  std::string script = R"(
    document.body.innerHTML = `
      <meta name="viewport" content="width=device-width, initial-scale=1">
      <div id="target" style="width:100px; height:100px;" unbounded></div>
    `;
    document.getElementById('target').showUnboundedElement();
  )";
  EXPECT_TRUE(ExecJs(primary_main_frame_host(), script));
  WaitForFrameReady();

  UnboundedSurfaceWindow* window =
      primary_main_frame_host()->GetUnboundedSurfaceWindow();
  ASSERT_TRUE(window);

  float dsf = primary_main_frame_host()
                  ->GetRenderWidgetHost()
                  ->GetView()
                  ->GetDeviceScaleFactor();
  EXPECT_EQ(2.0f, dsf);

  gfx::Rect bounds = window->GetBounds();
  EXPECT_EQ(100, bounds.width());
  EXPECT_EQ(100, bounds.height());
}

IN_PROC_BROWSER_TEST_F(UnboundedElementBrowserTest,
                       VisualOverflowBoundsAndMasking) {
  GURL url(embedded_test_server()->GetURL("/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));

  std::string script = R"(
    document.body.innerHTML = `
      <meta name="viewport" content="width=device-width, initial-scale=1">
      <style>
        #child {
          width: 200px;
          height: 90px;
          border-radius: 6px;
          box-shadow: 0 4px 12px rgba(0,0,0,0.15);
        }
      </style>
      <div id="child" unbounded>
        <div class="item">Content</div>
      </div>
    `;
    document.getElementById('child').showUnboundedElement();
  )";

  EXPECT_TRUE(ExecJs(primary_main_frame_host(), script));
  WaitForFrameReady();

  EXPECT_EQ("visible", EvalJs(primary_main_frame_host(), R"(
    window.getComputedStyle(document.querySelector('.item')).visibility;
  )"));

  RenderFrameHostImpl* rfh =
      static_cast<RenderFrameHostImpl*>(primary_main_frame_host());
  UnboundedSurfaceWindow* window = rfh->GetUnboundedSurfaceWindow();
  ASSERT_TRUE(window);
  gfx::Rect popup_bounds = window->GetBounds();
  EXPECT_GE(popup_bounds.width(), 200);
  EXPECT_GE(popup_bounds.height(), 90);
}

IN_PROC_BROWSER_TEST_F(UnboundedElementBrowserTest, PopupInputEventRouting) {
#if BUILDFLAG(IS_CHROMEOS)
  // TODO(crbug.com/508672616): Not yet working on ChromeOS due to Aura/Ash
  // popup container positioning and coordinate conversion issues.
  GTEST_SKIP();
#elif BUILDFLAG(IS_OZONE)
  if (ui::OzonePlatform::RunningOnWaylandForTest()) {
    // TODO(crbug.com/523970924): Not yet working on Wayland due to
    // Ozone/Wayland popup container positioning and coordinate conversion
    // issues.
    GTEST_SKIP();
  }
#endif
#if !BUILDFLAG(IS_CHROMEOS)
  GURL url(embedded_test_server()->GetURL("/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));

  std::string script = R"(
    document.body.style.margin = '0';
    document.body.innerHTML = `
      <meta name="viewport" content="width=device-width, initial-scale=1">
      <div id="child" style="width:100px; height:100px;" unbounded></div>
    `;
    const div = document.getElementById('child');
    div.addEventListener('mousemove', (e) => {
      window.__mouse_x = e.clientX;
      window.__mouse_y = e.clientY;
    });
    div.showUnboundedElement();
  )";

  EXPECT_TRUE(ExecJs(primary_main_frame_host(), script));
  WaitForFrameReady();

  RenderFrameHostImpl* rfh =
      static_cast<RenderFrameHostImpl*>(primary_main_frame_host());
  UnboundedSurfaceWindow* window = rfh->GetUnboundedSurfaceWindow();
  ASSERT_TRUE(window);

  blink::WebMouseEvent event(blink::WebInputEvent::Type::kMouseMove,
                             blink::WebInputEvent::kNoModifiers,
                             base::TimeTicks::Now());
  event.button = blink::WebMouseEvent::Button::kNoButton;
  gfx::Rect popup_bounds = window->GetBounds();
  const int kMouseOffsetX = 50;
  const int kMouseOffsetY = 50;
  event.SetPositionInWidget(kMouseOffsetX, kMouseOffsetY);
  event.SetPositionInScreen(popup_bounds.x() + kMouseOffsetX,
                            popup_bounds.y() + kMouseOffsetY);

  window->RouteMouseEvent(event);
  RunUntilInputProcessed(primary_main_frame_host()->GetRenderWidgetHost());

  EXPECT_EQ(kMouseOffsetX,
            EvalJs(primary_main_frame_host(), "window.__mouse_x"));
  EXPECT_EQ(kMouseOffsetY,
            EvalJs(primary_main_frame_host(), "window.__mouse_y"));
#endif
}

IN_PROC_BROWSER_TEST_F(UnboundedElementBrowserTest,
                       PopupOutsideViewportInputEventRouting) {
#if BUILDFLAG(IS_CHROMEOS)
  // TODO(crbug.com/508672616): Not yet working on ChromeOS due to Aura/Ash
  // popup container positioning and coordinate conversion issues.
  GTEST_SKIP();
#elif BUILDFLAG(IS_OZONE)
  if (ui::OzonePlatform::RunningOnWaylandForTest()) {
    // TODO(crbug.com/523970924): Not yet working on Wayland due to
    // Ozone/Wayland popup container positioning and coordinate conversion
    // issues.
    GTEST_SKIP();
  }
#endif
#if !BUILDFLAG(IS_CHROMEOS)
  GURL url(embedded_test_server()->GetURL("/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));

  const int kOutsideElementLeft = 50;
  const int kOutsideElementTop = 400;
  std::string script = base::StringPrintf(
      R"(
    document.body.style.margin = '0';
    document.body.innerHTML = `
      <meta name="viewport" content="width=device-width, initial-scale=1">
      <div id="child" style="width:100px; height:100px; position:absolute;
           top:%dpx; left:%dpx;" unbounded></div>
    `;
    const div = document.getElementById('child');
    div.addEventListener('mousemove', (e) => {
      window.__mouse_x = e.clientX;
      window.__mouse_y = e.clientY;
    });
    div.showUnboundedElement();
  )",
      kOutsideElementTop, kOutsideElementLeft);

  EXPECT_TRUE(ExecJs(primary_main_frame_host(), script));
  WaitForFrameReady();

  RenderFrameHostImpl* rfh =
      static_cast<RenderFrameHostImpl*>(primary_main_frame_host());
  UnboundedSurfaceWindow* window = rfh->GetUnboundedSurfaceWindow();
  ASSERT_TRUE(window);

  blink::WebMouseEvent event(blink::WebInputEvent::Type::kMouseMove,
                             blink::WebInputEvent::kNoModifiers,
                             base::TimeTicks::Now());
  event.button = blink::WebMouseEvent::Button::kNoButton;
  gfx::Rect popup_bounds = window->GetBounds();
  const int kMouseOffsetX = 50;
  const int kMouseOffsetY = 70;
  event.SetPositionInWidget(kMouseOffsetX, kMouseOffsetY);
  event.SetPositionInScreen(popup_bounds.x() + kMouseOffsetX,
                            popup_bounds.y() + kMouseOffsetY);

  window->RouteMouseEvent(event);
  RunUntilInputProcessed(primary_main_frame_host()->GetRenderWidgetHost());

  // The expected document coordinates are calculated as:
  // element_coordinate + mouse_offset_inside_popup.
  constexpr int kExpectedMouseX = kOutsideElementLeft + kMouseOffsetX;
  constexpr int kExpectedMouseY = kOutsideElementTop + kMouseOffsetY;
  EXPECT_EQ(kExpectedMouseX,
            EvalJs(primary_main_frame_host(), "window.__mouse_x"));
  EXPECT_EQ(kExpectedMouseY,
            EvalJs(primary_main_frame_host(), "window.__mouse_y"));
#endif
}

IN_PROC_BROWSER_TEST_F(UnboundedElementBrowserTest,
                       InputEventRoutingWithScroll) {
  GURL url(embedded_test_server()->GetURL("/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));

  std::string script = R"(
    document.body.style.margin = '0';
    document.body.style.height = '2000px';
    document.body.innerHTML = `
      <meta name="viewport" content="width=device-width, initial-scale=1">
      <div id="child" style="width:100px; height:100px; position:absolute;
           top:400px; left:50px;" unbounded></div>
    `;
    window.scrollTo(0, 100);
    const div = document.getElementById('child');
    div.addEventListener('mousemove', (e) => {
      window.__mouse_x = e.clientX;
      window.__mouse_y = e.clientY;
    });
    div.showUnboundedElement();
  )";
  EXPECT_TRUE(ExecJs(primary_main_frame_host(), script));
  WaitForFrameReady();

  // Element is at document (50, 400).
  // Scroll is 100 down.
  // Element is visible at viewport (50, 300).
  // Simulate mouse move at viewport (100, 370) which is offset (50, 70) inside
  // the element.
  SimulateMouseEvent(web_contents(), blink::WebInputEvent::Type::kMouseMove,
                     gfx::Point(100, 370));
  RunUntilInputProcessed(primary_main_frame_host()->GetRenderWidgetHost());

  // We expect the event to be received with client coordinates matching the
  // simulation.
  EXPECT_EQ(100, EvalJs(primary_main_frame_host(), "window.__mouse_x"));
  EXPECT_EQ(370, EvalJs(primary_main_frame_host(), "window.__mouse_y"));
}

// TODO(crbug.com/508672616): Unbounded elements within frames are not yet
// working properly.
IN_PROC_BROWSER_TEST_F(UnboundedElementBrowserTest,
                       DISABLED_IframeInputEventRouting) {
  GURL url(embedded_test_server()->GetURL("/page_with_iframe.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));

  // Position and style the iframe.
  std::string setup_script =
      "document.getElementById('test_iframe').style.cssText = "
      "'width:100px; height:100px; border:none; margin:0; position:absolute; "
      "top:50px; left:50px;';";
  EXPECT_TRUE(ExecJs(primary_main_frame_host(), setup_script));

  RenderFrameHost* iframe = ChildFrameAt(primary_main_frame_host(), 0);
  ASSERT_TRUE(iframe);

  // Set up the unbounded element inside the iframe.
  std::string iframe_script = R"(
    document.body.style.margin = '0';
    document.body.innerHTML = `
      <div id="child" style="width:50px; height:50px; position:absolute;
           top:120px; left:120px;" unbounded></div>
    `;
    const div = document.getElementById('child');
    div.addEventListener('mousemove', (e) => {
      window.__mouse_x = e.clientX;
      window.__mouse_y = e.clientY;
    });
    div.showUnboundedElement();
  )";
  EXPECT_TRUE(ExecJs(iframe, iframe_script));
  WaitForFrameReady();

  // The iframe is at document (50, 50). Its bounds are [50, 50] to [150, 150].
  // The child element is at iframe-document (120, 120), which is document (170,
  // 170). Simulate mouse move at viewport (180, 180) which is offset (10, 10)
  // inside the child element, and completely outside the iframe bounds.
  SimulateMouseEvent(web_contents(), blink::WebInputEvent::Type::kMouseMove,
                     gfx::Point(180, 180));
  RunUntilInputProcessed(primary_main_frame_host()->GetRenderWidgetHost());

  // We expect the event to be received by the iframe with client coordinates
  // matching the simulation.
  EXPECT_EQ(130, EvalJs(iframe, "window.__mouse_x"));
  EXPECT_EQ(130, EvalJs(iframe, "window.__mouse_y"));
}

IN_PROC_BROWSER_TEST_F(UnboundedElementBrowserTest, DynamicBoundsSync) {
  GURL url(embedded_test_server()->GetURL("/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));

  std::string script = R"(
    document.body.innerHTML = `
      <meta name="viewport" content="width=device-width, initial-scale=1">
      <div id="child" style="width:100px; height:100px; position:absolute;
           top:0; left:0;" unbounded></div>
    `;
    const div = document.getElementById('child');
    div.showUnboundedElement();
  )";

  EXPECT_TRUE(ExecJs(primary_main_frame_host(), script));
  WaitForFrameReady();

  UnboundedSurfaceWindow* window =
      primary_main_frame_host()->GetUnboundedSurfaceWindow();
  ASSERT_TRUE(window);

  // Verify initial bounds
  {
    gfx::Rect bounds = window->GetBounds();
    EXPECT_EQ(100, bounds.width());
    EXPECT_EQ(100, bounds.height());
  }

  // Update style properties to trigger bounds update
  std::string update_script = R"(
    const div = document.getElementById('child');
    div.style.width = '150px';
    div.style.height = '200px';
    div.style.left = '50px';
    div.style.top = '50px';
  )";
  EXPECT_TRUE(ExecJs(primary_main_frame_host(), update_script));

  // Allow layout and pre-paint to propagate the new bounds to the browser
  std::ignore = EvalJs(primary_main_frame_host(),
                       "new Promise(r => requestAnimationFrame(() => "
                       "requestAnimationFrame(r)))");

  // Verify updated bounds
  {
    gfx::Rect bounds = window->GetBounds();
    EXPECT_EQ(150, bounds.width());
    EXPECT_EQ(200, bounds.height());
  }
}

IN_PROC_BROWSER_TEST_F(UnboundedElementBrowserTest,
                       NestedChildBoundsExpansionTriggersRedraw) {
  GURL url(embedded_test_server()->GetURL("/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));

  std::string script = R"(
    document.body.innerHTML = `
      <div id="parent" style="display:flex; width:100px; height:100px;"
           unbounded>
        <div style="width:100px; height:100px; flex-shrink:0;"></div>
        <div id="child" style="width:100px; height:100px; flex-shrink:0;
             display:none;"></div>
      </div>
    `;
    const parent = document.getElementById('parent');
    parent.showUnboundedElement().then(() => "OK", e => e.name);
  )";
  EXPECT_EQ("OK", EvalJs(primary_main_frame_host(), script));
  WaitForFrameReady();

  UnboundedSurfaceWindow* window =
      primary_main_frame_host()->GetUnboundedSurfaceWindow();
  ASSERT_TRUE(window);

  // Initial bounds should only be parent (100x100)
  {
    gfx::Rect bounds = window->GetBounds();
    EXPECT_EQ(100, bounds.width());
    EXPECT_EQ(100, bounds.height());
  }

  // Show child
  std::string update_script = R"(
    document.getElementById('child').style.display = 'block';
  )";
  EXPECT_TRUE(ExecJs(primary_main_frame_host(), update_script));

  std::ignore = EvalJs(primary_main_frame_host(),
                       "new Promise(r => requestAnimationFrame(() => "
                       "requestAnimationFrame(r)))");

  EXPECT_EQ(200, window->GetBounds().width());
}

IN_PROC_BROWSER_TEST_F(UnboundedElementBrowserTest,
                       AnimatedChildWithBoxShadowSubmitsFrame) {
  GURL url(embedded_test_server()->GetURL("/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));

  std::string script = R"(
    document.body.innerHTML = `
      <style>
        @keyframes fadeIn { from { opacity: 0; } to { opacity: 1; } }
        .submenu {
          display: none;
          width: 100px;
          height: 100px;
        }
        .submenu.is-open {
          display: block;
          animation: fadeIn 0.2s ease-in-out;
          box-shadow: 0 14px 28px rgba(0, 0, 0, 0.22);
        }
      </style>
      <div id="parent" style="display:flex; width:100px; height:100px;"
           unbounded>
        <div style="width:100px; height:100px;"></div>
        <div id="child" class="submenu"></div>
      </div>
    `;
    const parent = document.getElementById('parent');
    parent.showUnboundedElement().then(() => "OK", e => e.name);
  )";
  EXPECT_EQ("OK", EvalJs(primary_main_frame_host(), script));
  WaitForFrameReady();

  UnboundedSurfaceWindow* window =
      primary_main_frame_host()->GetUnboundedSurfaceWindow();
  ASSERT_TRUE(window);

  std::string update_script = R"(
    document.getElementById('child').classList.add('is-open');
  )";
  EXPECT_TRUE(ExecJs(primary_main_frame_host(), update_script));

  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return window->GetBounds().width() > 100; }));

  // Force subsequent animation frame under the same LocalSurfaceId
  std::ignore = EvalJs(primary_main_frame_host(),
                       "new Promise(r => requestAnimationFrame(() => "
                       "requestAnimationFrame(r)))");

  base::test::TestFuture<const content::CopyFromSurfaceResult&> future_result;
  window->CopyFromSurface(gfx::Rect(), window->GetBounds().size(),
                          base::TimeDelta(), future_result.GetCallback());
  auto result = future_result.Take();
  EXPECT_TRUE(result.has_value());
  if (result.has_value()) {
    EXPECT_FALSE(result.value().bitmap.drawsNothing());
  }
}

IN_PROC_BROWSER_TEST_F(UnboundedElementBrowserTest,
                       IframeDeletionDoesNotDismissUnboundedSurface) {
  GURL url(embedded_test_server()->GetURL("/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));

  std::string script = R"(
    document.body.innerHTML = `
      <meta name="viewport" content="width=device-width, initial-scale=1">
      <div id="target" style="width:100px; height:100px;" unbounded></div>
      <iframe id="test_iframe" src="about:blank"></iframe>
    `;
    document.getElementById('target').showUnboundedElement();
  )";
  EXPECT_TRUE(ExecJs(primary_main_frame_host(), script));
  WaitForFrameReady();

  UnboundedSurfaceWindow* window =
      primary_main_frame_host()->GetUnboundedSurfaceWindow();
  ASSERT_TRUE(window);
  EXPECT_TRUE(window->is_valid());

  // Remove the iframe and verify it doesn't dismiss the unbounded surface.
  EXPECT_TRUE(ExecJs(primary_main_frame_host(),
                     "document.getElementById('test_iframe').remove();"));
  RunUntilInputProcessed(primary_main_frame_host()->GetRenderWidgetHost());
  EXPECT_TRUE(window->is_valid());
}

IN_PROC_BROWSER_TEST_F(UnboundedElementBrowserTest,
                       DoesNotStealFocusWhenOpened) {
  GURL url(embedded_test_server()->GetURL("/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));

  std::string script = R"(
    document.body.innerHTML = `
      <meta name="viewport" content="width=device-width, initial-scale=1">
      <div id="c" style="width: 100px; height: 100px;">
        <input id="i">
      </div>
    `;
    const i = document.getElementById('i');
    const c = document.getElementById('c');
    i.focus();
    c.setAttribute('unbounded', '');
    c.showUnboundedElement();
  )";
  EXPECT_TRUE(ExecJs(primary_main_frame_host(), script));

  WaitForFrameReady();
  UnboundedSurfaceWindow* window =
      primary_main_frame_host()->GetUnboundedSurfaceWindow();
  ASSERT_TRUE(window);
  EXPECT_TRUE(window->is_valid());

  SimulateKeyPress(web_contents(), ui::DomKey::FromCharacter('a'),
                   ui::DomCode::US_A, ui::VKEY_A, false, false, false, false);
  RunUntilInputProcessed(primary_main_frame_host()->GetRenderWidgetHost());

  EXPECT_EQ("a", EvalJs(primary_main_frame_host(),
                        "document.getElementById('i').value"));
}

IN_PROC_BROWSER_TEST_F(UnboundedElementBrowserTest, CloseOnWindowFocusLost) {
  GURL url(embedded_test_server()->GetURL("/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));

  std::string script = R"(
    document.body.innerHTML = `
      <meta name="viewport" content="width=device-width, initial-scale=1">
      <div id="target" style="width:50px; height:50px;" unbounded></div>
    `;
    document.getElementById('target').showUnboundedElement();
  )";
  ASSERT_TRUE(ExecJs(primary_main_frame_host(), script));
  WaitForFrameReady();

  RenderFrameHostImpl* rfh =
      static_cast<RenderFrameHostImpl*>(primary_main_frame_host());
  UnboundedSurfaceWindow* window = rfh->GetUnboundedSurfaceWindow();
  ASSERT_TRUE(window);
  EXPECT_TRUE(window->is_valid());
  auto tracker = CreateDestructionTracker(*window);

  // Simulate the browser window losing focus.
  primary_main_frame_host()->GetRenderWidgetHost()->Blur();

  RunUntilInputProcessed(primary_main_frame_host()->GetRenderWidgetHost());
  EXPECT_FALSE(rfh->GetUnboundedSurfaceWindow());

  std::string get_style =
      "getComputedStyle(document.getElementById('target')).visibility";
  EXPECT_EQ("hidden", EvalJs(primary_main_frame_host(), get_style));

  WaitForDestruction(std::move(tracker));
}

IN_PROC_BROWSER_TEST_F(UnboundedElementBrowserTest,
                       MultipleUnboundedElementsDismissesFirst) {
  GURL url(embedded_test_server()->GetURL("/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));

  std::string script = R"JS(
    document.body.innerHTML = `
      <meta name="viewport" content="width=device-width, initial-scale=1">
      <div id="first" style="width:50px; height:50px;" unbounded></div>
      <div id="second" style="width:50px; height:50px;" unbounded></div>
    `;
    const first = document.getElementById('first');
    const second = document.getElementById('second');
    let results = [];
    first.showUnboundedElement()
      .then(() => {
        results.push(getComputedStyle(first).visibility);
        results.push(getComputedStyle(second).visibility);
        return second.showUnboundedElement();
      })
      .then(() => {
        results.push(getComputedStyle(first).visibility);
        results.push(getComputedStyle(second).visibility);
        return results.join(',');
      }, err => "error:" + err);
  )JS";

  EXPECT_EQ("visible,hidden,hidden,visible",
            EvalJs(primary_main_frame_host(), script).ExtractString());
}

IN_PROC_BROWSER_TEST_F(UnboundedElementBrowserTest,
                       DisplayNoneDismissesSurface) {
  GURL url(embedded_test_server()->GetURL("/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));

  std::string script = R"(
    document.body.innerHTML = `
      <div id="wrapper" unbounded
           style="display:flex; gap:20px; width:max-content;">
        <div id="m1" style="width:100px; height:100px; background:red;">
          Menu1
        </div>
        <div id="m2" style="width:100px; height:100px; background:blue;">
          Menu2
        </div>
      </div>
    `;
    const wrapper = document.getElementById('wrapper');
    wrapper.showUnboundedElement().then(() => "OK", e => e.name);
  )";
  EXPECT_EQ("OK", EvalJs(primary_main_frame_host(), script));
  WaitForFrameReady();

  UnboundedSurfaceWindow* window =
      primary_main_frame_host()->GetUnboundedSurfaceWindow();
  ASSERT_TRUE(window);
  auto tracker = CreateDestructionTracker(*window);

  EXPECT_TRUE(ExecJs(primary_main_frame_host(), R"(
    const wrapper = document.getElementById('wrapper');
    wrapper.style.display = 'none';
    document.body.offsetHeight;
  )"));

  RunUntilInputProcessed(primary_main_frame_host()->GetRenderWidgetHost());

  EXPECT_FALSE(primary_main_frame_host()->GetUnboundedSurfaceWindow());
  EXPECT_EQ(false,
            EvalJs(primary_main_frame_host(),
                   "document.getElementById('wrapper').matches(':unbounded')"));
  WaitForDestruction(std::move(tracker));
}

struct UnboundedElementPermutationTestParams {
  bool unbounded_element_base_feature;
  bool unbounded_element_runtime_feature;
  bool open_web_base_feature;
  bool open_web_runtime_feature;
  bool is_privileged;
};

class UnboundedElementPermutationBrowserTest
    : public UnboundedElementBrowserTestBase,
      public ::testing::WithParamInterface<
          UnboundedElementPermutationTestParams> {
 public:
  UnboundedElementPermutationBrowserTest() = default;
  ~UnboundedElementPermutationBrowserTest() override = default;

  void SetUp() override {
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
    GTEST_SKIP();
#else
    const auto& params = GetParam();
    std::vector<base::test::FeatureRef> enabled_features;
    std::vector<base::test::FeatureRef> disabled_features;

    if (params.unbounded_element_base_feature) {
      enabled_features.push_back(blink::features::kUnboundedElement);
    } else {
      disabled_features.push_back(blink::features::kUnboundedElement);
    }

    if (params.open_web_base_feature) {
      enabled_features.push_back(
          blink::features::kUnboundedElementOnTheOpenWeb);
    } else {
      disabled_features.push_back(
          blink::features::kUnboundedElementOnTheOpenWeb);
    }

    disabled_features.push_back(::features::kTreesInViz);

    feature_list_.InitWithFeatures(enabled_features, disabled_features);
    UnboundedElementBrowserTestBase::SetUp();
#endif
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    UnboundedElementBrowserTestBase::SetUpCommandLine(command_line);
    const auto& params = GetParam();

    std::vector<std::string> enabled_blink_features;
    std::vector<std::string> disabled_blink_features;

    if (params.unbounded_element_runtime_feature) {
      enabled_blink_features.push_back("UnboundedElement");
    } else {
      disabled_blink_features.push_back("UnboundedElement");
    }

    if (params.open_web_runtime_feature) {
      enabled_blink_features.push_back("UnboundedElementOnTheOpenWeb");
    } else {
      disabled_blink_features.push_back("UnboundedElementOnTheOpenWeb");
    }

    if (!enabled_blink_features.empty()) {
      command_line->AppendSwitchASCII(
          switches::kEnableBlinkFeatures,
          base::JoinString(enabled_blink_features, ","));
    }
    if (!disabled_blink_features.empty()) {
      command_line->AppendSwitchASCII(
          switches::kDisableBlinkFeatures,
          base::JoinString(disabled_blink_features, ","));
    }
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_P(UnboundedElementPermutationBrowserTest,
                       CheckPermutation) {
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  GTEST_SKIP();
#else
  const auto& params = GetParam();

  if (params.is_privileged) {
    GURL webui_url(std::string(kChromeUIScheme) + "://" +
                   std::string(kChromeUIGpuHost));
    EXPECT_TRUE(NavigateToURL(shell(), webui_url));
  } else {
    GURL url(embedded_test_server()->GetURL("/title1.html"));
    EXPECT_TRUE(NavigateToURL(shell(), url));
  }

  // Blink's UnboundedElement runtime feature is implied if
  // UnboundedElementOnTheOpenWeb runtime feature is enabled.
  bool effective_unbounded_runtime = params.unbounded_element_runtime_feature ||
                                     params.open_web_runtime_feature;

  // The JS API (showUnboundedElement) is exposed in Blink renderer if:
  // 1. Base feature kUnboundedElement is enabled.
  // 2. Effective UnboundedElement runtime feature is enabled.
  // 3. AND at least one of (is_privileged, open_web_base_feature) is true.
  bool expected_api_exposed =
      params.unbounded_element_base_feature && effective_unbounded_runtime &&
      (params.is_privileged || params.open_web_base_feature);

  std::string check_api_script = R"(
    const div = document.createElement('div');
    'showUnboundedElement' in div ? "Present" : "Missing";
  )";
  EXPECT_EQ(expected_api_exposed ? "Present" : "Missing",
            EvalJs(primary_main_frame_host(), check_api_script,
                   EXECUTE_SCRIPT_NO_USER_GESTURE));

  if (expected_api_exposed) {
    std::string invoke_script = R"(
      const div = document.createElement('div');
      div.setAttribute('unbounded', '');
      div.style.width = '100px';
      div.style.height = '100px';
      document.body.appendChild(div);
      div.showUnboundedElement().then(() => "Success", e => e.name);
    )";

    bool expected_browser_allowed =
        params.unbounded_element_base_feature &&
        (params.is_privileged || params.open_web_base_feature);

    if (params.is_privileged) {
      // Privileged pages bypass user activation.
      if (expected_browser_allowed) {
        EXPECT_EQ("Success", EvalJs(primary_main_frame_host(), invoke_script,
                                    EXECUTE_SCRIPT_NO_USER_GESTURE));
      } else {
        ScopedAllowRendererCrashes scoped_allow_renderer_crashes(
            primary_main_frame_host()->GetProcess());
        RenderProcessHostBadMojoMessageWaiter kill_waiter(
            primary_main_frame_host()->GetProcess());
        ExecuteScriptAsync(primary_main_frame_host(), invoke_script);
        EXPECT_TRUE(kill_waiter.Wait().has_value());
      }
    } else {
      // Non-privileged pages reject in JS with NotAllowedError when user
      // activation is missing.
      EXPECT_EQ("NotAllowedError",
                EvalJs(primary_main_frame_host(), invoke_script,
                       EXECUTE_SCRIPT_NO_USER_GESTURE));

      // With user activation:
      if (expected_browser_allowed) {
        EXPECT_EQ("Success", EvalJs(primary_main_frame_host(), invoke_script));
      } else {
        ScopedAllowRendererCrashes scoped_allow_renderer_crashes(
            primary_main_frame_host()->GetProcess());
        RenderProcessHostBadMojoMessageWaiter kill_waiter(
            primary_main_frame_host()->GetProcess());
        ExecuteScriptAsync(primary_main_frame_host(), invoke_script);
        EXPECT_TRUE(kill_waiter.Wait().has_value());
      }
    }
  }
#endif
}

namespace {
std::vector<UnboundedElementPermutationTestParams> GeneratePermutations() {
  std::vector<UnboundedElementPermutationTestParams> params;
  for (bool unbounded_base : {false, true}) {
    for (bool unbounded_runtime : {false, true}) {
      for (bool open_web_base : {false, true}) {
        for (bool open_web_runtime : {false, true}) {
          for (bool is_privileged : {false, true}) {
            params.push_back({unbounded_base, unbounded_runtime, open_web_base,
                              open_web_runtime, is_privileged});
          }
        }
      }
    }
  }
  return params;
}
}  // namespace

INSTANTIATE_TEST_SUITE_P(
    All,
    UnboundedElementPermutationBrowserTest,
    testing::ValuesIn(GeneratePermutations()),
    [](const testing::TestParamInfo<UnboundedElementPermutationTestParams>&
           info) {
      return base::StringPrintf(
          "UnboundedBase%s_UnboundedRuntime%s_OpenWebBase%s_OpenWebRuntime%s_"
          "%s",
          info.param.unbounded_element_base_feature ? "On" : "Off",
          info.param.unbounded_element_runtime_feature ? "On" : "Off",
          info.param.open_web_base_feature ? "On" : "Off",
          info.param.open_web_runtime_feature ? "On" : "Off",
          info.param.is_privileged ? "Privileged" : "OpenWeb");
    });

}  // namespace content
