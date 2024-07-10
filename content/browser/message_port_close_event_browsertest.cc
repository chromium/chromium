// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/site_isolation_policy.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/back_forward_cache_util.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/test/content_browser_test_utils_internal.h"
#include "net/dns/mock_host_resolver.h"

namespace content {

class MessagePortCloseEventBrowserTest : public ContentBrowserTest {
 public:
  MessagePortCloseEventBrowserTest() {
    InitBackForwardCacheFeature(&feature_list_for_back_forward_cache_,
                                /*enable_back_forward_cache=*/true);
  }

  ~MessagePortCloseEventBrowserTest() override = default;

  void SetUpOnMainThread() override {
    // Setup the server to allow serving separate sites, so we can perform
    // cross-process navigation.
    host_resolver()->AddRule("*", "127.0.0.1");
  }

 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(switches::kEnableBlinkFeatures,
                                    "MessagePortCloseEvent");
  }

  RenderFrameHost* GetPrimaryMainFrame() {
    return shell()->web_contents()->GetPrimaryMainFrame();
  }

  base::test::ScopedFeatureList feature_list_for_back_forward_cache_;
};

// Confirm the close event is fired when the page crashes.
IN_PROC_BROWSER_TEST_F(MessagePortCloseEventBrowserTest,
                       CloseEventHappensIfProcessCrashes) {
  // If site isolation is turned off, A and B shares the same SiteInstance.
  // So, when A crashes, B also crashes.
  if (!SiteIsolationPolicy::UseDedicatedProcessesForAllSites()) {
    GTEST_SKIP() << "test requires site isolation";
  }

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // 1) Navigate to A and open a popup.
  ASSERT_TRUE(NavigateToURL(shell(), url_a));
  WebContents* contents_a = shell()->web_contents();
  RenderFrameHostImplWrapper rfh_a(GetPrimaryMainFrame());
  Shell* popup = OpenPopup(rfh_a.get(), url_b, "");
  WebContents* contents_b = popup->web_contents();
  ASSERT_EQ(2u, rfh_a->GetSiteInstance()->GetRelatedActiveContentsCount());

  // 2) Set up a MessageChannel between page A and page B. The message channel
  // is created in page B and one of the ports is passed to page A.
  ASSERT_TRUE(ExecJs(contents_a, R"(
      window.onmessage = (e) => {
        const port = e.ports[0];
        port.start();
      }
    )"));

  ASSERT_TRUE(ExecJs(contents_b, R"(
      const {port1, port2} = new MessageChannel();
      port1.start();
      var closeEventPromise = new Promise(resolve => port1.onclose = resolve);
      window.opener.postMessage('', '*', [port2]);
    )"));

  // 3) Crash the renderer A.
  CrashTab(contents_a);

  // 4) Confirm the close event is fired on the port in page B.
  EXPECT_TRUE(ExecJs(contents_b, "closeEventPromise"));
}

// Confirm the close event is fired when the document stored in the BFCache
// is destroyed.
IN_PROC_BROWSER_TEST_F(MessagePortCloseEventBrowserTest,
                       CloseEventHappensIfPageEvictedFromBackForwardCache) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // 1) Load a page that registers a service worker.
  ASSERT_TRUE(NavigateToURL(shell(),
                            embedded_test_server()->GetURL(
                                "/service_worker/create_service_worker.html")));
  ASSERT_EQ("DONE",
            EvalJs(shell(), "register('message_port_close_event.js');"));

  // 2) Load the page again so we are controlled.
  ASSERT_TRUE(NavigateToURL(shell(),
                            embedded_test_server()->GetURL(
                                "/service_worker/create_service_worker.html")));
  ASSERT_EQ(true, EvalJs(shell(), "!!navigator.serviceWorker.controller"));

  // 3) Establish message port connection with the service worker.
  ASSERT_TRUE(ExecJs(shell(), R"(
      const {port1, port2} = new MessageChannel();
      port1.start();
      const ctrl = navigator.serviceWorker.controller;
      ctrl.postMessage("init", [port2]);
    )"));
  RenderFrameHostImplWrapper rfh_1(GetPrimaryMainFrame());
  WebContents* web_contents = shell()->web_contents();

  // 4) Navigate to the empty page in the service worker's scope and confirm the
  // previous page is in BackForwardCache.
  ASSERT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("/service_worker/empty.html")));
  ASSERT_TRUE(rfh_1->IsInBackForwardCache());
  RenderFrameHostImplWrapper rfh_2(GetPrimaryMainFrame());

  // 5) Ask the service worker to create the promise that resolves when
  // it receives a close event from the BFCached page when that page gets
  // evicted.
  ASSERT_TRUE(ExecJs(rfh_2.get(), R"(
        navigator.serviceWorker.controller.postMessage("wait for close event");
        var closeEventPromise = new Promise(resolve => {
            navigator.serviceWorker.addEventListener('message', (event) => {
            resolve(event.data);
        })});
    )"));

  // 6) Flush the cache and evict the previously BFCached page.
  web_contents->GetController().GetBackForwardCache().Flush();

  // 7) Confirm the previous page is evicted.
  ASSERT_TRUE(rfh_1.WaitUntilRenderFrameDeleted());

  // 8) Confirm the close event is fired on the port in the service worker.
  EXPECT_EQ("close event is fired", EvalJs(rfh_2.get(), "closeEventPromise"));
}
}  // namespace content
