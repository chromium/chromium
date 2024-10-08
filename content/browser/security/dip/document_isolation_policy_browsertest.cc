// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/document_isolation_policy.h"

#include "base/command_line.h"
#include "base/strings/escape.h"
#include "base/test/bind.h"
#include "base/test/gtest_util.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "content/browser/process_lock.h"
#include "content/browser/renderer_host/navigation_request.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/content_navigation_policy.h"
#include "content/public/browser/site_isolation_policy.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/back_forward_cache_util.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_content_browser_client.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/content_mock_cert_verifier.h"
#include "content/public/test/prerender_test_util.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/url_loader_interceptor.h"
#include "content/shell/browser/shell.h"
#include "content/test/content_browser_test_utils_internal.h"
#include "content/test/render_document_feature.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/default_handlers.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/test/embedded_test_server/request_handler_util.h"
#include "services/network/public/cpp/cross_origin_embedder_policy.h"
#include "services/network/public/cpp/features.h"
#include "testing/gmock/include/gmock/gmock.h"

using ::testing::HasSubstr;

namespace content {

namespace {

network::DocumentIsolationPolicy DipIsolateAndRequireCorp() {
  network::DocumentIsolationPolicy dip;
  dip.value =
      network::mojom::DocumentIsolationPolicyValue::kIsolateAndRequireCorp;
  return dip;
}

network::DocumentIsolationPolicy DipIsolateAndCredentialless() {
  network::DocumentIsolationPolicy dip;
  dip.value =
      network::mojom::DocumentIsolationPolicyValue::kIsolateAndCredentialless;
  return dip;
}

network::DocumentIsolationPolicy DipNone() {
  return network::DocumentIsolationPolicy();
}

std::unique_ptr<net::test_server::HttpResponse> ServeDipOnSecondNavigation(
    unsigned int& navigation_counter,
    const net::test_server::HttpRequest& request) {
  ++navigation_counter;
  auto http_response = std::make_unique<net::test_server::BasicHttpResponse>();
  http_response->set_code(net::HttpStatusCode::HTTP_OK);
  http_response->AddCustomHeader("Cache-Control", "no-store, must-revalidate");
  if (navigation_counter > 1) {
    http_response->AddCustomHeader("Document-Isolation-Policy",
                                   "isolate-and-require-corp");
  }
  return http_response;
}

class DocumentIsolationPolicyBrowserTest
    : public ContentBrowserTest,
      public ::testing::WithParamInterface<
          std::tuple<std::string, bool, bool>> {
 public:
  DocumentIsolationPolicyBrowserTest()
      : prerender_helper_(base::BindRepeating(
            &DocumentIsolationPolicyBrowserTest::prerender_web_contents,
            base::Unretained(this))),
        https_server_(net::EmbeddedTestServer::TYPE_HTTPS) {
    // Enable DIP and disable speculative RFH creation deferral. Currently,
    // speculative RFH creation deferral makes it impossible to check that a
    // speculative RFH is not created at the start of the navigation. This is
    // needed to check that we do not create an extra process when navigating
    // between two same-origin documents with DIP. Once RenderDocument ships, a
    // speculative RFH will always be created, we'll then check that it is in
    // the same SiteInstance as the current one. Then, we'll be able to
    // re-enable the deferred speculative RFH creation, and just wait for the
    // deferred creation of the speculative RenderDocument.
    feature_list_.InitWithFeatures(
        {network::features::kDocumentIsolationPolicy},
        {features::kDeferSpeculativeRFHCreation, features::kSharedArrayBuffer});

    // Enable RenderDocument:
    InitAndEnableRenderDocumentFeature(&feature_list_for_render_document_,
                                       std::get<0>(GetParam()));
    // Enable BackForwardCache:
    if (IsBackForwardCacheEnabled()) {
      feature_list_for_back_forward_cache_.InitWithFeaturesAndParameters(
          GetDefaultEnabledBackForwardCacheFeaturesForTesting(
              /*ignore_outstanding_network_request=*/false),
          GetDefaultDisabledBackForwardCacheFeaturesForTesting());
    } else {
      feature_list_for_back_forward_cache_.InitWithFeatures(
          {}, {features::kBackForwardCache});
    }
  }

  // Provides meaningful param names instead of /0, /1, ...
  static std::string DescribeParams(
      const testing::TestParamInfo<ParamType>& info) {
    auto [render_document_level, enable_back_forward_cache, require_corp] =
        info.param;
    return base::StringPrintf(
        "%s_%s_%s",
        GetRenderDocumentLevelNameForTestParams(render_document_level).c_str(),
        enable_back_forward_cache ? "BFCacheEnabled" : "BFCacheDisabled",
        require_corp ? "RequireCorp" : "Credentialless");
  }

  bool IsBackForwardCacheEnabled() { return std::get<1>(GetParam()); }

  net::EmbeddedTestServer* https_server() { return &https_server_; }

  test::PrerenderTestHelper& prerender_helper() { return prerender_helper_; }

 protected:
  WebContentsImpl* web_contents() const {
    return static_cast<WebContentsImpl*>(shell()->web_contents());
  }

  RenderFrameHostImpl* current_frame_host() {
    return web_contents()->GetPrimaryMainFrame();
  }

  void SetUpOnMainThread() override {
    ContentBrowserTest::SetUpOnMainThread();
    mock_cert_verifier_.mock_cert_verifier()->set_default_result(net::OK);

    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());

    https_server()->ServeFilesFromSourceDirectory(GetTestDataFilePath());
    SetupCrossSiteRedirector(https_server());
    net::test_server::RegisterDefaultHandlers(&https_server_);
    AddRedirectOnSecondNavigationHandler(&https_server_);
    unsigned int navigation_counter = 0;
    https_server_.RegisterDefaultHandler(base::BindRepeating(
        &net::test_server::HandlePrefixedRequest,
        "/serve-dip-on-second-navigation",
        base::BindRepeating(&ServeDipOnSecondNavigation,
                            base::OwnedRef(navigation_counter))));

    prerender_helper().RegisterServerRequestMonitor(&https_server_);

    ASSERT_TRUE(https_server()->Start());
  }

  GURL GetDocumentIsolationPolicyURL(
      const std::string& host,
      const std::optional<std::string>& additional_header = std::nullopt) {
    std::string headers = "/set-header?";

    if (std::get<2>(GetParam())) {
      // Isolate-and-require-corp version of the test.
      headers += "document-isolation-policy: isolate-and-require-corp";
    } else {
      // Isolate-and-credentialless version of the test.
      headers += "document-isolation-policy: isolate-and-credentialless";
    }

    if (additional_header.has_value()) {
      headers += "&" + additional_header.value();
    }
    return https_server()->GetURL(host, headers);
  }

  network::DocumentIsolationPolicy GetDocumentIsolationPolicy() {
    // Isolate-and-require-corp version of the test.
    if (std::get<2>(GetParam())) {
      return DipIsolateAndRequireCorp();
    }

    // Isolate-and-credentialless version of the test.
    return DipIsolateAndCredentialless();
  }

 private:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    mock_cert_verifier_.SetUpCommandLine(command_line);

    // Enable strict SiteIsolation. Currently DIP only supports strict
    // SiteIsolation so force it in tests.
    IsolateAllSitesForTesting(command_line);
  }

  void SetUpInProcessBrowserTestFixture() override {
    ContentBrowserTest::SetUpInProcessBrowserTestFixture();
    mock_cert_verifier_.SetUpInProcessBrowserTestFixture();
  }

  void TearDownInProcessBrowserTestFixture() override {
    ContentBrowserTest::TearDownInProcessBrowserTestFixture();
    mock_cert_verifier_.TearDownInProcessBrowserTestFixture();
  }

  // Variation of web_contents(), that returns a WebContents* instead of a
  // WebContentsImpl*, required to bind the prerender_helper_ in the
  // constructor.
  WebContents* prerender_web_contents() { return shell()->web_contents(); }

  content::ContentMockCertVerifier mock_cert_verifier_;

  // This needs to be before ScopedFeatureLists, because it contains one
  // internally and the destruction order matters.
  test::PrerenderTestHelper prerender_helper_;

  base::test::ScopedFeatureList feature_list_;
  base::test::ScopedFeatureList feature_list_for_render_document_;
  base::test::ScopedFeatureList feature_list_for_back_forward_cache_;
  net::EmbeddedTestServer https_server_;
};

class DocumentIsolationPolicyWithoutFeatureBrowserTest
    : public ContentBrowserTest,
      public ::testing::WithParamInterface<
          std::tuple<std::string, bool, bool>> {
 public:
  DocumentIsolationPolicyWithoutFeatureBrowserTest()
      : https_server_(net::EmbeddedTestServer::TYPE_HTTPS) {
    // Disable the DocumentIsolationPolicy feature.
    feature_list_.InitWithFeatures(
        {}, {network::features::kDocumentIsolationPolicy});

    // Enable RenderDocument:
    InitAndEnableRenderDocumentFeature(&feature_list_for_render_document_,
                                       std::get<0>(GetParam()));
    // Enable BackForwardCache:
    if (IsBackForwardCacheEnabled()) {
      feature_list_for_back_forward_cache_.InitWithFeaturesAndParameters(
          GetDefaultEnabledBackForwardCacheFeaturesForTesting(
              /*ignore_outstanding_network_request=*/false),
          GetDefaultDisabledBackForwardCacheFeaturesForTesting());
    } else {
      feature_list_for_back_forward_cache_.InitWithFeatures(
          {}, {features::kBackForwardCache});
    }
  }

  // Provides meaningful param names instead of /0, /1, ...
  static std::string DescribeParams(
      const testing::TestParamInfo<ParamType>& info) {
    auto [render_document_level, enable_back_forward_cache, require_corp] =
        info.param;
    return base::StringPrintf(
        "%s_%s_%s",
        GetRenderDocumentLevelNameForTestParams(render_document_level).c_str(),
        enable_back_forward_cache ? "BFCacheEnabled" : "BFCacheDisabled",
        require_corp ? "RequireCorp" : "Credentialless");
  }

  net::EmbeddedTestServer* https_server() { return &https_server_; }

 protected:
  RenderFrameHostImpl* current_frame_host() {
    return static_cast<WebContentsImpl*>(shell()->web_contents())
        ->GetPrimaryMainFrame();
  }

  GURL GetDocumentIsolationPolicyURL(const std::string& host) {
    // Isolate-and-require-corp version of the test.
    if (std::get<2>(GetParam())) {
      return https_server()->GetURL(
          host,
          "/set-header?document-isolation-policy: isolate-and-require-corp");
    }

    // Isolate-and-credentialless version of the test.
    return https_server()->GetURL(
        host,
        "/set-header?document-isolation-policy: isolate-and-credentialless");
  }

  void SetUpOnMainThread() override {
    ContentBrowserTest::SetUpOnMainThread();
    mock_cert_verifier_.mock_cert_verifier()->set_default_result(net::OK);

    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());

    https_server()->ServeFilesFromSourceDirectory(GetTestDataFilePath());
    SetupCrossSiteRedirector(https_server());
    net::test_server::RegisterDefaultHandlers(&https_server_);

    ASSERT_TRUE(https_server()->Start());
  }

 private:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    ContentBrowserTest::SetUpCommandLine(command_line);
    mock_cert_verifier_.SetUpCommandLine(command_line);
  }

  void SetUpInProcessBrowserTestFixture() override {
    ContentBrowserTest::SetUpInProcessBrowserTestFixture();
    mock_cert_verifier_.SetUpInProcessBrowserTestFixture();
  }

  void TearDownInProcessBrowserTestFixture() override {
    ContentBrowserTest::TearDownInProcessBrowserTestFixture();
    mock_cert_verifier_.TearDownInProcessBrowserTestFixture();
  }

  content::ContentMockCertVerifier mock_cert_verifier_;

  base::test::ScopedFeatureList feature_list_;
  base::test::ScopedFeatureList feature_list_for_render_document_;
  base::test::ScopedFeatureList feature_list_for_back_forward_cache_;
  net::EmbeddedTestServer https_server_;
};

class DocumentIsolationPolicyWithoutSiteIsolationBrowserTest
    : public DocumentIsolationPolicyBrowserTest {
 public:
  DocumentIsolationPolicyWithoutSiteIsolationBrowserTest() = default;

 private:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    ContentBrowserTest::SetUpCommandLine(command_line);
    mock_cert_verifier_.SetUpCommandLine(command_line);

    // Disable SiteIsolation.
    command_line->AppendSwitch(switches::kDisableSiteIsolation);
  }

  content::ContentMockCertVerifier mock_cert_verifier_;
};

}  // namespace

// Checks that a Document-Isolation-Policy header is ignored if the
// DocumentIsolationPolicy feature flag is not enabled.
IN_PROC_BROWSER_TEST_P(DocumentIsolationPolicyWithoutFeatureBrowserTest,
                       DIP_Disabled) {
  GURL starting_page = GetDocumentIsolationPolicyURL("a.test");
  EXPECT_TRUE(NavigateToURL(shell(), starting_page));
  EXPECT_EQ(current_frame_host()
                ->policy_container_host()
                ->policies()
                .document_isolation_policy,
            DipNone());
}

// Checks that a Document-Isolation-Policy header is ignored if SiteIsolation is
// not enabled.
IN_PROC_BROWSER_TEST_P(DocumentIsolationPolicyWithoutSiteIsolationBrowserTest,
                       DIP_Disabled) {
  GURL starting_page = GetDocumentIsolationPolicyURL("a.test");
  EXPECT_TRUE(NavigateToURL(shell(), starting_page));
  EXPECT_EQ(current_frame_host()
                ->policy_container_host()
                ->policies()
                .document_isolation_policy,
            DipNone());
}

// Checks that DocumentIsolationPolicy is properly inherited from its creator by
// the about:blank document in a new popup.
IN_PROC_BROWSER_TEST_P(DocumentIsolationPolicyBrowserTest,
                       NewPopup_InheritsDIP) {
  GURL starting_page = GetDocumentIsolationPolicyURL("a.test");
  GURL no_dip(https_server()->GetURL("a.test", "/empty.html"));
  EXPECT_TRUE(NavigateToURL(shell(), starting_page));

  RenderFrameHostImpl* main_rfh = current_frame_host();

  // Open a popup.
  ShellAddedObserver shell_observer;
  EXPECT_TRUE(ExecJs(main_rfh, "window.open('about:blank')"));

  auto* popup_webcontents =
      static_cast<WebContentsImpl*>(shell_observer.GetShell()->web_contents());
  RenderFrameHostImpl* popup_rfh = popup_webcontents->GetPrimaryMainFrame();

  EXPECT_EQ(
      main_rfh->policy_container_host()->policies().document_isolation_policy,
      GetDocumentIsolationPolicy());
  EXPECT_EQ(
      popup_rfh->policy_container_host()->policies().document_isolation_policy,
      GetDocumentIsolationPolicy());

  // Navigate the popup to a page without DIP. It should not longer have a
  // DocumentIsolationPolicy.
  ASSERT_TRUE(NavigateToURL(popup_webcontents, no_dip));
  popup_rfh = popup_webcontents->GetPrimaryMainFrame();
  EXPECT_EQ(
      popup_rfh->policy_container_host()->policies().document_isolation_policy,
      DipNone());

  // Now add an iframe without DIP which will open a popup.
  ASSERT_TRUE(ExecJs(main_rfh, R"(
    const frame = document.createElement('iframe');
    frame.src = '/empty.html';
    document.body.appendChild(frame);
  )"));
  EXPECT_TRUE(WaitForLoadStop(web_contents()));

  ShellAddedObserver shell_observer_2;
  RenderFrameHostImpl* iframe_rfh = main_rfh->child_at(0)->current_frame_host();
  EXPECT_TRUE(ExecJs(iframe_rfh, "window.open('about:blank')"));

  RenderFrameHostImpl* popup_rfh_2 =
      static_cast<WebContentsImpl*>(shell_observer_2.GetShell()->web_contents())
          ->GetPrimaryMainFrame();

  EXPECT_EQ(
      iframe_rfh->policy_container_host()->policies().document_isolation_policy,
      DipNone());
  EXPECT_EQ(popup_rfh_2->policy_container_host()
                ->policies()
                .document_isolation_policy,
            DipNone());
}

// Checks that a navigation to a Blob URL inherits the DocumentIsolationPolicy
// of its creator.
IN_PROC_BROWSER_TEST_P(DocumentIsolationPolicyBrowserTest, BlobInheritsDIP) {
  GURL starting_page = GetDocumentIsolationPolicyURL("a.test");
  EXPECT_TRUE(NavigateToURL(shell(), starting_page));

  // Create and open blob.
  ShellAddedObserver shell_observer;
  ASSERT_TRUE(ExecJs(current_frame_host(), R"(
    const blob = new Blob(['foo'], {type : 'text/html'});
    const url = URL.createObjectURL(blob);
    window.open(url);
  )"));
  EXPECT_TRUE(WaitForLoadStop(shell_observer.GetShell()->web_contents()));
  RenderFrameHostImpl* popup_rfh =
      static_cast<WebContentsImpl*>(shell_observer.GetShell()->web_contents())
          ->GetPrimaryMainFrame();

  // DIP inherited from Blob creator
  EXPECT_EQ(
      popup_rfh->policy_container_host()->policies().document_isolation_policy,
      GetDocumentIsolationPolicy());
}

// Checks that an about:blank iframe inherits its DocumentIsolationPolicy from
// its creator.
IN_PROC_BROWSER_TEST_P(DocumentIsolationPolicyBrowserTest,
                       AboutBlankInheritsDip) {
  GURL starting_page = GetDocumentIsolationPolicyURL("a.test");
  EXPECT_TRUE(NavigateToURL(shell(), starting_page));

  EXPECT_EQ(current_frame_host()
                ->policy_container_host()
                ->policies()
                .document_isolation_policy,
            GetDocumentIsolationPolicy());

  // Add an about:blank iframe.
  EXPECT_TRUE(ExecJs(current_frame_host(),
                     "g_iframe = document.createElement('iframe');"
                     "g_iframe.src = 'about:blank';"
                     "document.body.appendChild(g_iframe);"));
  WaitForLoadStop(web_contents());

  RenderFrameHostImpl* iframe_rfh =
      current_frame_host()->child_at(0)->current_frame_host();

  // Document-Isolation-Policy should have been inherited.
  EXPECT_EQ(
      iframe_rfh->policy_container_host()->policies().document_isolation_policy,
      GetDocumentIsolationPolicy());
}

// Checks that an iframe can enable DocumentIsolationPolicy even if its parent
// does not, and that it will be placed in an appropriate SiteInstance.
IN_PROC_BROWSER_TEST_P(DocumentIsolationPolicyBrowserTest, IframeCanSetDip) {
  GURL starting_page(
      https_server()->GetURL("a.com", "/cross_site_iframe_factory.html?a(b)"));
  GURL iframe_navigation_url = GetDocumentIsolationPolicyURL("b.com");
  EXPECT_TRUE(NavigateToURL(shell(), starting_page));

  RenderFrameHostImpl* main_rfh = current_frame_host();
  FrameTreeNode* iframe_ftn = main_rfh->child_at(0);
  RenderFrameHostImpl* iframe_rfh = iframe_ftn->current_frame_host();
  scoped_refptr<SiteInstanceImpl> non_dip_iframe_site_instance =
      iframe_rfh->GetSiteInstance();

  // The iframe should not have a DocumentIsolationPolicy.
  EXPECT_EQ(
      iframe_rfh->policy_container_host()->policies().document_isolation_policy,
      DipNone());

  // Navigate the iframe same-origin to a document with DIP header. The
  // header should be taken into account.
  EXPECT_TRUE(NavigateToURLFromRenderer(iframe_ftn, iframe_navigation_url));
  iframe_rfh = iframe_ftn->current_frame_host();

  // The navigation should have used a different SiteInstance from the one
  // previously used as the DocumentIsolationPolicy do not match, even if the
  // navigation is same-origin.
  EXPECT_EQ(iframe_rfh->GetLastCommittedURL(), iframe_navigation_url);
  EXPECT_NE(iframe_rfh->GetSiteInstance(), non_dip_iframe_site_instance);

  EXPECT_EQ(
      iframe_rfh->policy_container_host()->policies().document_isolation_policy,
      GetDocumentIsolationPolicy());
}

// Checks that navigations are placed in the appropriate renderer process
// depending on their DocumentIsolationPolicy, even if the current renderer
// process is crashed or crashes during the navigation. In particular, this
// tests the navigation from a page without DIP to a page with DIP.
IN_PROC_BROWSER_TEST_P(DocumentIsolationPolicyBrowserTest,
                       NonDipPageCrashIntoDip) {
  GURL non_dip_page(https_server()->GetURL("a.test", "/title1.html"));
  GURL dip_page = GetDocumentIsolationPolicyURL("a.test");

  // Test a crash before the navigation.
  {
    // Navigate to a non dip page.
    EXPECT_TRUE(NavigateToURL(shell(), non_dip_page));
    scoped_refptr<SiteInstance> initial_site_instance(
        current_frame_host()->GetSiteInstance());

    // Simulate the renderer process crashing.
    RenderProcessHost* process = initial_site_instance->GetProcess();
    ASSERT_TRUE(process);
    std::unique_ptr<RenderProcessHostWatcher> crash_observer(
        new RenderProcessHostWatcher(
            process, RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT));
    process->Shutdown(0);
    crash_observer->Wait();
    crash_observer.reset();

    // Navigate to a DIP page.
    EXPECT_TRUE(NavigateToURL(shell(), dip_page));
    EXPECT_EQ(current_frame_host()
                  ->policy_container_host()
                  ->policies()
                  .document_isolation_policy,
              GetDocumentIsolationPolicy());
  }

  // Test a crash during the navigation.
  {
    // Navigate to a non dip page.
    EXPECT_TRUE(NavigateToURL(shell(), non_dip_page));
    scoped_refptr<SiteInstance> initial_site_instance(
        current_frame_host()->GetSiteInstance());

    // Start navigating to a DIP page.
    TestNavigationManager dip_navigation(web_contents(), dip_page);
    shell()->LoadURL(dip_page);
    EXPECT_TRUE(dip_navigation.WaitForRequestStart());

    // Simulate the renderer process crashing.
    RenderProcessHost* process = initial_site_instance->GetProcess();
    ASSERT_TRUE(process);
    std::unique_ptr<RenderProcessHostWatcher> crash_observer(
        new RenderProcessHostWatcher(
            process, RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT));
    process->Shutdown(0);
    crash_observer->Wait();
    crash_observer.reset();

    // Finish the navigation to the DIP page.
    ASSERT_TRUE(dip_navigation.WaitForNavigationFinished());

    // The navigation will fail if we create speculative RFH when the navigation
    // started (instead of only when the response started), because the renderer
    // process will crash and trigger deletion of the speculative RFH and the
    // navigation using that speculative RFH. BFCache forces a BrowsingInstance
    // swap (even in this same-site case), hence it also necessitates a
    // speculative RFH.
    // TODO(crbug.com/40261276): If the final RenderFrameHost picked for
    // the navigation doesn't use the same process as the crashed process, we
    // can crash the process after the final RenderFrameHost has been picked
    // instead, and the navigation will commit normally.
    if (ShouldCreateNewHostForAllFrames() || IsBackForwardCacheEnabled()) {
      EXPECT_FALSE(dip_navigation.was_committed());
      return;
    }

    EXPECT_TRUE(dip_navigation.was_successful());
    EXPECT_EQ(current_frame_host()
                  ->policy_container_host()
                  ->policies()
                  .document_isolation_policy,
              GetDocumentIsolationPolicy());
  }
}

// Checks that navigations are placed in the appropriate renderer process
// depending on their DocumentIsolationPolicy, even if the current renderer
// process is crashed or crashes during the navigation. In particular, this
// tests the navigation from a page with DIP to a page without DIP.
IN_PROC_BROWSER_TEST_P(DocumentIsolationPolicyBrowserTest,
                       DipPageCrashIntoNonDip) {
  GURL dip_page = GetDocumentIsolationPolicyURL("a.test");
  GURL non_dip_page(https_server()->GetURL("a.test", "/empty.html"));
  // Test a crash before the navigation.
  {
    // Navigate to a DIP page.
    EXPECT_TRUE(NavigateToURL(shell(), dip_page));
    scoped_refptr<SiteInstance> initial_site_instance(
        current_frame_host()->GetSiteInstance());

    // Simulate the renderer process crashing.
    RenderProcessHost* process = initial_site_instance->GetProcess();
    ASSERT_TRUE(process);
    std::unique_ptr<RenderProcessHostWatcher> crash_observer(
        new RenderProcessHostWatcher(
            process, RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT));
    process->Shutdown(0);
    crash_observer->Wait();
    crash_observer.reset();

    // Navigate to a non DIP page.
    EXPECT_TRUE(NavigateToURL(shell(), non_dip_page));
    EXPECT_EQ(current_frame_host()
                  ->policy_container_host()
                  ->policies()
                  .document_isolation_policy,
              DipNone());
  }

  // Test a crash during the navigation.
  {
    // Navigate to a DIP page.
    EXPECT_TRUE(NavigateToURL(shell(), dip_page));
    scoped_refptr<SiteInstance> initial_site_instance(
        current_frame_host()->GetSiteInstance());

    // Start navigating to a non DIP page.
    TestNavigationManager non_dip_navigation(web_contents(), non_dip_page);
    shell()->LoadURL(non_dip_page);
    EXPECT_TRUE(non_dip_navigation.WaitForRequestStart());

    // Simulate the renderer process crashing.
    RenderProcessHost* process = initial_site_instance->GetProcess();
    ASSERT_TRUE(process);
    std::unique_ptr<RenderProcessHostWatcher> crash_observer(
        new RenderProcessHostWatcher(
            process, RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT));
    process->Shutdown(0);
    crash_observer->Wait();
    crash_observer.reset();

    // Finish the navigation to the non DIP page.
    // TODO(crbug.com/343914483): This might need to change and match the test
    // above if we implement an optimization to assume DIP value hasn't changed
    // until response time.
    ASSERT_TRUE(non_dip_navigation.WaitForNavigationFinished());

    EXPECT_TRUE(non_dip_navigation.was_successful());
    EXPECT_EQ(current_frame_host()
                  ->policy_container_host()
                  ->policies()
                  .document_isolation_policy,
              DipNone());
  }
}

// Checks that navigations are placed in the appropriate renderer process
// depending on their DocumentIsolationPolicy, even if the current renderer
// process is crashed or crashes during the navigation. In particular, this
// tests the navigation between two pages with DIP, including a reload of a
// crashed page.
IN_PROC_BROWSER_TEST_P(DocumentIsolationPolicyBrowserTest,
                       DipPageCrashIntoDip) {
  GURL dip_page = GetDocumentIsolationPolicyURL("a.test");

  // Test a crash before the navigation.
  {
    // Navigate to a DIP page.
    EXPECT_TRUE(NavigateToURL(shell(), dip_page));
    scoped_refptr<SiteInstance> initial_site_instance(
        current_frame_host()->GetSiteInstance());
    EXPECT_EQ(current_frame_host()
                  ->policy_container_host()
                  ->policies()
                  .document_isolation_policy,
              GetDocumentIsolationPolicy());

    // Simulate the renderer process crashing.
    RenderProcessHost* process = initial_site_instance->GetProcess();
    ASSERT_TRUE(process);
    std::unique_ptr<RenderProcessHostWatcher> crash_observer(
        new RenderProcessHostWatcher(
            process, RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT));
    process->Shutdown(0);
    crash_observer->Wait();
    crash_observer.reset();

    // Reload the DIP page.
    ReloadBlockUntilNavigationsComplete(shell(), 1);
    EXPECT_TRUE(current_frame_host()->GetSiteInstance()->IsRelatedSiteInstance(
        initial_site_instance.get()));
    EXPECT_EQ(current_frame_host()
                  ->policy_container_host()
                  ->policies()
                  .document_isolation_policy,
              GetDocumentIsolationPolicy());
  }

  // Test a crash during the navigation.
  {
    // Navigate to a DIP page.
    EXPECT_TRUE(NavigateToURL(shell(), dip_page));
    scoped_refptr<SiteInstance> initial_site_instance(
        current_frame_host()->GetSiteInstance());

    // Start navigating to a DIP page.
    TestNavigationManager dip_navigation(web_contents(), dip_page);
    shell()->LoadURL(dip_page);
    EXPECT_TRUE(dip_navigation.WaitForRequestStart());

    // Simulate the renderer process crashing.
    RenderProcessHost* process = initial_site_instance->GetProcess();
    ASSERT_TRUE(process);
    std::unique_ptr<RenderProcessHostWatcher> crash_observer(
        new RenderProcessHostWatcher(
            process, RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT));
    process->Shutdown(0);
    crash_observer->Wait();
    crash_observer.reset();

    // Finish the navigation to the DIP page.
    ASSERT_TRUE(dip_navigation.WaitForNavigationFinished());

    EXPECT_EQ(current_frame_host()
                  ->policy_container_host()
                  ->policies()
                  .document_isolation_policy,
              GetDocumentIsolationPolicy());
  }
}

// Checks that a navigation to a document with DocumentIsolationPolicy will be
// placed in a separate process even if the process limit has been reached.
IN_PROC_BROWSER_TEST_P(DocumentIsolationPolicyBrowserTest,
                       IsolateInNewProcessDespiteLimitReached) {
  // Set a process limit of 1 for testing.
  RenderProcessHostImpl::SetMaxRendererProcessCount(1);

  // Navigate to a starting page.
  GURL starting_page(https_server()->GetURL("a.test", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), starting_page));
  scoped_refptr<SiteInstance> initial_site_instance(
      current_frame_host()->GetSiteInstance());

  // Open a popup with DocumentIsolationPolicy set.
  GURL url_openee = GetDocumentIsolationPolicyURL("a.test");
  auto* popup_webcontents =
      OpenPopup(current_frame_host(), url_openee, "popup")->web_contents();
  EXPECT_TRUE(WaitForLoadStop(popup_webcontents));

  // The page and its popup should be in different processes even though the
  // process limit was reached.
  EXPECT_NE(initial_site_instance,
            popup_webcontents->GetPrimaryMainFrame()->GetSiteInstance());
  EXPECT_NE(current_frame_host()->GetProcess(),
            popup_webcontents->GetPrimaryMainFrame()->GetProcess());
}

// Checks that a process hosting a document with DocumentIsolationPolicy is not
// reused for documents without DocumentIsolationPolicy, even if the process
// limit has been reached.
IN_PROC_BROWSER_TEST_P(DocumentIsolationPolicyBrowserTest,
                       NoProcessReuseForDIPProcesses) {
  // Set a process limit of 1 for testing.
  RenderProcessHostImpl::SetMaxRendererProcessCount(1);

  // Navigate to a starting page with DocumentIsolationPolicy set.
  GURL starting_page = GetDocumentIsolationPolicyURL("a.test");
  EXPECT_TRUE(NavigateToURL(shell(), starting_page));
  scoped_refptr<SiteInstance> initial_site_instance(
      current_frame_host()->GetSiteInstance());

  // Create a new shell.
  Shell* new_shell = CreateBrowser();

  // Navigate it to a same-origin page without DIP.
  GURL non_dip_url = https_server()->GetURL("a.test", "/title1.html");
  EXPECT_TRUE(NavigateToURL(new_shell, non_dip_url));

  // The original page and the page in the new shell should be in different
  // processes even though the process limit was reached.
  EXPECT_NE(
      initial_site_instance,
      new_shell->web_contents()->GetPrimaryMainFrame()->GetSiteInstance());
  EXPECT_NE(current_frame_host()->GetProcess(),
            new_shell->web_contents()->GetPrimaryMainFrame()->GetProcess());
}

IN_PROC_BROWSER_TEST_P(DocumentIsolationPolicyBrowserTest,
                       SpeculativeRfhsAndDip) {
  GURL non_dip_page(https_server()->GetURL("a.test", "/title1.html"));
  GURL dip_page = GetDocumentIsolationPolicyURL("a.test");

  // Non-DIP into DIP.
  {
    SCOPED_TRACE("Non-DIP to DIP");

    // Start on a non DIP page.
    EXPECT_TRUE(NavigateToURL(shell(), non_dip_page));
    scoped_refptr<SiteInstance> initial_site_instance(
        current_frame_host()->GetSiteInstance());

    // Navigate to a DIP page.
    TestNavigationManager dip_navigation(web_contents(), dip_page);
    shell()->LoadURL(dip_page);
    EXPECT_TRUE(dip_navigation.WaitForRequestStart());

    auto* speculative_rfh = web_contents()
                                ->GetPrimaryFrameTree()
                                .root()
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

    ASSERT_TRUE(dip_navigation.WaitForNavigationFinished());

    // Even if the origin of the documents is the same, because their
    // DocumentIsolationPolicies do not match, the navigation is classified as a
    // cross-site browser-initiated request and the browser triggers a
    // speculative BrowsingInstance swap.
    EXPECT_FALSE(current_frame_host()->GetSiteInstance()->IsRelatedSiteInstance(
        initial_site_instance.get()));
    EXPECT_EQ(current_frame_host()
                  ->policy_container_host()
                  ->policies()
                  .document_isolation_policy,
              GetDocumentIsolationPolicy());
  }

  // DIP into non-DIP.
  {
    SCOPED_TRACE("DIP to non-DIP");

    // Start on a DIP page.
    EXPECT_TRUE(NavigateToURL(shell(), dip_page));
    scoped_refptr<SiteInstance> initial_site_instance(
        current_frame_host()->GetSiteInstance());

    // Navigate to a non DIP page.
    TestNavigationManager non_dip_navigation(web_contents(), non_dip_page);
    shell()->LoadURL(non_dip_page);
    EXPECT_TRUE(non_dip_navigation.WaitForRequestStart());

    auto* speculative_rfh = web_contents()
                                ->GetPrimaryFrameTree()
                                .root()
                                ->render_manager()
                                ->speculative_frame_host();
    // The navigation is considered cross-site, because the AgentClusterKey of
    // the current page has an IsolationKey, and the request does not have one.
    // TODO(https://issues.chromium.org/343914483): Avoid creating a speculative
    // RFH in this case.
    EXPECT_TRUE(speculative_rfh);

    ASSERT_TRUE(non_dip_navigation.WaitForNavigationFinished());

    // Even if the origin of the documents is the same, because their
    // DocumentIsolationPolicies do not match, the navigation is classified as a
    // cross-site browser-initiated request and the browser triggers a
    // speculative BrowsingInstance swap.
    EXPECT_FALSE(current_frame_host()->GetSiteInstance()->IsRelatedSiteInstance(
        initial_site_instance.get()));
    EXPECT_EQ(current_frame_host()
                  ->policy_container_host()
                  ->policies()
                  .document_isolation_policy,
              DipNone());
  }

  // DIP into DIP.
  {
    SCOPED_TRACE("DIP to DIP");

    // Start on a DIP page.
    EXPECT_TRUE(NavigateToURL(shell(), dip_page));
    scoped_refptr<SiteInstance> initial_site_instance(
        current_frame_host()->GetSiteInstance());

    // Navigate to a DIP page.
    TestNavigationManager dip_navigation(web_contents(), dip_page);
    shell()->LoadURL(dip_page);
    EXPECT_TRUE(dip_navigation.WaitForRequestStart());

    auto* speculative_rfh = web_contents()
                                ->GetPrimaryFrameTree()
                                .root()
                                ->render_manager()
                                ->speculative_frame_host();
    // The navigation is considered cross-site, because the AgentClusterKey of
    // the current page has an IsolationKey, and the request does not have one.
    // TODO(https://issues.chromium.org/343914483): Avoid creating a speculative
    // RFH in this case.
    EXPECT_TRUE(speculative_rfh);

    ASSERT_TRUE(dip_navigation.WaitForNavigationFinished());

    EXPECT_TRUE(current_frame_host()->GetSiteInstance()->IsRelatedSiteInstance(
        initial_site_instance.get()));
    EXPECT_EQ(current_frame_host()
                  ->policy_container_host()
                  ->policies()
                  .document_isolation_policy,
              GetDocumentIsolationPolicy());
  }
}

// A test to make sure that loading a page with DIP sets
// requires_origin_keyed_process() on the SiteInstance's SiteInfo.
IN_PROC_BROWSER_TEST_P(DocumentIsolationPolicyBrowserTest, DipOriginKeyed) {
  GURL isolated_page = GetDocumentIsolationPolicyURL("a.test");

  EXPECT_TRUE(NavigateToURL(shell(), isolated_page));
  SiteInstanceImpl* current_si = current_frame_host()->GetSiteInstance();
  EXPECT_TRUE(current_si->IsCrossOriginIsolated());
  // Currently, use of the DIP header does not cause
  // SiteInfo::requires_origin_keyed_process() to return true. In practice, the
  // process will be origin-keyed because the AgentClusterKey is. Once we
  // refactor Origin-Agent-Cluster to use the AgentClusterKey, using DIP should
  // also cause SiteInfo::requires_origin_keyed_process() to return true.
  // Note: if kOriginKeyedProcessesByDefault is enabled, then
  // requires_origin_keyed_process() will return true.
  EXPECT_EQ(SiteIsolationPolicy::AreOriginKeyedProcessesEnabledByDefault(),
            current_si->GetSiteInfo().requires_origin_keyed_process());
  EXPECT_TRUE(current_si->GetSiteInfo().agent_cluster_key()->IsOriginKeyed());
}

// Tests that main frame navigations are correctly assigned cross-origin
// isolated SiteInstances based on their DocumentIsolationPolicy.
IN_PROC_BROWSER_TEST_P(DocumentIsolationPolicyBrowserTest,
                       CrossOriginIsolatedSiteInstance_MainFrame) {
  GURL isolated_page = GetDocumentIsolationPolicyURL("a.test");
  GURL isolated_page_b = GetDocumentIsolationPolicyURL("cdn.a.test");
  GURL non_isolated_page(https_server()->GetURL("a.test", "/title1.html"));

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
    EXPECT_NE(current_si->GetProcess(), previous_si->GetProcess());

    // The navigation triggers a speculative BrowsingInstance swap because it is
    // browser-initiated and end up being cross-site due to the DIP mismatch.
    EXPECT_FALSE(current_si->IsRelatedSiteInstance(previous_si.get()));
  }

  // Navigation to a non cross-origin isolated page.
  {
    scoped_refptr<SiteInstanceImpl> previous_si =
        current_frame_host()->GetSiteInstance();
    EXPECT_TRUE(NavigateToURL(shell(), non_isolated_page));
    SiteInstanceImpl* current_si = current_frame_host()->GetSiteInstance();
    EXPECT_FALSE(current_si->IsCrossOriginIsolated());
    EXPECT_NE(current_si->GetProcess(), previous_si->GetProcess());

    // The navigation triggers a speculative BrowsingInstance swap because it is
    // browser-initiated and end up being cross-site due to the DIP mismatch.
    EXPECT_FALSE(current_si->IsRelatedSiteInstance(previous_si.get()));
  }

  // Back navigation from a a non cross-origin isolated page to a cross-origin
  // isolated page.
  {
    scoped_refptr<SiteInstanceImpl> previous_si =
        current_frame_host()->GetSiteInstance();
    web_contents()->GetController().GoBack();
    EXPECT_TRUE(WaitForLoadStop(web_contents()));
    SiteInstanceImpl* current_si = current_frame_host()->GetSiteInstance();
    EXPECT_TRUE(current_si->IsCrossOriginIsolated());
    EXPECT_NE(current_si->GetProcess(), previous_si->GetProcess());

    // The navigation triggers a speculative BrowsingInstance swap because it is
    // browser-initiated and end up being cross-site due to the DIP mismatch.
    EXPECT_FALSE(current_si->IsRelatedSiteInstance(previous_si.get()));
  }

  // Back navigation to the non cross-origin isolated initial page.
  {
    scoped_refptr<SiteInstanceImpl> previous_si =
        current_frame_host()->GetSiteInstance();
    web_contents()->GetController().GoBack();
    EXPECT_TRUE(WaitForLoadStop(web_contents()));
    SiteInstanceImpl* current_si = current_frame_host()->GetSiteInstance();
    EXPECT_FALSE(current_si->IsCrossOriginIsolated());
    EXPECT_NE(current_si->GetProcess(), previous_si->GetProcess());

    // The navigation triggers a speculative BrowsingInstance swap because it is
    // browser-initiated and end up being cross-site due to the DIP mismatch.
    EXPECT_FALSE(current_si->IsRelatedSiteInstance(previous_si.get()));
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
    EXPECT_NE(site_instance_1->GetProcess(), site_instance_2->GetProcess());

    // The navigation triggers a speculative BrowsingInstance swap because it is
    // browser-initiated and cross-site.
    EXPECT_FALSE(site_instance_1->IsRelatedSiteInstance(site_instance_2));
  }
}

// Tests that renderer-initiated main frames navigations are assigned a
// cross-origin isolated SiteInstance based on their DocumentIsolationPolicy.
IN_PROC_BROWSER_TEST_P(
    DocumentIsolationPolicyBrowserTest,
    CrossOriginIsolatedSiteInstance_MainFrameRendererInitiated) {
  GURL isolated_page = GetDocumentIsolationPolicyURL("a.test");
  GURL isolated_page_b = GetDocumentIsolationPolicyURL("cdn.a.test");
  GURL non_isolated_page(https_server()->GetURL("a.test", "/title1.html"));

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
    EXPECT_NE(current_si->GetProcess(), previous_si->GetProcess());

    // When BfCache is enabled, a pro-active BrowsingInstance swap happens.
    if (IsBackForwardCacheEnabled()) {
      EXPECT_FALSE(current_si->IsRelatedSiteInstance(previous_si.get()));
    } else {
      EXPECT_TRUE(current_si->IsRelatedSiteInstance(previous_si.get()));
    }
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
    EXPECT_NE(current_si->GetProcess(), previous_si->GetProcess());

    // When BfCache is enabled, a pro-active BrowsingInstance swap happens.
    if (IsBackForwardCacheEnabled()) {
      EXPECT_FALSE(current_si->IsRelatedSiteInstance(previous_si.get()));
    } else {
      EXPECT_TRUE(current_si->IsRelatedSiteInstance(previous_si.get()));
    }
  }

  // Navigate back to a cross-origin isolated page.
  {
    scoped_refptr<SiteInstanceImpl> previous_si =
        current_frame_host()->GetSiteInstance();
    web_contents()->GetController().GoBack();
    ASSERT_TRUE(WaitForLoadStop(web_contents()));
    SiteInstanceImpl* current_si = current_frame_host()->GetSiteInstance();
    EXPECT_TRUE(current_si->IsCrossOriginIsolated());
    EXPECT_NE(current_si->GetProcess(), previous_si->GetProcess());

    // When BfCache is enabled, a pro-active BrowsingInstance swap happens.
    if (IsBackForwardCacheEnabled()) {
      EXPECT_FALSE(current_si->IsRelatedSiteInstance(previous_si.get()));
    } else {
      EXPECT_TRUE(current_si->IsRelatedSiteInstance(previous_si.get()));
    }
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
    EXPECT_NE(site_instance_1->GetProcess(), site_instance_2->GetProcess());

    // When BfCache is enabled, a pro-active BrowsingInstance swap happens.
    if (IsBackForwardCacheEnabled()) {
      EXPECT_FALSE(
          site_instance_2->IsRelatedSiteInstance(site_instance_1.get()));
    } else {
      EXPECT_TRUE(
          site_instance_2->IsRelatedSiteInstance(site_instance_1.get()));
    }
  }
}

// Tests that iframe navigations are assigned a cross-origin isolated
// SiteInstance based on their DocumentIsolationPolicy.
IN_PROC_BROWSER_TEST_P(DocumentIsolationPolicyBrowserTest,
                       CrossOriginIsolatedSiteInstance_IFrame) {
  GURL isolated_page = GetDocumentIsolationPolicyURL("a.test");
  GURL isolated_page_b = GetDocumentIsolationPolicyURL("cdn.a.test");
  GURL non_isolated_page(https_server()->GetURL("a.test", "/title1.html"));

  // Initial cross-origin isolated page.
  EXPECT_TRUE(NavigateToURL(shell(), isolated_page));
  SiteInstanceImpl* main_si = current_frame_host()->GetSiteInstance();
  EXPECT_TRUE(main_si->IsCrossOriginIsolated());

  // Same origin cross-origin isolated iframe.
  TestNavigationManager coi_iframe_navigation(web_contents(), isolated_page);

  EXPECT_TRUE(
      ExecJs(web_contents(),
             JsReplace("const iframe = document.createElement('iframe'); "
                       "iframe.src = $1; "
                       "document.body.appendChild(iframe);",
                       isolated_page)));

  ASSERT_TRUE(coi_iframe_navigation.WaitForNavigationFinished());
  EXPECT_TRUE(coi_iframe_navigation.was_successful());
  RenderFrameHostImpl* coi_iframe_rfh =
      current_frame_host()->child_at(0)->current_frame_host();
  SiteInstanceImpl* coi_iframe_si = coi_iframe_rfh->GetSiteInstance();
  EXPECT_EQ(coi_iframe_si, main_si);

  // Same origin non cross-origin isolated iframe.
  TestNavigationManager non_coi_iframe_navigation(web_contents(),
                                                  non_isolated_page);

  EXPECT_TRUE(
      ExecJs(web_contents(),
             JsReplace("const iframe = document.createElement('iframe'); "
                       "iframe.src = $1; "
                       "document.body.appendChild(iframe);",
                       non_isolated_page)));

  ASSERT_TRUE(non_coi_iframe_navigation.WaitForNavigationFinished());
  EXPECT_TRUE(non_coi_iframe_navigation.was_successful());
  RenderFrameHostImpl* non_coi_iframe_rfh =
      current_frame_host()->child_at(1)->current_frame_host();
  SiteInstanceImpl* non_coi_iframe_si = non_coi_iframe_rfh->GetSiteInstance();
  EXPECT_FALSE(non_coi_iframe_si->IsCrossOriginIsolated());
  EXPECT_TRUE(non_coi_iframe_si->IsRelatedSiteInstance(main_si));
  EXPECT_NE(non_coi_iframe_si, main_si);
  EXPECT_NE(non_coi_iframe_si->GetProcess(), main_si->GetProcess());
  EXPECT_NE(non_coi_iframe_si, coi_iframe_si);
  EXPECT_NE(non_coi_iframe_si->GetProcess(), coi_iframe_si->GetProcess());

  // Cross origin iframe.
  TestNavigationManager cross_origin_iframe_navigation(web_contents(),
                                                       isolated_page_b);

  EXPECT_TRUE(
      ExecJs(web_contents(),
             JsReplace("const iframe = document.createElement('iframe'); "
                       "iframe.src = $1; "
                       "document.body.appendChild(iframe);",
                       isolated_page_b)));

  ASSERT_TRUE(cross_origin_iframe_navigation.WaitForNavigationFinished());
  EXPECT_TRUE(cross_origin_iframe_navigation.was_successful());
  RenderFrameHostImpl* iframe_rfh =
      current_frame_host()->child_at(2)->current_frame_host();
  SiteInstanceImpl* cross_origin_iframe_si = iframe_rfh->GetSiteInstance();
  EXPECT_TRUE(cross_origin_iframe_si->IsCrossOriginIsolated());
  EXPECT_TRUE(cross_origin_iframe_si->IsRelatedSiteInstance(main_si));
  EXPECT_NE(cross_origin_iframe_si, main_si);
  EXPECT_NE(cross_origin_iframe_si->GetProcess(), main_si->GetProcess());
  EXPECT_NE(cross_origin_iframe_si, coi_iframe_si);
  EXPECT_NE(cross_origin_iframe_si->GetProcess(), coi_iframe_si->GetProcess());
  EXPECT_NE(cross_origin_iframe_si, non_coi_iframe_si);
  EXPECT_NE(cross_origin_iframe_si->GetProcess(),
            non_coi_iframe_si->GetProcess());

  // Navigate to a non cross-origin isolated page with a cross-origin isolated
  // iframe.
  {
    scoped_refptr<SiteInstanceImpl> previous_si =
        current_frame_host()->GetSiteInstance();
    EXPECT_TRUE(NavigateToURLFromRenderer(shell(), non_isolated_page));
    main_si = current_frame_host()->GetSiteInstance();
    EXPECT_FALSE(main_si->IsCrossOriginIsolated());
    EXPECT_NE(main_si->GetProcess(), previous_si->GetProcess());

    // When BfCache is enabled, a pro-active BrowsingInstance swap happens.
    if (IsBackForwardCacheEnabled()) {
      EXPECT_FALSE(main_si->IsRelatedSiteInstance(previous_si.get()));
    } else {
      EXPECT_TRUE(main_si->IsRelatedSiteInstance(previous_si.get()));
    }

    TestNavigationManager same_origin_iframe_navigation(web_contents(),
                                                        isolated_page);

    EXPECT_TRUE(
        ExecJs(web_contents(),
               JsReplace("const iframe = document.createElement('iframe'); "
                         "iframe.src = $1; "
                         "document.body.appendChild(iframe);",
                         isolated_page)));

    ASSERT_TRUE(same_origin_iframe_navigation.WaitForNavigationFinished());
    EXPECT_TRUE(same_origin_iframe_navigation.was_successful());
    iframe_rfh = current_frame_host()->child_at(0)->current_frame_host();
    SiteInstanceImpl* iframe_si = iframe_rfh->GetSiteInstance();
    EXPECT_TRUE(iframe_si->IsCrossOriginIsolated());
    EXPECT_TRUE(iframe_si->IsRelatedSiteInstance(main_si));
    EXPECT_NE(iframe_si, main_si);
    EXPECT_NE(iframe_si->GetProcess(), main_si->GetProcess());
  }
}

// Tests that navigations in popups are correctly assigned a cross-origin
// isolated SiteInstance based on their DocumentIsolationPolicy.
IN_PROC_BROWSER_TEST_P(DocumentIsolationPolicyBrowserTest,
                       CrossOriginIsolatedSiteInstance_Popup) {
  GURL isolated_page = GetDocumentIsolationPolicyURL("a.test");
  GURL isolated_page_b = GetDocumentIsolationPolicyURL("cdn.a.test");
  GURL non_isolated_page(
      embedded_test_server()->GetURL("a.test", "/title1.html"));

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
            ->GetPrimaryMainFrame();

    EXPECT_FALSE(popup_rfh->GetSiteInstance()->IsCrossOriginIsolated());
    EXPECT_TRUE(popup_rfh->GetSiteInstance()->IsRelatedSiteInstance(
        current_frame_host()->GetSiteInstance()));
  }

  // Open an isolated popup.
  {
    RenderFrameHostImpl* popup_rfh =
        static_cast<WebContentsImpl*>(
            OpenPopup(current_frame_host(), isolated_page, "")->web_contents())
            ->GetPrimaryMainFrame();

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
            ->GetPrimaryMainFrame();

    EXPECT_TRUE(popup_rfh->GetSiteInstance()->IsCrossOriginIsolated());
    EXPECT_TRUE(popup_rfh->GetSiteInstance()->IsRelatedSiteInstance(
        current_frame_host()->GetSiteInstance()));
    EXPECT_NE(popup_rfh->GetSiteInstance()->GetProcess(),
              current_frame_host()->GetSiteInstance()->GetProcess());
  }
}

// Tests that navigations involving error pages are correctly assigned a
// cross-origin isolated SiteInstance based on their DocumentIsolationPolicy
// status.
IN_PROC_BROWSER_TEST_P(DocumentIsolationPolicyBrowserTest,
                       CrossOriginIsolatedSiteInstance_ErrorPage) {
  GURL isolated_page = GetDocumentIsolationPolicyURL(
      "a.test", "Cross-Origin-Embedder-Policy: require-corp");
  GURL non_coep_page(https_server()->GetURL("b.test",
                                            "/set-header?"
                                            "Access-Control-Allow-Origin: *"));

  GURL invalid_url(
      https_server()->GetURL("a.test", "/this_page_does_not_exist.html"));

  GURL error_url(https_server()->GetURL("a.test", "/page404.html"));
  GURL non_isolated_page(
      https_server()->GetURL("a.test",
                             "/set-header?"
                             "Cross-Origin-Embedder-Policy: require-corp"));

  // Initial cross-origin isolated page.
  EXPECT_TRUE(NavigateToURL(shell(), isolated_page));
  SiteInstanceImpl* main_si = current_frame_host()->GetSiteInstance();
  EXPECT_TRUE(main_si->IsCrossOriginIsolated());

  // Iframe.
  {
    TestNavigationManager iframe_navigation(web_contents(), invalid_url);

    EXPECT_TRUE(
        ExecJs(web_contents(),
               JsReplace("const iframe = document.createElement('iframe'); "
                         "iframe.src = $1; "
                         "document.body.appendChild(iframe);",
                         invalid_url)));

    ASSERT_TRUE(iframe_navigation.WaitForNavigationFinished());
    EXPECT_FALSE(iframe_navigation.was_successful());
    RenderFrameHostImpl* iframe_rfh =
        current_frame_host()->child_at(0)->current_frame_host();
    SiteInstanceImpl* iframe_si = iframe_rfh->GetSiteInstance();
    // The load of the document with 404 status code is blocked by COEP.
    // An error page is expected in lieu of that document.
    EXPECT_EQ(GURL(kUnreachableWebDataURL),
              EvalJs(iframe_rfh, "document.location.href;"));
    EXPECT_TRUE(IsExpectedSubframeErrorTransition(main_si, iframe_si));
    EXPECT_FALSE(iframe_si->IsCrossOriginIsolated());
    EXPECT_NE(iframe_si, main_si);
  }

  // Iframe with a body added to the HTTP 404.
  {
    TestNavigationManager iframe_navigation(web_contents(), error_url);

    EXPECT_TRUE(
        ExecJs(web_contents(),
               JsReplace("const iframe = document.createElement('iframe'); "
                         "iframe.src = $1; "
                         "document.body.appendChild(iframe);",
                         error_url)));

    ASSERT_TRUE(iframe_navigation.WaitForNavigationFinished());
    EXPECT_FALSE(iframe_navigation.was_successful());
    RenderFrameHostImpl* iframe_rfh =
        current_frame_host()->child_at(0)->current_frame_host();
    SiteInstanceImpl* iframe_si = iframe_rfh->GetSiteInstance();
    EXPECT_TRUE(IsExpectedSubframeErrorTransition(main_si, iframe_si));

    // The load of the document with 404 status code and custom body is blocked
    // by COEP. An error page is expected in lieu of that document.
    EXPECT_EQ(GURL(kUnreachableWebDataURL),
              EvalJs(iframe_rfh, "document.location.href;"));
    EXPECT_FALSE(iframe_si->IsCrossOriginIsolated());
    EXPECT_NE(iframe_si, main_si);
  }

  // Iframe blocked by coep.
  {
    TestNavigationManager iframe_navigation(web_contents(), non_coep_page);

    EXPECT_TRUE(
        ExecJs(web_contents(),
               JsReplace("const iframe = document.createElement('iframe'); "
                         "iframe.src = $1; "
                         "document.body.appendChild(iframe);",
                         non_coep_page)));

    ASSERT_TRUE(iframe_navigation.WaitForNavigationFinished());
    EXPECT_FALSE(iframe_navigation.was_successful());
    RenderFrameHostImpl* iframe_rfh =
        current_frame_host()->child_at(0)->current_frame_host();
    SiteInstanceImpl* iframe_si = iframe_rfh->GetSiteInstance();
    EXPECT_TRUE(IsExpectedSubframeErrorTransition(main_si, iframe_si));
    EXPECT_FALSE(iframe_si->IsCrossOriginIsolated());
  }

  // Top frame.
  {
    scoped_refptr<SiteInstanceImpl> previous_si =
        current_frame_host()->GetSiteInstance();
    EXPECT_FALSE(NavigateToURL(shell(), invalid_url));
    SiteInstanceImpl* current_si = current_frame_host()->GetSiteInstance();
    if (IsBackForwardCacheEnabled()) {
      EXPECT_FALSE(current_si->IsRelatedSiteInstance(previous_si.get()));
    } else {
      EXPECT_TRUE(current_si->IsRelatedSiteInstance(previous_si.get()));
    }
    EXPECT_NE(current_si->GetProcess(), previous_si->GetProcess());
    EXPECT_FALSE(current_si->IsCrossOriginIsolated());
  }

  // DIP iframe inside non-DIP page.
  {
    EXPECT_TRUE(NavigateToURL(shell(), non_isolated_page));
    SiteInstanceImpl* current_si = current_frame_host()->GetSiteInstance();
    EXPECT_FALSE(current_si->IsCrossOriginIsolated());

    // First, add an iframe with DocumentIsolationPolicy.
    TestNavigationManager iframe_navigation(web_contents(), isolated_page);
    EXPECT_TRUE(
        ExecJs(web_contents(),
               JsReplace("const iframe = document.createElement('iframe'); "
                         "iframe.id = 'iframe';"
                         "iframe.src = $1; "
                         "document.body.appendChild(iframe);",
                         isolated_page)));
    ASSERT_TRUE(iframe_navigation.WaitForNavigationFinished());
    EXPECT_TRUE(iframe_navigation.was_successful());
    RenderFrameHostImpl* iframe_rfh =
        current_frame_host()->child_at(0)->current_frame_host();
    scoped_refptr<SiteInstanceImpl> iframe_si = iframe_rfh->GetSiteInstance();
    EXPECT_TRUE(iframe_si->IsCrossOriginIsolated());
    EXPECT_NE(current_si, iframe_si);
    EXPECT_NE(current_si->GetProcess(), iframe_si->GetProcess());

    // Now navigate the iframe to an error page.
    TestNavigationManager error_navigation(web_contents(), invalid_url);
    EXPECT_TRUE(ExecJs(
        web_contents(),
        JsReplace("document.getElementById('iframe').src = $1;", invalid_url)));
    ASSERT_TRUE(error_navigation.WaitForNavigationFinished());
    EXPECT_FALSE(error_navigation.was_successful());
    iframe_rfh = current_frame_host()->child_at(0)->current_frame_host();
    scoped_refptr<SiteInstanceImpl> error_si = iframe_rfh->GetSiteInstance();
    EXPECT_FALSE(error_si->IsCrossOriginIsolated());
    EXPECT_NE(error_si, iframe_si);
    EXPECT_NE(error_si->GetProcess(), iframe_si->GetProcess());
    EXPECT_EQ(error_si, current_si);
    EXPECT_EQ(error_si->GetProcess(), current_si->GetProcess());
  }
}

// Tests that a reload navigation that redirects to a page with a
// Document-Isolation-Policy header is placed in a cross-origin isolated
// SiteInstance, even if the original page did not have DocumentIsolationPolicy.
IN_PROC_BROWSER_TEST_P(DocumentIsolationPolicyBrowserTest,
                       ReloadRedirectsToDipPage) {
  GURL dip_page = GetDocumentIsolationPolicyURL("a.test");
  GURL redirect_page(https_server()->GetURL(
      "a.test", "/redirect-on-second-navigation?" + dip_page.spec()));

  // Navigate to the redirect page. On the first navigation, this is a simple
  // empty page with no headers.
  EXPECT_TRUE(NavigateToURL(shell(), redirect_page));
  scoped_refptr<SiteInstanceImpl> main_si =
      current_frame_host()->GetSiteInstance();
  EXPECT_EQ(current_frame_host()->GetLastCommittedURL(), redirect_page);

  // Reload. This time we should be redirected to a DIP:
  // isolate-and-require-corp page.
  ReloadBlockUntilNavigationsComplete(shell(), 1);
  EXPECT_EQ(current_frame_host()->GetLastCommittedURL(), dip_page);

  // We should have swapped SiteInstance.
  EXPECT_NE(main_si, current_frame_host()->GetSiteInstance());
  EXPECT_TRUE(
      main_si->IsRelatedSiteInstance(current_frame_host()->GetSiteInstance()));
  EXPECT_NE(main_si->GetProcess(),
            current_frame_host()->GetSiteInstance()->GetProcess());
}

// Tests that a reload navigation where the page starts sending
// DocumentIsolationPolicy header on the reload (while the initial load did not
// have them). Tha navigation should end up in a cross-origin isolated
// SiteInstance.
IN_PROC_BROWSER_TEST_P(DocumentIsolationPolicyBrowserTest,
                       ReloadPageWithUpdatedDipHeader) {
  GURL changing_dip_page(
      https_server()->GetURL("a.test", "/serve-dip-on-second-navigation"));

  // Navigate to the page. On the first navigation, this is a simple empty page
  // with no headers.
  EXPECT_TRUE(NavigateToURL(shell(), changing_dip_page));
  scoped_refptr<SiteInstanceImpl> main_si =
      current_frame_host()->GetSiteInstance();

  // Reload. This time the page should be served with DIP:
  // isolate-and-require-corp.
  ReloadBlockUntilNavigationsComplete(shell(), 1);

  // We should have swapped SiteInstance.
  EXPECT_NE(main_si, current_frame_host()->GetSiteInstance());
  EXPECT_TRUE(
      main_si->IsRelatedSiteInstance(current_frame_host()->GetSiteInstance()));
  EXPECT_NE(main_si->GetProcess(),
            current_frame_host()->GetSiteInstance()->GetProcess());
}

// Checks that a cross-origin but same site iframe is placed in a different
// SiteInstance from its parent when both have DocumentIsolationPolicy.
IN_PROC_BROWSER_TEST_P(DocumentIsolationPolicyBrowserTest,
                       CrossOriginSameSiteIframe) {
  GURL isolated_page = GetDocumentIsolationPolicyURL("a.test");
  GURL isolated_page_b = GetDocumentIsolationPolicyURL("cdn.a.test");

  // Initial cross-origin isolated page.
  EXPECT_TRUE(NavigateToURL(shell(), isolated_page));
  SiteInstanceImpl* main_si = current_frame_host()->GetSiteInstance();
  EXPECT_TRUE(main_si->IsCrossOriginIsolated());

  TestNavigationManager cross_origin_iframe_navigation(web_contents(),
                                                       isolated_page_b);

  // Add a cross-origin but same-site iframe.
  EXPECT_TRUE(
      ExecJs(web_contents(),
             JsReplace("const iframe = document.createElement('iframe'); "
                       "iframe.src = $1; "
                       "document.body.appendChild(iframe);",
                       isolated_page_b)));

  ASSERT_TRUE(cross_origin_iframe_navigation.WaitForNavigationFinished());
  EXPECT_TRUE(cross_origin_iframe_navigation.was_successful());
  RenderFrameHostImpl* iframe_rfh =
      current_frame_host()->child_at(0)->current_frame_host();
  SiteInstanceImpl* iframe_si = iframe_rfh->GetSiteInstance();
  EXPECT_NE(iframe_si, main_si);
  EXPECT_TRUE(iframe_si->IsCrossOriginIsolated());
  EXPECT_TRUE(iframe_si->IsRelatedSiteInstance(main_si));
  EXPECT_NE(iframe_si->GetProcess(), main_si->GetProcess());

  // Open an isolated popup from the cross-origin but same-site iframe. It
  // should end up in the same SiteInstance as the main frame, since they are
  // same-origin with the same Document-Isolation-Policy.
  {
    RenderFrameHostImpl* popup_rfh =
        static_cast<WebContentsImpl*>(
            OpenPopup(iframe_rfh, isolated_page, "", "", true)->web_contents())
            ->GetPrimaryMainFrame();

    EXPECT_EQ(main_si, popup_rfh->GetSiteInstance());
  }
}

// Checks that the WebExposedIsolationLevel of a RenderFrameHost is properly
// computed when cross-origin isolation is enabled through
// DocumentIsolationPolicy.
IN_PROC_BROWSER_TEST_P(DocumentIsolationPolicyBrowserTest,
                       WebExposedIsolationLevel) {
  GURL isolated_page = GetDocumentIsolationPolicyURL("a.test");
  GURL isolated_page_b = GetDocumentIsolationPolicyURL("b.test");

  // Not isolated:
  EXPECT_TRUE(NavigateToURL(shell(), https_server()->GetURL("/empty.html")));
  EXPECT_EQ(WebExposedIsolationLevel::kNotIsolated,
            current_frame_host()->GetWebExposedIsolationLevel());

  // Cross-Origin Isolated:
  EXPECT_TRUE(NavigateToURL(shell(), isolated_page));
  EXPECT_EQ(WebExposedIsolationLevel::kIsolated,
            current_frame_host()->GetWebExposedIsolationLevel());

  // Cross-origin isolated iframe without permission delegation. The iframe
  // should be cross-origin isolated, as the permission only applies to
  // cross-origin isolation inherited from the parent (and enabled by COOP &
  // COEP).
  std::string create_iframe = R"(
    new Promise(resolve => {
      const iframe = document.createElement('iframe');
      iframe.src = $1;
      iframe.allow = "cross-origin-isolated 'none'";
      iframe.addEventListener('load', () => resolve(true));
      document.body.appendChild(iframe);
    });
  )";
  EXPECT_TRUE(ExecJs(shell(), JsReplace(create_iframe, isolated_page_b)));
  RenderFrameHostImpl* iframe_rfh =
      current_frame_host()->child_at(0)->current_frame_host();
  EXPECT_EQ(WebExposedIsolationLevel::kIsolated,
            iframe_rfh->GetWebExposedIsolationLevel());
}

// Checks that a document with document isolation policy has its
// crossOriginIsolated property set to true and can instantiate
// SharedArrayBuffers.
IN_PROC_BROWSER_TEST_P(DocumentIsolationPolicyBrowserTest, SAB) {
  GURL url = GetDocumentIsolationPolicyURL("a.test");
  EXPECT_TRUE(NavigateToURL(shell(), url));
  EXPECT_EQ(true, EvalJs(current_frame_host(), "self.crossOriginIsolated"));
  EXPECT_EQ(true,
            EvalJs(current_frame_host(), "'SharedArrayBuffer' in globalThis"));
}

// Checks that a document with document isolation policy can transfer a
// SharedArrayBuffer to a crossOriginIsolated same-origin iframe.
IN_PROC_BROWSER_TEST_P(DocumentIsolationPolicyBrowserTest,
                       SAB_TransferToIframe) {
  CHECK(!base::FeatureList::IsEnabled(features::kSharedArrayBuffer));
  GURL url = GetDocumentIsolationPolicyURL("a.test");
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
    const sab = new SharedArrayBuffer(1234);
    g_iframe.contentWindow.postMessage(sab, "*");
  )"));

  EXPECT_EQ(1234, EvalJs(sub_document, "g_sab_size"));
}

// Checks that a document with document isolation policy can transfer a
// SharedArrayBuffer to a crossOriginIsolated same-origin about:blank iframe.
IN_PROC_BROWSER_TEST_P(DocumentIsolationPolicyBrowserTest,
                       SAB_TransferToAboutBlankIframe) {
  CHECK(!base::FeatureList::IsEnabled(features::kSharedArrayBuffer));
  GURL url = GetDocumentIsolationPolicyURL("a.test");
  EXPECT_TRUE(NavigateToURL(shell(), url));
  EXPECT_TRUE(ExecJs(current_frame_host(),
                     "g_iframe = document.createElement('iframe');"
                     "g_iframe.src = 'about:blank';"
                     "document.body.appendChild(g_iframe);"));
  WaitForLoadStop(web_contents());

  RenderFrameHostImpl* main_document = current_frame_host();
  RenderFrameHostImpl* sub_document =
      current_frame_host()->child_at(0)->current_frame_host();

  EXPECT_EQ(true, EvalJs(main_document, "self.crossOriginIsolated"));
  EXPECT_EQ(true, EvalJs(sub_document, "self.crossOriginIsolated"));
  EXPECT_EQ(true, EvalJs(sub_document, "'SharedArrayBuffer' in globalThis"));
}

// Checks that an about:blank iframe created by a cross-origin isolated document
// (through Document-Isolation-Policy) is set as crossOriginIsolated
// synchronously.
IN_PROC_BROWSER_TEST_P(DocumentIsolationPolicyBrowserTest,
                       AboutBlankIsSetCOISynchronously) {
  CHECK(!base::FeatureList::IsEnabled(features::kSharedArrayBuffer));
  GURL url = GetDocumentIsolationPolicyURL("a.test");
  EXPECT_TRUE(NavigateToURL(shell(), url));
  EXPECT_EQ(true, EvalJs(current_frame_host(),
                         "const iframe = document.createElement('iframe');"
                         "document.body.appendChild(iframe);"
                         "iframe.contentWindow.crossOriginIsolated;"));
}

// Transfer a SharedArrayBuffer in between two documents with a parent/child
// relationship. The child has not set Document-Isolation-Policy, and is not
// cross-origin isolated. It cannot receive the object.
IN_PROC_BROWSER_TEST_P(DocumentIsolationPolicyBrowserTest,
                       SAB_TransferToNoCrossOriginIsolatedIframe) {
  CHECK(!base::FeatureList::IsEnabled(features::kSharedArrayBuffer));
  GURL main_url = GetDocumentIsolationPolicyURL("a.test");
  GURL iframe_url = https_server()->GetURL("a.test", "/title1.html");
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

  // The parent and its child frame are in different processes, so it's not
  // possible to transfer a SharedArrayBuffer between the two of them.
  EXPECT_NE(main_document->GetSiteInstance()->GetProcess(),
            sub_document->GetSiteInstance()->GetProcess());
}

// Transfer a SharedArrayBuffer in between two documents with a
// parent/child relationship. The child does not have Document-Isolation-Policy.
// This non-cross-origin-isolated document cannot transfer a SharedArrayBuffer
// toward the cross-origin-isolated one.
IN_PROC_BROWSER_TEST_P(DocumentIsolationPolicyBrowserTest,
                       SAB_TransferFromNoCrossOriginIsolatedIframe) {
  CHECK(!base::FeatureList::IsEnabled(features::kSharedArrayBuffer));
  GURL main_url = GetDocumentIsolationPolicyURL("a.test");
  GURL iframe_url = https_server()->GetURL("a.test", "/title1.html");
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

  // Unlike crossOriginIsolation enabled by COOP and COEP, the SAB cannot be
  // created in the non-crossOriginIsolated iframe.
  // See https://crbug.com/1144838 for discussions about this behavior in COOP
  // and COEP.
  EXPECT_FALSE(ExecJs(sub_document, R"(
    // Create a WebAssembly Memory to try to bypass the SAB constructor
    // restriction.
    const sab = new (new WebAssembly.Memory(
        { shared:true, initial:1, maximum:1 }).buffer.constructor)(1234);
    parent.postMessage(sab, "*");
  )"));
}

// TODO(crbug.com/349104385): Add a test checking that the
// Document-Isolation-Policy header is ignored on redirect responses.

static auto kTestParams =
    testing::Combine(testing::ValuesIn(RenderDocumentFeatureLevelValues()),
                     testing::Bool(),
                     testing::Bool());
INSTANTIATE_TEST_SUITE_P(All,
                         DocumentIsolationPolicyBrowserTest,
                         kTestParams,
                         DocumentIsolationPolicyBrowserTest::DescribeParams);
INSTANTIATE_TEST_SUITE_P(All,
                         DocumentIsolationPolicyWithoutFeatureBrowserTest,
                         kTestParams,
                         DocumentIsolationPolicyBrowserTest::DescribeParams);
INSTANTIATE_TEST_SUITE_P(All,
                         DocumentIsolationPolicyWithoutSiteIsolationBrowserTest,
                         kTestParams,
                         DocumentIsolationPolicyBrowserTest::DescribeParams);

}  // namespace content
