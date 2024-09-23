// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/342213636): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include <stdlib.h>

#include "base/command_line.h"
#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "content/browser/preloading/prerender/prerender_final_status.h"
#include "content/browser/preloading/prerender/prerender_host_registry.h"
#include "content/browser/renderer_host/frame_tree.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/screen_orientation/screen_orientation_provider.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/screen_orientation_delegate.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/prerender_test_util.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/shell/common/shell_switches.h"
#include "net/dns/mock_host_resolver.h"
#include "third_party/blink/public/common/features.h"
#include "ui/compositor/compositor_switches.h"
#include "ui/display/screen.h"

namespace content {

class ScreenOrientationBrowserTest : public ContentBrowserTest  {
 public:
  ScreenOrientationBrowserTest() = default;

  ScreenOrientationBrowserTest(const ScreenOrientationBrowserTest&) = delete;
  ScreenOrientationBrowserTest& operator=(const ScreenOrientationBrowserTest&) =
      delete;

  // ContentBrowserTest:
  void SetUpOnMainThread() override {
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  WebContentsImpl* web_contents() {
    return static_cast<WebContentsImpl*>(shell()->web_contents());
  }

 protected:
  void SendFakeScreenOrientation(unsigned angle, const std::string& str_type) {
    RenderWidgetHostImpl* main_frame_rwh = static_cast<RenderWidgetHostImpl*>(
        web_contents()->GetPrimaryMainFrame()->GetRenderWidgetHost());

    display::mojom::ScreenOrientation type =
        display::mojom::ScreenOrientation::kUndefined;
    if (str_type == "portrait-primary") {
      type = display::mojom::ScreenOrientation::kPortraitPrimary;
    } else if (str_type == "portrait-secondary") {
      type = display::mojom::ScreenOrientation::kPortraitSecondary;
    } else if (str_type == "landscape-primary") {
      type = display::mojom::ScreenOrientation::kLandscapePrimary;
    } else if (str_type == "landscape-secondary") {
      type = display::mojom::ScreenOrientation::kLandscapeSecondary;
    }
    ASSERT_NE(display::mojom::ScreenOrientation::kUndefined, type);

    // This simulates what the browser process does when the screen orientation
    // is changed.
    main_frame_rwh->SetScreenOrientationForTesting(angle, type);
  }

  int GetOrientationAngle() {
    return EvalJs(shell()->web_contents()->GetPrimaryMainFrame(),
                  "screen.orientation.angle")
        .ExtractInt();
  }

  std::string GetOrientationType() {
    return EvalJs(shell()->web_contents()->GetPrimaryMainFrame(),
                  "screen.orientation.type")
        .ExtractString();
  }

  bool ScreenOrientationSupported() {
    return EvalJs(shell()->web_contents()->GetPrimaryMainFrame(),
                  "'orientation' in screen")
        .ExtractBool();
  }

  bool WindowOrientationSupported() {
    return EvalJs(shell()->web_contents()->GetPrimaryMainFrame(),
                  "'orientation' in window")
        .ExtractBool();
  }

  int GetWindowOrientationAngle() {
    return EvalJs(shell()->web_contents()->GetPrimaryMainFrame(),
                  "window.orientation")
        .ExtractInt();
  }
};

class ScreenOrientationOOPIFBrowserTest : public ScreenOrientationBrowserTest {
 public:
  ScreenOrientationOOPIFBrowserTest() {}

  ScreenOrientationOOPIFBrowserTest(const ScreenOrientationOOPIFBrowserTest&) =
      delete;
  ScreenOrientationOOPIFBrowserTest& operator=(
      const ScreenOrientationOOPIFBrowserTest&) = delete;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    IsolateAllSitesForTesting(command_line);
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    SetupCrossSiteRedirector(embedded_test_server());
    ScreenOrientationBrowserTest::SetUpOnMainThread();
  }
};

// This test doesn't work on MacOS X but the reason is mostly because it is not
// used Aura. It could be set as !BUILDFLAG(IS_MAC) but the rule below will
// actually support MacOS X if and when it switches to Aura.
#if defined(USE_AURA) || BUILDFLAG(IS_ANDROID)
// Flaky on Chrome OS: http://crbug.com/468259
#if BUILDFLAG(IS_CHROMEOS_ASH)
#define MAYBE_ScreenOrientationChange DISABLED_ScreenOrientationChange
#else
#define MAYBE_ScreenOrientationChange ScreenOrientationChange
#endif
IN_PROC_BROWSER_TEST_F(ScreenOrientationBrowserTest,
                       MAYBE_ScreenOrientationChange) {
  std::string types[] = { "portrait-primary",
                          "portrait-secondary",
                          "landscape-primary",
                          "landscape-secondary" };
  GURL test_url = GetTestUrl("screen_orientation",
                             "screen_orientation_screenorientationchange.html");

  {
    TestNavigationObserver navigation_observer(shell()->web_contents(), 1);
    shell()->LoadURL(test_url);
    navigation_observer.Wait();
    WaitForResizeComplete(shell()->web_contents());
  }

  int angle = GetOrientationAngle();

  for (int i = 0; i < 4; ++i) {
    angle = (angle + 90) % 360;
    SendFakeScreenOrientation(angle, types[i]);

    TestNavigationObserver navigation_observer(shell()->web_contents());
    navigation_observer.Wait();
    EXPECT_EQ(angle, GetOrientationAngle());
    EXPECT_EQ(types[i], GetOrientationType());
  }
}
#endif  // defined(USE_AURA) || BUILDFLAG(IS_ANDROID)

// Flaky on Chrome OS: http://crbug.com/468259
#if BUILDFLAG(IS_CHROMEOS_ASH)
#define MAYBE_WindowOrientationChange DISABLED_WindowOrientationChange
#else
#define MAYBE_WindowOrientationChange WindowOrientationChange
#endif
IN_PROC_BROWSER_TEST_F(ScreenOrientationBrowserTest,
                       MAYBE_WindowOrientationChange) {
  GURL test_url = GetTestUrl("screen_orientation",
                             "screen_orientation_windoworientationchange.html");

  {
    TestNavigationObserver navigation_observer(shell()->web_contents(), 1);
    shell()->LoadURL(test_url);
    navigation_observer.Wait();
#if USE_AURA || BUILDFLAG(IS_ANDROID)
    WaitForResizeComplete(shell()->web_contents());
#endif  // USE_AURA || BUILDFLAG(IS_ANDROID)
  }

  if (!WindowOrientationSupported())
    return;

  int angle = GetWindowOrientationAngle();

  for (int i = 0; i < 4; ++i) {
    angle = (angle + 90) % 360;
    SendFakeScreenOrientation(angle, "portrait-primary");

    TestNavigationObserver navigation_observer(shell()->web_contents(), 1);
    navigation_observer.Wait();
    EXPECT_EQ(angle == 270 ? -90 : angle, GetWindowOrientationAngle());
  }
}

// LockSmoke test seems to have become flaky on all non-ChromeOS platforms.
// The cause is unfortunately unknown. See https://crbug.com/448876
// Chromium Android does not support fullscreen
IN_PROC_BROWSER_TEST_F(ScreenOrientationBrowserTest, DISABLED_LockSmoke) {
  GURL test_url = GetTestUrl("screen_orientation",
                             "screen_orientation_lock_smoke.html");

  TestNavigationObserver navigation_observer(shell()->web_contents(), 2);
  shell()->LoadURL(test_url);

  navigation_observer.Wait();
#if USE_AURA || BUILDFLAG(IS_ANDROID)
  WaitForResizeComplete(shell()->web_contents());
#endif  // USE_AURA || BUILDFLAG(IS_ANDROID)

  std::string expected =
#if BUILDFLAG(IS_ANDROID)
      "SecurityError";  // WebContents need to be fullscreen.
#else
      "NotSupportedError"; // Locking isn't supported.
#endif

  EXPECT_EQ(expected, shell()->web_contents()->GetLastCommittedURL().ref());
}

// Check that using screen orientation after a frame is detached doesn't crash
// the renderer process.
// This could be a web test if they were not using a mock screen orientation
// controller.
IN_PROC_BROWSER_TEST_F(ScreenOrientationBrowserTest, CrashTest_UseAfterDetach) {
  GURL test_url(embedded_test_server()->GetURL(
      "/screen_orientation/screen_orientation_use_after_detach.html"));

  TestNavigationObserver navigation_observer(shell()->web_contents(), 2);
  shell()->LoadURL(test_url);

  navigation_observer.Wait();

  // This is a success if the renderer process did not crash, thus, we end up
  // here.
}

#if BUILDFLAG(IS_ANDROID)
class ScreenOrientationLockDisabledBrowserTest : public ContentBrowserTest  {
 public:
  ScreenOrientationLockDisabledBrowserTest() {}
  ~ScreenOrientationLockDisabledBrowserTest() override {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kDisableScreenOrientationLock);
  }
};

// Check that when --disable-screen-orientation-lock is passed to the command
// line, screen.orientation.lock() correctly reports to not be supported.
IN_PROC_BROWSER_TEST_F(ScreenOrientationLockDisabledBrowserTest,
    DISABLED_NotSupported) {
  GURL test_url = GetTestUrl("screen_orientation",
                             "screen_orientation_lock_disabled.html");

  {
    TestNavigationObserver navigation_observer(shell()->web_contents(), 1);
    shell()->LoadURL(test_url);
    navigation_observer.Wait();
  }

  {
    ASSERT_TRUE(ExecJs(shell(), "run();"));

    TestNavigationObserver navigation_observer(shell()->web_contents(), 1);
    navigation_observer.Wait();
    EXPECT_EQ("NotSupportedError",
              shell()->web_contents()->GetLastCommittedURL().ref());
  }
}
#endif  // BUILDFLAG(IS_ANDROID)

IN_PROC_BROWSER_TEST_F(ScreenOrientationOOPIFBrowserTest, ScreenOrientation) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
#if USE_AURA || BUILDFLAG(IS_ANDROID)
  WaitForResizeComplete(shell()->web_contents());
#endif  // USE_AURA || BUILDFLAG(IS_ANDROID)

  std::string types[] = {"portrait-primary", "portrait-secondary",
                         "landscape-primary", "landscape-secondary"};

  int angle = GetOrientationAngle();

  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  FrameTreeNode* child = root->child_at(0);
  MainThreadFrameObserver root_observer(
      root->current_frame_host()->GetRenderWidgetHost());
  MainThreadFrameObserver child_observer(
      child->current_frame_host()->GetRenderWidgetHost());
  for (int i = 0; i < 4; ++i) {
    angle = (angle + 90) % 360;
    SendFakeScreenOrientation(angle, types[i]);

    root_observer.Wait();
    child_observer.Wait();

    EXPECT_EQ(angle,
              EvalJs(root->current_frame_host(), "screen.orientation.angle"));
    EXPECT_EQ(angle,
              EvalJs(child->current_frame_host(), "screen.orientation.angle"));

    EXPECT_EQ(types[i],
              EvalJs(root->current_frame_host(), "screen.orientation.type"));
    EXPECT_EQ(types[i],
              EvalJs(child->current_frame_host(), "screen.orientation.type"));
  }
}

// Regression test for triggering a screen orientation change for a pending
// main frame RenderFrameHost.  See https://crbug.com/764202.  In the bug, this
// was triggered via the DevTools audit panel and
// blink::mojom::FrameWidget::EnableDeviceEmulation, which calls
// RenderWidget::Resize on the renderer side.  The test fakes this by directly
// sending the resize message to the widget.
#if BUILDFLAG(IS_CHROMEOS_ASH)
#define MAYBE_ScreenOrientationInPendingMainFrame \
  DISABLED_ScreenOrientationInPendingMainFrame
#else
#define MAYBE_ScreenOrientationInPendingMainFrame \
  ScreenOrientationInPendingMainFrame
#endif
IN_PROC_BROWSER_TEST_F(ScreenOrientationOOPIFBrowserTest,
                       MAYBE_ScreenOrientationInPendingMainFrame) {
  GURL main_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
#if USE_AURA || BUILDFLAG(IS_ANDROID)
  WaitForResizeComplete(shell()->web_contents());
#endif  // USE_AURA || BUILDFLAG(IS_ANDROID)

  // Set up a fake Resize message with a screen orientation change.
  RenderWidgetHost* main_frame_rwh =
      web_contents()->GetPrimaryMainFrame()->GetRenderWidgetHost();
  display::ScreenInfo screen_info = main_frame_rwh->GetScreenInfo();
  int expected_angle = (screen_info.orientation_angle + 90) % 360;

  // Start a cross-site navigation, but don't commit yet.
  GURL second_url(embedded_test_server()->GetURL("b.com", "/title1.html"));
  TestNavigationManager delayer(shell()->web_contents(), second_url);
  shell()->LoadURL(second_url);
  delayer.WaitForSpeculativeRenderFrameHostCreation();

  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  RenderFrameHostImpl* pending_rfh =
      root->render_manager()->speculative_frame_host();

  // Send the orientation change to the pending RFH's widget.
  static_cast<RenderWidgetHostImpl*>(pending_rfh->GetRenderWidgetHost())
      ->SetScreenOrientationForTesting(expected_angle,
                                       screen_info.orientation_type);

  // Let the navigation finish and make sure it succeeded.
  ASSERT_TRUE(delayer.WaitForNavigationFinished());
  EXPECT_EQ(second_url,
            web_contents()->GetPrimaryMainFrame()->GetLastCommittedURL());

#if USE_AURA || BUILDFLAG(IS_ANDROID)
  WaitForResizeComplete(shell()->web_contents());
#endif  // USE_AURA || BUILDFLAG(IS_ANDROID)

  EXPECT_EQ(expected_angle,
            EvalJs(root->current_frame_host(), "screen.orientation.angle"));
}

#if BUILDFLAG(IS_ANDROID)
// This test is disabled because th trybots run in system portrait lock, which
// prevents the test from changing the screen orientation.
IN_PROC_BROWSER_TEST_F(ScreenOrientationOOPIFBrowserTest,
                       DISABLED_ScreenOrientationLock) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  WaitForResizeComplete(shell()->web_contents());

  const char* types[] = {"portrait-primary", "portrait-secondary",
                         "landscape-primary", "landscape-secondary"};

  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  FrameTreeNode* child = root->child_at(0);
  RenderFrameHostImpl* frames[] = {root->current_frame_host(),
                                   child->current_frame_host()};

  EXPECT_TRUE(ExecJs(root->current_frame_host(),
                     "document.body.webkitRequestFullscreen()"));
  for (const char* type : types) {
    std::string script =
        base::StringPrintf("screen.orientation.lock('%s')", type);
    EXPECT_TRUE(ExecJs(child->current_frame_host(), script));

    for (auto* frame : frames) {
      std::string orientation_type;
      while (type != orientation_type) {
        orientation_type =
            EvalJs(frame, "screen.orientation.type").ExtractString();
      }
    }

    EXPECT_TRUE(
        ExecJs(child->current_frame_host(), "screen.orientation.unlock()"));
  }
}
#endif  // BUILDFLAG(IS_ANDROID)

class ScreenOrientationLockForPrerenderBrowserTest
    : public ScreenOrientationBrowserTest {
 public:
  ScreenOrientationLockForPrerenderBrowserTest()
      : prerender_helper_(base::BindRepeating(
            &ScreenOrientationLockForPrerenderBrowserTest::web_contents,
            base::Unretained(this))) {
  }

  // ScreenOrientationBrowserTest:
  void SetUp() override {
    prerender_helper_.RegisterServerRequestMonitor(embedded_test_server());
    ScreenOrientationBrowserTest::SetUp();
  }

  content::WebContents* web_contents() { return shell()->web_contents(); }

 protected:
  test::PrerenderTestHelper prerender_helper_;
};

class FakeScreenOrientationDelegate : public ScreenOrientationDelegate {
 public:
  FakeScreenOrientationDelegate() {
    ScreenOrientationProvider::SetDelegate(this);
  }
  ~FakeScreenOrientationDelegate() override {
    ScreenOrientationProvider::SetDelegate(nullptr);
  }

  // ScreenOrientationDelegate:
  bool FullScreenRequired(WebContents* web_contents) override {
    return full_screen_required_;
  }
  bool ScreenOrientationProviderSupported(WebContents* web_contents) override {
    return true;
  }
  void Lock(
      WebContents* web_contents,
      device::mojom::ScreenOrientationLockType lock_orientation) override {
    lock_count_++;
  }
  void Unlock(WebContents* web_contents) override { unlock_count_++; }

  void set_full_screen_required(bool is_required) {
    full_screen_required_ = is_required;
  }

  int lock_count() const { return lock_count_; }
  int unlock_count() const { return unlock_count_; }

 private:
  int lock_count_ = 0;
  int unlock_count_ = 0;
  bool full_screen_required_ = false;
};

// Unlock should not triggered the orientation upon the completion of a
// non-primary navigation.
IN_PROC_BROWSER_TEST_F(ScreenOrientationLockForPrerenderBrowserTest,
                       ShouldNotUnlockWhenPrerenderNavigation) {
  FakeScreenOrientationDelegate delegate;

  // Navigate to a site.
  GURL initial_url = embedded_test_server()->GetURL("/empty.html");
  NavigateToURLBlockUntilNavigationsComplete(shell(), initial_url, 1);

  EXPECT_TRUE(ExecJs(web_contents()->GetPrimaryMainFrame(),
                     "screen.orientation.lock('portrait')",
                     EXECUTE_SCRIPT_NO_RESOLVE_PROMISES));

  // Delegate did apply lock once.
  EXPECT_EQ(1, delegate.lock_count());
  EXPECT_EQ(0, delegate.unlock_count());

  GURL prerender_url = embedded_test_server()->GetURL("/title2.html");

  // Prerender to another site.
  prerender_helper_.AddPrerender(prerender_url);

  // Delegate should not apply unlock.
  EXPECT_EQ(0, delegate.unlock_count());

  // Navigate to the prerendered site.
  prerender_helper_.NavigatePrimaryPage(prerender_url);

  // Delegate did apply unlock once.
  EXPECT_EQ(1, delegate.unlock_count());
}

// Test for ScreenOrientationProvider::DidToggleFullscreenModeForTab which
// overrides from WebContentsObserver. The prerendered page shouldn't trigger
// unlock the screen orientation in fullscreen mode.
IN_PROC_BROWSER_TEST_F(ScreenOrientationLockForPrerenderBrowserTest,
                       KeepFullscreenLockWhilePrerendering) {
  FakeScreenOrientationDelegate delegate;
  delegate.set_full_screen_required(true);

  // Enter the full screen and request lock from the primary page.
  const GURL initial_url = embedded_test_server()->GetURL("/simple_page.html");
  ASSERT_TRUE(NavigateToURL(shell(), initial_url));
  EXPECT_TRUE(ExecJs(shell(),
                     "document.body.requestFullscreen();"
                     "screen.orientation.lock('any');"));
  EXPECT_EQ(1, delegate.lock_count());

  // Start a prerender.
  const GURL prerender_url = embedded_test_server()->GetURL("/title1.html");
  FrameTreeNodeId host_id = prerender_helper_.AddPrerender(prerender_url);
  ASSERT_TRUE(host_id);

  // Shut down the prerendered page. It shouldn't trigger orientation unlock.
  test::PrerenderHostObserver prerender_observer(*web_contents(), host_id);
  PrerenderHostRegistry* registry =
      static_cast<WebContentsImpl*>(web_contents())->GetPrerenderHostRegistry();
  registry->CancelHost(host_id, PrerenderFinalStatus::kRendererProcessKilled);
  prerender_observer.WaitForDestroyed();

  // Delegate should not apply unlock.
  EXPECT_EQ(0, delegate.unlock_count());
}

} // namespace content
