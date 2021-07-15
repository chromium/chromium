// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/bind.h"
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
#include "content/public/test/browser_test_utils.h"
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
         network::features::kCrossOriginOpenerPolicyReporting},
        {});

    // Enable RenderDocument:
    InitAndEnableRenderDocumentFeature(&feature_list_for_render_document_,
                                       std::get<0>(GetParam()));
    // Enable BackForwardCache:
    if (std::get<1>(GetParam())) {
      feature_list_for_back_forward_cache_.InitWithFeaturesAndParameters(
          {{features::kBackForwardCache,
            {{"TimeToLiveInBackForwardCacheInSeconds", "3600"}}}},
          // Allow BackForwardCache for all devices regardless of their memory.
          {features::kBackForwardCacheMemoryControls});
    } else {
      feature_list_for_back_forward_cache_.InitWithFeatures(
          {}, {features::kBackForwardCache});
    }

    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kIgnoreCertificateErrors);
    }

  net::EmbeddedTestServer* https_server() { return &https_server_; }

 protected:
  WebContentsImpl* web_contents() const {
    return static_cast<WebContentsImpl*>(shell()->web_contents());
  }

  RenderFrameHostImpl* current_frame_host() {
    return web_contents()->GetMainFrame();
  }

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

 private:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    ContentBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kIgnoreCertificateErrors);
  }

  base::test::ScopedFeatureList feature_list_;
  base::test::ScopedFeatureList feature_list_for_render_document_;
  base::test::ScopedFeatureList feature_list_for_back_forward_cache_;
  net::EmbeddedTestServer https_server_;
};

// Same as CrossOriginOpenerPolicyBrowserTest, but disable SharedArrayBuffer by
// default for non crossOriginIsolated process. This is the state we will reach
// after resolving: https://crbug.com/1144104
class NoSharedArrayBufferByDefault : public CrossOriginOpenerPolicyBrowserTest {
 public:
  NoSharedArrayBufferByDefault() {
    // Disable SharedArrayBuffer in non crossOriginIsolated process.
    feature_list_.InitWithFeatures(
        // Enabled:
        {},
        // Disabled:
        {
            features::kSharedArrayBuffer,
        });
  }

 private:
  base::test::ScopedFeatureList feature_list_;
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
  GURL starting_page(https_server()->GetURL(
      "a.com", "/set-header?cross-origin-opener-policy: same-origin"));
  EXPECT_TRUE(NavigateToURL(shell(), starting_page));

  RenderFrameHostImpl* main_rfh = current_frame_host();

  // Create same origin child frame.
  ASSERT_TRUE(ExecJs(main_rfh, R"(
    let frame = document.createElement('iframe');
    frame.src = '/empty.html';
    document.body.appendChild(frame);
  )"));
  EXPECT_TRUE(WaitForLoadStop(web_contents()));

  ShellAddedObserver shell_observer;
  RenderFrameHostImpl* iframe_rfh = main_rfh->child_at(0)->current_frame_host();
  EXPECT_TRUE(ExecJs(iframe_rfh, "window.open('about:blank')"));

  RenderFrameHostImpl* popup_rfh =
      static_cast<WebContentsImpl*>(shell_observer.GetShell()->web_contents())
          ->GetFrameTree()
          ->root()
          ->current_frame_host();

  EXPECT_EQ(main_rfh->cross_origin_opener_policy(), CoopSameOrigin());
  EXPECT_EQ(popup_rfh->cross_origin_opener_policy(), CoopSameOrigin());
}

IN_PROC_BROWSER_TEST_P(CrossOriginOpenerPolicyBrowserTest,
                       NewPopupCOOP_InheritsSameOriginAllowPopups) {
  GURL starting_page(https_server()->GetURL(
      "a.com",
      "/set-header?cross-origin-opener-policy: same-origin-allow-popups"));
  EXPECT_TRUE(NavigateToURL(shell(), starting_page));

  RenderFrameHostImpl* main_rfh = current_frame_host();

  // Create same origin child frame.
  ASSERT_TRUE(ExecJs(current_frame_host(), R"(
    let frame = document.createElement('iframe');
    frame.src = '/empty.html';
    document.body.appendChild(frame);
  )"));
  EXPECT_TRUE(WaitForLoadStop(web_contents()));

  ShellAddedObserver shell_observer;
  RenderFrameHostImpl* iframe_rfh = main_rfh->child_at(0)->current_frame_host();
  EXPECT_TRUE(ExecJs(iframe_rfh, "window.open('about:blank')"));

  RenderFrameHostImpl* popup_rfh =
      static_cast<WebContentsImpl*>(shell_observer.GetShell()->web_contents())
          ->GetFrameTree()
          ->root()
          ->current_frame_host();

  EXPECT_EQ(main_rfh->cross_origin_opener_policy(),
            CoopSameOriginAllowPopups());
  EXPECT_EQ(popup_rfh->cross_origin_opener_policy(),
            CoopSameOriginAllowPopups());
}

IN_PROC_BROWSER_TEST_P(CrossOriginOpenerPolicyBrowserTest,
                       NewPopupCOOP_CrossOriginDoesNotInherit) {
  GURL starting_page(https_server()->GetURL(
      "a.com", "/set-header?cross-origin-opener-policy: same-origin"));
  GURL url_b(https_server()->GetURL("b.com", "/empty.html"));

  EXPECT_TRUE(NavigateToURL(shell(), starting_page));

  RenderFrameHostImpl* main_rfh = current_frame_host();

  // Create cross origin child frame.
  ASSERT_TRUE(ExecJs(main_rfh, JsReplace(R"(
    let frame = document.createElement('iframe');
    frame.src = $1;
    document.body.appendChild(frame);
  )",
                                         url_b)));
  EXPECT_TRUE(WaitForLoadStop(web_contents()));

  ShellAddedObserver shell_observer;
  RenderFrameHostImpl* iframe_rfh = main_rfh->child_at(0)->current_frame_host();
  EXPECT_TRUE(ExecJs(iframe_rfh, "window.open('about:blank')"));

  RenderFrameHostImpl* popup_rfh =
      static_cast<WebContentsImpl*>(shell_observer.GetShell()->web_contents())
          ->GetFrameTree()
          ->root()
          ->current_frame_host();

  EXPECT_EQ(main_rfh->cross_origin_opener_policy(), CoopSameOrigin());
  EXPECT_EQ(popup_rfh->cross_origin_opener_policy(), CoopUnsafeNone());
}

IN_PROC_BROWSER_TEST_P(
    CrossOriginOpenerPolicyBrowserTest,
    NewPopupCOOP_SameOriginPolicyAndCrossOriginIframeSetsNoopener) {
  for (const char* header :
       {"cross-origin-opener-policy: same-origin",
        "cross-origin-opener-policy: same-origin&cross-origin-embedder-policy: "
        "require-corp"}) {
    GURL starting_page(
        https_server()->GetURL("a.com", std::string("/set-header?") + header));
    GURL url_b(https_server()->GetURL("b.com", "/empty.html"));

    EXPECT_TRUE(NavigateToURL(shell(), starting_page));

    RenderFrameHostImpl* main_rfh = current_frame_host();

    // Create cross origin child frame.
    ASSERT_TRUE(ExecJs(main_rfh, JsReplace(R"(
        let frame = document.createElement('iframe');
        frame.src = $1;
        document.body.appendChild(frame);
    )",
                                           url_b)));
    EXPECT_TRUE(WaitForLoadStop(web_contents()));

    ShellAddedObserver new_shell_observer;
    RenderFrameHostImpl* iframe_rfh =
        main_rfh->child_at(0)->current_frame_host();
    EXPECT_TRUE(ExecJs(iframe_rfh, "window.open('about:blank')"));

    Shell* new_shell = new_shell_observer.GetShell();
    RenderFrameHostImpl* popup_rfh =
        static_cast<WebContentsImpl*>(new_shell->web_contents())
            ->GetFrameTree()
            ->root()
            ->current_frame_host();

    scoped_refptr<SiteInstance> main_rfh_site_instance(
        main_rfh->GetSiteInstance());
    scoped_refptr<SiteInstance> iframe_site_instance(
        iframe_rfh->GetSiteInstance());
    scoped_refptr<SiteInstance> popup_site_instance(
        popup_rfh->GetSiteInstance());

    ASSERT_TRUE(main_rfh_site_instance);
    ASSERT_TRUE(iframe_site_instance);
    ASSERT_TRUE(popup_site_instance);
    EXPECT_FALSE(main_rfh_site_instance->IsRelatedSiteInstance(
        popup_site_instance.get()));
    EXPECT_FALSE(
        iframe_site_instance->IsRelatedSiteInstance(popup_site_instance.get()));

    // Check that `window.opener` is not set.
    EXPECT_EQ(true, EvalJs(new_shell, "window.opener == null;"))
        << "window.opener is set";
  }
}

IN_PROC_BROWSER_TEST_P(CrossOriginOpenerPolicyBrowserTest,
                       NetworkErrorOnSandboxedPopups) {
  GURL starting_page(https_server()->GetURL(
      "a.com", "/cross-origin-opener-policy_sandbox_popup.html"));
  GURL openee_url = https_server()->GetURL(
      "a.com", "/set-header?Cross-Origin-Opener-Policy: same-origin");
  EXPECT_TRUE(NavigateToURL(shell(), starting_page));

  ShellAddedObserver shell_observer;
  RenderFrameHostImpl* iframe_rfh =
      current_frame_host()->child_at(0)->current_frame_host();

  EXPECT_TRUE(ExecJs(iframe_rfh, JsReplace("window.open($1);", openee_url)));

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
  EXPECT_TRUE(NavigateToURLFromRenderer(iframe_ftn, iframe_navigation_url));
  iframe_rfh = iframe_ftn->current_frame_host();

  // We expect the navigation to have used the same SiteInstance that was used
  // in the first place since they are same origin and COOP is ignored.
  EXPECT_EQ(iframe_rfh->GetLastCommittedURL(), iframe_navigation_url);
  EXPECT_EQ(iframe_rfh->GetSiteInstance(), non_coop_iframe_site_instance);

  // The iframe's COOP value is defaulted to unsafe-none since the iframe is
  // cross origin with its top frame.
  EXPECT_EQ(iframe_rfh->cross_origin_opener_policy(), CoopUnsafeNone());
}

IN_PROC_BROWSER_TEST_P(CrossOriginOpenerPolicyBrowserTest,
                       CoopSameOriginIframeInheritance) {
  GURL coop_url(embedded_test_server()->GetURL(
      "/set-header?cross-origin-opener-policy: same-origin"));
  ASSERT_TRUE(NavigateToURL(shell(), coop_url));

  // Create same origin child frame.
  ASSERT_TRUE(ExecJs(current_frame_host(), R"(
    let frame = document.createElement('iframe');
    frame.src = '/empty.html';
    document.body.appendChild(frame);
  )"));
  EXPECT_TRUE(WaitForLoadStop(web_contents()));

  RenderFrameHostImpl* child_rfh =
      current_frame_host()->child_at(0)->current_frame_host();

  // The embedded document has a COOP value that is always inherited from its
  // top level document if they are same-origin. This has no incidence on the
  // embeddee but is inherited by the popup opened hereafter.
  EXPECT_EQ(
      network::mojom::CrossOriginOpenerPolicyValue::kSameOrigin,
      child_rfh->policy_container_host()->cross_origin_opener_policy().value);

  // Create a popup from the iframe.
  ShellAddedObserver shell_observer;
  EXPECT_TRUE(ExecJs(child_rfh, R"(
    w = window.open("about:blank");
  )"));
  WebContentsImpl* popup_webcontents =
      static_cast<WebContentsImpl*>(shell_observer.GetShell()->web_contents());
  RenderFrameHostImpl* popup_rfh = popup_webcontents->GetMainFrame();

  // Verify inheritance from the opener:
  // The second about:blank document of the popup, due to the synchronous
  // re-navigation to about:blank, inherits COOP from its opener.
  // When the opener is same-origin with its top-level document, the top-level
  // document's COOP value (same-origin) is used.
  // In practice policy container handles the inheritance, taking the value
  // from the opener directly, which was properly set when the document was
  // committed.
  EXPECT_EQ(
      network::mojom::CrossOriginOpenerPolicyValue::kSameOrigin,
      popup_rfh->policy_container_host()->cross_origin_opener_policy().value);

  PolicyContainerHost* popup_initial_policy_container =
      popup_rfh->policy_container_host();

  // Navigate the popup from the iframe to about:blank.
  EXPECT_TRUE(ExecJs(child_rfh, R"(
    w.location.href = "about:blank";
  )"));
  EXPECT_TRUE(WaitForLoadStop(popup_webcontents));
  popup_rfh = popup_webcontents->GetMainFrame();

  // Verify the policy container changed, highlighting that the popup has
  // navigated to a different about:blank document.
  EXPECT_NE(popup_initial_policy_container, popup_rfh->policy_container_host());

  // Verify inheritance from the initiator:
  // The navigation to a local scheme inherits COOP from the initiator. When the
  // initiator is same-origin with its top-level document, the top-level
  // document's COOP value (same-origin) is used.
  // In practice policy container handles the inheritance, taking the value
  // from the initiator directly, which was properly set when the document was
  // committed.
  EXPECT_EQ(
      network::mojom::CrossOriginOpenerPolicyValue::kSameOrigin,
      popup_rfh->policy_container_host()->cross_origin_opener_policy().value);
}

IN_PROC_BROWSER_TEST_P(CrossOriginOpenerPolicyBrowserTest,
                       CoopCrossOriginIframeInheritance) {
  GURL coop_url(embedded_test_server()->GetURL(
      "/set-header?cross-origin-opener-policy: same-origin-allow-popups"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/empty.html"));
  ASSERT_TRUE(NavigateToURL(shell(), coop_url));

  // Create child frame.
  ASSERT_TRUE(ExecJs(current_frame_host(), JsReplace(R"(
    let frame = document.createElement('iframe');
    frame.src = $1;
    document.body.appendChild(frame);
  )",
                                                     url_b)));
  EXPECT_TRUE(WaitForLoadStop(web_contents()));

  RenderFrameHostImpl* child_rfh =
      current_frame_host()->child_at(0)->current_frame_host();

  // The embedded document has a COOP value that is always defaulted when it is
  // cross origin with its top level document. This has no incidence on the
  // embeddee but is inherited by the popup opened hereafter.
  EXPECT_EQ(
      network::mojom::CrossOriginOpenerPolicyValue::kUnsafeNone,
      child_rfh->policy_container_host()->cross_origin_opener_policy().value);

  // Create a popup from the iframe.
  ShellAddedObserver shell_observer;
  EXPECT_TRUE(ExecJs(child_rfh, R"(
    w = window.open("about:blank");
  )"));
  WebContentsImpl* popup_webcontents =
      static_cast<WebContentsImpl*>(shell_observer.GetShell()->web_contents());
  RenderFrameHostImpl* popup_rfh = popup_webcontents->GetMainFrame();

  // The second about:blank document of the popup, due to the synchronous
  // re-navigation to about:blank, inherits COOP from its opener.
  // When the opener is cross-origin with its top-level document, the COOP value
  // is defaulted to unsafe-none.
  // In practice policy container handles the inheritance, taking the value
  // from the opener directly, which was properly set when the document was
  // committed.
  EXPECT_EQ(
      network::mojom::CrossOriginOpenerPolicyValue::kUnsafeNone,
      popup_rfh->policy_container_host()->cross_origin_opener_policy().value);

  PolicyContainerHost* popup_initial_policy_container =
      popup_rfh->policy_container_host();

  // Navigate the popup from the iframe.
  EXPECT_TRUE(ExecJs(child_rfh, R"(
    w.location.href = "about:blank";
  )"));
  EXPECT_TRUE(WaitForLoadStop(popup_webcontents));
  popup_rfh = popup_webcontents->GetMainFrame();

  // Verify the policy container changed, highlighting that the popup has
  // navigated to a different about:blank document.
  EXPECT_NE(popup_initial_policy_container, popup_rfh->policy_container_host());

  // Verify inheritance from the initiator:
  // The navigation to a local scheme inherits COOP from the initiator. When the
  // initiator is cross-origin with its top-level document, the COOP value is
  // defaulted to unsafe-none.
  // In practice policy container handles the inheritance, taking the value
  // from the initiator directly, which was properly set when the document was
  // committed.
  EXPECT_EQ(
      network::mojom::CrossOriginOpenerPolicyValue::kUnsafeNone,
      popup_rfh->policy_container_host()->cross_origin_opener_policy().value);
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
  IsolateAllSitesForTesting(base::CommandLine::ForCurrentProcess());
  GURL coop_allow_popups_page(https_server()->GetURL(
      "a.com",
      "/set-header?Cross-Origin-Opener-Policy: same-origin-allow-popups"));
  GURL non_coop_page(https_server()->GetURL("a.com", "/title1.html"));
  GURL cross_origin_non_coop_page(
      https_server()->GetURL("b.com", "/title1.html"));
  // Test a crash before the navigation.
  {
    // Navigate to a COOP page.
    EXPECT_TRUE(NavigateToURL(shell(), coop_allow_popups_page));
    scoped_refptr<SiteInstance> initial_site_instance(
        current_frame_host()->GetSiteInstance());

    // Ensure it has a RenderFrameHostProxy for another cross-site page.
    OpenPopup(current_frame_host(), cross_origin_non_coop_page, "");
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
    EXPECT_TRUE(NavigateToURL(shell(), coop_allow_popups_page));
    scoped_refptr<SiteInstance> initial_site_instance(
        current_frame_host()->GetSiteInstance());

    // Ensure it has a RenderFrameHostProxy for another cross-site page.
    OpenPopup(current_frame_host(), cross_origin_non_coop_page, "");
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
  IsolateAllSitesForTesting(base::CommandLine::ForCurrentProcess());
  GURL coop_allow_popups_page(https_server()->GetURL(
      "a.com",
      "/set-header?Cross-Origin-Opener-Policy: same-origin-allow-popups"));
  GURL cross_origin_non_coop_page(
      https_server()->GetURL("b.com", "/title1.html"));

  // Test a crash before the navigation.
  {
    // Navigate to a COOP page.
    EXPECT_TRUE(NavigateToURL(shell(), coop_allow_popups_page));
    scoped_refptr<SiteInstance> initial_site_instance(
        current_frame_host()->GetSiteInstance());
    EXPECT_EQ(current_frame_host()->cross_origin_opener_policy(),
              CoopSameOriginAllowPopups());

    // Ensure it has a RenderFrameHostProxy for another cross-site page.
    OpenPopup(current_frame_host(), cross_origin_non_coop_page, "");

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
    EXPECT_TRUE(NavigateToURL(shell(), coop_allow_popups_page));
    EXPECT_TRUE(current_frame_host()->GetSiteInstance()->IsRelatedSiteInstance(
        initial_site_instance.get()));
    EXPECT_EQ(current_frame_host()->cross_origin_opener_policy(),
              CoopSameOriginAllowPopups());

    EXPECT_EQ(web_contents()
                  ->GetFrameTree()
                  ->root()
                  ->render_manager()
                  ->GetProxyCount(),
              1u);
  }

  // Test a crash during the navigation.
  {
    // Navigate to a COOP page.
    EXPECT_TRUE(NavigateToURL(shell(), coop_allow_popups_page));
    scoped_refptr<SiteInstance> initial_site_instance(
        current_frame_host()->GetSiteInstance());

    // Ensure it has a RenderFrameHostProxy for another cross-site page.
    OpenPopup(current_frame_host(), cross_origin_non_coop_page, "");
    EXPECT_EQ(web_contents()
                  ->GetFrameTree()
                  ->root()
                  ->render_manager()
                  ->GetProxyCount(),
              1u);

    // Start navigating to a COOP page.
    TestNavigationManager coop_navigation(web_contents(),
                                          coop_allow_popups_page);
    shell()->LoadURL(coop_allow_popups_page);
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
              CoopSameOriginAllowPopups());

    EXPECT_EQ(web_contents()
                  ->GetFrameTree()
                  ->root()
                  ->render_manager()
                  ->GetProxyCount(),
              1u);
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
//
// Variants:
// 1. CrossOriginIsolatedOpeneeCspSandbox
// 2. CrossOriginIsolatedOpeneeOpenerSandbox
IN_PROC_BROWSER_TEST_P(CrossOriginOpenerPolicyBrowserTest,
                       CrossOriginIsolatedWithOpeneeCspSandbox) {
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

  // We have two main documents in different cross-origin isolated process.
  EXPECT_NE(opener_current_main_document->GetLastCommittedOrigin(),
            openee_current_main_document->GetLastCommittedOrigin());
  EXPECT_NE(opener_current_main_document->GetProcess(),
            openee_current_main_document->GetProcess());
  EXPECT_NE(opener_current_main_document->GetSiteInstance(),
            openee_current_main_document->GetSiteInstance());

  EXPECT_TRUE(
      opener_current_main_document->GetSiteInstance()->IsCrossOriginIsolated());
  EXPECT_TRUE(
      openee_current_main_document->GetSiteInstance()->IsCrossOriginIsolated());
}

// Variants:
// 1. CrossOriginIsolatedOpeneeCspSandbox
// 2. CrossOriginIsolatedOpeneeOpenerSandbox
IN_PROC_BROWSER_TEST_P(CrossOriginOpenerPolicyBrowserTest,
                       CrossOriginIsolatedOpeneeOpenerSandbox) {
  // The URL used by both the openee and the opener.
  GURL url = https_server()->GetURL(
      "a.com",
      "/set-header?"
      "Cross-Origin-Opener-Policy: same-origin&"
      "Cross-Origin-Embedder-Policy: require-corp&"
      "Content-Security-Policy: sandbox allow-scripts allow-popups");

  // Load the first window.
  EXPECT_TRUE(NavigateToURL(shell(), url));
  RenderFrameHostImpl* opener_current_main_document = current_frame_host();

  // Load the second window.
  ShellAddedObserver shell_observer;
  EXPECT_TRUE(ExecJs(current_frame_host(), JsReplace("window.open($1)", url)));
  WebContents* popup = shell_observer.GetShell()->web_contents();
  WaitForLoadStop(popup);

  RenderFrameHostImpl* openee_current_main_document =
      static_cast<WebContentsImpl*>(popup)
          ->GetFrameTree()
          ->root()
          ->current_frame_host();

  // Popups with a sandboxing flag, inherited from their opener, are not
  // allowed to navigate to a document with a Cross-Origin-Opener-Policy that
  // is not "unsafe-none". As a result, the navigation in the popup ended up
  // loading an error document.

  EXPECT_EQ(opener_current_main_document->GetLastCommittedURL(), url);
  EXPECT_EQ(openee_current_main_document->GetLastCommittedURL(), url);
  EXPECT_EQ(opener_current_main_document->last_http_status_code(), 200);
  EXPECT_EQ(openee_current_main_document->last_http_status_code(), 0);

  EXPECT_NE(opener_current_main_document->GetLastCommittedOrigin(),
            openee_current_main_document->GetLastCommittedOrigin());
  EXPECT_NE(opener_current_main_document->GetProcess(),
            openee_current_main_document->GetProcess());
  EXPECT_NE(opener_current_main_document->GetSiteInstance(),
            openee_current_main_document->GetSiteInstance());

  EXPECT_TRUE(
      opener_current_main_document->GetSiteInstance()->IsCrossOriginIsolated());
  EXPECT_FALSE(
      openee_current_main_document->GetSiteInstance()->IsCrossOriginIsolated());
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

// This test is flaky on Win: https://crbug.com/1125998.
#if defined(OS_WIN)
#define MAYBE_CrossOriginIsolatedSiteInstance_MainFrame \
  DISABLED_CrossOriginIsolatedSiteInstance_MainFrame
#else
#define MAYBE_CrossOriginIsolatedSiteInstance_MainFrame \
  CrossOriginIsolatedSiteInstance_MainFrame
#endif
IN_PROC_BROWSER_TEST_P(CrossOriginOpenerPolicyBrowserTest,
                       MAYBE_CrossOriginIsolatedSiteInstance_MainFrame) {
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
    EXPECT_FALSE(current_si->IsCrossOriginIsolated());
  }

  // Navigation to a cross-origin isolated page.
  {
    scoped_refptr<SiteInstanceImpl> previous_si =
        current_frame_host()->GetSiteInstance();
    EXPECT_TRUE(NavigateToURL(shell(), isolated_page));
    SiteInstanceImpl* current_si = current_frame_host()->GetSiteInstance();
    EXPECT_TRUE(current_si->IsCrossOriginIsolated());
    EXPECT_FALSE(current_si->IsRelatedSiteInstance(previous_si.get()));
    EXPECT_NE(current_si->GetProcess(), previous_si->GetProcess());
  }

  // Navigation to the same cross-origin isolated page.
  {
    scoped_refptr<SiteInstanceImpl> previous_si =
        current_frame_host()->GetSiteInstance();
    EXPECT_TRUE(NavigateToURL(shell(), isolated_page));
    SiteInstanceImpl* current_si = current_frame_host()->GetSiteInstance();
    EXPECT_TRUE(current_si->IsCrossOriginIsolated());
    EXPECT_EQ(current_si, previous_si);
  }

  // Navigation to a non cross-origin isolated page.
  {
    scoped_refptr<SiteInstanceImpl> previous_si =
        current_frame_host()->GetSiteInstance();
    EXPECT_TRUE(NavigateToURL(shell(), non_isolated_page));
    SiteInstanceImpl* current_si = current_frame_host()->GetSiteInstance();
    EXPECT_FALSE(current_si->IsCrossOriginIsolated());
    EXPECT_FALSE(current_si->IsRelatedSiteInstance(previous_si.get()));
    EXPECT_NE(current_si->GetProcess(), previous_si->GetProcess());
  }

  // Back navigation from a cross-origin isolated page to a non cross-origin
  // isolated page.
  {
    EXPECT_TRUE(NavigateToURL(shell(), isolated_page));
    scoped_refptr<SiteInstanceImpl> cross_origin_isolated_site_instance =
        current_frame_host()->GetSiteInstance();

    EXPECT_TRUE(cross_origin_isolated_site_instance->IsCrossOriginIsolated());
    web_contents()->GetController().GoBack();
    EXPECT_TRUE(WaitForLoadStop(web_contents()));

    scoped_refptr<SiteInstanceImpl> non_cross_origin_isolated_site_instance =
        current_frame_host()->GetSiteInstance();

    EXPECT_FALSE(
        non_cross_origin_isolated_site_instance->IsCrossOriginIsolated());
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
    EXPECT_TRUE(site_instance_1->IsCrossOriginIsolated());
    EXPECT_TRUE(site_instance_2->IsCrossOriginIsolated());
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
    EXPECT_FALSE(current_si->IsCrossOriginIsolated());
  }

  // Navigation to a cross-origin isolated page.
  {
    scoped_refptr<SiteInstanceImpl> previous_si =
        current_frame_host()->GetSiteInstance();
    EXPECT_TRUE(NavigateToURLFromRenderer(shell(), isolated_page));
    SiteInstanceImpl* current_si = current_frame_host()->GetSiteInstance();
    EXPECT_TRUE(current_si->IsCrossOriginIsolated());
    EXPECT_FALSE(current_si->IsRelatedSiteInstance(previous_si.get()));
    EXPECT_NE(current_si->GetProcess(), previous_si->GetProcess());
  }

  // Navigation to the same cross-origin isolated page.
  {
    scoped_refptr<SiteInstanceImpl> previous_si =
        current_frame_host()->GetSiteInstance();
    EXPECT_TRUE(NavigateToURLFromRenderer(shell(), isolated_page));
    SiteInstanceImpl* current_si = current_frame_host()->GetSiteInstance();
    EXPECT_TRUE(current_si->IsCrossOriginIsolated());
    EXPECT_EQ(current_si, previous_si);
  }

  // Navigation to a non cross-origin isolated page.
  {
    scoped_refptr<SiteInstanceImpl> previous_si =
        current_frame_host()->GetSiteInstance();
    EXPECT_TRUE(NavigateToURLFromRenderer(shell(), non_isolated_page));
    SiteInstanceImpl* current_si = current_frame_host()->GetSiteInstance();
    EXPECT_FALSE(current_si->IsCrossOriginIsolated());
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
    EXPECT_TRUE(site_instance_1->IsCrossOriginIsolated());
    EXPECT_TRUE(site_instance_2->IsCrossOriginIsolated());
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
  EXPECT_TRUE(main_si->IsCrossOriginIsolated());

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
    RenderFrameHostImpl* iframe_rfh =
        current_frame_host()->child_at(0)->current_frame_host();
    SiteInstanceImpl* iframe_si = iframe_rfh->GetSiteInstance();
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
    RenderFrameHostImpl* iframe_rfh =
        current_frame_host()->child_at(1)->current_frame_host();
    SiteInstanceImpl* iframe_si = iframe_rfh->GetSiteInstance();
    EXPECT_TRUE(iframe_si->IsCrossOriginIsolated());
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
  EXPECT_TRUE(main_si->IsCrossOriginIsolated());

  // Open a non isolated popup.
  {
    RenderFrameHostImpl* popup_rfh =
        static_cast<WebContentsImpl*>(
            OpenPopup(current_frame_host(), non_isolated_page, "")
                ->web_contents())
            ->GetFrameTree()
            ->root()
            ->current_frame_host();

    EXPECT_FALSE(popup_rfh->GetSiteInstance()->IsCrossOriginIsolated());
    EXPECT_FALSE(popup_rfh->GetSiteInstance()->IsRelatedSiteInstance(
        current_frame_host()->GetSiteInstance()));
    EXPECT_FALSE(popup_rfh->frame_tree_node()->opener());
  }

  // Open an isolated popup.
  {
    RenderFrameHostImpl* popup_rfh =
        static_cast<WebContentsImpl*>(
            OpenPopup(current_frame_host(), isolated_page, "")->web_contents())
            ->GetFrameTree()
            ->root()
            ->current_frame_host();

    EXPECT_TRUE(popup_rfh->GetSiteInstance()->IsCrossOriginIsolated());
    EXPECT_EQ(popup_rfh->GetSiteInstance(),
              current_frame_host()->GetSiteInstance());
  }

  // Open an isolated popup, but cross-origin.
  {
    RenderFrameHostImpl* popup_rfh =
        static_cast<WebContentsImpl*>(
            OpenPopup(current_frame_host(), isolated_page_b, "")
                ->web_contents())
            ->GetFrameTree()
            ->root()
            ->current_frame_host();

    EXPECT_TRUE(popup_rfh->GetSiteInstance()->IsCrossOriginIsolated());
    EXPECT_FALSE(popup_rfh->GetSiteInstance()->IsRelatedSiteInstance(
        current_frame_host()->GetSiteInstance()));
    EXPECT_FALSE(popup_rfh->frame_tree_node()->opener());
    EXPECT_NE(popup_rfh->GetSiteInstance()->GetProcess(),
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
  EXPECT_TRUE(main_si->IsCrossOriginIsolated());

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
    RenderFrameHostImpl* iframe_rfh =
        current_frame_host()->child_at(0)->current_frame_host();
    SiteInstanceImpl* iframe_si = iframe_rfh->GetSiteInstance();
    // The load of the document with 404 status code is blocked by COEP.
    // An error page is expected in lieu of that document.
    EXPECT_EQ(GURL(kUnreachableWebDataURL),
              EvalJs(iframe_rfh, "document.location.href;"));
    EXPECT_TRUE(IsExpectedSubframeErrorTransition(main_si, iframe_si));
    EXPECT_TRUE(iframe_si->IsCrossOriginIsolated());
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
    RenderFrameHostImpl* iframe_rfh =
        current_frame_host()->child_at(0)->current_frame_host();
    SiteInstanceImpl* iframe_si = iframe_rfh->GetSiteInstance();
    EXPECT_TRUE(IsExpectedSubframeErrorTransition(main_si, iframe_si));

    // The load of the document with 404 status code and custom body is blocked
    // by COEP. An error page is expected in lieu of that document.
    EXPECT_EQ(GURL(kUnreachableWebDataURL),
              EvalJs(iframe_rfh, "document.location.href;"));
    EXPECT_TRUE(iframe_si->IsCrossOriginIsolated());
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
    RenderFrameHostImpl* iframe_rfh =
        current_frame_host()->child_at(0)->current_frame_host();
    SiteInstanceImpl* iframe_si = iframe_rfh->GetSiteInstance();
    EXPECT_TRUE(IsExpectedSubframeErrorTransition(main_si, iframe_si));
    EXPECT_TRUE(iframe_si->IsCrossOriginIsolated());
  }

  // Top frame.
  {
    scoped_refptr<SiteInstanceImpl> previous_si =
        current_frame_host()->GetSiteInstance();
    EXPECT_FALSE(NavigateToURL(shell(), invalid_url));
    SiteInstanceImpl* current_si = current_frame_host()->GetSiteInstance();
    EXPECT_FALSE(current_si->IsRelatedSiteInstance(previous_si.get()));
    EXPECT_NE(current_si->GetProcess(), previous_si->GetProcess());
    EXPECT_FALSE(current_si->IsCrossOriginIsolated());
  }
}

// Regression test for https://crbug.com/1226909.
IN_PROC_BROWSER_TEST_P(CrossOriginOpenerPolicyBrowserTest,
                       NavigatePopupToErrorAndCrash) {
  GURL isolated_page(
      https_server()->GetURL("a.com",
                             "/set-header?"
                             "Cross-Origin-Opener-Policy: same-origin&"
                             "Cross-Origin-Embedder-Policy: require-corp"));

  // Initial cross-origin isolated page.
  EXPECT_TRUE(NavigateToURL(shell(), isolated_page));
  SiteInstanceImpl* main_si = current_frame_host()->GetSiteInstance();
  EXPECT_TRUE(main_si->IsCrossOriginIsolated());

  ShellAddedObserver shell_observer;
  GURL error_url(embedded_test_server()->GetURL("/close-socket"));
  EXPECT_TRUE(ExecJs(current_frame_host(),
                     JsReplace("window.w = open($1);", error_url)));
  WebContentsImpl* popup_web_contents =
      static_cast<WebContentsImpl*>(shell_observer.GetShell()->web_contents());
  WaitForLoadStop(popup_web_contents);

  // The popup should commit an error page with default COOP.
  EXPECT_EQ(PAGE_TYPE_ERROR, popup_web_contents->GetController()
                                 .GetLastCommittedEntry()
                                 ->GetPageType());
  EXPECT_FALSE(popup_web_contents->GetMainFrame()
                   ->GetSiteInstance()
                   ->IsCrossOriginIsolated());
  EXPECT_EQ(CoopUnsafeNone(),
            popup_web_contents->GetMainFrame()->cross_origin_opener_policy());

  url::Origin error_origin =
      popup_web_contents->GetMainFrame()->GetLastCommittedOrigin();

  // Simulate the popup renderer process crashing.
  RenderProcessHost* popup_process =
      popup_web_contents->GetMainFrame()->GetProcess();
  EXPECT_NE(popup_process, current_frame_host()->GetProcess());

  ASSERT_TRUE(popup_process);
  {
    RenderProcessHostWatcher crash_observer(
        popup_process, RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
    popup_process->Shutdown(0);
    crash_observer.Wait();
  }

  // Try to navigate the popup. This should not be possible, since the opener
  // relationship should be closed.
  EXPECT_TRUE(
      ExecJs(current_frame_host(), "window.w.location = 'about:blank';"));
  WaitForLoadStop(popup_web_contents);

  // The popup should not have navigated.
  EXPECT_EQ(error_origin,
            popup_web_contents->GetMainFrame()->GetLastCommittedOrigin());
  EXPECT_FALSE(popup_web_contents->GetMainFrame()
                   ->GetSiteInstance()
                   ->IsCrossOriginIsolated());
  EXPECT_EQ(CoopUnsafeNone(),
            popup_web_contents->GetMainFrame()->cross_origin_opener_policy());
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
  EXPECT_FALSE(current_si->IsCrossOriginIsolated());

  EXPECT_TRUE(NavigateToURL(shell(), redirect_isolated_page, isolated_page));
  current_si = current_frame_host()->GetSiteInstance();
  EXPECT_TRUE(current_si->IsCrossOriginIsolated());
  EXPECT_TRUE(
      current_si->GetWebExposedIsolationInfo().origin().IsSameOriginWith(
          url::Origin::Create(isolated_page)));
}

// Reproducer test for https://crbug.com/1150938.
IN_PROC_BROWSER_TEST_P(CrossOriginOpenerPolicyBrowserTest,
                       MainFrameA_IframeB_Opens_WindowA) {
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
  EXPECT_TRUE(main_si->IsCrossOriginIsolated());

  TestNavigationManager cross_origin_iframe_navigation(web_contents(),
                                                       isolated_page_b);

  EXPECT_TRUE(ExecJs(web_contents(),
                     JsReplace("var iframe = document.createElement('iframe'); "
                               "iframe.src = $1; "
                               "document.body.appendChild(iframe);",
                               isolated_page_b)));

  cross_origin_iframe_navigation.WaitForNavigationFinished();
  EXPECT_TRUE(cross_origin_iframe_navigation.was_successful());
  RenderFrameHostImpl* iframe_rfh =
      current_frame_host()->child_at(0)->current_frame_host();
  SiteInstanceImpl* iframe_si = iframe_rfh->GetSiteInstance();
  EXPECT_TRUE(iframe_si->IsCrossOriginIsolated());
  EXPECT_TRUE(iframe_si->IsRelatedSiteInstance(main_si));
  EXPECT_EQ(iframe_si->GetProcess(), main_si->GetProcess());

  // Open an isolated popup, but cross-origin.
  {
    RenderFrameHostImpl* popup_rfh =
        static_cast<WebContentsImpl*>(
            OpenPopup(iframe_rfh, isolated_page, "", "", false)->web_contents())
            ->GetFrameTree()
            ->root()
            ->current_frame_host();

    EXPECT_TRUE(popup_rfh->GetSiteInstance()->IsCrossOriginIsolated());
    EXPECT_FALSE(popup_rfh->GetSiteInstance()->IsRelatedSiteInstance(
        current_frame_host()->GetSiteInstance()));
    EXPECT_FALSE(popup_rfh->frame_tree_node()->opener());
    EXPECT_NE(popup_rfh->GetSiteInstance()->GetProcess(),
              current_frame_host()->GetSiteInstance()->GetProcess());
  }
}

// Regression test for https://crbug.com/1183571. This used to crash.
// A grand child, same-origin with its parent, but cross-origin with the main
// document is accessing a popup.
//
// TODO(arthursonzogni): Add a similar WPT test.
IN_PROC_BROWSER_TEST_P(CrossOriginOpenerPolicyBrowserTest,
                       GrandChildAccessCrash1183571) {
  GURL a_url_coop(https_server()->GetURL(
      "a.com",
      "/set-header?Cross-Origin-Opener-Policy-Report-Only: same-origin"));
  GURL b_url(https_server()->GetURL("b.com", "/empty.html"));
  GURL c_url(https_server()->GetURL("c.com", "/empty.html"));

  // 1. Start from COOP-Report-Only:same-origin. (a.com COOP-RO)
  EXPECT_TRUE(NavigateToURL(shell(), a_url_coop));
  RenderFrameHostImpl* opener_rfh = current_frame_host();

  // 2. Add a window in a different (virtual) browsing context group.
  //
  // The new popup won't be used, but it is created to avoid the
  // DOMWindow::ReportCoopAccess() fast early return. The original bug won't
  // reproduce without this.
  {
    ShellAddedObserver shell_observer;
    EXPECT_TRUE(ExecJs(opener_rfh, JsReplace(R"(
      window.open($1);
    )",
                                             b_url)));
    WaitForLoadStop(shell_observer.GetShell()->web_contents());
  }

  // 3. Insert a cross-origin iframe. (b.com)
  EXPECT_TRUE(ExecJs(opener_rfh, JsReplace(R"(
    const iframe = document.createElement("iframe");
    iframe.src = $1;
    document.body.appendChild(iframe);
  )",
                                           b_url)));
  WaitForLoadStop(web_contents());
  RenderFrameHostImpl* opener_child_rfh =
      opener_rfh->child_at(0)->current_frame_host();

  // 4. Insert a grand-child iframe (b.com).
  EXPECT_TRUE(ExecJs(opener_child_rfh, JsReplace(R"(
    const iframe = document.createElement("iframe");
    iframe.src = $1;
    document.body.appendChild(iframe);
  )",
                                                 b_url)));
  WaitForLoadStop(web_contents());
  RenderFrameHostImpl* opener_grand_child_rfh =
      opener_child_rfh->child_at(0)->current_frame_host();

  // 5. The grand child creates a new cross-origin popup...
  ShellAddedObserver shell_observer;
  EXPECT_TRUE(ExecJs(opener_grand_child_rfh, JsReplace(R"(
    window.openee = window.open($1);
  )",
                                                       c_url)));
  WaitForLoadStop(shell_observer.GetShell()->web_contents());

  // 6. ... and tries to access it.
  EXPECT_EQ("I didn't crash", EvalJs(opener_grand_child_rfh, R"(
    window.openee.closed;
    "I didn't crash";
  )"));
}

// TODO(https://crbug.com/1101339). Test inheritance of the virtual browsing
// context group when using window.open from an iframe, same-origin and
// cross-origin.

static auto kTestParams =
    testing::Combine(testing::ValuesIn(RenderDocumentFeatureLevelValues()),
                     testing::Bool());
INSTANTIATE_TEST_SUITE_P(All, CrossOriginOpenerPolicyBrowserTest, kTestParams);
INSTANTIATE_TEST_SUITE_P(All, VirtualBrowsingContextGroupTest, kTestParams);
INSTANTIATE_TEST_SUITE_P(All, NoSharedArrayBufferByDefault, kTestParams);

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
  EXPECT_EQ(coop.reporting_endpoint, absl::nullopt);
  EXPECT_EQ(coop.report_only_reporting_endpoint, absl::nullopt);
  EXPECT_EQ(coop.value,
            network::mojom::CrossOriginOpenerPolicyValue::kSameOriginPlusCoep);
  EXPECT_EQ(coop.report_only_value,
            network::mojom::CrossOriginOpenerPolicyValue::kUnsafeNone);
}

IN_PROC_BROWSER_TEST_F(CoopReportingOriginTrialBrowserTest,
                       CoopStateWithToken) {
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

IN_PROC_BROWSER_TEST_P(NoSharedArrayBufferByDefault, BaseCase) {
  GURL url = https_server()->GetURL("a.com", "/empty.html");
  EXPECT_TRUE(NavigateToURL(shell(), url));
  EXPECT_EQ(false, EvalJs(current_frame_host(), "self.crossOriginIsolated"));
  EXPECT_EQ(false,
            EvalJs(current_frame_host(), "'SharedArrayBuffer' in globalThis"));
}

IN_PROC_BROWSER_TEST_P(NoSharedArrayBufferByDefault, CoopCoepIsolated) {
  GURL url =
      https_server()->GetURL("a.com",
                             "/set-header?"
                             "Cross-Origin-Opener-Policy: same-origin&"
                             "Cross-Origin-Embedder-Policy: require-corp");
  EXPECT_TRUE(NavigateToURL(shell(), url));
  EXPECT_EQ(true, EvalJs(current_frame_host(), "self.crossOriginIsolated"));
  EXPECT_EQ(true,
            EvalJs(current_frame_host(), "'SharedArrayBuffer' in globalThis"));
}

IN_PROC_BROWSER_TEST_P(NoSharedArrayBufferByDefault,
                       CoopCoepTransferSharedArrayBufferToIframe) {
  CHECK(!base::FeatureList::IsEnabled(features::kSharedArrayBuffer));
  GURL url =
      https_server()->GetURL("a.com",
                             "/set-header?"
                             "Cross-Origin-Opener-Policy: same-origin&"
                             "Cross-Origin-Embedder-Policy: require-corp");
  EXPECT_TRUE(NavigateToURL(shell(), url));
  EXPECT_TRUE(ExecJs(current_frame_host(),
                     "g_iframe = document.createElement('iframe');"
                     "g_iframe.src = location.href;"
                     "document.body.appendChild(g_iframe);"));
  WaitForLoadStop(web_contents());

  RenderFrameHostImpl* main_document = current_frame_host();
  RenderFrameHostImpl* sub_document =
      current_frame_host()->child_at(0)->current_frame_host();

  EXPECT_EQ(true, EvalJs(main_document, "self.crossOriginIsolated"));
  EXPECT_EQ(true, EvalJs(sub_document, "self.crossOriginIsolated"));

  EXPECT_TRUE(ExecJs(sub_document, R"(
    g_sab_size = new Promise(resolve => {
      addEventListener("message", event => resolve(event.data.byteLength));
    });
  )",
                     EXECUTE_SCRIPT_NO_RESOLVE_PROMISES));

  EXPECT_TRUE(ExecJs(main_document, R"(
    let sab = new SharedArrayBuffer(1234);
    g_iframe.contentWindow.postMessage(sab, "*");
  )"));

  EXPECT_EQ(1234, EvalJs(sub_document, "g_sab_size"));
}

// Transfer a SharedArrayBuffer in between two COOP+COEP document with a
// parent/child relationship. The child has set Permissions-Policy:
// cross-origin-isolated=(). As a result, it can't receive the object.
IN_PROC_BROWSER_TEST_P(
    NoSharedArrayBufferByDefault,
    CoopCoepTransferSharedArrayBufferToNoCrossOriginIsolatedIframe) {
  CHECK(!base::FeatureList::IsEnabled(features::kSharedArrayBuffer));
  GURL main_url =
      https_server()->GetURL("a.com",
                             "/set-header?"
                             "Cross-Origin-Opener-Policy: same-origin&"
                             "Cross-Origin-Embedder-Policy: require-corp");
  GURL iframe_url =
      https_server()->GetURL("a.com",
                             "/set-header?"
                             "Cross-Origin-Embedder-Policy: require-corp&"
                             "Cross-Origin-Resource-Policy: cross-origin&"
                             "Permissions-Policy: cross-origin-isolated%3D()");
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  EXPECT_TRUE(ExecJs(current_frame_host(),
                     JsReplace("g_iframe = document.createElement('iframe');"
                               "g_iframe.src = $1;"
                               "document.body.appendChild(g_iframe);",
                               iframe_url)));
  WaitForLoadStop(web_contents());

  RenderFrameHostImpl* main_document = current_frame_host();
  RenderFrameHostImpl* sub_document =
      current_frame_host()->child_at(0)->current_frame_host();

  EXPECT_EQ(true, EvalJs(main_document, "self.crossOriginIsolated"));
  EXPECT_EQ(false, EvalJs(sub_document, "self.crossOriginIsolated"));

  auto postSharedArrayBuffer = EvalJs(main_document, R"(
    let sab = new SharedArrayBuffer(1234);
    g_iframe.contentWindow.postMessage(sab,"*");
  )");

  EXPECT_THAT(postSharedArrayBuffer.error,
              HasSubstr("Failed to execute 'postMessage' on 'Window':"));
}

// Transfer a SharedArrayBuffer in between two COOP+COEP document with a
// parent/child relationship. The child has set Permissions-Policy:
// cross-origin-isolated=(). This non-cross-origin-isolated document can
// transfer a SharedArrayBuffer toward the cross-origin-isolated one.
// See https://crbug.com/1144838 for discussions about this behavior.
IN_PROC_BROWSER_TEST_P(
    NoSharedArrayBufferByDefault,
    CoopCoepTransferSharedArrayBufferFromNoCrossOriginIsolatedIframe) {
  CHECK(!base::FeatureList::IsEnabled(features::kSharedArrayBuffer));
  GURL main_url =
      https_server()->GetURL("a.com",
                             "/set-header?"
                             "Cross-Origin-Opener-Policy: same-origin&"
                             "Cross-Origin-Embedder-Policy: require-corp");
  GURL iframe_url =
      https_server()->GetURL("a.com",
                             "/set-header?"
                             "Cross-Origin-Embedder-Policy: require-corp&"
                             "Cross-Origin-Resource-Policy: cross-origin&"
                             "Permissions-Policy: cross-origin-isolated%3D()");
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  EXPECT_TRUE(ExecJs(current_frame_host(),
                     JsReplace("g_iframe = document.createElement('iframe');"
                               "g_iframe.src = $1;"
                               "document.body.appendChild(g_iframe);",
                               iframe_url)));
  WaitForLoadStop(web_contents());

  RenderFrameHostImpl* main_document = current_frame_host();
  RenderFrameHostImpl* sub_document =
      current_frame_host()->child_at(0)->current_frame_host();

  EXPECT_EQ(true, EvalJs(main_document, "self.crossOriginIsolated"));
  EXPECT_EQ(false, EvalJs(sub_document, "self.crossOriginIsolated"));

  EXPECT_TRUE(ExecJs(main_document, R"(
    g_sab_size = new Promise(resolve => {
      addEventListener("message", event => resolve(event.data.byteLength));
    });
  )",
                     EXECUTE_SCRIPT_NO_RESOLVE_PROMISES));

  EXPECT_EQ(false, EvalJs(sub_document, "'SharedArrayBuffer' in globalThis"));

  // TODO(https://crbug.com/1144838): Being able to share SharedArrayBuffer from
  // a document with self.crossOriginIsolated == false sounds wrong.
  EXPECT_TRUE(ExecJs(sub_document, R"(
    // Create a WebAssembly Memory to bypass the SAB constructor restriction.
    let sab = new (new WebAssembly.Memory(
        { shared:true, initial:1, maximum:1 }).buffer.constructor)(1234);
    parent.postMessage(sab, "*");
  )"));

  EXPECT_EQ(1234, EvalJs(main_document, "g_sab_size"));
}

// Ensure the UnrestrictedSharedArrayBuffer reverse origin trial is correctly
// implemented.
class UnrestrictedSharedArrayBufferOriginTrialBrowserTest
    : public ContentBrowserTest {
 public:
  UnrestrictedSharedArrayBufferOriginTrialBrowserTest() {
    feature_list_.InitWithFeatures(
        {
            // Enabled
        },
        {
            // Disabled
            features::kSharedArrayBuffer,
        });
  }

  // Origin Trials key generated with:
  //
  // tools/origin_trials/generate_token.py --expire-days 5000 --version 3
  // https://coop.security:9999 UnrestrictedSharedArrayBuffer
  static std::string OriginTrialToken() {
    return "A8TH8Ylk6lUuL84RdQ2+FTyupad3leg5sMk+MYEoVlwkURyBtVq1IFncJAc2k"
           "Knhh5w3SvIR4XuEtyMzeI2u4wAAAABqeyJvcmlnaW4iOiAiaHR0cHM6Ly9jb2"
           "9wLnNlY3VyaXR5Ojk5OTkiLCAiZmVhdHVyZSI6ICJVbnJlc3RyaWN0ZWRTaGF"
           "yZWRBcnJheUJ1ZmZlciIsICJleHBpcnkiOiAyMDQ1Njk0NDMyfQ==";
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

  void SetUpCommandLine(base::CommandLine* command_line) final {
    ContentBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kIgnoreCertificateErrors);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  net::EmbeddedTestServer https_server_;
};

IN_PROC_BROWSER_TEST_F(UnrestrictedSharedArrayBufferOriginTrialBrowserTest,
                       HasSharedArrayBuffer) {
  URLLoaderInterceptor interceptor(base::BindLambdaForTesting(
      [&](URLLoaderInterceptor::RequestParams* params) {
        DCHECK_EQ(params->url_request.url, OriginTrialURL());
        URLLoaderInterceptor::WriteResponse(
            "HTTP/1.1 200 OK\n"
            "Content-type: text/html\n"
            "Origin-Trial: " +
                OriginTrialToken() + "\n\n",
            "", params->client.get());
        return true;
      }));
  EXPECT_TRUE(NavigateToURL(shell(), OriginTrialURL()));

  EXPECT_EQ(false, EvalJs(current_frame_host(), "self.crossOriginIsolated"));
#if !defined(OS_ANDROID)
  EXPECT_EQ(true,
            EvalJs(current_frame_host(), "'SharedArrayBuffer' in globalThis"));
#else   // defined(OS_ANDROID)
  EXPECT_EQ(false,
            EvalJs(current_frame_host(), "'SharedArrayBuffer' in globalThis"));
#endif  // defined(OS_ANDROID)
}

// Check setting the OriginTrial works, even in popups where the javascript
// context of the initial empty document is reused.
IN_PROC_BROWSER_TEST_F(UnrestrictedSharedArrayBufferOriginTrialBrowserTest,
                       HasSharedArrayBufferReuseContext) {
  // Create a document without the origin trial in a renderer process.
  {
    URLLoaderInterceptor interceptor(base::BindLambdaForTesting(
        [&](URLLoaderInterceptor::RequestParams* params) {
          DCHECK_EQ(params->url_request.url, OriginTrialURL());
          URLLoaderInterceptor::WriteResponse(
              "HTTP/1.1 200 OK\n"
              "Content-type: text/html\n\n",
              "", params->client.get());
          return true;
        }));
    EXPECT_TRUE(NavigateToURL(shell(), OriginTrialURL()));
    EXPECT_EQ(false, EvalJs(current_frame_host(),
                            "'SharedArrayBuffer' in globalThis"));
  }

  // In the same process, open a popup. The document loaded defines an
  // OriginTrial. It will reuse the javascript context created for the initial
  // empty document.
  {
    URLLoaderInterceptor interceptor(base::BindLambdaForTesting(
        [&](URLLoaderInterceptor::RequestParams* params) {
          DCHECK_EQ(params->url_request.url, OriginTrialURL());
          URLLoaderInterceptor::WriteResponse(
              "HTTP/1.1 200 OK\n"
              "Content-type: text/html\n"
              "Origin-Trial: " +
                  OriginTrialToken() + "\n\n",
              "", params->client.get());
          return true;
        }));
    ShellAddedObserver shell_observer;
    EXPECT_TRUE(ExecJs(current_frame_host(), "window.open(location.href)"));

    auto* popup = static_cast<WebContentsImpl*>(
        shell_observer.GetShell()->web_contents());
    WaitForLoadStop(popup);

#if defined(OS_ANDROID)
    EXPECT_EQ(false, EvalJs(popup, "'SharedArrayBuffer' in globalThis"));
#else
    EXPECT_EQ(true, EvalJs(popup, "'SharedArrayBuffer' in globalThis"));
#endif
  }
}

IN_PROC_BROWSER_TEST_F(UnrestrictedSharedArrayBufferOriginTrialBrowserTest,
                       SupportForMeta) {
  URLLoaderInterceptor interceptor(base::BindLambdaForTesting(
      [&](URLLoaderInterceptor::RequestParams* params) {
        DCHECK_EQ(params->url_request.url, OriginTrialURL());
        URLLoaderInterceptor::WriteResponse(
            "HTTP/1.1 200 OK\n"
            "Content-type: text/html\n",
            "<meta http-equiv=\"origin-trial\" content=\"" +
                OriginTrialToken() + "\">",
            params->client.get());
        return true;
      }));
  EXPECT_TRUE(NavigateToURL(shell(), OriginTrialURL()));

  EXPECT_EQ(false, EvalJs(current_frame_host(), "self.crossOriginIsolated"));

#if defined(OS_ANDROID)
  EXPECT_EQ(false,
            EvalJs(current_frame_host(), "'SharedArrayBuffer' in globalThis"));
#else
  EXPECT_EQ(true,
            EvalJs(current_frame_host(), "'SharedArrayBuffer' in globalThis"));
#endif
}

IN_PROC_BROWSER_TEST_F(UnrestrictedSharedArrayBufferOriginTrialBrowserTest,
                       TransferSharedArrayBuffer) {
  URLLoaderInterceptor interceptor(base::BindLambdaForTesting(
      [&](URLLoaderInterceptor::RequestParams* params) {
        DCHECK_EQ(params->url_request.url, OriginTrialURL());
        URLLoaderInterceptor::WriteResponse(
            "HTTP/1.1 200 OK\n"
            "Content-type: text/html\n"
            "Origin-Trial: " +
                OriginTrialToken() + "\n\n",
            "", params->client.get());
        return true;
      }));
  EXPECT_TRUE(NavigateToURL(shell(), OriginTrialURL()));

  EXPECT_TRUE(ExecJs(current_frame_host(),
                     "g_iframe = document.createElement('iframe');"
                     "g_iframe.src = location.href;"
                     "document.body.appendChild(g_iframe);"));
  WaitForLoadStop(web_contents());

  RenderFrameHostImpl* main_document = current_frame_host();
  RenderFrameHostImpl* sub_document =
      current_frame_host()->child_at(0)->current_frame_host();

  EXPECT_EQ(false, EvalJs(main_document, "self.crossOriginIsolated"));
  EXPECT_EQ(false, EvalJs(sub_document, "self.crossOriginIsolated"));

#if !defined(OS_ANDROID)
  EXPECT_TRUE(ExecJs(sub_document, R"(
    g_sab_size = new Promise(resolve => {
      addEventListener("message", event => resolve(event.data.byteLength));
    });
  )",
                     EXECUTE_SCRIPT_NO_RESOLVE_PROMISES));

  EXPECT_TRUE(ExecJs(main_document, R"(
    let sab = new SharedArrayBuffer(1234);
    g_iframe.contentWindow.postMessage(sab, "*");
  )"));

  EXPECT_EQ(1234, EvalJs(sub_document, "g_sab_size"));
#else   // defined(OS_ANDROID)
  auto postSharedArrayBuffer = EvalJs(main_document, R"(
    // Create a WebAssembly Memory to bypass the SAB constructor restriction.
    const sab =
        new WebAssembly.Memory({ shared:true, initial:1, maximum:1 }).buffer;
    g_iframe.contentWindow.postMessage(sab,"*");
  )");

  EXPECT_THAT(postSharedArrayBuffer.error,
              HasSubstr("Failed to execute 'postMessage' on 'Window'"));
#endif  // defined(OS_ANDROID)
}

// Enable the reverse OriginTrial via a <meta> tag. Then send a Webassembly's
// SharedArrayBuffer toward the iframe.
// Regression test for https://crbug.com/1201589).
#if !defined(OS_ANDROID) // The SAB reverse origin trial only work on Desktop.
IN_PROC_BROWSER_TEST_F(UnrestrictedSharedArrayBufferOriginTrialBrowserTest,
                       CrashForBug1201589) {
  URLLoaderInterceptor interceptor(base::BindLambdaForTesting(
      [&](URLLoaderInterceptor::RequestParams* params) {
        DCHECK_EQ(params->url_request.url, OriginTrialURL());
        URLLoaderInterceptor::WriteResponse(
            "HTTP/1.1 200 OK\n"
            "Content-type: text/html\n",
            "<meta http-equiv=\"origin-trial\" content=\"" +
                OriginTrialToken() + "\">",
            params->client.get());
        return true;
      }));
  EXPECT_TRUE(NavigateToURL(shell(), OriginTrialURL()));

  EXPECT_TRUE(ExecJs(current_frame_host(),
                     "g_iframe = document.createElement('iframe');"
                     "g_iframe.src = location.href;"
                     "document.body.appendChild(g_iframe);"));
  WaitForLoadStop(web_contents());

  RenderFrameHostImpl* main_document = current_frame_host();
  RenderFrameHostImpl* sub_document =
      current_frame_host()->child_at(0)->current_frame_host();

  EXPECT_EQ(false, EvalJs(main_document, "self.crossOriginIsolated"));
  EXPECT_EQ(false, EvalJs(sub_document, "self.crossOriginIsolated"));

  EXPECT_EQ(true, EvalJs(main_document, "'SharedArrayBuffer' in globalThis"));
  EXPECT_EQ(true, EvalJs(sub_document, "'SharedArrayBuffer' in globalThis"));

  EXPECT_TRUE(ExecJs(sub_document, R"(
    g_sab_size = new Promise(resolve => {
      addEventListener("message", event => resolve(event.data.byteLength));
    });
  )",
                     EXECUTE_SCRIPT_NO_RESOLVE_PROMISES));

  EXPECT_TRUE(ExecJs(main_document, R"(
    let wasm_shared_memory = new WebAssembly.Memory({
      shared:true, initial:0, maximum:0 });
    g_iframe.contentWindow.postMessage(wasm_shared_memory.buffer, "*");
  )"));
  EXPECT_EQ(0, EvalJs(sub_document, "g_sab_size"));
}
#endif

// Ensure the SharedArrayBufferOnDesktop kill switch is correctly implemented.
class SharedArrayBufferOnDesktopBrowserTest
    : public CrossOriginOpenerPolicyBrowserTest {
 public:
  SharedArrayBufferOnDesktopBrowserTest() {
    feature_list_.InitWithFeatures(
        {
            // Enabled
            features::kSharedArrayBufferOnDesktop,
        },
        {
            // Disabled
            features::kSharedArrayBuffer,
        });
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         SharedArrayBufferOnDesktopBrowserTest,
                         kTestParams);

IN_PROC_BROWSER_TEST_P(SharedArrayBufferOnDesktopBrowserTest,
                       DesktopHasSharedArrayBuffer) {
  CHECK(!base::FeatureList::IsEnabled(features::kSharedArrayBuffer));
  GURL url = https_server()->GetURL("a.com", "/empty.html");
  EXPECT_TRUE(NavigateToURL(shell(), url));
  EXPECT_EQ(false, EvalJs(current_frame_host(), "self.crossOriginIsolated"));
#if !defined(OS_ANDROID)
  EXPECT_EQ(true,
            EvalJs(current_frame_host(), "'SharedArrayBuffer' in globalThis"));
#else   // defined(OS_ANDROID)
  EXPECT_EQ(false,
            EvalJs(current_frame_host(), "'SharedArrayBuffer' in globalThis"));
#endif  // defined(OS_ANDROID)
}

IN_PROC_BROWSER_TEST_P(SharedArrayBufferOnDesktopBrowserTest,
                       DesktopTransferSharedArrayBuffer) {
  CHECK(!base::FeatureList::IsEnabled(features::kSharedArrayBuffer));
  GURL main_url = https_server()->GetURL("a.com", "/empty.html");
  GURL iframe_url = https_server()->GetURL("a.com", "/empty.html");
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  EXPECT_TRUE(ExecJs(current_frame_host(),
                     JsReplace("g_iframe = document.createElement('iframe');"
                               "g_iframe.src = $1;"
                               "document.body.appendChild(g_iframe);",
                               iframe_url)));
  WaitForLoadStop(web_contents());

  RenderFrameHostImpl* main_document = current_frame_host();
  RenderFrameHostImpl* sub_document =
      current_frame_host()->child_at(0)->current_frame_host();

  EXPECT_EQ(false, EvalJs(main_document, "self.crossOriginIsolated"));
  EXPECT_EQ(false, EvalJs(sub_document, "self.crossOriginIsolated"));

  EXPECT_TRUE(ExecJs(main_document, R"(
    g_sab_size = new Promise(resolve => {
      addEventListener("message", event => resolve(event.data.byteLength));
    });
  )",
                     EXECUTE_SCRIPT_NO_RESOLVE_PROMISES));

#if !defined(OS_ANDROID)
  EXPECT_TRUE(ExecJs(sub_document, R"(
    let sab = new SharedArrayBuffer(1234);
    parent.postMessage(sab, "*");
  )"));

  EXPECT_EQ(1234, EvalJs(main_document, "g_sab_size"));
#else   // defined(OS_ANDROID)
  EXPECT_FALSE(ExecJs(sub_document, R"(
    let sab = new SharedArrayBuffer(1234);
    parent.postMessage(sab, "*");
  )"));
#endif  // defined(OS_ANDROID)
}
}  // namespace content
