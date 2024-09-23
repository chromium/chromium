// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/closewatcher/close_listener_host.h"

#include "base/base_switches.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

namespace content {

class CloseListenerHostBrowserTest : public ContentBrowserTest {
 public:
  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(
        switches::kEnableExperimentalWebPlatformFeatures);
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

}  // namespace content
