// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdlib.h>

#include "base/command_line.h"
#include "base/macros.h"
#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "content/browser/frame_host/frame_tree.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/page_messages.h"
#include "content/common/view_messages.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/shell/common/shell_switches.h"
#include "net/dns/mock_host_resolver.h"
#include "ui/compositor/compositor_switches.h"
#include "ui/display/screen.h"

namespace content {

class ScreenOrientationBrowserTest : public ContentBrowserTest  {
 public:
  ScreenOrientationBrowserTest() {
  }

  WebContentsImpl* web_contents() {
    return static_cast<WebContentsImpl*>(shell()->web_contents());
  }

 protected:
  void SendFakeScreenOrientation(unsigned angle, const std::string& str_type) {
    RenderWidgetHostImpl* main_frame_rwh = static_cast<RenderWidgetHostImpl*>(
        web_contents()->GetMainFrame()->GetRenderWidgetHost());

    ScreenOrientationValues type = SCREEN_ORIENTATION_VALUES_DEFAULT;
    if (str_type == "portrait-primary") {
      type = SCREEN_ORIENTATION_VALUES_PORTRAIT_PRIMARY;
    } else if (str_type == "portrait-secondary") {
      type = SCREEN_ORIENTATION_VALUES_PORTRAIT_SECONDARY;
    } else if (str_type == "landscape-primary") {
      type = SCREEN_ORIENTATION_VALUES_LANDSCAPE_PRIMARY;
    } else if (str_type == "landscape-secondary") {
      type = SCREEN_ORIENTATION_VALUES_LANDSCAPE_SECONDARY;
    }
    ASSERT_NE(SCREEN_ORIENTATION_VALUES_DEFAULT, type);

    ScreenInfo screen_info;
    main_frame_rwh->GetScreenInfo(&screen_info);
    screen_info.orientation_angle = angle;
    screen_info.orientation_type = type;

    std::set<RenderWidgetHost*> rwhs;
    for (RenderFrameHost* rfh : web_contents()->GetAllFrames()) {
      if (rfh == web_contents()->GetMainFrame())
        continue;

      rwhs.insert(static_cast<RenderFrameHostImpl*>(rfh)
                      ->frame_tree_node()
                      ->render_manager()
                      ->GetRenderWidgetHostView()
                      ->GetRenderWidgetHost());
    }

    // This simulates what the browser process does when the screen orientation
    // is changed.
    main_frame_rwh->SetScreenOrientationForTesting(angle, type);
  }

  int GetOrientationAngle() {
    int angle =
        ExecuteScriptAndGetValue(shell()->web_contents()->GetMainFrame(),
                                 "screen.orientation.angle")
            .GetInt();
    return angle;
  }

  std::string GetOrientationType() {
    std::string type =
        ExecuteScriptAndGetValue(shell()->web_contents()->GetMainFrame(),
                                 "screen.orientation.type")
            .GetString();
    return type;
  }

  bool ScreenOrientationSupported() {
    bool support =
        ExecuteScriptAndGetValue(shell()->web_contents()->GetMainFrame(),
                                 "'orientation' in screen")
            .GetBool();
    return support;
  }

  bool WindowOrientationSupported() {
    bool support =
        ExecuteScriptAndGetValue(shell()->web_contents()->GetMainFrame(),
                                 "'orientation' in window")
            .GetBool();
    return support;
  }

  int GetWindowOrientationAngle() {
    int angle =
        ExecuteScriptAndGetValue(shell()->web_contents()->GetMainFrame(),
                                 "window.orientation")
            .GetInt();
    return angle;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ScreenOrientationBrowserTest);
};

class ScreenOrientationOOPIFBrowserTest : public ScreenOrientationBrowserTest {
 public:
  ScreenOrientationOOPIFBrowserTest() {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    IsolateAllSitesForTesting(command_line);
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    SetupCrossSiteRedirector(embedded_test_server());
    ASSERT_TRUE(embedded_test_server()->Start());
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ScreenOrientationOOPIFBrowserTest);
};

// This test doesn't work on MacOS X but the reason is mostly because it is not
// used Aura. It could be set as !defined(OS_MACOSX) but the rule below will
// actually support MacOS X if and when it switches to Aura.
#if defined(USE_AURA) || defined(OS_ANDROID)
// Flaky on Chrome OS: http://crbug.com/468259
#if defined(OS_CHROMEOS)
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

  TestNavigationObserver navigation_observer(shell()->web_contents(), 1);
  shell()->LoadURL(test_url);
  navigation_observer.Wait();
  WaitForResizeComplete(shell()->web_contents());

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
#endif // defined(USE_AURA) || defined(OS_ANDROID)

// Flaky on Chrome OS: http://crbug.com/468259
#if defined(OS_CHROMEOS)
#define MAYBE_WindowOrientationChange DISABLED_WindowOrientationChange
#else
#define MAYBE_WindowOrientationChange WindowOrientationChange
#endif
IN_PROC_BROWSER_TEST_F(ScreenOrientationBrowserTest,
                       MAYBE_WindowOrientationChange) {
  GURL test_url = GetTestUrl("screen_orientation",
                             "screen_orientation_windoworientationchange.html");

  TestNavigationObserver navigation_observer(shell()->web_contents(), 1);
  shell()->LoadURL(test_url);
  navigation_observer.Wait();
#if USE_AURA || defined(OS_ANDROID)
  WaitForResizeComplete(shell()->web_contents());
#endif  // USE_AURA || defined(OS_ANDROID)

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
#if USE_AURA || defined(OS_ANDROID)
  WaitForResizeComplete(shell()->web_contents());
#endif  // USE_AURA || defined(OS_ANDROID)

  std::string expected =
#if defined(OS_ANDROID)
      "SecurityError"; // WebContents need to be fullscreen.
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
  GURL test_url = GetTestUrl("screen_orientation",
                             "screen_orientation_use_after_detach.html");

  TestNavigationObserver navigation_observer(shell()->web_contents(), 2);
  shell()->LoadURL(test_url);

  navigation_observer.Wait();

  // This is a success if the renderer process did not crash, thus, we end up
  // here.
}

#if defined(OS_ANDROID)
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

  TestNavigationObserver navigation_observer(shell()->web_contents(), 1);
  shell()->LoadURL(test_url);
  navigation_observer.Wait();

  {
    ASSERT_TRUE(ExecuteScript(shell(), "run();"));

    TestNavigationObserver navigation_observer(shell()->web_contents(), 1);
    navigation_observer.Wait();
    EXPECT_EQ("NotSupportedError",
              shell()->web_contents()->GetLastCommittedURL().ref());
  }
}
#endif // defined(OS_ANDROID)

IN_PROC_BROWSER_TEST_F(ScreenOrientationOOPIFBrowserTest, ScreenOrientation) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
#if USE_AURA || defined(OS_ANDROID)
  WaitForResizeComplete(shell()->web_contents());
#endif  // USE_AURA || defined(OS_ANDROID)

  std::string types[] = {"portrait-primary", "portrait-secondary",
                         "landscape-primary", "landscape-secondary"};

  int angle = GetOrientationAngle();

  FrameTreeNode* root = web_contents()->GetFrameTree()->root();
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

    int orientation_angle;
    std::string orientation_type;

    EXPECT_TRUE(ExecuteScriptAndExtractInt(
        root->current_frame_host(),
        "window.domAutomationController.send(screen.orientation.angle)",
        &orientation_angle));
    EXPECT_EQ(angle, orientation_angle);
    EXPECT_TRUE(ExecuteScriptAndExtractInt(
        child->current_frame_host(),
        "window.domAutomationController.send(screen.orientation.angle)",
        &orientation_angle));
    EXPECT_EQ(angle, orientation_angle);

    EXPECT_TRUE(ExecuteScriptAndExtractString(
        root->current_frame_host(),
        "window.domAutomationController.send(screen.orientation.type)",
        &orientation_type));
    EXPECT_EQ(types[i], orientation_type);
    EXPECT_TRUE(ExecuteScriptAndExtractString(
        child->current_frame_host(),
        "window.domAutomationController.send(screen.orientation.type)",
        &orientation_type));
    EXPECT_EQ(types[i], orientation_type);
  }
}

// Regression test for triggering a screen orientation change for a pending
// main frame RenderFrameHost.  See https://crbug.com/764202.  In the bug, this
// was triggered via the DevTools audit panel and
// WidgetMsg_EnableDeviceEmulation, which calls RenderWidget::Resize on the
// renderer side.  The test fakes this by directly sending the resize message
// to the widget.
IN_PROC_BROWSER_TEST_F(ScreenOrientationOOPIFBrowserTest,
                       ScreenOrientationInPendingMainFrame) {
  GURL main_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
#if USE_AURA || defined(OS_ANDROID)
  WaitForResizeComplete(shell()->web_contents());
#endif  // USE_AURA || defined(OS_ANDROID)

  // Set up a fake Resize message with a screen orientation change.
  RenderWidgetHost* main_frame_rwh =
      web_contents()->GetMainFrame()->GetRenderWidgetHost();
  ScreenInfo screen_info;
  main_frame_rwh->GetScreenInfo(&screen_info);
  int expected_angle = (screen_info.orientation_angle + 90) % 360;

  // Start a cross-site navigation, but don't commit yet.
  GURL second_url(embedded_test_server()->GetURL("b.com", "/title1.html"));
  TestNavigationManager delayer(shell()->web_contents(), second_url);
  shell()->LoadURL(second_url);
  EXPECT_TRUE(delayer.WaitForRequestStart());

  FrameTreeNode* root = web_contents()->GetFrameTree()->root();
  RenderFrameHostImpl* pending_rfh =
      root->render_manager()->speculative_frame_host();

  // Send the orientation change to the pending RFH's widget.
  static_cast<RenderWidgetHostImpl*>(pending_rfh->GetRenderWidgetHost())
      ->SetScreenOrientationForTesting(expected_angle,
                                       screen_info.orientation_type);

  // Let the navigation finish and make sure it succeeded.
  delayer.WaitForNavigationFinished();
  EXPECT_EQ(second_url, web_contents()->GetMainFrame()->GetLastCommittedURL());

#if USE_AURA || defined(OS_ANDROID)
  WaitForResizeComplete(shell()->web_contents());
#endif  // USE_AURA || defined(OS_ANDROID)

  int orientation_angle;
  EXPECT_TRUE(ExecuteScriptAndExtractInt(
      root->current_frame_host(),
      "window.domAutomationController.send(screen.orientation.angle)",
      &orientation_angle));
  EXPECT_EQ(expected_angle, orientation_angle);
}

#ifdef OS_ANDROID
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

  FrameTreeNode* root = web_contents()->GetFrameTree()->root();
  FrameTreeNode* child = root->child_at(0);
  RenderFrameHostImpl* frames[] = {root->current_frame_host(),
                                   child->current_frame_host()};

  EXPECT_TRUE(ExecuteScript(root->current_frame_host(),
                            "document.body.webkitRequestFullscreen()"));
  for (const char* type : types) {
    std::string script =
        base::StringPrintf("screen.orientation.lock('%s')", type);
    EXPECT_TRUE(ExecuteScript(child->current_frame_host(), script));

    for (auto* frame : frames) {
      std::string orientation_type;
      while (type != orientation_type) {
        EXPECT_TRUE(ExecuteScriptAndExtractString(
            frame,
            "window.domAutomationController.send(screen.orientation.type)",
            &orientation_type));
      }
    }

    EXPECT_TRUE(ExecuteScript(child->current_frame_host(),
                              "screen.orientation.unlock()"));
  }
}
#endif  // OS_ANDROID

} // namespace content
