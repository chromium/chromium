// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/closewatcher/close_listener_host.h"

#include "base/base_switches.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/back_forward_cache_util.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

namespace content {

namespace {

class InterceptTrackingWebContentsDelegate : public WebContentsDelegate {
 public:
  bool IsBackForwardCacheSupported(WebContents& web_contents) override {
    return true;
  }

  void DidChangeCloseSignalInterceptStatus() override {
    ++intercept_status_change_count_;
  }

  int intercept_status_change_count() const {
    return intercept_status_change_count_;
  }

 private:
  int intercept_status_change_count_ = 0;
};

}  // namespace

class CloseListenerHostBrowserTest : public ContentBrowserTest {
 public:
  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  WebContentsImpl* web_contents() const {
    return static_cast<WebContentsImpl*>(shell()->web_contents());
  }

  void InstallCloseWatcherAndSignal() {
    // Install a CloseWatcher.
    std::string script =
        "let watcher = new CloseWatcher(); "
        "watcher.onclose = () => window.document.title = 'SUCCESS';";
    EXPECT_TRUE(ExecJs(web_contents(), script));

    RenderFrameHostImpl* render_frame_host_impl =
        web_contents()->GetPrimaryFrameTree().root()->current_frame_host();
    EXPECT_TRUE(
        CloseListenerHost::GetOrCreateForCurrentDocument(render_frame_host_impl)
            ->SignalIfActive());

    const std::u16string signaled_title = u"SUCCESS";
    TitleWatcher watcher(web_contents(), signaled_title);
    watcher.AlsoWaitForTitle(signaled_title);
    EXPECT_EQ(signaled_title, watcher.WaitAndGetTitle());
  }
};

IN_PROC_BROWSER_TEST_F(CloseListenerHostBrowserTest,
                       SignalCloseWatcherIfActive) {
  NavigationController& controller = web_contents()->GetController();
  GURL main_url(embedded_test_server()->GetURL("foo.com", "/title1.html"));
  ASSERT_TRUE(NavigateToURL(shell(), main_url));
  EXPECT_EQ(1, controller.GetEntryCount());

  InstallCloseWatcherAndSignal();
}

IN_PROC_BROWSER_TEST_F(CloseListenerHostBrowserTest,
                       SignalCloseWatcherIfActiveAfterReload) {
  NavigationController& controller = web_contents()->GetController();
  GURL main_url(embedded_test_server()->GetURL("foo.com", "/title1.html"));
  ASSERT_TRUE(NavigateToURL(shell(), main_url));
  EXPECT_EQ(1, controller.GetEntryCount());

  InstallCloseWatcherAndSignal();
  ReloadBlockUntilNavigationsComplete(shell(), 1);
  InstallCloseWatcherAndSignal();
}

IN_PROC_BROWSER_TEST_F(CloseListenerHostBrowserTest, CloseWatcherOnDialog) {
  NavigationController& controller = web_contents()->GetController();
  GURL main_url(embedded_test_server()->GetURL("foo.com", "/title1.html"));
  ASSERT_TRUE(NavigateToURL(shell(), main_url));
  EXPECT_EQ(1, controller.GetEntryCount());

  // Add a dialog with closedby=none
  std::string script = R"(
    let dialog = document.createElement('dialog');
    dialog.closedBy = 'none';
    document.body.appendChild(dialog);
    dialog.showModal();
  )";
  EXPECT_TRUE(ExecJs(web_contents(), script));
  base::RunLoop().RunUntilIdle();

  // Try to signal the close watcher - should *fail* because there are no
  // close watchers that have |enabled| true.
  RenderFrameHostImpl* render_frame_host_impl =
      web_contents()->GetPrimaryFrameTree().root()->current_frame_host();
  EXPECT_FALSE(
      CloseListenerHost::GetOrCreateForCurrentDocument(render_frame_host_impl)
          ->SignalIfActive());

  // Make the dialog closable, which should allow it to be signaled.
  script = R"(
    let dialog = document.querySelector('dialog');
    dialog.closedBy = 'any';
  )";
  EXPECT_TRUE(ExecJs(web_contents(), script));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(
      CloseListenerHost::GetOrCreateForCurrentDocument(render_frame_host_impl)
          ->SignalIfActive());
}

IN_PROC_BROWSER_TEST_F(CloseListenerHostBrowserTest,
                       InterceptStatusUpdatedAfterBackForwardCache) {
  if (!BackForwardCache::IsBackForwardCacheFeatureEnabled()) {
    return;
  }

  InterceptTrackingWebContentsDelegate delegate;
  web_contents()->SetDelegate(&delegate);

  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title2.html"));

  // 1) Navigate to A and create an active CloseWatcher.
  ASSERT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostWrapper rfh_a(web_contents()->GetPrimaryMainFrame());
  EXPECT_EQ(0, delegate.intercept_status_change_count());

  EXPECT_TRUE(ExecJs(rfh_a.get(), R"(
    window.watcher = new CloseWatcher();
    window.watcher.onclose = () => { document.title = 'CLOSED_A'; };
  )"));
  // Intercept status changed: false -> true.
  EXPECT_EQ(1, delegate.intercept_status_change_count());

  // 2) Navigate to B while A's CloseWatcher is still active. A enters BFCache.
  ASSERT_TRUE(NavigateToURL(shell(), url_b));
  EXPECT_EQ(RenderFrameHost::LifecycleState::kInBackForwardCache,
            rfh_a->GetLifecycleState());
  // Intercept status changed: true -> false (since B has no CloseWatcher).
  EXPECT_EQ(2, delegate.intercept_status_change_count());

  // 3) Create a CloseWatcher on B and verify the delegate is notified.
  RenderFrameHostImpl* rfh_b = web_contents()->GetPrimaryMainFrame();
  EXPECT_TRUE(ExecJs(rfh_b, R"(
    window.watcher = new CloseWatcher();
    window.watcher.onclose = () => { document.title = 'CLOSED_B'; };
  )"));
  // Intercept status changed: false -> true.
  EXPECT_EQ(3, delegate.intercept_status_change_count());

  // 4) Signal B's CloseWatcher and verify it closes and updates status.
  EXPECT_TRUE(CloseListenerHost::GetOrCreateForCurrentDocument(rfh_b)
                  ->SignalIfActive());
  {
    const std::u16string expected_title = u"CLOSED_B";
    TitleWatcher title_watcher(web_contents(), expected_title);
    EXPECT_EQ(expected_title, title_watcher.WaitAndGetTitle());
  }
  // Intercept status changed: true -> false.
  EXPECT_EQ(4, delegate.intercept_status_change_count());

  // 5) Navigate back to A (restored from BFCache with its active CloseWatcher).
  ASSERT_TRUE(HistoryGoBack(web_contents()));
  EXPECT_EQ(rfh_a.get(), web_contents()->GetPrimaryMainFrame());
  // Intercept status changed: false -> true.
  EXPECT_EQ(5, delegate.intercept_status_change_count());

  // 6) Signal A's CloseWatcher and verify it closes and updates status.
  EXPECT_TRUE(CloseListenerHost::GetOrCreateForCurrentDocument(rfh_a.get())
                  ->SignalIfActive());
  {
    const std::u16string expected_title = u"CLOSED_A";
    TitleWatcher title_watcher(web_contents(), expected_title);
    EXPECT_EQ(expected_title, title_watcher.WaitAndGetTitle());
  }
  // Intercept status changed: true -> false.
  EXPECT_EQ(6, delegate.intercept_status_change_count());

  web_contents()->SetDelegate(shell());
}

}  // namespace content
