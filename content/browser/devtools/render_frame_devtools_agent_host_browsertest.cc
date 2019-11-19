// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/render_frame_devtools_agent_host.h"
#include "content/browser/frame_host/frame_tree_node.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/devtools_agent_host_client.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/back_forward_cache_util.h"
#include "content/public/test/browser_test_base.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/shell/browser/shell.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/controllable_http_response.h"

namespace content {

class RenderFrameDevToolsAgentHostBrowserTest : public ContentBrowserTest {
 public:
  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    SetupCrossSiteRedirector(embedded_test_server());
  }
};

namespace {

// A DevToolsAgentHostClient implementation doing nothing.
class StubDevToolsAgentHostClient : public content::DevToolsAgentHostClient {
 public:
  StubDevToolsAgentHostClient() {}
  ~StubDevToolsAgentHostClient() override {}
  void AgentHostClosed(content::DevToolsAgentHost* agent_host) override {}
  void DispatchProtocolMessage(content::DevToolsAgentHost* agent_host,
                               const std::string& message) override {}
};

}  // namespace

// This test checks which RenderFrameHostImpl the RenderFrameDevToolsAgentHost
// is tracking while a cross-site navigation is canceled after having reached
// the ReadyToCommit stage.
// See https://crbug.com/695203.
IN_PROC_BROWSER_TEST_F(RenderFrameDevToolsAgentHostBrowserTest,
                       CancelCrossOriginNavigationAfterReadyToCommit) {
  net::test_server::ControllableHttpResponse response_b(embedded_test_server(),
                                                        "/response_b");
  net::test_server::ControllableHttpResponse response_c(embedded_test_server(),
                                                        "/response_c");
  EXPECT_TRUE(embedded_test_server()->Start());

  // 1) Loads a document.
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  WebContentsImpl* web_contents_impl =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  FrameTreeNode* root = web_contents_impl->GetFrameTree()->root();

  // 2) Open DevTools.
  scoped_refptr<DevToolsAgentHost> devtools_agent =
      DevToolsAgentHost::GetOrCreateFor(web_contents_impl);
  RenderFrameDevToolsAgentHost* rfh_devtools_agent =
      static_cast<RenderFrameDevToolsAgentHost*>(devtools_agent.get());

  // 3) Tries to navigate cross-site, but do not commit the navigation.
  //    Send only the headers such that it reaches the ReadyToCommit stage, but
  //    not further.

  // 3.a) Navigation: Start.
  GURL url_b(embedded_test_server()->GetURL("b.com", "/response_b"));
  TestNavigationManager observer_b(shell()->web_contents(), url_b);
  shell()->LoadURL(url_b);
  EXPECT_TRUE(observer_b.WaitForRequestStart());

  RenderFrameHostImpl* current_rfh =
      root->render_manager()->current_frame_host();
  RenderFrameHostImpl* speculative_rfh_b =
      root->render_manager()->speculative_frame_host();
  EXPECT_TRUE(current_rfh);
  EXPECT_TRUE(speculative_rfh_b);
  EXPECT_EQ(current_rfh, rfh_devtools_agent->GetFrameHostForTesting());

  // 3.b) Navigation: ReadyToCommit.
  observer_b.ResumeNavigation();  // Send the request.
  response_b.WaitForRequest();
  response_b.Send(
      "HTTP/1.1 200 OK\r\n"
      "Content-Type: text/html; charset=utf-8\r\n"
      "\r\n");
  EXPECT_TRUE(observer_b.WaitForResponse());  // Headers are received.
  observer_b.ResumeNavigation();  // ReadyToCommitNavigation is called.
  EXPECT_EQ(speculative_rfh_b, rfh_devtools_agent->GetFrameHostForTesting());
  auto speculative_rfh_b_site_id =
      speculative_rfh_b->GetSiteInstance()->GetId();
  if (AreDefaultSiteInstancesEnabled())
    EXPECT_TRUE(speculative_rfh_b->GetSiteInstance()->IsDefaultSiteInstance());

  // 4) Navigate elsewhere, it will cancel the previous navigation.

  // 4.a) Navigation: Start.
  GURL url_c(embedded_test_server()->GetURL("c.com", "/response_c"));
  TestNavigationManager observer_c(shell()->web_contents(), url_c);
  shell()->LoadURL(url_c);
  EXPECT_TRUE(observer_c.WaitForRequestStart());
  RenderFrameHostImpl* speculative_rfh_c =
      root->render_manager()->speculative_frame_host();
  EXPECT_TRUE(speculative_rfh_c);
  auto speculative_rfh_c_site_id =
      speculative_rfh_c->GetSiteInstance()->GetId();
  if (AreDefaultSiteInstancesEnabled()) {
    // Verify that this new URL also belongs to the default SiteInstance and
    // therefore the RenderFrameHost from the previous navigation could be
    // reused.
    EXPECT_TRUE(speculative_rfh_c->GetSiteInstance()->IsDefaultSiteInstance());
    EXPECT_EQ(speculative_rfh_c, rfh_devtools_agent->GetFrameHostForTesting());
    EXPECT_EQ(speculative_rfh_b_site_id, speculative_rfh_c_site_id);
  } else {
    // Verify that the RenderFrameHost is restored because the new URL required
    // a new SiteInstance.
    EXPECT_EQ(current_rfh, rfh_devtools_agent->GetFrameHostForTesting());
    EXPECT_NE(speculative_rfh_b_site_id, speculative_rfh_c_site_id);
  }

  // 4.b) Navigation: ReadyToCommit.
  observer_c.ResumeNavigation();  // Send the request.
  response_c.WaitForRequest();
  response_c.Send(
      "HTTP/1.1 200 OK\r\n"
      "Content-Type: text/html; charset=utf-8\r\n"
      "\r\n");
  EXPECT_TRUE(observer_c.WaitForResponse());  // Headers are received.
  observer_c.ResumeNavigation();  // ReadyToCommitNavigation is called.
  EXPECT_EQ(speculative_rfh_c, rfh_devtools_agent->GetFrameHostForTesting());

  // 4.c) Navigation: Commit.
  response_c.Send("<html><body> response's body </body></html>");
  response_c.Done();
  observer_c.WaitForNavigationFinished();
  EXPECT_EQ(speculative_rfh_c, root->render_manager()->current_frame_host());
  EXPECT_EQ(speculative_rfh_c, rfh_devtools_agent->GetFrameHostForTesting());
}

// Regression test for https://crbug.com/795694.
// * Open chrome://dino
// * Open DevTools
// * Reload from DevTools must work.
IN_PROC_BROWSER_TEST_F(RenderFrameDevToolsAgentHostBrowserTest,
                       ReloadDinoPage) {
  // 1) Navigate to chrome://dino.
  GURL dino_url(kChromeUIScheme + std::string("://") + kChromeUIDinoHost);
  EXPECT_FALSE(NavigateToURL(shell(), dino_url));

  // 2) Open DevTools.
  scoped_refptr<DevToolsAgentHost> devtools_agent_host =
      DevToolsAgentHost::GetOrCreateFor(shell()->web_contents());
  StubDevToolsAgentHostClient devtools_agent_host_client;
  devtools_agent_host->AttachClient(&devtools_agent_host_client);

  // 3) Reload from DevTools.
  TestNavigationObserver reload_observer(shell()->web_contents());
  devtools_agent_host->DispatchProtocolMessage(
      &devtools_agent_host_client,
      R"({"id":1,"method": "Page.reload"})");
  reload_observer.Wait();
  devtools_agent_host->DetachClient(&devtools_agent_host_client);
}

IN_PROC_BROWSER_TEST_F(RenderFrameDevToolsAgentHostBrowserTest,
                       DevToolsDisableBackForwardCache) {
  content::BackForwardCacheDisabledTester tester;
  EXPECT_TRUE(embedded_test_server()->Start());

  // Navigate to a page.
  const GURL a_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), a_url));
  content::RenderFrameHost* main_frame =
      shell()->web_contents()->GetMainFrame();
  int process_id = main_frame->GetProcess()->GetID();
  int frame_routing_id = main_frame->GetRoutingID();

  // Open DevTools.
  scoped_refptr<DevToolsAgentHost> devtools_agent_host =
      DevToolsAgentHost::GetOrCreateFor(shell()->web_contents());
  StubDevToolsAgentHostClient devtools_agent_host_client;
  devtools_agent_host->AttachClient(&devtools_agent_host_client);

  // Navigate away from the page. This should block bfcache.
  const GURL b_url(embedded_test_server()->GetURL("b.com", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), b_url));
  EXPECT_TRUE(tester.IsDisabledForFrameWithReason(
      process_id, frame_routing_id, "RenderFrameDevToolsAgentHost"));

  devtools_agent_host->DetachClient(&devtools_agent_host_client);
}

}  // namespace content
