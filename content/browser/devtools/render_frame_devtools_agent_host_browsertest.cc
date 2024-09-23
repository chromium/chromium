// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/342213636): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "content/browser/devtools/render_frame_devtools_agent_host.h"

#include <string_view>

#include "build/build_config.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/content_navigation_policy.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/devtools_agent_host_client.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/result_codes.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_base.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/fenced_frame_test_util.h"
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

const char kFencedFramePath[] = "/devtools/navigation.html";

// A DevToolsAgentHostClient implementation doing nothing.
class StubDevToolsAgentHostClient : public content::DevToolsAgentHostClient {
 public:
  StubDevToolsAgentHostClient() {}
  ~StubDevToolsAgentHostClient() override {}
  void AgentHostClosed(content::DevToolsAgentHost* agent_host) override {}
  void DispatchProtocolMessage(content::DevToolsAgentHost* agent_host,
                               base::span<const uint8_t> message) override {}
  bool MayAttachToURL(const GURL& url, bool is_webui) override {
    // Return a false in case that the url is a fenced frame test url to detach
    // the attached client in order to test that a fenced frame calls
    // OnNavigationRequestWillBeSent through the outer document.
    if (url.path_piece().find(kFencedFramePath) != std::string_view::npos) {
      return false;
    }
    return true;
  }
};

}  // namespace

// This test checks which RenderFrameHostImpl the RenderFrameDevToolsAgentHost
// is tracking while a cross-site navigation is canceled after having reached
// the ReadyToCommit stage.
// See https://crbug.com/695203.
// TODO(crbug.com/40916125): Re-enable this test
#define MAYBE_CancelCrossOriginNavigationAfterReadyToCommit \
  DISABLED_CancelCrossOriginNavigationAfterReadyToCommit
IN_PROC_BROWSER_TEST_F(RenderFrameDevToolsAgentHostBrowserTest,
                       MAYBE_CancelCrossOriginNavigationAfterReadyToCommit) {
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
  FrameTreeNode* root = web_contents_impl->GetPrimaryFrameTree().root();

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
  observer_b.WaitForSpeculativeRenderFrameHostCreation();
  RenderFrameHostImpl* current_rfh =
      root->render_manager()->current_frame_host();
  RenderFrameHostImpl* speculative_rfh_b =
      root->render_manager()->speculative_frame_host();
  EXPECT_TRUE(current_rfh);
  EXPECT_TRUE(speculative_rfh_b);
  EXPECT_EQ(current_rfh, rfh_devtools_agent->GetFrameHostForTesting());

  // 3.b) Navigation: ReadyToCommit.
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

  // 4) Navigate elsewhere, it will cancel the previous navigation if navigation
  // queueing is not enabled.

  // 4.a) Navigation: Start.
  GURL url_c(embedded_test_server()->GetURL("c.com", "/response_c"));
  TestNavigationManager observer_c(shell()->web_contents(), url_c);
  shell()->LoadURL(url_c);
  EXPECT_TRUE(observer_c.WaitForRequestStart());

  RenderFrameHostImpl* speculative_rfh_c = nullptr;

  if (ShouldQueueNavigationsWhenPendingCommitRFHExists()) {
    // When navigation queueing is enabled, starting a new navigation won't
    // cancel an existing pending commit navigation, so wait for the first
    // navigation to finish first before continuing.
    EXPECT_EQ(speculative_rfh_b,
              root->render_manager()->speculative_frame_host());
    EXPECT_EQ(speculative_rfh_b, rfh_devtools_agent->GetFrameHostForTesting());
    ASSERT_TRUE(observer_b.WaitForNavigationFinished());
  } else {
    speculative_rfh_c = root->render_manager()->speculative_frame_host();
    EXPECT_TRUE(speculative_rfh_c);
    auto speculative_rfh_c_site_id =
        speculative_rfh_c->GetSiteInstance()->GetId();
    if (AreDefaultSiteInstancesEnabled()) {
      // Verify that this new URL also belongs to the default SiteInstance and
      // therefore the RenderFrameHost from the previous navigation could be
      // reused.
      EXPECT_TRUE(
          speculative_rfh_c->GetSiteInstance()->IsDefaultSiteInstance());
      EXPECT_EQ(speculative_rfh_c,
                rfh_devtools_agent->GetFrameHostForTesting());
      EXPECT_EQ(speculative_rfh_b_site_id, speculative_rfh_c_site_id);
    } else {
      // Verify that the RenderFrameHost is restored because the new URL
      // required a new SiteInstance.
      EXPECT_EQ(current_rfh, rfh_devtools_agent->GetFrameHostForTesting());
      EXPECT_NE(speculative_rfh_b_site_id, speculative_rfh_c_site_id);
    }
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
  speculative_rfh_c = root->render_manager()->speculative_frame_host();
  EXPECT_EQ(speculative_rfh_c, rfh_devtools_agent->GetFrameHostForTesting());

  // 4.c) Navigation: Commit.
  response_c.Send("<html><body> response's body </body></html>");
  response_c.Done();
  ASSERT_TRUE(observer_c.WaitForNavigationFinished());
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
  constexpr char kMsg[] = R"({"id":1,"method":"Page.reload"})";
  devtools_agent_host->DispatchProtocolMessage(
      &devtools_agent_host_client,
      base::as_bytes(base::make_span(kMsg, strlen(kMsg))));
  reload_observer.Wait();
  devtools_agent_host->DetachClient(&devtools_agent_host_client);
}

IN_PROC_BROWSER_TEST_F(RenderFrameDevToolsAgentHostBrowserTest,
                       CheckDebuggerAttachedToTabTarget) {
  EXPECT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url_a));

  WebContents* web_contents = shell()->web_contents();

  scoped_refptr<DevToolsAgentHost> devtools_agent_host =
      DevToolsAgentHost::GetOrCreateForTab(web_contents);
  StubDevToolsAgentHostClient devtools_agent_host_client;
  devtools_agent_host->AttachClient(&devtools_agent_host_client);

  EXPECT_TRUE(DevToolsAgentHost::IsDebuggerAttached(web_contents));
  devtools_agent_host->DetachClient(&devtools_agent_host_client);
}

class RenderFrameDevToolsAgentHostFencedFrameBrowserTest
    : public RenderFrameDevToolsAgentHostBrowserTest {
 public:
  RenderFrameDevToolsAgentHostFencedFrameBrowserTest() = default;
  ~RenderFrameDevToolsAgentHostFencedFrameBrowserTest() override = default;
  RenderFrameDevToolsAgentHostFencedFrameBrowserTest(
      const RenderFrameDevToolsAgentHostFencedFrameBrowserTest&) = delete;

  RenderFrameDevToolsAgentHostFencedFrameBrowserTest& operator=(
      const RenderFrameDevToolsAgentHostFencedFrameBrowserTest&) = delete;

  void SetUpOnMainThread() override {
    RenderFrameDevToolsAgentHostBrowserTest::SetUpOnMainThread();
    embedded_test_server()->ServeFilesFromSourceDirectory("content/test/data");
    EXPECT_TRUE(embedded_test_server()->Start());
  }

  content::test::FencedFrameTestHelper& fenced_frame_test_helper() {
    return fenced_frame_helper_;
  }

  bool IsCrashed(RenderFrameDevToolsAgentHost* rfh_devtools_agent) {
    return rfh_devtools_agent->render_frame_crashed_;
  }

 private:
  content::test::FencedFrameTestHelper fenced_frame_helper_;
};

IN_PROC_BROWSER_TEST_F(RenderFrameDevToolsAgentHostFencedFrameBrowserTest,
                       CallNavigationRequestCallbackViaOuterDocument) {
  GURL url = embedded_test_server()->GetURL("/title1.html");
  EXPECT_TRUE(NavigateToURL(shell(), url));

  scoped_refptr<DevToolsAgentHost> devtools_agent_host =
      DevToolsAgentHost::GetOrCreateFor(shell()->web_contents());
  StubDevToolsAgentHostClient devtools_agent_host_client;
  devtools_agent_host->AttachClient(&devtools_agent_host_client);
  EXPECT_TRUE(devtools_agent_host->IsAttached());

  // Create a fenced frame.
  GURL fenced_frame_url =
      embedded_test_server()->GetURL("a.com", kFencedFramePath);
  content::RenderFrameHostImpl* fenced_frame_host =
      static_cast<content::RenderFrameHostImpl*>(
          fenced_frame_test_helper().CreateFencedFrame(
              shell()->web_contents()->GetPrimaryMainFrame(),
              fenced_frame_url));
  ASSERT_TRUE(fenced_frame_host);
  // The client should be detached by the fenced frame calling
  // OnNavigationRequestWillBeSent through the outer document.
  EXPECT_FALSE(devtools_agent_host->IsAttached());
}

IN_PROC_BROWSER_TEST_F(RenderFrameDevToolsAgentHostFencedFrameBrowserTest,
                       PageCrashInFencedFrame) {
  // Ensure all sites get dedicated processes during the test.
  IsolateAllSitesForTesting(base::CommandLine::ForCurrentProcess());

  GURL url = embedded_test_server()->GetURL("a.com", "/title1.html");
  EXPECT_TRUE(NavigateToURL(shell(), url));

  // Create a fenced frame.
  const GURL fenced_frame_url =
      embedded_test_server()->GetURL("b.com", "/fenced_frames/title1.html");

  RenderFrameHostImpl* fenced_frame_host =
      static_cast<content::RenderFrameHostImpl*>(
          fenced_frame_test_helper().CreateFencedFrame(
              shell()->web_contents()->GetPrimaryMainFrame(),
              fenced_frame_url));
  ASSERT_TRUE(fenced_frame_host);

  // Terminate the fenced frame process.
  {
    RenderProcessHostWatcher termination_observer(
        fenced_frame_host->GetProcess(),
        RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
    EXPECT_TRUE(
        fenced_frame_host->GetProcess()->Shutdown(content::RESULT_CODE_KILLED));
    termination_observer.Wait();
  }
  EXPECT_FALSE(fenced_frame_host->IsRenderFrameLive());

  // Open DevTools.
  scoped_refptr<DevToolsAgentHost> devtools_agent =
      RenderFrameDevToolsAgentHost::GetOrCreateFor(
          static_cast<RenderFrameHostImpl*>(
              shell()->web_contents()->GetPrimaryMainFrame())
              ->frame_tree_node());
  RenderFrameDevToolsAgentHost* main_rfh_devtools_agent =
      static_cast<RenderFrameDevToolsAgentHost*>(devtools_agent.get());

  scoped_refptr<DevToolsAgentHost> ff_devtools_agent =
      RenderFrameDevToolsAgentHost::GetOrCreateFor(
          fenced_frame_host->frame_tree_node());
  RenderFrameDevToolsAgentHost* ff_rfh_devtools_agent =
      static_cast<RenderFrameDevToolsAgentHost*>(ff_devtools_agent.get());

  EXPECT_FALSE(IsCrashed(main_rfh_devtools_agent));
  EXPECT_TRUE(IsCrashed(ff_rfh_devtools_agent));
}

}  // namespace content
