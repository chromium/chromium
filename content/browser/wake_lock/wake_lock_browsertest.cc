// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/run_loop.h"
#include "base/test/test_timeouts.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/test/content_browser_test_utils_internal.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

namespace content {

namespace {

const char kBlinkWakeLockFeature[] = "WakeLock";

void OnHasWakeLock(bool* out, bool has_wakelock) {
  *out = has_wakelock;
  base::RunLoop::QuitCurrentDeprecated();
}

}  // namespace

class WakeLockTest : public ContentBrowserTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(switches::kEnableBlinkFeatures,
                                    kBlinkWakeLockFeature);
    command_line->AppendSwitch(switches::kSitePerProcess);
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    EXPECT_TRUE(embedded_test_server()->Start());
    // To prevent occlusion events from changing page visibility.
    GetWebContents()->IncrementCapturerCount(gfx::Size());
  }

  void TearDownOnMainThread() override {
    GetWebContents()->DecrementCapturerCount();
  }

 protected:
  WebContents* GetWebContents() { return shell()->web_contents(); }

  WebContentsImpl* GetWebContentsImpl() {
    return static_cast<WebContentsImpl*>(GetWebContents());
  }

  RenderFrameHost* GetMainFrame() { return GetWebContents()->GetMainFrame(); }

  FrameTreeNode* GetNestedFrameNode() {
    FrameTreeNode* root = GetWebContentsImpl()->GetFrameTree()->root();
    CHECK_EQ(1U, root->child_count());
    return root->child_at(0);
  }

  RenderFrameHost* GetNestedFrame() {
    return GetNestedFrameNode()->current_frame_host();
  }

  device::mojom::WakeLock* GetRendererWakeLock() {
    return GetWebContentsImpl()->GetRendererWakeLock();
  }

  bool HasWakeLock() {
    bool has_wakelock = false;
    base::RunLoop run_loop;

    GetRendererWakeLock()->HasWakeLockForTests(
        base::BindOnce(&OnHasWakeLock, &has_wakelock));
    run_loop.Run();
    return has_wakelock;
  }

  void WaitForPossibleUpdate() {
    // As Mojo channels have no common FIFO order in respect to each other and
    // to the Chromium IPC, we cannot assume that when screen.keepAwake state
    // is changed from within a script, mojom::WakeLock will receive an
    // update request before ExecuteScript() returns. Therefore, some time slack
    // is needed to make sure that mojom::WakeLock has received any
    // possible update requests before checking the resulting wake lock state.
    base::PlatformThread::Sleep(TestTimeouts::tiny_timeout());
    RunAllPendingInMessageLoop();
  }

  void ScreenWakeLockInMainFrame() {
    EXPECT_TRUE(ExecuteScript(GetMainFrame(), "screen.keepAwake = true;"));
    WaitForPossibleUpdate();
    EXPECT_TRUE(HasWakeLock());
  }

  bool EvaluateAsBool(const ToRenderFrameHost& adapter,
                      const std::string& expr) {
    bool result;
    CHECK(ExecuteScriptAndExtractBool(
        adapter, "window.domAutomationController.send(" + expr + ");",
        &result));
    return result;
  }
};

IN_PROC_BROWSER_TEST_F(WakeLockTest, WakeLockApiIsPresent) {
  NavigateToURL(shell(), embedded_test_server()->GetURL("/simple_page.html"));
  EXPECT_TRUE(
      EvaluateAsBool(GetMainFrame(), "typeof screen.keepAwake !== undefined"));
}

IN_PROC_BROWSER_TEST_F(WakeLockTest, LockAndUnlockScreenInMainFrame) {
  NavigateToURL(shell(), embedded_test_server()->GetURL("/simple_page.html"));

  // Should not have screen wake lock initially.
  EXPECT_FALSE(HasWakeLock());

  // Check attribute 'screen.keepAwake' in main frame.
  EXPECT_FALSE(EvaluateAsBool(GetMainFrame(), "screen.keepAwake"));

  // Set keep awake flag in main frame.
  EXPECT_TRUE(ExecuteScript(GetMainFrame(), "screen.keepAwake = true;"));
  WaitForPossibleUpdate();

  // Keep awake flag should be set in main frame.
  EXPECT_TRUE(EvaluateAsBool(GetMainFrame(), "screen.keepAwake"));

  // Should create screen wake lock.
  EXPECT_TRUE(HasWakeLock());

  // Reset keep awake flag in main frame.
  EXPECT_TRUE(ExecuteScript(GetMainFrame(), "screen.keepAwake = false;"));
  WaitForPossibleUpdate();

  // Keep awake flag should not be set in main frame.
  EXPECT_FALSE(EvaluateAsBool(GetMainFrame(), "screen.keepAwake"));

  // Should release screen wake lock.
  EXPECT_FALSE(HasWakeLock());
}

IN_PROC_BROWSER_TEST_F(WakeLockTest, MultipleLockThenUnlock) {
  NavigateToURL(shell(), embedded_test_server()->GetURL("/simple_page.html"));

  // Set keep awake flag.
  EXPECT_TRUE(ExecuteScript(GetMainFrame(), "screen.keepAwake = true;"));
  WaitForPossibleUpdate();

  // Set keep awake flag again.
  EXPECT_TRUE(ExecuteScript(GetMainFrame(), "screen.keepAwake = true;"));
  WaitForPossibleUpdate();

  // Screen should still be locked.
  EXPECT_TRUE(HasWakeLock());

  // Reset keep awake flag.
  EXPECT_TRUE(ExecuteScript(GetMainFrame(), "screen.keepAwake = false;"));
  WaitForPossibleUpdate();

  // Should release screen wake lock.
  EXPECT_FALSE(HasWakeLock());
}

IN_PROC_BROWSER_TEST_F(WakeLockTest, LockInMainFrameAndNestedFrame) {
  NavigateToURL(shell(),
                embedded_test_server()->GetURL("/frame_tree/2-4.html"));
  EXPECT_FALSE(HasWakeLock());

  // Lock screen in nested frame.
  EXPECT_TRUE(ExecuteScript(GetNestedFrame(), "screen.keepAwake = true;"));
  WaitForPossibleUpdate();

  // Should create screen wake lock.
  EXPECT_TRUE(HasWakeLock());

  // screen.keepAwake should be false in the main frame.
  EXPECT_FALSE(EvaluateAsBool(GetMainFrame(), "screen.keepAwake"));

  // Lock screen in main frame.
  EXPECT_TRUE(ExecuteScript(GetMainFrame(), "screen.keepAwake = true;"));
  WaitForPossibleUpdate();

  // screen.keepAwake should be true in the main frame.
  EXPECT_TRUE(EvaluateAsBool(GetMainFrame(), "screen.keepAwake"));

  // Screen wake lock should not change.
  EXPECT_TRUE(HasWakeLock());

  // Unlock screen in nested frame.
  EXPECT_TRUE(ExecuteScript(GetNestedFrame(), "screen.keepAwake = false;"));
  WaitForPossibleUpdate();

  // Screen wake lock should be present, as the main frame is still requesting
  // it.
  EXPECT_TRUE(HasWakeLock());

  // Unlock screen in main frame.
  EXPECT_TRUE(ExecuteScript(GetMainFrame(), "screen.keepAwake = false;"));
  WaitForPossibleUpdate();

  // Screen wake lock should be released, as no frames are requesting it.
  EXPECT_FALSE(HasWakeLock());
}

IN_PROC_BROWSER_TEST_F(WakeLockTest, FrameRemoved) {
  NavigateToURL(shell(),
                embedded_test_server()->GetURL("/frame_tree/2-4.html"));
  EXPECT_FALSE(HasWakeLock());

  // Lock screen in nested frame.
  EXPECT_TRUE(ExecuteScript(GetNestedFrame(), "screen.keepAwake = true;"));
  WaitForPossibleUpdate();

  EXPECT_TRUE(HasWakeLock());

  // Remove nested frame.
  EXPECT_TRUE(ExecuteScript(GetMainFrame(),
                            "var iframe = document.getElementById('3-1-id');"
                            "iframe.parentNode.removeChild(iframe);"));

  // Screen wake lock should be released.
  EXPECT_FALSE(HasWakeLock());
}

IN_PROC_BROWSER_TEST_F(WakeLockTest, UnlockAfterTabCrashed) {
  NavigateToURL(shell(), embedded_test_server()->GetURL("/simple_page.html"));
  ScreenWakeLockInMainFrame();

  // Crash the tab.
  CrashTab(GetWebContents());

  // Screen wake lock should be released.
  EXPECT_FALSE(HasWakeLock());
}

IN_PROC_BROWSER_TEST_F(WakeLockTest, UnlockAfterNavigation) {
  NavigateToURL(shell(), embedded_test_server()->GetURL("/simple_page.html"));
  ScreenWakeLockInMainFrame();

  // Navigate to a different document.
  NavigateToURL(shell(), embedded_test_server()->GetURL("/title1.html"));

  // Screen wake lock should be released after navigation.
  EXPECT_FALSE(HasWakeLock());
}

IN_PROC_BROWSER_TEST_F(WakeLockTest, UnlockAfterNavigationToSelf) {
  NavigateToURL(shell(), embedded_test_server()->GetURL("/simple_page.html"));
  ScreenWakeLockInMainFrame();

  // Navigate to the same document.
  NavigateToURL(shell(), embedded_test_server()->GetURL("/simple_page.html"));

  // Screen wake lock should be released after navigation to the same URL.
  EXPECT_FALSE(HasWakeLock());
}

IN_PROC_BROWSER_TEST_F(WakeLockTest, KeepLockAfterInPageNavigation) {
  GURL test_url(
      embedded_test_server()->GetURL("/session_history/fragment.html"));
  GURL test_in_page_url(test_url.spec() + "#a");

  NavigateToURL(shell(), test_url);
  ScreenWakeLockInMainFrame();

  NavigateToURL(shell(), test_in_page_url);
  EXPECT_TRUE(HasWakeLock());
}

IN_PROC_BROWSER_TEST_F(WakeLockTest, UnlockAfterReload) {
  NavigateToURL(shell(), embedded_test_server()->GetURL("/simple_page.html"));
  ScreenWakeLockInMainFrame();

  shell()->Reload();
  WaitForLoadStop(GetWebContents());

  // Screen wake lock should be released after reload.
  EXPECT_FALSE(HasWakeLock());
}

IN_PROC_BROWSER_TEST_F(WakeLockTest, BrowserInitiatedFrameNavigation) {
  NavigateToURL(shell(),
                embedded_test_server()->GetURL("/frame_tree/2-4.html"));

  EXPECT_FALSE(HasWakeLock());

  // Lock screen in nested frame.
  EXPECT_TRUE(ExecuteScript(GetNestedFrame(), "screen.keepAwake = true;"));
  WaitForPossibleUpdate();

  // Screen wake lock should be present.
  EXPECT_TRUE(HasWakeLock());

  // Navigate the nested frame (browser-initiated).
  NavigateFrameToURL(GetNestedFrameNode(),
                     embedded_test_server()->GetURL("/simple_page.html"));
  WaitForPossibleUpdate();

  // Screen wake lock should be released.
  EXPECT_FALSE(HasWakeLock());
}

IN_PROC_BROWSER_TEST_F(WakeLockTest, RendererInitiatedFrameNavigation) {
  NavigateToURL(shell(),
                embedded_test_server()->GetURL("/frame_tree/2-4.html"));

  EXPECT_FALSE(HasWakeLock());

  // Lock screen in nested frame.
  EXPECT_TRUE(ExecuteScript(GetNestedFrame(), "screen.keepAwake = true;"));
  WaitForPossibleUpdate();

  // Screen wake lock should be present.
  EXPECT_TRUE(HasWakeLock());

  // Navigate the nested frame (renderer-initiated).
  NavigateIframeToURL(GetWebContents(), "3-1-id",
                      embedded_test_server()->GetURL("/simple_page.html"));
  WaitForPossibleUpdate();

  // Screen wake lock should be released.
  EXPECT_FALSE(HasWakeLock());
}

IN_PROC_BROWSER_TEST_F(WakeLockTest, OutOfProcessFrame) {
  NavigateToURL(shell(), embedded_test_server()->GetURL(
                             "a.com", "/cross_site_iframe_factory.html?a(a)"));
  EXPECT_FALSE(HasWakeLock());

  // Ensure that the nested frame is same-process.
  EXPECT_FALSE(GetNestedFrame()->IsCrossProcessSubframe());

  // Lock screen in same-site nested frame.
  EXPECT_TRUE(ExecuteScript(GetNestedFrame(), "screen.keepAwake = true;"));
  WaitForPossibleUpdate();
  EXPECT_TRUE(HasWakeLock());

  // Grab a watcher for the RenderFrameHost of the first site.
  RenderFrameDeletedObserver frame_observer(
      GetNestedFrameNode()->current_frame_host());

  // Navigate nested frame to a cross-site document.
  NavigateFrameToURL(GetNestedFrameNode(), embedded_test_server()->GetURL(
                                               "b.com", "/simple_page.html"));
  WaitForPossibleUpdate();

  // Ensure that a new process has been created for the nested frame.
  EXPECT_TRUE(GetNestedFrame()->IsCrossProcessSubframe());

  // While the navigation to the second URL has completed, the teardown of the
  // host-side objects for the first URL may not have yet. The WakeLock will
  // only be released once the first renderer is cleaned up since it is held
  // by that renderer.
  // TODO(crbug.com/899384): This races with the new renderer then, would it
  // cause us to release a WakeLock that it requested before the old renderer
  // was torn down?
  frame_observer.WaitUntilDeleted();

  // Screen wake lock should be released.
  EXPECT_FALSE(HasWakeLock());

  // Lock screen in the cross-site nested frame.
  EXPECT_TRUE(ExecuteScript(GetNestedFrame(), "screen.keepAwake = true;"));
  WaitForPossibleUpdate();

  // Screen wake lock should be created.
  EXPECT_TRUE(HasWakeLock());
}

IN_PROC_BROWSER_TEST_F(WakeLockTest, UnlockAfterCrashOutOfProcessFrame) {
  // Load a page with cross-site iframe.
  NavigateToURL(shell(), embedded_test_server()->GetURL(
                             "a.com", "/cross_site_iframe_factory.html?a(b)"));
  EXPECT_FALSE(HasWakeLock());

  // Ensure that a new process has been created for the nested frame.
  EXPECT_TRUE(GetNestedFrame()->IsCrossProcessSubframe());

  // Lock screen in cross-site nested frame.
  EXPECT_TRUE(ExecuteScript(GetNestedFrame(), "screen.keepAwake = true;"));
  WaitForPossibleUpdate();
  EXPECT_TRUE(HasWakeLock());

  // Crash process that owns the out-of-process frame.
  RenderProcessHostWatcher watcher(
      GetNestedFrame()->GetProcess(),
      RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  GetNestedFrame()->GetProcess()->Shutdown(0);
  watcher.Wait();

  // Screen wake lock should be released.
  EXPECT_FALSE(HasWakeLock());
}

}  // namespace content
