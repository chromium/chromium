// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/bind_test_util.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "content/browser/renderer_host/navigation_request.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/content_navigation_policy.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/url_loader_interceptor.h"
#include "content/shell/browser/shell.h"
#include "content/test/content_browser_test_utils_internal.h"
#include "content/test/render_document_feature.h"
#include "net/base/escape.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/default_handlers.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/test/embedded_test_server/request_handler_util.h"
#include "services/network/public/cpp/cross_origin_opener_policy.h"
#include "services/network/public/cpp/features.h"
#include "testing/gmock/include/gmock/gmock.h"

using ::testing::HasSubstr;

namespace content {

namespace {

network::CrossOriginOpenerPolicy CoopSameOrigin() {
  network::CrossOriginOpenerPolicy coop;
  coop.value = network::mojom::CrossOriginOpenerPolicyValue::kSameOrigin;
  return coop;
}

network::CrossOriginOpenerPolicy CoopSameOriginPlusCoep() {
  network::CrossOriginOpenerPolicy coop;
  coop.value =
      network::mojom::CrossOriginOpenerPolicyValue::kSameOriginPlusCoep;
  return coop;
}

network::CrossOriginOpenerPolicy CoopSameOriginAllowPopups() {
  network::CrossOriginOpenerPolicy coop;
  coop.value =
      network::mojom::CrossOriginOpenerPolicyValue::kSameOriginAllowPopups;
  return coop;
}

network::CrossOriginOpenerPolicy CoopUnsafeNone() {
  network::CrossOriginOpenerPolicy coop;
  // Using the default value.
  return coop;
}

std::unique_ptr<net::test_server::HttpResponse>
CrossOriginIsolatedCrossOriginRedirectHandler(
    const net::test_server::HttpRequest& request) {
  GURL request_url = request.GetURL();
  std::string dest =
      base::UnescapeBinaryURLComponent(request_url.query_piece());
  net::test_server::RequestQuery query =
      net::test_server::ParseQuery(request_url);

  auto http_response = std::make_unique<net::test_server::BasicHttpResponse>();
  http_response->set_code(net::HttpStatusCode::HTTP_FOUND);
  http_response->AddCustomHeader("Location", dest);
  http_response->AddCustomHeader("Cross-Origin-Opener-Policy", "same-origin");
  http_response->AddCustomHeader("Cross-Origin-Embedder-Policy",
                                 "require-corp");
  return http_response;
}

class CrossOriginOpenerPolicyBrowserTest
    : public ContentBrowserTest,
      public ::testing::WithParamInterface<std::tuple<std::string, bool>> {
 public:
  CrossOriginOpenerPolicyBrowserTest()
      : https_server_(net::EmbeddedTestServer::TYPE_HTTPS) {
    // Enable COOP/COEP:
    feature_list_.InitWithFeatures(
        {network::features::kCrossOriginOpenerPolicy,
         network::features::kCrossOriginOpenerPolicyReporting,
         network::features::kCrossOriginEmbedderPolicy,
         network::features::kCrossOriginIsolated},
        {});

    // Enable RenderDocument:
    InitAndEnableRenderDocumentFeature(&feature_list_for_render_document_,
                                       std::get<0>(GetParam()));
    // Enable BackForwardCache:
    if (std::get<1>(GetParam())) {
      feature_list_for_back_forward_cache_.InitWithFeatures(
          {features::kBackForwardCache}, {});
    } else {
      feature_list_for_back_forward_cache_.InitWithFeatures(
          {}, {features::kBackForwardCache});
    }

    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kIgnoreCertificateErrors);
    }

  net::EmbeddedTestServer* https_server() { return &https_server_; }

 protected:
  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());

    https_server()->ServeFilesFromSourceDirectory(GetTestDataFilePath());
    SetupCrossSiteRedirector(https_server());
    net::test_server::RegisterDefaultHandlers(&https_server_);
    https_server_.RegisterDefaultHandler(base::BindRepeating(
        &net::test_server::HandlePrefixedRequest,
        "/redirect-with-coop-coep-headers",
        base::BindRepeating(CrossOriginIsolatedCrossOriginRedirectHandler)));

    ASSERT_TRUE(https_server()->Start());
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ContentBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kIgnoreCertificateErrors);
  }

  WebContentsImpl* web_contents() const {
    return static_cast<WebContentsImpl*>(shell()->web_contents());
  }

  RenderFrameHostImpl* current_frame_host() {
    return web_contents()->GetMainFrame();
  }

  base::test::ScopedFeatureList feature_list_;
  base::test::ScopedFeatureList feature_list_for_render_document_;
  base::test::ScopedFeatureList feature_list_for_back_forward_cache_;
  net::EmbeddedTestServer https_server_;
};

using VirtualBrowsingContextGroupTest = CrossOriginOpenerPolicyBrowserTest;

int VirtualBrowsingContextGroup(WebContents* wc) {
  return static_cast<WebContentsImpl*>(wc)
      ->GetMainFrame()
      ->virtual_browsing_context_group();
}

}  // namespace

IN_PROC_BROWSER_TEST_P(CrossOriginOpenerPolicyBrowserTest,
                       NewPopupCOOP_InheritsSameOrigin) {
  GURL starting_page(
      https_server()->GetURL("a.com", "/cross_site_iframe_factory.html?a(a)"));
  EXPECT_TRUE(NavigateToURL(shell(), starting_page));

  RenderFrameHostImpl* main_frame = current_frame_host();
  main_frame->set_cross_origin_opener_policy_for_testing(CoopSameOrigin());

  ShellAddedObserver shell_observer;
  RenderFrameHostImpl* iframe = main_frame->child_at(0)->current_frame_host();
  EXPECT_TRUE(ExecJs(iframe, "window.open('about:blank')"));

  RenderFrameHostImpl* popup_frame =
      static_cast<WebContentsImpl*>(shell_observer.GetShell()->web_contents())
          ->GetFrameTree()
          ->root()
          ->current_frame_host();

  EXPECT_EQ(main_frame->cross_origin_opener_policy(), CoopSameOrigin());
  EXPECT_EQ(popup_frame->cross_origin_opener_policy(), CoopSameOrigin());
}

IN_PROC_BROWSER_TEST_P(CrossOriginOpenerPolicyBrowserTest,
                       NewPopupCOOP_InheritsSameOriginAllowPopups) {
  GURL starting_page(
      https_server()->GetURL("a.com", "/cross_site_iframe_factory.html?a(a)"));
  EXPECT_TRUE(NavigateToURL(shell(), starting_page));

  RenderFrameHostImpl* main_frame = current_frame_host();
  main_frame->set_cross_origin_opener_policy_for_testing(
      CoopSameOriginAllowPopups());

  ShellAddedObserver shell_observer;
  RenderFrameHostImpl* iframe = main_frame->child_at(0)->current_frame_host();
  EXPECT_TRUE(ExecJs(iframe, "window.open('about:blank')"));

  RenderFrameHostImpl* popup_frame =
      static_cast<WebContentsImpl*>(shell_observer.GetShell()->web_contents())
          ->GetFrameTree()
          ->root()
          ->current_frame_host();

  EXPECT_EQ(main_frame->cross_origin_opener_policy(),
            CoopSameOriginAllowPopups());
  EXPECT_EQ(popup_frame->cross_origin_opener_policy(),
            CoopSameOriginAllowPopups());
}

IN_PROC_BROWSER_TEST_P(CrossOriginOpenerPolicyBrowserTest,
                       NewPopupCOOP_CrossOriginDoesNotInherit) {
  GURL starting_page(
      https_server()->GetURL("a.com", "/cross_site_iframe_factory.html?a(b)"));
  EXPECT_TRUE(NavigateToURL(shell(), starting_page));

  RenderFrameHostImpl* main_frame = current_frame_host();
  main_frame->set_cross_origin_opener_policy_for_testing(CoopSameOrigin());

  ShellAddedObserver shell_observer;
  RenderFrameHostImpl* iframe = main_frame->child_at(0)->current_frame_host();
  EXPECT_TRUE(ExecJs(iframe, "window.open('about:blank')"));

  RenderFrameHostImpl* popup_frame =
      static_cast<WebContentsImpl*>(shell_observer.GetShell()->web_contents())
          ->GetFrameTree()
          ->root()
          ->current_frame_host();

  EXPECT_EQ(main_frame->cross_origin_opener_policy(), CoopSameOrigin());
  EXPECT_EQ(popup_frame->cross_origin_opener_policy(), CoopUnsafeNone());
}

IN_PROC_BROWSER_TEST_P(
    CrossOriginOpenerPolicyBrowserTest,
    NewPopupCOOP_SameOriginPolicyAndCrossOriginIframeSetsNoopener) {
  GURL starting_page(
      https_server()->GetURL("a.com", "/cross_site_iframe_factory.html?a(b)"));
  EXPECT_TRUE(NavigateToURL(shell(), starting_page));

  RenderFrameHostImpl* main_frame = current_frame_host();
  main_frame->set_cross_origin_opener_policy_for_testing(CoopSameOrigin());

  ShellAddedObserver new_shell_observer;
  RenderFrameHostImpl* iframe = main_frame->child_at(0)->current_frame_host();
  EXPECT_TRUE(ExecJs(iframe, "window.open('about:blank')"));

  Shell* new_shell = new_shell_observer.GetShell();
  RenderFrameHostImpl* popup_frame =
      static_cast<WebContentsImpl*>(new_shell->web_contents())
          ->GetFrameTree()
          ->root()
          ->current_frame_host();

  scoped_refptr<SiteInstance> main_frame_site_instance(
      main_frame->GetSiteInstance());
  scoped_refptr<SiteInstance> iframe_site_instance(iframe->GetSiteInstance());
  scoped_refptr<SiteInstance> popup_site_instance(
      popup_frame->GetSiteInstance());

  ASSERT_TRUE(main_frame_site_instance);
  ASSERT_TRUE(iframe_site_instance);
  ASSERT_TRUE(popup_site_instance);
  EXPECT_FALSE(main_frame_site_instance->IsRelatedSiteInstance(
      popup_site_instance.get()));
  EXPECT_FALSE(
      iframe_site_instance->IsRelatedSiteInstance(popup_site_instance.get()));

  // Check that `window.opener` is not set.
  bool success = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      new_shell, "window.domAutomationController.send(window.opener == null);",
      &success));
  EXPECT_TRUE(success) << "window.opener is set";
}

IN_PROC_BROWSER_TEST_P(CrossOriginOpenerPolicyBrowserTest,
                       NetworkErrorOnSandboxedPopups) {
  GURL starting_page(https_server()->GetURL(
      "a.com", "/cross-origin-opener-policy_sandbox_popup.html"));
  GURL openee_url = https_server()->GetURL(
      "a.com", "/set-header?Cross-Origin-Opener-Policy: same-origin");
  EXPECT_TRUE(NavigateToURL(shell(), starting_page));

  ShellAddedObserver shell_observer;
  RenderFrameHostImpl* iframe =
      current_frame_host()->child_at(0)->current_frame_host();

  EXPECT_TRUE(ExecJs(iframe, JsReplace("window.open($1);", openee_url)));

  auto* popup_webcontents =
      static_cast<WebContentsImpl*>(shell_observer.GetShell()->web_contents());
  WaitForLoadStop(popup_webcontents);

  EXPECT_EQ(
      popup_webcontents->GetController().GetLastCommittedEntry()->GetPageType(),
      PAGE_TYPE_ERROR);
}

IN_PROC_BROWSER_TEST_P(CrossOriginOpenerPolicyBrowserTest,
                       NoNetworkErrorOnSandboxedDocuments) {
  GURL starting_page(https_server()->GetURL(
      "a.com", "/set-header?Content-Security-Policy: sandbox allow-scripts"));
  EXPECT_TRUE(NavigateToURL(shell(), starting_page));
  EXPECT_NE(current_frame_host()->active_sandbox_flags(),
            network::mojom::WebSandboxFlags::kNone)
      << "Document should be sandboxed.";

  GURL next_page = https_server()->GetURL(
      "a.com", "/set-header?Cross-Origin-Opener-Policy: same-origin");

  EXPECT_TRUE(NavigateToURL(shell(), next_page));
  EXPECT_EQ(
      web_contents()->GetController().GetLastCommittedEntry()->GetPageType(),
      PAGE_TYPE_NORMAL);
}

class CrossOriginPolicyHeadersObserver : public WebContentsObserver {
 public:
  explicit CrossOriginPolicyHeadersObserver(
      WebContents* web_contents,
      network::mojom::CrossOriginEmbedderPolicyValue expected_coep,
      network::CrossOriginOpenerPolicy expected_coop)
      : WebContentsObserver(web_contents),
        expected_coep_(expected_coep),
        expected_coop_(expected_coop) {}

  ~CrossOriginPolicyHeadersObserver() override = default;

  void DidRedirectNavigation(NavigationHandle* navigation_handle) override {
    // Verify that the COOP/COEP headers were parsed.
    NavigationRequest* navigation_request =
        static_cast<NavigationRequest*>(navigation_handle);
    CHECK(navigation_request->response()
              ->parsed_headers->cross_origin_embedder_policy.value ==
          expected_coep_);
    CHECK(navigation_request->response()
              ->parsed_headers->cross_origin_opener_policy == expected_coop_);
  }

  void DidFinishNavigation(NavigationHandle* navigation_handle) override {
    // Verify that the COOP/COEP headers were parsed.
    NavigationRequest* navigation_request =
        static_cast<NavigationRequest*>(navigation_handle);
    CHECK(navigation_request->response()
              ->parsed_headers->cross_origin_embedder_policy.value ==
          expected_coep_);
    CHECK(navigation_request->response()
              ->parsed_headers->cross_origin_opener_policy == expected_coop_);
  }

 private:
  network::mojom::CrossOriginEmbedderPolicyValue expected_coep_;
  network::CrossOriginOpenerPolicy expected_coop_;
};

IN_PROC_BROWSER_TEST_P(CrossOriginOpenerPolicyBrowserTest,
                       RedirectsParseCoopAndCoepHeaders) {
  GURL redirect_initial_page(https_server()->GetURL(
      "a.com", "/cross-origin-opener-policy_redirect_initial.html"));
  GURL redirect_final_page(https_server()->GetURL(
      "a.com", "/cross-origin-opener-policy_redirect_final.html"));

  CrossOriginPolicyHeadersObserver obs(
      web_contents(),
      network::mojom::CrossOriginEmbedderPolicyValue::kRequireCorp,
      CoopSameOriginPlusCoep());

  EXPECT_TRUE(
      NavigateToURL(shell(), redirect_initial_page, redirect_final_page));
}

IN_PROC_BROWSER_TEST_P(CrossOriginOpenerPolicyBrowserTest,
                       CoopIsIgnoredOverHttp) {
  WebContentsConsoleObserver console_observer(shell()->web_contents());
  console_observer.SetPattern("*Cross-Origin-Opener-Policy * ignored*");

  GURL non_coop_page(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL coop_page = embedded_test_server()->GetURL(
      "a.com", "/set-header?Cross-Origin-Opener-Policy: same-origin");

  EXPECT_TRUE(NavigateToURL(shell(), non_coop_page));
  scoped_refptr<SiteInstance> initial_site_instance(
      current_frame_host()->GetSiteInstance());

  EXPECT_TRUE(NavigateToURL(shell(), coop_page));
  if (CanSameSiteMainFrameNavigationsChangeSiteInstances()) {
    // When ProactivelySwapBrowsingInstance is enabled on same-site navigations,
    // the SiteInstance will change on same-site navigations (but COOP should
    // still be ignored).
    EXPECT_NE(current_frame_host()->GetSiteInstance(), initial_site_instance);
  } else {
    EXPECT_EQ(current_frame_host()->GetSiteInstance(), initial_site_instance);
  }
  EXPECT_EQ(current_frame_host()->cross_origin_opener_policy(),
            CoopUnsafeNone());

  console_observer.Wait();
}

IN_PROC_BROWSER_TEST_P(CrossOriginOpenerPolicyBrowserTest,
                       CoopIsIgnoredOnIframes) {
  GURL starting_page(
      https_server()->GetURL("a.com", "/cross_site_iframe_factory.html?a(b)"));
  GURL iframe_navigation_url = https_server()->GetURL(
      "b.com", "/set-header?Cross-Origin-Opener-Policy: same-origin");
  EXPECT_TRUE(NavigateToURL(shell(), starting_page));

  RenderFrameHostImpl* main_rfh = current_frame_host();
  FrameTreeNode* iframe_ftn = main_rfh->child_at(0);
  RenderFrameHostImpl* iframe_rfh = iframe_ftn->current_frame_host();
  SiteInstanceImpl* non_coop_iframe_site_instance =
      iframe_rfh->GetSiteInstance();

  // Navigate the iframe same-origin to a document with the COOP header. The
  // header must be ignored in iframes.
  NavigateFrameToURL(iframe_ftn, iframe_navigation_url);
  iframe_rfh = iframe_ftn->current_frame_host();

  // We expect the navigation to have used the same SiteInstance that was used
  // in the first place since they are same origin and COOP is ignored.
  EXPECT_EQ(iframe_rfh->GetLastCommittedURL(), iframe_navigation_url);
  EXPECT_EQ(iframe_rfh->GetSiteInstance(), non_coop_iframe_site_instance);

  EXPECT_EQ(iframe_rfh->cross_origin_opener_policy(), CoopUnsafeNone());
}

IN_PROC_BROWSER_TEST_P(CrossOriginOpenerPolicyBrowserTest,
                       NonCoopPageCrashIntoCoop) {
  IsolateAllSitesForTesting(base::CommandLine::ForCurrentProcess());
  GURL non_coop_page(https_server()->GetURL("a.com", "/title1.html"));
  GURL coop_page = https_server()->GetURL(
      "a.com", "/set-header?Cross-Origin-Opener-Policy: same-origin");

  // Test a crash before the navigation.
  {
    // Navigate to a non coop page.
    EXPECT_TRUE(NavigateToURL(shell(), non_coop_page));
    scoped_refptr<SiteInstance> initial_site_instance(
        current_frame_host()->GetSiteInstance());

    // Ensure it has a RenderFrameHostProxy for another cross-site page.
    GURL non_coop_cross_site_page(
        https_server()->GetURL("b.com", "/title1.html"));
    OpenPopup(current_frame_host(), non_coop_cross_site_page, "");
    EXPECT_EQ(web_contents()
                  ->GetFrameTree()
                  ->root()
                  ->render_manager()
                  ->GetProxyCount(),
              1u);

    // Simulate the renderer process crashing.
    RenderProcessHost* process = initial_site_instance->GetProcess();
    ASSERT_TRUE(process);
    std::unique_ptr<RenderProcessHostWatcher> crash_observer(
        new RenderProcessHostWatcher(
            process, RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT));
    process->Shutdown(0);
    crash_observer->Wait();
    crash_observer.reset();

    // Navigate to a COOP page.
    EXPECT_TRUE(NavigateToURL(shell(), coop_page));
    EXPECT_FALSE(current_frame_host()->GetSiteInstance()->IsRelatedSiteInstance(
        initial_site_instance.get()));
    EXPECT_EQ(current_frame_host()->cross_origin_opener_policy(),
              CoopSameOrigin());

    // The COOP page should no longer have any RenderFrameHostProxies.
    EXPECT_EQ(web_contents()
                  ->GetFrameTree()
                  ->root()
                  ->render_manager()
                  ->GetProxyCount(),
              0u);
  }

  // Test a crash during the navigation.
  {
    // Navigate to a non coop page.
    EXPECT_TRUE(NavigateToURL(shell(), non_coop_page));
    scoped_refptr<SiteInstance> initial_site_instance(
        current_frame_host()->GetSiteInstance());
    GURL non_coop_cross_site_page(
        https_server()->GetURL("b.com", "/title1.html"));

    // Ensure it has a RenderFrameHostProxy for another cross-site page.
    OpenPopup(current_frame_host(), non_coop_cross_site_page, "");
    EXPECT_EQ(web_contents()
                  ->GetFrameTree()
                  ->root()
                  ->render_manager()
                  ->GetProxyCount(),
              1u);

    // Start navigating to a COOP page.
    TestNavigationManager coop_navigation(web_contents(), coop_page);
    shell()->LoadURL(coop_page);
    EXPECT_TRUE(coop_navigation.WaitForRequestStart());

    // Simulate the renderer process crashing.
    RenderProcessHost* process = initial_site_instance->GetProcess();
    ASSERT_TRUE(process);
    std::unique_ptr<RenderProcessHostWatcher> crash_observer(
        new RenderProcessHostWatcher(
            process, RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT));
    process->Shutdown(0);
    crash_observer->Wait();
    crash_observer.reset();

    // Finish the navigation to the COOP page.
    coop_navigation.WaitForNavigationFinished();
    EXPECT_TRUE(coop_navigation.was_successful());
    EXPECT_FALSE(current_frame_host()->GetSiteInstance()->IsRelatedSiteInstance(
        initial_site_instance.get()));
    EXPECT_EQ(current_frame_host()->cross_origin_opener_policy(),
              CoopSameOrigin());

    // The COOP page should no longer have any RenderFrameHostProxies.
    EXPECT_EQ(web_contents()
                  ->GetFrameTree()
                  ->root()
                  ->render_manager()
                  ->GetProxyCount(),
              0u);
  }
}

IN_PROC_BROWSER_TEST_P(CrossOriginOpenerPolicyBrowserTest,
                       CoopPageCrashIntoNonCoop) {
  // TODO(http://crbug.com/1066376): Remove this when the test case passes.
  if (ShouldCreateNewHostForCrashedFrame())
    return;
  IsolateAllSitesForTesting(base::CommandLine::ForCurrentProcess());
  GURL non_coop_page(https_server()->GetURL("a.com", "/title1.html"));
  GURL coop_page = https_server()->GetURL(
      "a.com", "/set-header?Cross-Origin-Opener-Policy: same-origin");
  // Test a crash before the navigation.
  {
    // Navigate to a COOP page.
    EXPECT_TRUE(NavigateToURL(shell(), coop_page));
    scoped_refptr<SiteInstance> initial_site_instance(
        current_frame_host()->GetSiteInstance());

    // Ensure it has a RenderFrameHostProxy for another cross-site page.
    Shell* popup_shell = OpenPopup(current_frame_host(), coop_page, "");
    GURL cross_site_iframe(https_server()->GetURL("b.com", "/title1.html"));
    TestNavigationManager iframe_navigation(popup_shell->web_contents(),
                                            cross_site_iframe);
    EXPECT_TRUE(
        ExecJs(popup_shell->web_contents(),
               JsReplace("var iframe = document.createElement('iframe');"
                         "iframe.src = $1;"
                         "document.body.appendChild(iframe);",
                         cross_site_iframe)));
    iframe_navigation.WaitForNavigationFinished();
    EXPECT_EQ(web_contents()
                  ->GetFrameTree()
                  ->root()
                  ->render_manager()
                  ->GetProxyCount(),
              1u);

    // Simulate the renderer process crashing.
    RenderProcessHost* process = initial_site_instance->GetProcess();
    ASSERT_TRUE(process);
    std::unique_ptr<RenderProcessHostWatcher> crash_observer(
        new RenderProcessHostWatcher(
            process, RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT));
    process->Shutdown(0);
    crash_observer->Wait();
    crash_observer.reset();

    // Navigate to a non COOP page.
    EXPECT_TRUE(NavigateToURL(shell(), non_coop_page));
    EXPECT_FALSE(current_frame_host()->GetSiteInstance()->IsRelatedSiteInstance(
        initial_site_instance.get()));
    EXPECT_EQ(current_frame_host()->cross_origin_opener_policy(),
              CoopUnsafeNone());

    // The non COOP page should no longer have any RenderFrameHostProxies.
    EXPECT_EQ(web_contents()
                  ->GetFrameTree()
                  ->root()
                  ->render_manager()
                  ->GetProxyCount(),
              0u);
  }

  // Test a crash during the navigation.
  {
    // Navigate to a COOP page.
    EXPECT_TRUE(NavigateToURL(shell(), coop_page));
    scoped_refptr<SiteInstance> initial_site_instance(
        current_frame_host()->GetSiteInstance());

    // Ensure it has a RenderFrameHostProxy for another cross-site page.
    Shell* popup_shell = OpenPopup(current_frame_host(), coop_page, "");
    GURL cross_site_iframe(https_server()->GetURL("b.com", "/title1.html"));
    TestNavigationManager iframe_navigation(popup_shell->web_contents(),
                                            cross_site_iframe);
    EXPECT_TRUE(
        ExecJs(popup_shell->web_contents(),
               JsReplace("var iframe = document.createElement('iframe');"
                         "iframe.src = $1;"
                         "document.body.appendChild(iframe);",
                         cross_site_iframe)));
    iframe_navigation.WaitForNavigationFinished();
    EXPECT_EQ(web_contents()
                  ->GetFrameTree()
                  ->root()
                  ->render_manager()
                  ->GetProxyCount(),
              1u);

    // Start navigating to a non COOP page.
    TestNavigationManager non_coop_navigation(web_contents(), non_coop_page);
    shell()->LoadURL(non_coop_page);
    EXPECT_TRUE(non_coop_navigation.WaitForRequestStart());

    // Simulate the renderer process crashing.
    RenderProcessHost* process = initial_site_instance->GetProcess();
    ASSERT_TRUE(process);
    std::unique_ptr<RenderProcessHostWatcher> crash_observer(
        new RenderProcessHostWatcher(
            process, RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT));
    process->Shutdown(0);
    crash_observer->Wait();
    crash_observer.reset();

    // Finish the navigation to the non COOP page.
    non_coop_navigation.WaitForNavigationFinished();
    EXPECT_TRUE(non_coop_navigation.was_successful());
    EXPECT_FALSE(current_frame_host()->GetSiteInstance()->IsRelatedSiteInstance(
        initial_site_instance.get()));
    EXPECT_EQ(current_frame_host()->cross_origin_opener_policy(),
              CoopUnsafeNone());

    // The non COOP page should no longer have any RenderFrameHostProxies.
    EXPECT_EQ(web_contents()
                  ->GetFrameTree()
                  ->root()
                  ->render_manager()
                  ->GetProxyCount(),
              0u);
  }
}

IN_PROC_BROWSER_TEST_P(CrossOriginOpenerPolicyBrowserTest,
                       CoopPageCrashIntoCoop) {
  // TODO(http://crbug.com/1066376): Remove this when the test case passes.
  if (ShouldCreateNewHostForCrashedFrame())
    return;
  IsolateAllSitesForTesting(base::CommandLine::ForCurrentProcess());
  GURL non_coop_page(https_server()->GetURL("a.com", "/title1.html"));
  GURL coop_page = https_server()->GetURL(
      "a.com", "/set-header?Cross-Origin-Opener-Policy: same-origin");

  // Test a crash before the navigation.
  {
    // Navigate to a COOP page.
    EXPECT_TRUE(NavigateToURL(shell(), coop_page));
    scoped_refptr<SiteInstance> initial_site_instance(
        current_frame_host()->GetSiteInstance());
    EXPECT_EQ(current_frame_host()->cross_origin_opener_policy(),
              CoopSameOrigin());

    // Ensure it has a RenderFrameHostProxy for another cross-site page.
    Shell* popup_shell = OpenPopup(current_frame_host(), coop_page, "");
    GURL cross_site_iframe(https_server()->GetURL("b.com", "/title1.html"));
    TestNavigationManager iframe_navigation(popup_shell->web_contents(),
                                            cross_site_iframe);
    EXPECT_TRUE(
        ExecJs(popup_shell->web_contents(),
               JsReplace("var iframe = document.createElement('iframe');"
                         "iframe.src = $1;"
                         "document.body.appendChild(iframe);",
                         cross_site_iframe)));
    iframe_navigation.WaitForNavigationFinished();
    EXPECT_EQ(web_contents()
                  ->GetFrameTree()
                  ->root()
                  ->render_manager()
                  ->GetProxyCount(),
              1u);

    // Simulate the renderer process crashing.
    RenderProcessHost* process = initial_site_instance->GetProcess();
    ASSERT_TRUE(process);
    std::unique_ptr<RenderProcessHostWatcher> crash_observer(
        new RenderProcessHostWatcher(
            process, RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT));
    process->Shutdown(0);
    crash_observer->Wait();
    crash_observer.reset();

    // Navigate to a COOP page.
    EXPECT_TRUE(NavigateToURL(shell(), coop_page));
    EXPECT_TRUE(current_frame_host()->GetSiteInstance()->IsRelatedSiteInstance(
        initial_site_instance.get()));
    EXPECT_EQ(current_frame_host()->cross_origin_opener_policy(),
              CoopSameOrigin());

    // TODO(pmeuleman): The COOP page should still have RenderFrameHostProxies.
    EXPECT_EQ(web_contents()
                  ->GetFrameTree()
                  ->root()
                  ->render_manager()
                  ->GetProxyCount(),
              0u);
  }

  // Test a crash during the navigation.
  {
    // Navigate to a COOP page.
    EXPECT_TRUE(NavigateToURL(shell(), coop_page));
    scoped_refptr<SiteInstance> initial_site_instance(
        current_frame_host()->GetSiteInstance());

    // Ensure it has a RenderFrameHostProxy for another cross-site page.
    Shell* popup_shell = OpenPopup(current_frame_host(), coop_page, "");
    GURL cross_site_iframe(https_server()->GetURL("b.com", "/title1.html"));
    TestNavigationManager iframe_navigation(popup_shell->web_contents(),
                                            cross_site_iframe);
    EXPECT_TRUE(
        ExecJs(popup_shell->web_contents(),
               JsReplace("var iframe = document.createElement('iframe');"
                         "iframe.src = $1;"
                         "document.body.appendChild(iframe);",
                         cross_site_iframe)));
    iframe_navigation.WaitForNavigationFinished();
    EXPECT_EQ(web_contents()
                  ->GetFrameTree()
                  ->root()
                  ->render_manager()
                  ->GetProxyCount(),
              1u);

    // Start navigating to a COOP page.
    TestNavigationManager coop_navigation(web_contents(), coop_page);
    shell()->LoadURL(coop_page);
    EXPECT_TRUE(coop_navigation.WaitForRequestStart());

    // Simulate the renderer process crashing.
    RenderProcessHost* process = initial_site_instance->GetProcess();
    ASSERT_TRUE(process);
    std::unique_ptr<RenderProcessHostWatcher> crash_observer(
        new RenderProcessHostWatcher(
            process, RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT));
    process->Shutdown(0);
    crash_observer->Wait();
    crash_observer.reset();

    // Finish the navigation to the COOP page.
    coop_navigation.WaitForNavigationFinished();
    EXPECT_TRUE(coop_navigation.was_successful());
    EXPECT_TRUE(current_frame_host()->GetSiteInstance()->IsRelatedSiteInstance(
        initial_site_instance.get()));
    EXPECT_EQ(current_frame_host()->cross_origin_opener_policy(),
              CoopSameOrigin());

    // TODO(pmeuleman): The COOP page should still have RenderFrameHostProxies.
    EXPECT_EQ(web_contents()
                  ->GetFrameTree()
                  ->root()
                  ->render_manager()
                  ->GetProxyCount(),
              0u);
  }
}

IN_PROC_BROWSER_TEST_P(CrossOriginOpenerPolicyBrowserTest,
                       ProxiesAreRemovedWhenCrossingCoopBoundary) {
  GURL non_coop_page(https_server()->GetURL("a.com", "/title1.html"));
  GURL coop_page = https_server()->GetURL(
      "a.com", "/set-header?Cross-Origin-Opener-Policy: same-origin");

  RenderFrameHostManager* main_window_rfhm =
      web_contents()->GetFrameTree()->root()->render_manager();
  EXPECT_TRUE(NavigateToURL(shell(), non_coop_page));
  EXPECT_EQ(main_window_rfhm->GetProxyCount(), 0u);

  Shell* popup_shell = OpenPopup(shell(), coop_page, "");

  // The main frame should not have the popup referencing it.
  EXPECT_EQ(main_window_rfhm->GetProxyCount(), 0u);

  // It should not have any other related SiteInstance.
  EXPECT_EQ(
      current_frame_host()->GetSiteInstance()->GetRelatedActiveContentsCount(),
      1u);

  // The popup should not have the main frame referencing it.
  FrameTreeNode* popup =
      static_cast<WebContentsImpl*>(popup_shell->web_contents())
          ->GetFrameTree()
          ->root();
  RenderFrameHostManager* popup_rfhm = popup->render_manager();
  EXPECT_EQ(popup_rfhm->GetProxyCount(), 0u);

  // The popup should have an empty opener.
  EXPECT_FALSE(popup->opener());
}

IN_PROC_BROWSER_TEST_P(CrossOriginOpenerPolicyBrowserTest,
                       ProxiesAreKeptWhenNavigatingFromCoopToCoop) {
  IsolateAllSitesForTesting(base::CommandLine::ForCurrentProcess());
  GURL coop_page = https_server()->GetURL(
      "a.com", "/set-header?Cross-Origin-Opener-Policy: same-origin");

  // Navigate to a COOP page.
  EXPECT_TRUE(NavigateToURL(shell(), coop_page));
  scoped_refptr<SiteInstance> initial_site_instance(
      current_frame_host()->GetSiteInstance());

  // Ensure it has a RenderFrameHostProxy for another cross-site page.
  Shell* popup_shell = OpenPopup(current_frame_host(), coop_page, "");
  GURL cross_site_iframe(https_server()->GetURL("b.com", "/title1.html"));
  TestNavigationManager iframe_navigation(popup_shell->web_contents(),
                                          cross_site_iframe);
  EXPECT_TRUE(ExecJs(popup_shell->web_contents(),
                     JsReplace("var iframe = document.createElement('iframe');"
                               "iframe.src = $1;"
                               "document.body.appendChild(iframe);",
                               cross_site_iframe)));
  iframe_navigation.WaitForNavigationFinished();
  EXPECT_EQ(
      web_contents()->GetFrameTree()->root()->render_manager()->GetProxyCount(),
      1u);

  // Navigate to a COOP page.
  EXPECT_TRUE(NavigateToURL(shell(), coop_page));

  // The COOP page should still have a RenderFrameProxyHost.
  EXPECT_EQ(
      web_contents()->GetFrameTree()->root()->render_manager()->GetProxyCount(),
      1u);
}

IN_PROC_BROWSER_TEST_P(CrossOriginOpenerPolicyBrowserTest,
                       IsolateInNewProcessDespiteLimitReached) {
  // Set a process limit of 1 for testing.
  RenderProcessHostImpl::SetMaxRendererProcessCount(1);

  // Navigate to a starting page.
  GURL starting_page(https_server()->GetURL("a.com", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), starting_page));

  // Open a popup with CrossOriginOpenerPolicy and CrossOriginEmbedderPolicy
  // set.
  GURL url_openee =
      https_server()->GetURL("a.com",
                             "/set-header?"
                             "Cross-Origin-Opener-Policy: same-origin&"
                             "Cross-Origin-Embedder-Policy: require-corp");
  ShellAddedObserver shell_observer;
  EXPECT_TRUE(
      ExecJs(current_frame_host(), JsReplace("window.open($1)", url_openee)));

  auto* popup_webcontents =
      static_cast<WebContentsImpl*>(shell_observer.GetShell()->web_contents());
  EXPECT_TRUE(WaitForLoadStop(popup_webcontents));

  // The page and its popup should be in different processes even though the
  // process limit was reached.
  EXPECT_NE(current_frame_host()->GetProcess(),
            popup_webcontents->GetMainFrame()->GetProcess());
}

IN_PROC_BROWSER_TEST_P(CrossOriginOpenerPolicyBrowserTest,
                       NoProcessReuseForCOOPProcesses) {
  // Set a process limit of 1 for testing.
  RenderProcessHostImpl::SetMaxRendererProcessCount(1);

  // Navigate to a starting page with CrossOriginOpenerPolicy and
  // CrossOriginEmbedderPolicy set.
  GURL starting_page =
      https_server()->GetURL("a.com",
                             "/set-header?"
                             "Cross-Origin-Opener-Policy: same-origin&"
                             "Cross-Origin-Embedder-Policy: require-corp");
  EXPECT_TRUE(NavigateToURL(shell(), starting_page));

  // Open a popup without CrossOriginOpenerPolicy and CrossOriginEmbedderPolicy
  // set.
  ShellAddedObserver shell_observer;
  EXPECT_TRUE(ExecJs(current_frame_host(), "window.open('/title1.html')"));

  auto* popup_webcontents =
      static_cast<WebContentsImpl*>(shell_observer.GetShell()->web_contents());
  EXPECT_TRUE(WaitForLoadStop(popup_webcontents));

  // The page and its popup should be in different processes even though the
  // process limit was reached.
  EXPECT_NE(current_frame_host()->GetProcess(),
            popup_webcontents->GetMainFrame()->GetProcess());

  // Navigate to a new page without COOP and COEP. Because of process reuse, it
  // is placed in the popup process.
  GURL final_page(https_server()->GetURL("a.com", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), final_page));
  EXPECT_EQ(current_frame_host()->GetProcess(),
            popup_webcontents->GetMainFrame()->GetProcess());
}

IN_PROC_BROWSER_TEST_P(CrossOriginOpenerPolicyBrowserTest,
                       SpeculativeRfhsAndCoop) {
  GURL non_coop_page(https_server()->GetURL("a.com", "/title1.html"));
  GURL coop_page =
      https_server()->GetURL("a.com",
                             "/set-header?"
                             "Cross-Origin-Opener-Policy: same-origin&"
                             "Cross-Origin-Embedder-Policy: require-corp");

  // Non-COOP into non-COOP.
  {
    // Start on a non COOP page.
    EXPECT_TRUE(NavigateToURL(shell(), non_coop_page));
    scoped_refptr<SiteInstance> initial_site_instance(
        current_frame_host()->GetSiteInstance());

    // Navigate to a non COOP page.
    TestNavigationManager non_coop_navigation(web_contents(), non_coop_page);
    shell()->LoadURL(non_coop_page);
    EXPECT_TRUE(non_coop_navigation.WaitForRequestStart());

    // TODO(ahemery): RenderDocument will always create a Speculative RFH.
    // Update these expectations to test the speculative RFH's SI relation when
    // RenderDocument lands.
    EXPECT_FALSE(web_contents()
                     ->GetFrameTree()
                     ->root()
                     ->render_manager()
                     ->speculative_frame_host());

    non_coop_navigation.WaitForNavigationFinished();

    EXPECT_TRUE(current_frame_host()->GetSiteInstance()->IsRelatedSiteInstance(
        initial_site_instance.get()));
    EXPECT_EQ(current_frame_host()->cross_origin_opener_policy().value,
              network::mojom::CrossOriginOpenerPolicyValue::kUnsafeNone);
  }

  // Non-COOP into COOP.
  {
    // Start on a non COOP page.
    EXPECT_TRUE(NavigateToURL(shell(), non_coop_page));
    scoped_refptr<SiteInstance> initial_site_instance(
        current_frame_host()->GetSiteInstance());

    // Navigate to a COOP page.
    TestNavigationManager coop_navigation(web_contents(), coop_page);
    shell()->LoadURL(coop_page);
    EXPECT_TRUE(coop_navigation.WaitForRequestStart());

    auto* speculative_rfh = web_contents()
                                ->GetFrameTree()
                                ->root()
                                ->render_manager()
                                ->speculative_frame_host();
    if (CanSameSiteMainFrameNavigationsChangeRenderFrameHosts()) {
      // When ProactivelySwapBrowsingInstance or RenderDocument is enabled on
      // same-site main-frame navigations, the navigation will result in a new
      // RFH, so it will create a pending RFH.
      EXPECT_TRUE(speculative_rfh);
    } else {
      EXPECT_FALSE(speculative_rfh);
    }

    coop_navigation.WaitForNavigationFinished();

    EXPECT_FALSE(current_frame_host()->GetSiteInstance()->IsRelatedSiteInstance(
        initial_site_instance.get()));
    EXPECT_EQ(
        current_frame_host()->cross_origin_opener_policy().value,
        network::mojom::CrossOriginOpenerPolicyValue::kSameOriginPlusCoep);
  }

  // COOP into non-COOP.
  {
    // Start on a COOP page.
    EXPECT_TRUE(NavigateToURL(shell(), coop_page));
    scoped_refptr<SiteInstance> initial_site_instance(
        current_frame_host()->GetSiteInstance());

    // Navigate to a non COOP page.
    TestNavigationManager non_coop_navigation(web_contents(), non_coop_page);
    shell()->LoadURL(non_coop_page);
    EXPECT_TRUE(non_coop_navigation.WaitForRequestStart());

    auto* speculative_rfh = web_contents()
                                ->GetFrameTree()
                                ->root()
                                ->render_manager()
                                ->speculative_frame_host();
    if (CanSameSiteMainFrameNavigationsChangeRenderFrameHosts()) {
      // When ProactivelySwapBrowsingInstance or RenderDocument is enabled on
      // same-site main-frame navigations, the navigation will result in a new
      // RFH, so it will create a pending RFH.
      EXPECT_TRUE(speculative_rfh);
    } else {
      EXPECT_FALSE(speculative_rfh);
    }

    non_coop_navigation.WaitForNavigationFinished();

    EXPECT_FALSE(current_frame_host()->GetSiteInstance()->IsRelatedSiteInstance(
        initial_site_instance.get()));
    EXPECT_EQ(current_frame_host()->cross_origin_opener_policy().value,
              network::mojom::CrossOriginOpenerPolicyValue::kUnsafeNone);
  }

  // COOP into COOP.
  {
    // Start on a COOP page.
    EXPECT_TRUE(NavigateToURL(shell(), coop_page));
    scoped_refptr<SiteInstance> initial_site_instance(
        current_frame_host()->GetSiteInstance());

    // Navigate to a COOP page.
    TestNavigationManager coop_navigation(web_contents(), coop_page);
    shell()->LoadURL(coop_page);
    EXPECT_TRUE(coop_navigation.WaitForRequestStart());

    // TODO(ahemery): RenderDocument will always create a Speculative RFH.
    // Update these expectations to test the speculative RFH's SI relation when
    // RenderDocument lands.
    EXPECT_FALSE(web_contents()
                     ->GetFrameTree()
                     ->root()
                     ->render_manager()
                     ->speculative_frame_host());

    coop_navigation.WaitForNavigationFinished();

    EXPECT_TRUE(current_frame_host()->GetSiteInstance()->IsRelatedSiteInstance(
        initial_site_instance.get()));
    EXPECT_EQ(
        current_frame_host()->cross_origin_opener_policy().value,
        network::mojom::CrossOriginOpenerPolicyValue::kSameOriginPlusCoep);
  }
}

// Try to host into the same cross-origin isolated process, two cross-origin
// documents. The second's response sets CSP:sandbox, so its origin is opaque
// and derived from the first.
IN_PROC_BROWSER_TEST_P(CrossOriginOpenerPolicyBrowserTest,
                       CrossOriginIsolatedWithDifferentOrigin) {
  GURL opener_url =
      https_server()->GetURL("a.com",
                             "/set-header?"
                             "Cross-Origin-Opener-Policy: same-origin&"
                             "Cross-Origin-Embedder-Policy: require-corp");
  GURL openee_url =
      https_server()->GetURL("a.com",
                             "/set-header?"
                             "Cross-Origin-Opener-Policy: same-origin&"
                             "Cross-Origin-Embedder-Policy: require-corp&"
                             "Content-Security-Policy: sandbox");

  // Load the first window.
  EXPECT_TRUE(NavigateToURL(shell(), opener_url));
  RenderFrameHostImpl* opener_current_main_document = current_frame_host();

  // Load the second window.
  ShellAddedObserver shell_observer;
  EXPECT_TRUE(
      ExecJs(current_frame_host(), JsReplace("window.open($1)", openee_url)));
  WebContents* popup = shell_observer.GetShell()->web_contents();
  WaitForLoadStop(popup);

  RenderFrameHostImpl* openee_current_main_document =
      static_cast<WebContentsImpl*>(popup)
          ->GetFrameTree()
          ->root()
          ->current_frame_host();

  // Those documents aren't error pages.
  EXPECT_EQ(opener_current_main_document->GetLastCommittedURL(), opener_url);
  EXPECT_EQ(openee_current_main_document->GetLastCommittedURL(), openee_url);
  EXPECT_EQ(opener_current_main_document->last_http_status_code(), 200);
  EXPECT_EQ(openee_current_main_document->last_http_status_code(), 200);

  // We have two main documents in the same cross-origin isolated process from a
  // different origin.
  // TODO(https://crbug.com/1115426): Investigate what needs to be done.
  EXPECT_NE(opener_current_main_document->GetLastCommittedOrigin(),
            openee_current_main_document->GetLastCommittedOrigin());
  EXPECT_EQ(opener_current_main_document->GetProcess(),
            openee_current_main_document->GetProcess());
  EXPECT_EQ(opener_current_main_document->GetSiteInstance(),
            openee_current_main_document->GetSiteInstance());

  // TODO(arthursonzogni): Check whether the processes are marked as
  // cross-origin isolated or not.
}

// Navigate in between two documents. Check the virtual browsing context group
// is properly updated.
IN_PROC_BROWSER_TEST_P(VirtualBrowsingContextGroupTest, Navigation) {
  const struct {
    GURL url_a;
    GURL url_b;
    bool expect_different_virtual_browsing_context_group;
  } kTestCases[] = {
      // non-coop <-> non-coop
      {
          // same-origin => keep.
          https_server()->GetURL("a.com", "/title1.html"),
          https_server()->GetURL("a.com", "/title2.html"),
          false,
      },
      {
          // different-origin => keep.
          https_server()->GetURL("a.a.com", "/title1.html"),
          https_server()->GetURL("b.a.com", "/title2.html"),
          false,
      },
      {
          // different-site => keep.
          https_server()->GetURL("a.com", "/title1.html"),
          https_server()->GetURL("b.com", "/title2.html"),
          false,
      },

      // non-coop <-> coop.
      {
          // same-origin => change.
          https_server()->GetURL("a.com", "/title1.html"),
          https_server()->GetURL("a.com",
                                 "/set-header?"
                                 "Cross-Origin-Opener-Policy: same-origin&"
                                 "Cross-Origin-Embedder-Policy: require-corp"),
          true,
      },
      {
          // different-origin => change.
          https_server()->GetURL("a.a.com", "/title1.html"),
          https_server()->GetURL("b.a.com",
                                 "/set-header?"
                                 "Cross-Origin-Opener-Policy: same-origin&"
                                 "Cross-Origin-Embedder-Policy: require-corp"),
          true,
      },
      {
          // different-site => change.
          https_server()->GetURL("a.com", "/title1.html"),
          https_server()->GetURL("b.com",
                                 "/set-header?"
                                 "Cross-Origin-Opener-Policy: same-origin&"
                                 "Cross-Origin-Embedder-Policy: require-corp"),
          true,
      },

      // coop <-> coop.
      {
          // same-origin => keep.
          https_server()->GetURL("a.com",
                                 "/set-header?"
                                 "Cross-Origin-Opener-Policy: same-origin&"
                                 "Cross-Origin-Embedder-Policy: require-corp"),
          https_server()->GetURL("a.com",
                                 "/set-header?"
                                 "Cross-Origin-Opener-Policy: same-origin&"
                                 "Cross-Origin-Embedder-Policy: require-corp"),
          false,
      },
      {
          // different-origin => change.
          https_server()->GetURL("a.a.com",
                                 "/set-header?"
                                 "Cross-Origin-Opener-Policy: same-origin&"
                                 "Cross-Origin-Embedder-Policy: require-corp"),
          https_server()->GetURL("b.a.com",
                                 "/set-header?"
                                 "Cross-Origin-Opener-Policy: same-origin&"
                                 "Cross-Origin-Embedder-Policy: require-corp"),
          true,
      },
      {
          // different-site => keep.
          https_server()->GetURL("a.com",
                                 "/set-header?"
                                 "Cross-Origin-Opener-Policy: same-origin&"
                                 "Cross-Origin-Embedder-Policy: require-corp"),
          https_server()->GetURL("b.com",
                                 "/set-header?"
                                 "Cross-Origin-Opener-Policy: same-origin&"
                                 "Cross-Origin-Embedder-Policy: require-corp"),
          true,
      },

      // non-coop <-> coop-ro.
      {
          // same-origin => change.
          https_server()->GetURL("a.com", "/title1.html"),
          https_server()->GetURL(
              "a.com",
              "/set-header?"
              "Cross-Origin-Opener-Policy-Report-Only: same-origin&"
              "Cross-Origin-Embedder-Policy: require-corp"),
          true,
      },
      {
          // different-origin => change.
          https_server()->GetURL("a.a.com", "/title1.html"),
          https_server()->GetURL(
              "b.a.com",
              "/set-header?"
              "Cross-Origin-Opener-Policy-Report-Only: same-origin&"
              "Cross-Origin-Embedder-Policy: require-corp"),
          true,
      },
      {
          // different-site => change.
          https_server()->GetURL("a.com", "/title1.html"),
          https_server()->GetURL(
              "b.com",
              "/set-header?"
              "Cross-Origin-Opener-Policy-Report-Only: same-origin&"
              "Cross-Origin-Embedder-Policy: require-corp"),
          true,
      },

      // coop-ro <-> coop-ro.
      {
          // same-origin => keep.
          https_server()->GetURL(
              "a.com",
              "/set-header?"
              "Cross-Origin-Opener-Policy-Report-Only: same-origin&"
              "Cross-Origin-Embedder-Policy: require-corp"),
          https_server()->GetURL(
              "a.com",
              "/set-header?"
              "Cross-Origin-Opener-Policy-Report-Only: same-origin&"
              "Cross-Origin-Embedder-Policy: require-corp"),
          false,
      },
      {
          // different-origin => change.
          https_server()->GetURL(
              "a.a.com",
              "/set-header?"
              "Cross-Origin-Opener-Policy-Report-Only: same-origin&"
              "Cross-Origin-Embedder-Policy: require-corp"),
          https_server()->GetURL(
              "b.a.com",
              "/set-header?"
              "Cross-Origin-Opener-Policy-Report-Only: same-origin&"
              "Cross-Origin-Embedder-Policy: require-corp"),
          true,
      },
      {
          // different-site => keep.
          https_server()->GetURL(
              "a.com",
              "/set-header?"
              "Cross-Origin-Opener-Policy-Report-Only: same-origin&"
              "Cross-Origin-Embedder-Policy: require-corp"),
          https_server()->GetURL(
              "b.com",
              "/set-header?"
              "Cross-Origin-Opener-Policy-Report-Only: same-origin&"
              "Cross-Origin-Embedder-Policy: require-corp"),
          true,
      },

      // coop <-> coop-ro.
      {
          // same-origin => change.
          https_server()->GetURL("a.com",
                                 "/set-header?"
                                 "Cross-Origin-Opener-Policy: same-origin&"
                                 "Cross-Origin-Embedder-Policy: require-corp"),
          https_server()->GetURL(
              "a.com",
              "/set-header?"
              "Cross-Origin-Opener-Policy-Report-Only: same-origin&"
              "Cross-Origin-Embedder-Policy: require-corp"),
          true,
      },
      {
          // different-origin => change.
          https_server()->GetURL("a.a.com",
                                 "/set-header?"
                                 "Cross-Origin-Opener-Policy: same-origin&"
                                 "Cross-Origin-Embedder-Policy: require-corp"),
          https_server()->GetURL(
              "b.a.com",
              "/set-header?"
              "Cross-Origin-Opener-Policy-Report-Only: same-origin&"
              "Cross-Origin-Embedder-Policy: require-corp"),
          true,
      },
      {
          // different-site => change
          https_server()->GetURL("a.com",
                                 "/set-header?"
                                 "Cross-Origin-Opener-Policy: same-origin&"
                                 "Cross-Origin-Embedder-Policy: require-corp"),
          https_server()->GetURL(
              "b.com",
              "/set-header?"
              "Cross-Origin-Opener-Policy-Report-Only: same-origin&"
              "Cross-Origin-Embedder-Policy: require-corp"),
          true,
      },
      // TODO(https://crbug.com/1101339). Test with COEP-RO.
      // TODO(https://crbug.com/1101339). Test with COOP-RO+COOP.
  };

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(testing::Message()
                 << std::endl
                 << "url_a = " << test_case.url_a << std::endl
                 << "url_b = " << test_case.url_b << std::endl);
    ASSERT_TRUE(NavigateToURL(shell(), test_case.url_a));
    int group_1 = VirtualBrowsingContextGroup(web_contents());

    ASSERT_TRUE(NavigateToURL(shell(), test_case.url_b));
    int group_2 = VirtualBrowsingContextGroup(web_contents());

    ASSERT_TRUE(NavigateToURL(shell(), test_case.url_a));
    int group_3 = VirtualBrowsingContextGroup(web_contents());

    // Note: Navigating from A to B and navigating from B to A must lead to the
    // same decision. We check both to avoid adding all the symmetric test
    // cases.
    if (test_case.expect_different_virtual_browsing_context_group) {
      EXPECT_NE(group_1, group_2);  // url_a -> url_b.
      EXPECT_NE(group_2, group_3);  // url_a <- url_b.
    } else {
      EXPECT_EQ(group_1, group_2);  // url_a -> url_b.
      EXPECT_EQ(group_2, group_3);  // url_b <- url_b.
    }
  }
}

// Use window.open(url). Check the virtual browsing context group of the two
// window.
IN_PROC_BROWSER_TEST_P(VirtualBrowsingContextGroupTest, WindowOpen) {
  const struct {
    GURL url_opener;
    GURL url_openee;
    bool expect_different_virtual_browsing_context_group;
  } kTestCases[] = {
      // Open with no URL => Always keep.
      {
          // From non-coop.
          https_server()->GetURL("a.com", "/title1.html"),
          GURL(),
          false,
      },
      {
          // From coop-ro.
          https_server()->GetURL(
              "a.com",
              "/set-header?"
              "Cross-Origin-Opener-Policy-Report-Only: same-origin&"
              "Cross-Origin-Embedder-Policy: require-corp"),
          GURL(),
          false,
      },
      {
          // From coop.
          https_server()->GetURL("a.com",
                                 "/set-header?"
                                 "Cross-Origin-Opener-Policy: same-origin&"
                                 "Cross-Origin-Embedder-Policy: require-corp"),
          GURL(),
          false,
      },

      // From here, we open a new window with an URL. This is equivalent to:
      // 1. opening a new window
      // 2. navigating the new window.
      //
      // (1) is tested by the 3 test cases above.
      // (2) is tested by the test VirtualBrowsingContextGroup.
      //
      // Here we are only providing a few test cases to test the sequence 1 & 2.

      // non-coop opens non-coop.
      {
          https_server()->GetURL("a.com", "/title1.html"),
          https_server()->GetURL("a.com", "/title1.html"),
          false,
      },

      // non-coop opens coop-ro.
      {
          https_server()->GetURL("a.com", "/title1.html"),
          https_server()->GetURL(
              "a.com",
              "/set-header?"
              "Cross-Origin-Opener-Policy-Report-Only: same-origin&"
              "Cross-Origin-Embedder-Policy: require-corp"),
          true,
      },

      // non-coop opens coop.
      {
          https_server()->GetURL("a.com", "/title1.html"),
          https_server()->GetURL("a.com",
                                 "/set-header?"
                                 "Cross-Origin-Opener-Policy: same-origin&"
                                 "Cross-Origin-Embedder-Policy: require-corp"),
          true,
      },

      // coop opens non-coop.
      {
          https_server()->GetURL("a.com",
                                 "/set-header?"
                                 "Cross-Origin-Opener-Policy: same-origin&"
                                 "Cross-Origin-Embedder-Policy: require-corp"),
          https_server()->GetURL("a.com", "/title1.html"),
          true,
      },

      // coop-ro opens coop-ro (same-origin).
      {
          https_server()->GetURL(
              "a.com",
              "/set-header?"
              "Cross-Origin-Opener-Policy-Report-Only: same-origin&"
              "Cross-Origin-Embedder-Policy: require-corp"),
          https_server()->GetURL(
              "a.com",
              "/set-header?"
              "Cross-Origin-Opener-Policy-Report-Only: same-origin&"
              "Cross-Origin-Embedder-Policy: require-corp"),
          false,
      },

      // coop-ro opens coop-ro (different-origin).
      {
          https_server()->GetURL(
              "a.com",
              "/set-header?"
              "Cross-Origin-Opener-Policy-Report-Only: same-origin&"
              "Cross-Origin-Embedder-Policy: require-corp"),
          https_server()->GetURL(
              "b.com",
              "/set-header?"
              "Cross-Origin-Opener-Policy-Report-Only: same-origin&"
              "Cross-Origin-Embedder-Policy: require-corp"),
          true,
      },

      // TODO(https://crbug.com/1101339). Test with COEP-RO.
      // TODO(https://crbug.com/1101339). Test with COOP-RO+COOP
  };

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(testing::Message()
                 << std::endl
                 << "url_opener = " << test_case.url_opener << std::endl
                 << "url_openee = " << test_case.url_openee << std::endl);

    ASSERT_TRUE(NavigateToURL(shell(), test_case.url_opener));
    int group_opener = VirtualBrowsingContextGroup(web_contents());

    ShellAddedObserver shell_observer;
    EXPECT_TRUE(ExecJs(current_frame_host(),
                       JsReplace("window.open($1)", test_case.url_openee)));
    WebContents* popup = shell_observer.GetShell()->web_contents();
    // The virtual browser context group will change, only after the popup has
    // navigated.
    WaitForLoadStop(popup);
    int group_openee = VirtualBrowsingContextGroup(popup);

    if (test_case.expect_different_virtual_browsing_context_group)
      EXPECT_NE(group_opener, group_openee);
    else
      EXPECT_EQ(group_opener, group_openee);

    popup->Close();
  }
}

namespace {
// Use two URLs, |url_a| and |url_b|. One of them at least uses
// COOP:same-origin-allow-popups, or COOP-Report-Only:same-origin-allow-popups,
// or both.
//
// Test two scenario:
// 1. From |url_a|, opens |url_b|
// 2. From |url_a|, navigates to |url_b|.
//
// In both cases, check whether a new virtual browsing context group has been
// used or not.
struct VirtualBcgAllowPopupTestCase {
  GURL url_a;
  GURL url_b;
  bool expect_different_group_window_open;
  bool expect_different_group_navigation;
};

void RunTest(const VirtualBcgAllowPopupTestCase& test_case, Shell* shell) {
  SCOPED_TRACE(testing::Message()
               << std::endl
               << "url_a = " << test_case.url_a << std::endl
               << "url_b = " << test_case.url_b << std::endl);
  ASSERT_TRUE(NavigateToURL(shell, test_case.url_a));
  int group_initial = VirtualBrowsingContextGroup(shell->web_contents());

  ShellAddedObserver shell_observer;
  EXPECT_TRUE(ExecJs(shell->web_contents()->GetMainFrame(),
                     JsReplace("window.open($1)", test_case.url_b)));
  WebContents* popup = shell_observer.GetShell()->web_contents();
  WaitForLoadStop(popup);
  int group_openee = VirtualBrowsingContextGroup(popup);

  ASSERT_TRUE(NavigateToURL(shell, test_case.url_b));
  int group_navigate = VirtualBrowsingContextGroup(shell->web_contents());

  if (test_case.expect_different_group_window_open)
    EXPECT_NE(group_initial, group_openee);
  else
    EXPECT_EQ(group_initial, group_openee);

  if (test_case.expect_different_group_navigation)
    EXPECT_NE(group_initial, group_navigate);
  else
    EXPECT_EQ(group_initial, group_navigate);

  popup->Close();
}

}  // namespace

IN_PROC_BROWSER_TEST_P(VirtualBrowsingContextGroupTest,
                       NonCoopToCoopAllowPopup) {
  const VirtualBcgAllowPopupTestCase kTestCases[] = {
      {
          // same-origin.
          https_server()->GetURL("a.com", "/title1.html"),
          https_server()->GetURL(
              "a.com",
              "/set-header?"
              "Cross-Origin-Opener-Policy: same-origin-allow-popups&"
              "Cross-Origin-Embedder-Policy: require-corp"),
          true,
          true,
      },
      {
          // cross-origin.
          https_server()->GetURL("a.a.com", "/title1.html"),
          https_server()->GetURL(
              "b.a.com",
              "/set-header?"
              "Cross-Origin-Opener-Policy: same-origin-allow-popups&"
              "Cross-Origin-Embedder-Policy: require-corp"),
          true,
          true,
      },
      {
          // cross-site.
          https_server()->GetURL("a.com", "/title1.html"),
          https_server()->GetURL(
              "b.com",
              "/set-header?"
              "Cross-Origin-Opener-Policy: same-origin-allow-popups&"
              "Cross-Origin-Embedder-Policy: require-corp"),
          true,
          true,
      },
  };
  for (const auto& test : kTestCases)
    RunTest(test, shell());
}

// coop:same-origin-allow-popup -> coop:none.
IN_PROC_BROWSER_TEST_P(VirtualBrowsingContextGroupTest,
                       CoopAllowPopup_NonCoop) {
  const VirtualBcgAllowPopupTestCase kTestCases[] = {
      {
          // same-origin.
          https_server()->GetURL(
              "a.com",
              "/set-header?"
              "Cross-Origin-Opener-Policy: same-origin-allow-popups&"
              "Cross-Origin-Embedder-Policy: require-corp"),
          https_server()->GetURL("a.com", "/title1.html"), false,
          true,
      },
      {
          // cross-origin.
          https_server()->GetURL(
              "b.a.com",
              "/set-header?"
              "Cross-Origin-Opener-Policy: same-origin-allow-popups&"
              "Cross-Origin-Embedder-Policy: require-corp"),
          https_server()->GetURL("a.a.com", "/title1.html"), false,
          true,
      },
      {
          // cross-site.
          https_server()->GetURL(
              "b.com",
              "/set-header?"
              "Cross-Origin-Opener-Policy: same-origin-allow-popups&"
              "Cross-Origin-Embedder-Policy: require-corp"),
          https_server()->GetURL("a.com", "/title1.html"), false,
          true,
      },
  };
  for (const auto& test : kTestCases)
    RunTest(test, shell());
}

// coop:none -> coop:same-origin-allow-popup.
IN_PROC_BROWSER_TEST_P(VirtualBrowsingContextGroupTest,
                       CoopRoAllowPopup_NonCoop) {
  const VirtualBcgAllowPopupTestCase kTestCases[] = {
      {
          // same-origin.
          https_server()->GetURL("a.com",
                                 "/set-header?"
                                 "Cross-Origin-Opener-Policy-Report-Only: "
                                 "same-origin-allow-popups&"
                                 "Cross-Origin-Embedder-Policy: require-corp"),
          https_server()->GetURL("a.com", "/title1.html"), false,
          true,
      },
      {
          // cross-origin.
          https_server()->GetURL("b.a.com",
                                 "/set-header?"
                                 "Cross-Origin-Opener-Policy-Report-Only: "
                                 "same-origin-allow-popups&"
                                 "Cross-Origin-Embedder-Policy: require-corp"),
          https_server()->GetURL("a.a.com", "/title1.html"), false,
          true,
      },
      {
          // cross-site.
          https_server()->GetURL("b.com",
                                 "/set-header?"
                                 "Cross-Origin-Opener-Policy-Report-Only: "
                                 "same-origin-allow-popups&"
                                 "Cross-Origin-Embedder-Policy: require-corp"),
          https_server()->GetURL("a.com", "/title1.html"), false,
          true,
      },
  };
  for (const auto& test : kTestCases)
    RunTest(test, shell());
}

// coop:same-origin-allow-popup -> coop:same-origin-allow-popup.
IN_PROC_BROWSER_TEST_P(VirtualBrowsingContextGroupTest,
                       CoopAllowPopup_CoopAllowPopup) {
  const VirtualBcgAllowPopupTestCase kTestCases[] = {
      {
          // same-origin.
          https_server()->GetURL(
              "a.com",
              "/set-header?"
              "Cross-Origin-Opener-Policy: same-origin-allow-popups&"
              "Cross-Origin-Embedder-Policy: require-corp"),
          https_server()->GetURL(
              "a.com",
              "/set-header?"
              "Cross-Origin-Opener-Policy: same-origin-allow-popups&"
              "Cross-Origin-Embedder-Policy: require-corp"),
          false,
          false,
      },
      {
          // cross-origin.
          https_server()->GetURL(
              "a.a.com",
              "/set-header?"
              "Cross-Origin-Opener-Policy: same-origin-allow-popups&"
              "Cross-Origin-Embedder-Policy: require-corp"),
          https_server()->GetURL(
              "b.a.com",
              "/set-header?"
              "Cross-Origin-Opener-Policy: same-origin-allow-popups&"
              "Cross-Origin-Embedder-Policy: require-corp"),
          true,
          true,
      },
      {
          // cross-site.
          https_server()->GetURL(
              "a.com",
              "/set-header?"
              "Cross-Origin-Opener-Policy: same-origin-allow-popups&"
              "Cross-Origin-Embedder-Policy: require-corp"),
          https_server()->GetURL(
              "b.com",
              "/set-header?"
              "Cross-Origin-Opener-Policy: same-origin-allow-popups&"
              "Cross-Origin-Embedder-Policy: require-corp"),
          true,
          true,
      },

  };
  for (const auto& test : kTestCases)
    RunTest(test, shell());
}

// coop:same-origin-allow-popup -> coop-ro:same-origin-allow-popup.
IN_PROC_BROWSER_TEST_P(VirtualBrowsingContextGroupTest,
                       CoopAllowPopup_CoopRoAllowPopup) {
  const VirtualBcgAllowPopupTestCase kTestCases[] = {
      {
          // same-origin.
          https_server()->GetURL("a.com",
                                 "/set-header?"
                                 "Cross-Origin-Opener-Policy: "
                                 "same-origin-allow-popups&"
                                 "Cross-Origin-Embedder-Policy: require-corp"),
          https_server()->GetURL("a.com",
                                 "/set-header?"
                                 "Cross-Origin-Opener-Policy-Report-Only: "
                                 "same-origin-allow-popups&"
                                 "Cross-Origin-Embedder-Policy: require-corp"),
          false,
          true,
      },
      {
          // cross-origin.
          https_server()->GetURL("a.a.com",
                                 "/set-header?"
                                 "Cross-Origin-Opener-Policy: "
                                 "same-origin-allow-popups&"
                                 "Cross-Origin-Embedder-Policy: require-corp"),
          https_server()->GetURL("b.a.com",
                                 "/set-header?"
                                 "Cross-Origin-Opener-Policy-Report-Only: "
                                 "same-origin-allow-popups&"
                                 "Cross-Origin-Embedder-Policy: require-corp"),
          true,
          true,
      },
      {
          // cross-site.
          https_server()->GetURL("a.com",
                                 "/set-header?"
                                 "Cross-Origin-Opener-Policy: "
                                 "same-origin-allow-popups&"
                                 "Cross-Origin-Embedder-Policy: require-corp"),
          https_server()->GetURL("b.com",
                                 "/set-header?"
                                 "Cross-Origin-Opener-Policy-Report-Only: "
                                 "same-origin-allow-popups&"
                                 "Cross-Origin-Embedder-Policy: require-corp"),
          true,
          true,
      },
  };
  for (const auto& test : kTestCases)
    RunTest(test, shell());
}

// coop-ro:same-origin-allow-popup -> coop:same-origin-allow-popup.
IN_PROC_BROWSER_TEST_P(VirtualBrowsingContextGroupTest,
                       CoopRoAllowPopup_CoopAllowPopup) {
  const VirtualBcgAllowPopupTestCase kTestCases[] = {
      {
          // same-origin.
          https_server()->GetURL("a.com",
                                 "/set-header?"
                                 "Cross-Origin-Opener-Policy-Report-Only: "
                                 "same-origin-allow-popups&"
                                 "Cross-Origin-Embedder-Policy: require-corp"),
          https_server()->GetURL("a.com",
                                 "/set-header?"
                                 "Cross-Origin-Opener-Policy: "
                                 "same-origin-allow-popups&"
                                 "Cross-Origin-Embedder-Policy: require-corp"),
          true,
          true,
      },
      {
          // cross-origin.
          https_server()->GetURL("a.a.com",
                                 "/set-header?"
                                 "Cross-Origin-Opener-Policy-Report-Only: "
                                 "same-origin-allow-popups&"
                                 "Cross-Origin-Embedder-Policy: require-corp"),
          https_server()->GetURL(
              "b.a.com",
              "/set-header?"
              "Cross-Origin-Opener-Policy: same-origin-allow-popups&"
              "Cross-Origin-Embedder-Policy: require-corp"),
          true,
          true,
      },
      {
          // cross-site.
          https_server()->GetURL(
              "a.com",
              "/set-header?"
              "Cross-Origin-Opener-Policy: same-origin-allow-popups&"
              "Cross-Origin-Embedder-Policy: require-corp"),
          https_server()->GetURL(
              "b.com",
              "/set-header?"
              "Cross-Origin-Opener-Policy: same-origin-allow-popups&"
              "Cross-Origin-Embedder-Policy: require-corp"),
          true,
          true,
      },
  };

  for (const auto& test : kTestCases)
    RunTest(test, shell());
}

// coop:same-origin-allow-popup + coop-ro:same-origin-allow-popup -> coop:none.
IN_PROC_BROWSER_TEST_P(VirtualBrowsingContextGroupTest,
                       CoopPopupRoSameOrigin_NonCoop) {
  const VirtualBcgAllowPopupTestCase kTestCases[] = {
      // coop:allow-popup, coop-ro:same-origin-> no-coop.
      {
          // same-origin.
          https_server()->GetURL(
              "a.com",
              "/set-header?"
              "Cross-Origin-Opener-Policy: same-origin-allow-popups&"
              "Cross-Origin-Opener-Policy-Report-Only: same-origin&"
              "Cross-Origin-Embedder-Policy: require-corp"),
          https_server()->GetURL("a.com", "/title1.html"),
          true,
          true,
      },
      {
          // cross-origin.
          https_server()->GetURL(
              "a.a.com",
              "/set-header?"
              "Cross-Origin-Opener-Policy: same-origin-allow-popups&"
              "Cross-Origin-Opener-Policy-Report-Only: same-origin&"
              "Cross-Origin-Embedder-Policy: require-corp"),
          https_server()->GetURL("b.a.com", "/title1.html"),
          true,
          true,
      },
      {
          // cross-site.
          https_server()->GetURL(
              "a.com",
              "/set-header?"
              "Cross-Origin-Opener-Policy: same-origin-allow-popups&"
              "Cross-Origin-Opener-Policy-Report-Only: same-origin&"
              "Cross-Origin-Embedder-Policy: require-corp"),
          https_server()->GetURL("b.com", "/title1.html"),
          true,
          true,
      },
  };

  for (const auto& test : kTestCases)
    RunTest(test, shell());
}

// Navigates in between two pages from a different browsing context group. Then
// use the history API to navigate back and forth. Check their virtual browsing
// context group isn't restored.
// The goal is to spot differences when the BackForwardCache is enabled. See
// https://crbug.com/1109648.
IN_PROC_BROWSER_TEST_P(VirtualBrowsingContextGroupTest, HistoryNavigation) {
  GURL url_a = https_server()->GetURL(
      "a.com",
      "/set-header?"
      "Cross-Origin-Opener-Policy-Report-Only: same-origin&"
      "Cross-Origin-Embedder-Policy: require-corp");
  GURL url_b = https_server()->GetURL(
      "b.com",
      "/set-header?"
      "Cross-Origin-Opener-Policy-Report-Only: same-origin&"
      "Cross-Origin-Embedder-Policy: require-corp");

  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  int group_1 = VirtualBrowsingContextGroup(web_contents());

  EXPECT_TRUE(NavigateToURL(shell(), url_b));
  int group_2 = VirtualBrowsingContextGroup(web_contents());

  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(web_contents()));
  int group_3 = VirtualBrowsingContextGroup(web_contents());

  web_contents()->GetController().GoForward();
  EXPECT_TRUE(WaitForLoadStop(web_contents()));
  int group_4 = VirtualBrowsingContextGroup(web_contents());

  // No matter whether the BackForwardCache is enabled or not, the navigation in
  // between the two URLs must always cross a virtual browsing context group.
  EXPECT_NE(group_1, group_2);
  EXPECT_NE(group_2, group_3);
  EXPECT_NE(group_3, group_4);
  EXPECT_NE(group_1, group_4);

  // TODO(https://crbug.com/1112256) During history navigation, the virtual
  // browsing context group must be restored whenever the SiteInstance is
  // restored. Currently, the SiteInstance is restored, but the virtual browsing
  // context group is new.

  if (IsBackForwardCacheEnabled()) {
    EXPECT_EQ(group_1, group_3);
    EXPECT_EQ(group_2, group_4);
  } else {
    EXPECT_NE(group_1, group_3);
    EXPECT_NE(group_2, group_4);
  }
}

// 1. A1 opens B2 (same virtual browsing context group).
// 2. B2 navigates to C3 (different virtual browsing context group).
// 3. C3 navigates back to B4 using the history (different virtual browsing
//    context group).
//
// A1 and B4 must not be in the same browsing context group.
IN_PROC_BROWSER_TEST_P(VirtualBrowsingContextGroupTest,
                       HistoryNavigationWithPopup) {
  GURL url_a = https_server()->GetURL("a.com", "/title1.html");
  GURL url_b = https_server()->GetURL("b.com", "/title1.html");
  GURL url_c = https_server()->GetURL(
      "c.com",
      "/set-header?"
      "Cross-Origin-Opener-Policy-Report-Only: same-origin&"
      "Cross-Origin-Embedder-Policy: require-corp");

  // Navigate to A1.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  int group_1 = VirtualBrowsingContextGroup(web_contents());

  // A1 opens B2.
  ShellAddedObserver shell_observer;
  EXPECT_TRUE(
      ExecJs(current_frame_host(), JsReplace("window.open($1)", url_b)));
  WebContents* popup = shell_observer.GetShell()->web_contents();
  EXPECT_TRUE(WaitForLoadStop(popup));
  int group_2 = VirtualBrowsingContextGroup(popup);

  // B2 navigates to C3.
  EXPECT_TRUE(ExecJs(popup, JsReplace("location.href = $1;", url_c)));
  EXPECT_TRUE(WaitForLoadStop(popup));
  int group_3 = VirtualBrowsingContextGroup(popup);

  // C3 navigates back to B4.
  EXPECT_TRUE(ExecJs(popup, JsReplace("history.back()")));
  EXPECT_TRUE(WaitForLoadStop(popup));
  int group_4 = VirtualBrowsingContextGroup(popup);

  EXPECT_EQ(group_1, group_2);
  EXPECT_NE(group_2, group_3);
  EXPECT_NE(group_3, group_4);
  EXPECT_NE(group_4, group_1);
}

IN_PROC_BROWSER_TEST_P(CrossOriginOpenerPolicyBrowserTest,
                       CrossOriginIsolatedSiteInstance_MainFrame) {
  GURL isolated_page(
      https_server()->GetURL("a.com",
                             "/set-header?"
                             "Cross-Origin-Opener-Policy: same-origin&"
                             "Cross-Origin-Embedder-Policy: require-corp"));
  GURL isolated_page_b(
      https_server()->GetURL("cdn.a.com",
                             "/set-header?"
                             "Cross-Origin-Opener-Policy: same-origin&"
                             "Cross-Origin-Embedder-Policy: require-corp"));
  GURL non_isolated_page(https_server()->GetURL("a.com", "/title1.html"));

  // Navigation from/to cross-origin isolated pages.

  // Initial non cross-origin isolated page.
  {
    EXPECT_TRUE(NavigateToURL(shell(), non_isolated_page));
    SiteInstanceImpl* current_si = current_frame_host()->GetSiteInstance();
    EXPECT_FALSE(current_si->IsCoopCoepCrossOriginIsolated());
  }

  // Navigation to a cross-origin isolated page.
  {
    scoped_refptr<SiteInstanceImpl> previous_si =
        current_frame_host()->GetSiteInstance();
    EXPECT_TRUE(NavigateToURL(shell(), isolated_page));
    SiteInstanceImpl* current_si = current_frame_host()->GetSiteInstance();
    EXPECT_TRUE(current_si->IsCoopCoepCrossOriginIsolated());
    EXPECT_FALSE(current_si->IsRelatedSiteInstance(previous_si.get()));
    EXPECT_NE(current_si->GetProcess(), previous_si->GetProcess());
  }

  // Navigation to the same cross-origin isolated page.
  {
    scoped_refptr<SiteInstanceImpl> previous_si =
        current_frame_host()->GetSiteInstance();
    EXPECT_TRUE(NavigateToURL(shell(), isolated_page));
    SiteInstanceImpl* current_si = current_frame_host()->GetSiteInstance();
    EXPECT_TRUE(current_si->IsCoopCoepCrossOriginIsolated());
    EXPECT_EQ(current_si, previous_si);
  }

  // Navigation to a non cross-origin isolated page.
  {
    scoped_refptr<SiteInstanceImpl> previous_si =
        current_frame_host()->GetSiteInstance();
    EXPECT_TRUE(NavigateToURL(shell(), non_isolated_page));
    SiteInstanceImpl* current_si = current_frame_host()->GetSiteInstance();
    EXPECT_FALSE(current_si->IsCoopCoepCrossOriginIsolated());
    EXPECT_FALSE(current_si->IsRelatedSiteInstance(previous_si.get()));
    EXPECT_NE(current_si->GetProcess(), previous_si->GetProcess());
  }

  // Back navigation from a cross-origin isolated page to a non cross-origin
  // isolated page.
  {
    EXPECT_TRUE(NavigateToURL(shell(), isolated_page));
    scoped_refptr<SiteInstanceImpl> cross_origin_isolated_site_instance =
        current_frame_host()->GetSiteInstance();

    EXPECT_TRUE(
        cross_origin_isolated_site_instance->IsCoopCoepCrossOriginIsolated());
    web_contents()->GetController().GoBack();
    EXPECT_TRUE(WaitForLoadStop(web_contents()));

    scoped_refptr<SiteInstanceImpl> non_cross_origin_isolated_site_instance =
        current_frame_host()->GetSiteInstance();

    EXPECT_FALSE(non_cross_origin_isolated_site_instance
                     ->IsCoopCoepCrossOriginIsolated());
    EXPECT_FALSE(non_cross_origin_isolated_site_instance->IsRelatedSiteInstance(
        cross_origin_isolated_site_instance.get()));
    EXPECT_NE(non_cross_origin_isolated_site_instance->GetProcess(),
              cross_origin_isolated_site_instance->GetProcess());
  }

  // Cross origin navigation in between two cross-origin isolated pages.
  {
    EXPECT_TRUE(NavigateToURL(shell(), isolated_page));
    scoped_refptr<SiteInstanceImpl> site_instance_1 =
        current_frame_host()->GetSiteInstance();
    EXPECT_TRUE(NavigateToURL(shell(), isolated_page_b));
    SiteInstanceImpl* site_instance_2 = current_frame_host()->GetSiteInstance();
    EXPECT_TRUE(site_instance_1->IsCoopCoepCrossOriginIsolated());
    EXPECT_TRUE(site_instance_2->IsCoopCoepCrossOriginIsolated());
    EXPECT_FALSE(site_instance_1->IsRelatedSiteInstance(site_instance_2));
    EXPECT_NE(site_instance_1->GetProcess(), site_instance_2->GetProcess());
  }
}

IN_PROC_BROWSER_TEST_P(
    CrossOriginOpenerPolicyBrowserTest,
    CrossOriginIsolatedSiteInstance_MainFrameRendererInitiated) {
  GURL isolated_page(
      https_server()->GetURL("a.com",
                             "/set-header?"
                             "Cross-Origin-Opener-Policy: same-origin&"
                             "Cross-Origin-Embedder-Policy: require-corp"));
  GURL isolated_page_b(
      https_server()->GetURL("cdn.a.com",
                             "/set-header?"
                             "Cross-Origin-Opener-Policy: same-origin&"
                             "Cross-Origin-Embedder-Policy: require-corp"));
  GURL non_isolated_page(https_server()->GetURL("a.com", "/title1.html"));

  // Navigation from/to cross-origin isolated pages.

  // Initial non cross-origin isolated page.
  {
    EXPECT_TRUE(NavigateToURL(shell(), non_isolated_page));
    SiteInstanceImpl* current_si = current_frame_host()->GetSiteInstance();
    EXPECT_FALSE(current_si->IsCoopCoepCrossOriginIsolated());
  }

  // Navigation to a cross-origin isolated page.
  {
    scoped_refptr<SiteInstanceImpl> previous_si =
        current_frame_host()->GetSiteInstance();
    EXPECT_TRUE(NavigateToURLFromRenderer(shell(), isolated_page));
    SiteInstanceImpl* current_si = current_frame_host()->GetSiteInstance();
    EXPECT_TRUE(current_si->IsCoopCoepCrossOriginIsolated());
    EXPECT_FALSE(current_si->IsRelatedSiteInstance(previous_si.get()));
    EXPECT_NE(current_si->GetProcess(), previous_si->GetProcess());
  }

  // Navigation to the same cross-origin isolated page.
  {
    scoped_refptr<SiteInstanceImpl> previous_si =
        current_frame_host()->GetSiteInstance();
    EXPECT_TRUE(NavigateToURLFromRenderer(shell(), isolated_page));
    SiteInstanceImpl* current_si = current_frame_host()->GetSiteInstance();
    EXPECT_TRUE(current_si->IsCoopCoepCrossOriginIsolated());
    EXPECT_EQ(current_si, previous_si);
  }

  // Navigation to a non cross-origin isolated page.
  {
    scoped_refptr<SiteInstanceImpl> previous_si =
        current_frame_host()->GetSiteInstance();
    EXPECT_TRUE(NavigateToURLFromRenderer(shell(), non_isolated_page));
    SiteInstanceImpl* current_si = current_frame_host()->GetSiteInstance();
    EXPECT_FALSE(current_si->IsCoopCoepCrossOriginIsolated());
    EXPECT_FALSE(current_si->IsRelatedSiteInstance(previous_si.get()));
    EXPECT_NE(current_si->GetProcess(), previous_si->GetProcess());
  }

  // Cross origin navigation in between two cross-origin isolated pages.
  {
    EXPECT_TRUE(NavigateToURLFromRenderer(shell(), isolated_page));
    scoped_refptr<SiteInstanceImpl> site_instance_1 =
        current_frame_host()->GetSiteInstance();
    EXPECT_TRUE(NavigateToURLFromRenderer(shell(), isolated_page_b));
    SiteInstanceImpl* site_instance_2 = current_frame_host()->GetSiteInstance();
    EXPECT_TRUE(site_instance_1->IsCoopCoepCrossOriginIsolated());
    EXPECT_TRUE(site_instance_2->IsCoopCoepCrossOriginIsolated());
    EXPECT_FALSE(site_instance_1->IsRelatedSiteInstance(site_instance_2));
    EXPECT_NE(site_instance_1->GetProcess(), site_instance_2->GetProcess());
  }
}

IN_PROC_BROWSER_TEST_P(CrossOriginOpenerPolicyBrowserTest,
                       CrossOriginIsolatedSiteInstance_IFrame) {
  GURL isolated_page(
      https_server()->GetURL("a.com",
                             "/set-header?"
                             "Cross-Origin-Opener-Policy: same-origin&"
                             "Cross-Origin-Embedder-Policy: require-corp"));
  GURL isolated_page_b(
      https_server()->GetURL("cdn.a.com",
                             "/set-header?"
                             "Cross-Origin-Embedder-Policy: require-corp&"
                             "Cross-Origin-Resource-Policy: cross-origin"));

  // Initial cross-origin isolated page.
  EXPECT_TRUE(NavigateToURL(shell(), isolated_page));
  SiteInstanceImpl* main_si = current_frame_host()->GetSiteInstance();
  EXPECT_TRUE(main_si->IsCoopCoepCrossOriginIsolated());

  // Same origin iframe.
  {
    TestNavigationManager same_origin_iframe_navigation(web_contents(),
                                                        isolated_page);

    EXPECT_TRUE(
        ExecJs(web_contents(),
               JsReplace("var iframe = document.createElement('iframe'); "
                         "iframe.src = $1; "
                         "document.body.appendChild(iframe);",
                         isolated_page)));

    same_origin_iframe_navigation.WaitForNavigationFinished();
    EXPECT_TRUE(same_origin_iframe_navigation.was_successful());
    RenderFrameHostImpl* iframe =
        current_frame_host()->child_at(0)->current_frame_host();
    SiteInstanceImpl* iframe_si = iframe->GetSiteInstance();
    EXPECT_EQ(iframe_si, main_si);
  }

  // Cross origin iframe.
  {
    TestNavigationManager cross_origin_iframe_navigation(web_contents(),
                                                         isolated_page_b);

    EXPECT_TRUE(
        ExecJs(web_contents(),
               JsReplace("var iframe = document.createElement('iframe'); "
                         "iframe.src = $1; "
                         "document.body.appendChild(iframe);",
                         isolated_page_b)));

    cross_origin_iframe_navigation.WaitForNavigationFinished();
    EXPECT_TRUE(cross_origin_iframe_navigation.was_successful());
    RenderFrameHostImpl* iframe =
        current_frame_host()->child_at(1)->current_frame_host();
    SiteInstanceImpl* iframe_si = iframe->GetSiteInstance();
    EXPECT_TRUE(iframe_si->IsCoopCoepCrossOriginIsolated());
    EXPECT_TRUE(iframe_si->IsRelatedSiteInstance(main_si));
    EXPECT_EQ(iframe_si->GetProcess(), main_si->GetProcess());
  }
}

IN_PROC_BROWSER_TEST_P(CrossOriginOpenerPolicyBrowserTest,
                       CrossOriginIsolatedSiteInstance_Popup) {
  GURL isolated_page(
      https_server()->GetURL("a.com",
                             "/set-header?"
                             "Cross-Origin-Opener-Policy: same-origin&"
                             "Cross-Origin-Embedder-Policy: require-corp"));
  GURL isolated_page_b(
      https_server()->GetURL("cdn.a.com",
                             "/set-header?"
                             "Cross-Origin-Opener-Policy: same-origin&"
                             "Cross-Origin-Embedder-Policy: require-corp"));
  GURL non_isolated_page(
      embedded_test_server()->GetURL("a.com", "/title1.html"));

  // Initial cross-origin isolated page.
  EXPECT_TRUE(NavigateToURL(shell(), isolated_page));
  SiteInstanceImpl* main_si = current_frame_host()->GetSiteInstance();
  EXPECT_TRUE(main_si->IsCoopCoepCrossOriginIsolated());

  // Open a non isolated popup.
  {
    RenderFrameHostImpl* popup_frame =
        static_cast<WebContentsImpl*>(
            OpenPopup(current_frame_host(), non_isolated_page, "")
                ->web_contents())
            ->GetFrameTree()
            ->root()
            ->current_frame_host();

    EXPECT_FALSE(
        popup_frame->GetSiteInstance()->IsCoopCoepCrossOriginIsolated());
    EXPECT_FALSE(popup_frame->GetSiteInstance()->IsRelatedSiteInstance(
        current_frame_host()->GetSiteInstance()));
    EXPECT_FALSE(popup_frame->frame_tree_node()->opener());
  }

  // Open an isolated popup.
  {
    RenderFrameHostImpl* popup_frame =
        static_cast<WebContentsImpl*>(
            OpenPopup(current_frame_host(), isolated_page, "")->web_contents())
            ->GetFrameTree()
            ->root()
            ->current_frame_host();

    EXPECT_TRUE(
        popup_frame->GetSiteInstance()->IsCoopCoepCrossOriginIsolated());
    EXPECT_EQ(popup_frame->GetSiteInstance(),
              current_frame_host()->GetSiteInstance());
  }

  // Open an isolated popup, but cross-origin.
  {
    RenderFrameHostImpl* popup_frame =
        static_cast<WebContentsImpl*>(
            OpenPopup(current_frame_host(), isolated_page_b, "")
                ->web_contents())
            ->GetFrameTree()
            ->root()
            ->current_frame_host();

    EXPECT_TRUE(
        popup_frame->GetSiteInstance()->IsCoopCoepCrossOriginIsolated());
    EXPECT_FALSE(popup_frame->GetSiteInstance()->IsRelatedSiteInstance(
        current_frame_host()->GetSiteInstance()));
    EXPECT_FALSE(popup_frame->frame_tree_node()->opener());
    EXPECT_NE(popup_frame->GetSiteInstance()->GetProcess(),
              current_frame_host()->GetSiteInstance()->GetProcess());
  }
}

IN_PROC_BROWSER_TEST_P(CrossOriginOpenerPolicyBrowserTest,
                       CrossOriginIsolatedSiteInstance_ErrorPage) {
  GURL isolated_page(
      https_server()->GetURL("a.com",
                             "/set-header?"
                             "Cross-Origin-Opener-Policy: same-origin&"
                             "Cross-Origin-Embedder-Policy: require-corp"));
  GURL non_coep_page(https_server()->GetURL("b.com",
                                            "/set-header?"
                                            "Access-Control-Allow-Origin: *"));

  GURL invalid_url(
      https_server()->GetURL("a.com", "/this_page_does_not_exist.html"));

  GURL error_url(https_server()->GetURL("a.com", "/page404.html"));

  // Initial cross-origin isolated page.
  EXPECT_TRUE(NavigateToURL(shell(), isolated_page));
  SiteInstanceImpl* main_si = current_frame_host()->GetSiteInstance();
  EXPECT_TRUE(main_si->IsCoopCoepCrossOriginIsolated());

  // Iframe.
  {
    TestNavigationManager iframe_navigation(web_contents(), invalid_url);

    EXPECT_TRUE(
        ExecJs(web_contents(),
               JsReplace("var iframe = document.createElement('iframe'); "
                         "iframe.src = $1; "
                         "document.body.appendChild(iframe);",
                         invalid_url)));

    iframe_navigation.WaitForNavigationFinished();
    EXPECT_FALSE(iframe_navigation.was_successful());
    RenderFrameHostImpl* iframe =
        current_frame_host()->child_at(0)->current_frame_host();
    SiteInstanceImpl* iframe_si = iframe->GetSiteInstance();
    // The load of the document with 404 status code is blocked by COEP.
    // An error page is expected in lieu of that document.
    EXPECT_EQ(GURL(kUnreachableWebDataURL),
              EvalJs(iframe, "document.location.href;"));
    EXPECT_EQ(iframe_si, main_si);
    EXPECT_TRUE(iframe_si->IsCoopCoepCrossOriginIsolated());
  }

  // Iframe with a body added to the HTTP 404.
  {
    TestNavigationManager iframe_navigation(web_contents(), error_url);

    EXPECT_TRUE(
        ExecJs(web_contents(),
               JsReplace("var iframe = document.createElement('iframe'); "
                         "iframe.src = $1; "
                         "document.body.appendChild(iframe);",
                         error_url)));

    iframe_navigation.WaitForNavigationFinished();
    EXPECT_FALSE(iframe_navigation.was_successful());
    RenderFrameHostImpl* iframe =
        current_frame_host()->child_at(0)->current_frame_host();
    SiteInstanceImpl* iframe_si = iframe->GetSiteInstance();
    EXPECT_EQ(iframe_si, main_si);
    // The load of the document with 404 status code and custom body is blocked
    // by COEP. An error page is expected in lieu of that document.
    EXPECT_EQ(GURL(kUnreachableWebDataURL),
              EvalJs(iframe, "document.location.href;"));
    EXPECT_TRUE(iframe_si->IsCoopCoepCrossOriginIsolated());
  }

  // Iframe blocked by coep.
  {
    TestNavigationManager iframe_navigation(web_contents(), non_coep_page);

    EXPECT_TRUE(
        ExecJs(web_contents(),
               JsReplace("var iframe = document.createElement('iframe'); "
                         "iframe.src = $1; "
                         "document.body.appendChild(iframe);",
                         non_coep_page)));

    iframe_navigation.WaitForNavigationFinished();
    EXPECT_FALSE(iframe_navigation.was_successful());
    RenderFrameHostImpl* iframe =
        current_frame_host()->child_at(0)->current_frame_host();
    SiteInstanceImpl* iframe_si = iframe->GetSiteInstance();
    EXPECT_EQ(iframe_si, main_si);
    EXPECT_TRUE(iframe_si->IsCoopCoepCrossOriginIsolated());
  }

  // Top frame.
  {
    scoped_refptr<SiteInstanceImpl> previous_si =
        current_frame_host()->GetSiteInstance();
    EXPECT_FALSE(NavigateToURL(shell(), invalid_url));
    SiteInstanceImpl* current_si = current_frame_host()->GetSiteInstance();
    EXPECT_FALSE(current_si->IsRelatedSiteInstance(previous_si.get()));
    EXPECT_NE(current_si->GetProcess(), previous_si->GetProcess());
    EXPECT_FALSE(current_si->IsCoopCoepCrossOriginIsolated());
  }
}

IN_PROC_BROWSER_TEST_P(CrossOriginOpenerPolicyBrowserTest,
                       CrossOriginRedirectHasProperCrossOriginIsolatedState) {
  GURL non_isolated_page(
      embedded_test_server()->GetURL("a.com", "/title1.html"));

  GURL isolated_page(
      https_server()->GetURL("c.com",
                             "/set-header?"
                             "Cross-Origin-Opener-Policy: same-origin&"
                             "Cross-Origin-Embedder-Policy: require-corp"));

  GURL redirect_isolated_page(https_server()->GetURL(
      "b.com", "/redirect-with-coop-coep-headers?" + isolated_page.spec()));

  EXPECT_TRUE(NavigateToURL(shell(), non_isolated_page));
  SiteInstanceImpl* current_si = current_frame_host()->GetSiteInstance();
  EXPECT_FALSE(current_si->IsCoopCoepCrossOriginIsolated());

  EXPECT_TRUE(NavigateToURL(shell(), redirect_isolated_page, isolated_page));
  current_si = current_frame_host()->GetSiteInstance();
  EXPECT_TRUE(current_si->IsCoopCoepCrossOriginIsolated());
  EXPECT_TRUE(current_si->CoopCoepCrossOriginIsolatedOrigin()->IsSameOriginWith(
      url::Origin::Create(isolated_page)));
}

// TODO(https://crbug.com/1101339). Test inheritance of the virtual browsing
// context group when using window.open from an iframe, same-origin and
// cross-origin.

static auto kTestParams =
    testing::Combine(testing::ValuesIn(RenderDocumentFeatureLevelValues()),
                     testing::Bool());
INSTANTIATE_TEST_SUITE_P(All, CrossOriginOpenerPolicyBrowserTest, kTestParams);
INSTANTIATE_TEST_SUITE_P(All, VirtualBrowsingContextGroupTest, kTestParams);

namespace {

// Ensure the CrossOriginOpenerPolicyReporting origin trial is correctly
// implemented.
class CoopReportingOriginTrialBrowserTest : public ContentBrowserTest {
 public:
  CoopReportingOriginTrialBrowserTest() {
    feature_list_.InitWithFeatures(
        {
            // Enabled
            network::features::kCrossOriginOpenerPolicy,
            network::features::kCrossOriginEmbedderPolicy,
            network::features::kCrossOriginOpenerPolicyAccessReporting,
            network::features::kCrossOriginOpenerPolicyReportingOriginTrial,
        },
        {
            // Disabled
            network::features::kCrossOriginOpenerPolicyReporting,
        });
  }

  // Origin Trials key generated with:
  //
  // tools/origin_trials/generate_token.py --expire-days 5000 --version 3
  // https://coop.security:9999 CrossOriginOpenerPolicyReporting
  static std::string OriginTrialToken() {
    return "A5U4dXG9lYhhLSumDmXNObrt5xJ0XVpSfw/"
           "w7q+MYzOziNnHfcl1ZShjKjecyEc3E5vDtHV+"
           "wiLMbqukLwhs8gIAAABteyJvcmlnaW4iOiAiaHR0cHM6Ly9jb29wLnNlY3VyaXR5Ojk"
           "5OTkiLCAiZmVhdHVyZSI6ICJDcm9zc09yaWdpbk9wZW5lclBvbGljeVJlcG9ydGluZy"
           "IsICJleHBpcnkiOiAyMDI5NzA4MDA3fQ==";
  }

  // The OriginTrial token is bound to a given origin. Since the
  // EmbeddedTestServer's port changes after every test run, it can't be used.
  // As a result, response must be served using a URLLoaderInterceptor.
  GURL OriginTrialURL() { return GURL("https://coop.security:9999"); }

  WebContentsImpl* web_contents() const {
    return static_cast<WebContentsImpl*>(shell()->web_contents());
  }

  RenderFrameHostImpl* current_frame_host() {
    return web_contents()->GetMainFrame();
  }

  net::EmbeddedTestServer* https_server() { return &https_server_; }

  // On every platforms, except on Android, ContainMain is called for
  // browsertest. This calls SetOriginTrialPolicyGetter.
  // On Android, a meager re-implementation of ContentMainRunnerImpl is made by
  // BrowserTestBase. This doesn't call SetOriginTrialPolicyGetter.
  //
  // So on Android + BrowserTestBase + browser process, the OriginTrial policy
  // isn't setup. This is tracked by https://crbug.com/1123953
  //
  // To fix this we could:
  //
  // 1) Fix https://crbug.com/1123953. Call SetOriginTrialPolicyGetter using
  //    GetContentClient()->GetOriginTrialPolicy() from BrowserTestBase. This
  //    doesn't work, because GetContentClient() is private to the
  //    implementation of content/, unreachable from the test.
  //
  // 2) Setup our own blink::OriginTrialPolicy here, based on
  //    embedder_support::OriginTrialPolicy. This doesn't work, because this
  //    violate the DEPS rules.
  //
  // 3) Copy-paste the implementation of embedder_support::OriginTrialPolicy
  //    here. This doesn't really worth the cost.
  //
  // Instead we abandon testing the OriginTrial on the Android platform :-(
  //
  // TODO(https://crbug.com/1123953). Remove this once fixed.
  bool IsOriginTrialPolicySetup() {
#if defined(OS_ANDROID)
    return false;
#else
    return true;
#endif
  }

 private:
  void SetUpOnMainThread() final {
    ContentBrowserTest::TearDownOnMainThread();

    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
    https_server()->ServeFilesFromSourceDirectory(GetTestDataFilePath());
    SetupCrossSiteRedirector(https_server());
    net::test_server::RegisterDefaultHandlers(&https_server_);
    ASSERT_TRUE(https_server()->Start());
  }
  void TearDownOnMainThread() final {
    ContentBrowserTest::TearDownOnMainThread();
  }

  void SetUpCommandLine(base::CommandLine* command_line) final {
    ContentBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kIgnoreCertificateErrors);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  net::EmbeddedTestServer https_server_;
};

}  // namespace

IN_PROC_BROWSER_TEST_F(CoopReportingOriginTrialBrowserTest,
                       CoopStateWithoutToken) {
  URLLoaderInterceptor interceptor(base::BindLambdaForTesting(
      [&](URLLoaderInterceptor::RequestParams* params) {
        if (params->url_request.url != OriginTrialURL())
          return false;
        URLLoaderInterceptor::WriteResponse(
            "HTTP/1.1 200 OK\n"
            "Content-type: text/html\n"
            "Cross-Origin-Opener-Policy: same-origin; report-to=\"a\"\n"
            "Cross-Origin-Opener-Policy-Report-Only: same-origin; "
            "report-to=\"b\"\n"
            "Cross-Origin-Embedder-Policy: require-corp\n"
            "\n",
            "", params->client.get());
        return true;
      }));
  EXPECT_TRUE(NavigateToURL(shell(), OriginTrialURL()));
  network::CrossOriginOpenerPolicy coop =
      current_frame_host()->cross_origin_opener_policy();
  EXPECT_EQ(coop.reporting_endpoint, base::nullopt);
  EXPECT_EQ(coop.report_only_reporting_endpoint, base::nullopt);
  EXPECT_EQ(coop.value,
            network::mojom::CrossOriginOpenerPolicyValue::kSameOriginPlusCoep);
  EXPECT_EQ(coop.report_only_value,
            network::mojom::CrossOriginOpenerPolicyValue::kUnsafeNone);
}

IN_PROC_BROWSER_TEST_F(CoopReportingOriginTrialBrowserTest,
                       CoopStateWithToken) {
  // TODO(https://crbug.com/1123953). Remove this once fixed.
  if (!IsOriginTrialPolicySetup())
    return;

  URLLoaderInterceptor interceptor(base::BindLambdaForTesting(
      [&](URLLoaderInterceptor::RequestParams* params) {
        if (params->url_request.url != OriginTrialURL())
          return false;
        URLLoaderInterceptor::WriteResponse(
            "HTTP/1.1 200 OK\n"
            "Content-type: text/html\n"
            "Cross-Origin-Opener-Policy: same-origin; report-to=\"a\"\n"
            "Cross-Origin-Opener-Policy-Report-Only: same-origin; "
            "report-to=\"b\"\n"
            "Cross-Origin-Embedder-Policy: require-corp\n"
            "Origin-Trial: " +
                OriginTrialToken() + "\n\n",
            "", params->client.get());
        return true;
      }));
  EXPECT_TRUE(NavigateToURL(shell(), OriginTrialURL()));
  network::CrossOriginOpenerPolicy coop =
      current_frame_host()->cross_origin_opener_policy();
  EXPECT_EQ(coop.reporting_endpoint, "a");
  EXPECT_EQ(coop.report_only_reporting_endpoint, "b");
  EXPECT_EQ(coop.value,
            network::mojom::CrossOriginOpenerPolicyValue::kSameOriginPlusCoep);
  EXPECT_EQ(coop.report_only_value,
            network::mojom::CrossOriginOpenerPolicyValue::kSameOriginPlusCoep);
}

// TODO(http://crbug.com/1119555): Flaky on android-bfcache-rel.
IN_PROC_BROWSER_TEST_F(CoopReportingOriginTrialBrowserTest,
                       DISABLED_AccessReportingWithoutToken) {
  URLLoaderInterceptor interceptor(base::BindLambdaForTesting(
      [&](URLLoaderInterceptor::RequestParams* params) {
        if (params->url_request.url != OriginTrialURL())
          return false;
        URLLoaderInterceptor::WriteResponse(
            "HTTP/1.1 200 OK\n"
            "Content-type: text/html\n"
            "Cross-Origin-Opener-Policy-Report-Only: same-origin; "
            "report-to=\"b\"\n"
            "Cross-Origin-Embedder-Policy: require-corp\n\n",
            "", params->client.get());
        return true;
      }));

  EXPECT_TRUE(NavigateToURL(shell(), OriginTrialURL()));
  ShellAddedObserver shell_observer;
  GURL openee_url = https_server()->GetURL("a.com", "/title1.html");
  EXPECT_TRUE(ExecJs(current_frame_host(),
                     JsReplace("openee = window.open($1);", openee_url)));
  auto* popup =
      static_cast<WebContentsImpl*>(shell_observer.GetShell()->web_contents());
  WaitForLoadStop(popup);

  auto eval = EvalJs(current_frame_host(), R"(
    new Promise(resolve => {
      let observer = new ReportingObserver(()=>{});
      observer.observe();
      openee.postMessage("hello");
      let reports = observer.takeRecords();
      resolve(JSON.stringify(reports));
    });
  )");
  std::string reports = eval.ExtractString();
  EXPECT_EQ("[]", reports);
}

IN_PROC_BROWSER_TEST_F(CoopReportingOriginTrialBrowserTest,
                       AccessReportingWithToken) {
  // TODO(https://crbug.com/1123953). Remove this once fixed.
  if (!IsOriginTrialPolicySetup())
    return;
  URLLoaderInterceptor interceptor(base::BindLambdaForTesting(
      [&](URLLoaderInterceptor::RequestParams* params) {
        if (params->url_request.url != OriginTrialURL())
          return false;
        URLLoaderInterceptor::WriteResponse(
            "HTTP/1.1 200 OK\n"
            "Content-type: text/html\n"
            "Cross-Origin-Opener-Policy-Report-Only: same-origin; "
            "report-to=\"b\"\n"
            "Cross-Origin-Embedder-Policy: require-corp\n"
            "Origin-Trial: " +
                OriginTrialToken() + "\n\n",
            "", params->client.get());
        return true;
      }));

  EXPECT_TRUE(NavigateToURL(shell(), OriginTrialURL()));
  ShellAddedObserver shell_observer;
  GURL openee_url = https_server()->GetURL("a.com", "/title1.html");
  EXPECT_TRUE(ExecJs(current_frame_host(),
                     JsReplace("openee = window.open($1);", openee_url)));
  auto* popup =
      static_cast<WebContentsImpl*>(shell_observer.GetShell()->web_contents());
  WaitForLoadStop(popup);

  auto eval = EvalJs(current_frame_host(), R"(
    new Promise(resolve => {
      let observer = new ReportingObserver(()=>{});
      observer.observe();
      openee.postMessage("hello");
      let reports = observer.takeRecords();
      resolve(JSON.stringify(reports));
    });
  )");
  std::string reports = eval.ExtractString();
  EXPECT_THAT(reports, HasSubstr("coop-access-violation"));
}

}  // namespace content
