// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/strings/escape.h"
#include "base/strings/stringprintf.h"
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
#include "services/network/public/cpp/cross_origin_opener_policy.h"
#include "services/network/public/cpp/features.h"
#include "testing/gmock/include/gmock/gmock.h"

using ::testing::HasSubstr;

namespace content {

namespace {

network::CrossOriginOpenerPolicy CoopSameOrigin() {
  network::CrossOriginOpenerPolicy coop;
  coop.value = network::mojom::CrossOriginOpenerPolicyValue::kSameOrigin;
  coop.soap_by_default_value =
      network::mojom::CrossOriginOpenerPolicyValue::kSameOrigin;
  return coop;
}

network::CrossOriginOpenerPolicy CoopSameOriginPlusCoep() {
  network::CrossOriginOpenerPolicy coop;
  coop.value =
      network::mojom::CrossOriginOpenerPolicyValue::kSameOriginPlusCoep;
  coop.soap_by_default_value =
      network::mojom::CrossOriginOpenerPolicyValue::kSameOriginPlusCoep;
  return coop;
}

network::CrossOriginOpenerPolicy CoopSameOriginAllowPopups() {
  network::CrossOriginOpenerPolicy coop;
  coop.value =
      network::mojom::CrossOriginOpenerPolicyValue::kSameOriginAllowPopups;
  coop.soap_by_default_value =
      network::mojom::CrossOriginOpenerPolicyValue::kSameOriginAllowPopups;
  return coop;
}

// This is the value of COOP when navigating to a page without COOP set:
//  - value is kUnsafeNone
//  - soap_by_default_value is kSameOriginAllowPopups
network::CrossOriginOpenerPolicy CoopUnsafeNoneWithSoapByDefault() {
  network::CrossOriginOpenerPolicy coop;
  coop.soap_by_default_value =
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
  std::string dest = base::UnescapeBinaryURLComponent(request_url.query());
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

std::unique_ptr<net::test_server::HttpResponse>
CoopAndCspSandboxRedirectHandler(const net::test_server::HttpRequest& request) {
  std::string dest = base::UnescapeBinaryURLComponent(request.GetURL().query());
  net::test_server::RequestQuery query =
      net::test_server::ParseQuery(request.GetURL());

  auto http_response = std::make_unique<net::test_server::BasicHttpResponse>();
  http_response->set_code(net::HttpStatusCode::HTTP_FOUND);
  http_response->AddCustomHeader("Location", dest);
  http_response->AddCustomHeader("Cross-Origin-Opener-Policy", "same-origin");
  http_response->AddCustomHeader("Content-Security-Policy", "sandbox");
  return http_response;
}

std::unique_ptr<net::test_server::HttpResponse> ServeCoopOnSecondNavigation(
    unsigned int& navigation_counter,
    const net::test_server::HttpRequest& request) {
  ++navigation_counter;
  auto http_response = std::make_unique<net::test_server::BasicHttpResponse>();
  http_response->set_code(net::HttpStatusCode::HTTP_OK);
  http_response->AddCustomHeader("Cache-Control", "no-store, must-revalidate");
  if (navigation_counter > 1)
    http_response->AddCustomHeader("Cross-Origin-Opener-Policy", "same-origin");
  return http_response;
}

class CrossOriginOpenerPolicyBrowserTest
    : public ContentBrowserTest,
      public ::testing::WithParamInterface<std::tuple<std::string, bool>> {
 public:
  CrossOriginOpenerPolicyBrowserTest()
      : prerender_helper_(base::BindRepeating(
            &CrossOriginOpenerPolicyBrowserTest::prerender_web_contents,
            base::Unretained(this))),
        https_server_(net::EmbeddedTestServer::TYPE_HTTPS) {
    // Enable COOP/COEP:
    feature_list_.InitWithFeatures(
        {network::features::kCrossOriginOpenerPolicy,
         network::features::kCoopNoopenerAllowPopups},
        {features::kProcessPerSiteUpToMainFrameThreshold});

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

    // Set the speculative RFH creation delay to 0 so that the speculative RFH
    // is always created before receiving the response. Otherwise the RFH will
    // always be created with the correct COOP header.
    feature_list_for_defer_speculative_rfh_.InitAndEnableFeatureWithParameters(
        features::kDeferSpeculativeRFHCreation,
        {{"create_speculative_rfh_delay_ms", "0"}});
  }

  // Provides meaningful param names instead of /0, /1, ...
  static std::string DescribeParams(
      const testing::TestParamInfo<ParamType>& info) {
    auto [render_document_level, enable_back_forward_cache] = info.param;
    return base::StringPrintf(
        "%s_%s",
        GetRenderDocumentLevelNameForTestParams(render_document_level).c_str(),
        enable_back_forward_cache ? "BFCacheEnabled" : "BFCacheDisabled");
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
    https_server_.RegisterDefaultHandler(base::BindRepeating(
        &net::test_server::HandlePrefixedRequest,
        "/redirect-with-coop-coep-headers",
        base::BindRepeating(CrossOriginIsolatedCrossOriginRedirectHandler)));
    https_server_.RegisterDefaultHandler(base::BindRepeating(
        &net::test_server::HandlePrefixedRequest,
        "/redirect-with-coop-and-csp-headers",
        base::BindRepeating(CoopAndCspSandboxRedirectHandler)));
    AddRedirectOnSecondNavigationHandler(&https_server_);
    unsigned int navigation_counter = 0;
    https_server_.RegisterDefaultHandler(base::BindRepeating(
        &net::test_server::HandlePrefixedRequest,
        "/serve-coop-on-second-navigation",
        base::BindRepeating(&ServeCoopOnSecondNavigation,
                            base::OwnedRef(navigation_counter))));

    prerender_helper().RegisterServerRequestMonitor(&https_server_);

    ASSERT_TRUE(https_server()->Start());
  }

 private:
  void SetUpCommandLine(base::CommandLine* command_line) override {
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
  base::test::ScopedFeatureList feature_list_for_defer_speculative_rfh_;
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

// Certain features are only active when SiteIsolation is off or restricted.
// This is the case for example for Default SiteInstances that are used on
// Android to limit the number of processes. Testing these particularities of
// the process model and their interaction with cross-origin isolation requires
// to disable SiteIsolation.
// TODO(crbug.com/390571607): Remove this version of the test in favour of the
// SiteInstanceGroup version below once default SiteInstanceGroups are
// enabled by default.
class NoSiteIsolationCrossOriginIsolationBrowserTest
    : public CrossOriginOpenerPolicyBrowserTest {
 public:
  NoSiteIsolationCrossOriginIsolationBrowserTest() {
    // Disable the heuristic to isolate COOP pages from the default
    // SiteInstance. This is otherwise on by default on Android. Disable default
    // SiteInstanceGroups, as that's covered in the SiteInstanceGroup version of
    // this test.
    feature_list_.InitWithFeatures(
        {}, {features::kSiteIsolationForCrossOriginOpenerPolicy,
             features::kDefaultSiteInstanceGroups});
  }

  void SetUpOnMainThread() override {
    CrossOriginOpenerPolicyBrowserTest::SetUpOnMainThread();
    browser_client_ = std::make_unique<NoSiteIsolationContentBrowserClient>();

    // The custom ContentBrowserClient above typically ensures that this test
    // runs without strict site isolation, but it's still possible to
    // inadvertently override this when running with --site-per-process on the
    // command line. This might happen on try bots, so these tests take this
    // into account to prevent failures, but this is not an intended
    // configuration for these tests.
    if (AreAllSitesIsolatedForTesting()) {
      LOG(WARNING) << "This test should be run without --site-per-process, "
                   << "as it's designed to exercise code paths when strict "
                   << "site isolation is turned off.";
    }
  }

  void TearDownOnMainThread() override {
    CrossOriginOpenerPolicyBrowserTest::TearDownOnMainThread();
    browser_client_.reset();
  }

  // A custom ContentBrowserClient to turn off strict site isolation, since
  // process model differences exist in environments like Android. Note that
  // kSitePerProcess is a higher-layer feature, so we can't just disable it
  // here.
  class NoSiteIsolationContentBrowserClient
      : public ContentBrowserTestContentBrowserClient {
   public:
    bool ShouldEnableStrictSiteIsolation() override { return false; }
  };

 private:
  std::unique_ptr<NoSiteIsolationContentBrowserClient> browser_client_;

  base::test::ScopedFeatureList feature_list_;
};

// This test is the same as NoSiteIsolationCrossOriginIsolationBrowserTest,
// except it enables and uses default SiteInstanceGroup instead of default
// SiteInstance.
class DefaultSiteInstanceGroupCrossOriginIsolationBrowserTest
    : public NoSiteIsolationCrossOriginIsolationBrowserTest {
 public:
  DefaultSiteInstanceGroupCrossOriginIsolationBrowserTest() {
    feature_list_.InitWithFeatures({features::kDefaultSiteInstanceGroups}, {});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

using VirtualBrowsingContextGroupTest = CrossOriginOpenerPolicyBrowserTest;
using SoapByDefaultVirtualBrowsingContextGroupTest =
    CrossOriginOpenerPolicyBrowserTest;

int VirtualBrowsingContextGroup(WebContents* wc) {
  return static_cast<WebContentsImpl*>(wc)
      ->GetPrimaryMainFrame()
      ->virtual_browsing_context_group();
}

int SoapByDefaultVirtualBrowsingContextGroup(WebContents* wc) {
  return static_cast<WebContentsImpl*>(wc)
      ->GetPrimaryMainFrame()
      ->soap_by_default_virtual_browsing_context_group();
}

class ProcessReuseOnPrerenderCOOPSwapBrowserTest
    : public CrossOriginOpenerPolicyBrowserTest {
 public:
  ProcessReuseOnPrerenderCOOPSwapBrowserTest() {
    feature_list_.InitAndEnableFeature(
        features::kProcessReuseOnPrerenderCOOPSwap);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

}  // namespace

IN_PROC_BROWSER_TEST_P(CrossOriginOpenerPolicyBrowserTest,
                       NewPopupCOOP_InheritsSameOrigin) {
  GURL starting_page(https_server()->GetURL(
      "a.test", "/set-header?cross-origin-opener-policy: same-origin"));
  EXPECT_TRUE(NavigateToURL(shell(), starting_page));

  RenderFrameHostImpl* main_rfh = current_frame_host();

  // Create same origin child frame.
  ASSERT_TRUE(ExecJs(main_rfh, R"(
    const frame = document.createElement('iframe');
    frame.src = '/empty.html';
    document.body.appendChild(frame);
  )"));
  EXPECT_TRUE(WaitForLoadStop(web_contents()));

  ShellAddedObserver shell_observer;
  RenderFrameHostImpl* iframe_rfh = main_rfh->child_at(0)->current_frame_host();
  EXPECT_TRUE(ExecJs(iframe_rfh, "window.open('about:blank')"));

  RenderFrameHostImpl* popup_rfh =
      static_cast<WebContentsImpl*>(shell_observer.GetShell()->web_contents())
          ->GetPrimaryMainFrame();

  EXPECT_EQ(main_rfh->cross_origin_opener_policy(), CoopSameOrigin());
  EXPECT_EQ(popup_rfh->cross_origin_opener_policy(), CoopSameOrigin());
}

IN_PROC_BROWSER_TEST_P(CrossOriginOpenerPolicyBrowserTest,
                       NewPopupCOOP_InheritsSameOriginAllowPopups) {
  GURL starting_page(https_server()->GetURL(
      "a.test",
      "/set-header?cross-origin-opener-policy: same-origin-allow-popups"));
  EXPECT_TRUE(NavigateToURL(shell(), starting_page));

  RenderFrameHostImpl* main_rfh = current_frame_host();

  // Create same origin child frame.
  ASSERT_TRUE(ExecJs(current_frame_host(), R"(
    const frame = document.createElement('iframe');
    frame.src = '/empty.html';
    document.body.appendChild(frame);
  )"));
  EXPECT_TRUE(WaitForLoadStop(web_contents()));

  ShellAddedObserver shell_observer;
  RenderFrameHostImpl* iframe_rfh = main_rfh->child_at(0)->current_frame_host();
  EXPECT_TRUE(ExecJs(iframe_rfh, "window.open('about:blank')"));

  RenderFrameHostImpl* popup_rfh =
      static_cast<WebContentsImpl*>(shell_observer.GetShell()->web_contents())
          ->GetPrimaryMainFrame();

  EXPECT_EQ(main_rfh->cross_origin_opener_policy(),
            CoopSameOriginAllowPopups());
  EXPECT_EQ(popup_rfh->cross_origin_opener_policy(),
            CoopSameOriginAllowPopups());
}

IN_PROC_BROWSER_TEST_P(CrossOriginOpenerPolicyBrowserTest,
                       NewPopupCOOP_CrossOriginDoesNotInheritSameOrigin) {
  GURL starting_page(https_server()->GetURL(
      "a.test", "/set-header?cross-origin-opener-policy: same-origin"));
  GURL url_b(https_server()->GetURL("b.test", "/empty.html"));

  EXPECT_TRUE(NavigateToURL(shell(), starting_page));

  RenderFrameHostImpl* main_rfh = current_frame_host();

  // Create cross origin child frame.
  ASSERT_TRUE(ExecJs(main_rfh, JsReplace(R"(
    const frame = document.createElement('iframe');
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
          ->GetPrimaryMainFrame();

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
        https_server()->GetURL("a.test", std::string("/set-header?") + header));
    GURL url_b(https_server()->GetURL("b.test", "/empty.html"));

    EXPECT_TRUE(NavigateToURL(shell(), starting_page));

    RenderFrameHostImpl* main_rfh = current_frame_host();

    // Create cross origin child frame.
    ASSERT_TRUE(ExecJs(main_rfh, JsReplace(R"(
        const frame = document.createElement('iframe');
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
            ->GetPrimaryMainFrame();

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
                       BlobInheritsCreatorSameOrigin) {
  GURL starting_page(https_server()->GetURL(
      "a.test", "/set-header?cross-origin-opener-policy: same-origin"));
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

  // COOP and COEP inherited from Blob creator
  EXPECT_EQ(popup_rfh->cross_origin_opener_policy(), CoopSameOrigin());
  EXPECT_EQ(popup_rfh->cross_origin_embedder_policy().value,
            network::mojom::CrossOriginEmbedderPolicyValue::kNone);
  EXPECT_FALSE(popup_rfh->GetSiteInstance()->IsCrossOriginIsolated());
}

IN_PROC_BROWSER_TEST_P(CrossOriginOpenerPolicyBrowserTest,
                       BlobInheritsInitiatorSameOriginPlusCoepCredentialless) {
  GURL starting_page(
      https_server()->GetURL("a.test",
                             "/set-header"
                             "?cross-origin-opener-policy: same-origin"
                             "&cross-origin-embedder-policy: credentialless"));
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

  // COOP and COEP inherited from Blob creator
  EXPECT_EQ(popup_rfh->cross_origin_opener_policy(), CoopSameOriginPlusCoep());
  EXPECT_EQ(popup_rfh->cross_origin_embedder_policy().value,
            network::mojom::CrossOriginEmbedderPolicyValue::kCredentialless);
  EXPECT_TRUE(popup_rfh->GetSiteInstance()->IsCrossOriginIsolated());
}

IN_PROC_BROWSER_TEST_P(CrossOriginOpenerPolicyBrowserTest,
                       BlobInheritsInitiatorSameOriginPlusCoep) {
  GURL starting_page(
      https_server()->GetURL("a.test",
                             "/set-header"
                             "?cross-origin-opener-policy: same-origin"
                             "&cross-origin-embedder-policy: require-corp"));
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

  // COOP and COEP inherited from Blob creator
  EXPECT_EQ(popup_rfh->cross_origin_opener_policy(), CoopSameOriginPlusCoep());
  EXPECT_EQ(popup_rfh->cross_origin_embedder_policy().value,
            network::mojom::CrossOriginEmbedderPolicyValue::kRequireCorp);
  EXPECT_TRUE(popup_rfh->GetSiteInstance()->IsCrossOriginIsolated());
}

IN_PROC_BROWSER_TEST_P(CrossOriginOpenerPolicyBrowserTest,
                       BlobInheritsCreatorSameOriginAllowPopups) {
  GURL starting_page(https_server()->GetURL(
      "a.test",
      "/set-header"
      "?cross-origin-opener-policy: same-origin-allow-popups"
      "&cross-origin-embedder-policy: require-corp"));
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

  // COOP and COEP inherited from Blob creator
  EXPECT_EQ(popup_rfh->cross_origin_opener_policy(),
            CoopSameOriginAllowPopups());
  EXPECT_EQ(popup_rfh->cross_origin_embedder_policy().value,
            network::mojom::CrossOriginEmbedderPolicyValue::kRequireCorp);
  EXPECT_FALSE(popup_rfh->GetSiteInstance()->IsCrossOriginIsolated());
}

IN_PROC_BROWSER_TEST_P(CrossOriginOpenerPolicyBrowserTest,
                       BlobInheritsCreatorTopFrameSameOriginCreatorIframeCOEP) {
  GURL starting_page(https_server()->GetURL(
      "a.test", "/set-header?cross-origin-opener-policy: same-origin"));
  GURL iframe_with_coep_url(https_server()->GetURL(
      "a.test", "/set-header?cross-origin-embedder-policy: require-corp"));

  EXPECT_TRUE(NavigateToURL(shell(), starting_page));

  // Create same origin child frame with COEP
  ASSERT_TRUE(ExecJs(current_frame_host(), JsReplace(R"(
    const frame = document.createElement('iframe');
    frame.src = $1;
    document.body.appendChild(frame);
  )",
                                                     iframe_with_coep_url)));
  EXPECT_TRUE(WaitForLoadStop(web_contents()));

  RenderFrameHostImpl* child_rfh =
      current_frame_host()->child_at(0)->current_frame_host();

  // Create and open blob from iframe.
  ShellAddedObserver shell_observer;
  ASSERT_TRUE(ExecJs(child_rfh, R"(
    const blob = new Blob(['foo'], {type : 'text/html'});
    const url = URL.createObjectURL(blob);
    window.open(url);
  )"));
  EXPECT_TRUE(WaitForLoadStop(shell_observer.GetShell()->web_contents()));
  RenderFrameHostImpl* popup_rfh =
      static_cast<WebContentsImpl*>(shell_observer.GetShell()->web_contents())
          ->GetPrimaryMainFrame();

  // COOP is inherited from creator's top level document, COEP is inherited from
  // creator.
  EXPECT_EQ(popup_rfh->cross_origin_opener_policy(), CoopSameOrigin());
  EXPECT_EQ(popup_rfh->cross_origin_embedder_policy().value,
            network::mojom::CrossOriginEmbedderPolicyValue::kRequireCorp);
  EXPECT_FALSE(popup_rfh->GetSiteInstance()->IsCrossOriginIsolated());
}

IN_PROC_BROWSER_TEST_P(CrossOriginOpenerPolicyBrowserTest,
                       BlobInheritsCreatorNotInitiator) {
  GURL starting_page(https_server()->GetURL(
      "a.test",
      "/set-header"
      "?cross-origin-opener-policy: same-origin-allow-popups"));
  EXPECT_TRUE(NavigateToURL(shell(), starting_page));

  // Create blob url in main page, which will be used later.
  // Then open a popup on a document that is same-origin without COOP.
  ShellAddedObserver first_shell_observer;
  ASSERT_TRUE(ExecJs(current_frame_host(), R"(
    const blob = new Blob(['foo'], {type : 'text/html'});
    window.url = URL.createObjectURL(blob);
    window.open("/empty.html");
  )"));
  EXPECT_TRUE(WaitForLoadStop(first_shell_observer.GetShell()->web_contents()));
  RenderFrameHostImpl* first_popup_rfh =
      static_cast<WebContentsImpl*>(
          first_shell_observer.GetShell()->web_contents())
          ->GetPrimaryMainFrame();

  // Open blob url created in opener.
  ShellAddedObserver second_shell_observer;
  ASSERT_TRUE(ExecJs(first_popup_rfh, R"(
    window.open(opener.url);
  )"));
  EXPECT_TRUE(
      WaitForLoadStop(second_shell_observer.GetShell()->web_contents()));
  RenderFrameHostImpl* second_popup_rfh =
      static_cast<WebContentsImpl*>(
          second_shell_observer.GetShell()->web_contents())
          ->GetPrimaryMainFrame();

  // COOP and COEP inherited from Blob creator (initial window) and not the
  // initiator (first popup)
  // TODO(crbug.com/40051710) COOP should be inherited from creator and
  // be same-origin-allow-popups, instead of inheriting from initiator.
  EXPECT_EQ(second_popup_rfh->cross_origin_opener_policy(),
            CoopUnsafeNoneWithSoapByDefault());
  EXPECT_EQ(second_popup_rfh->cross_origin_embedder_policy().value,
            network::mojom::CrossOriginEmbedderPolicyValue::kNone);
  EXPECT_FALSE(second_popup_rfh->GetSiteInstance()->IsCrossOriginIsolated());
}

// Verify that a opening a popup to a COOP page, with sandbox flags inherited
// from the initiator ends up as an error page.
IN_PROC_BROWSER_TEST_P(CrossOriginOpenerPolicyBrowserTest,
                       SandboxViaInheritanceWithCoop) {
  GURL main_page_url = https_server()->GetURL(
      "a.test", "/cross-origin-opener-policy_sandbox_popup.html");
  GURL coop_url = https_server()->GetURL(
      "a.test", "/set-header?Cross-Origin-Opener-Policy: same-origin");

  ASSERT_TRUE(NavigateToURL(shell(), main_page_url));
  ShellAddedObserver shell_observer;
  RenderFrameHostImpl* iframe_rfh =
      current_frame_host()->child_at(0)->current_frame_host();

  ASSERT_TRUE(ExecJs(iframe_rfh, JsReplace("window.open($1);", coop_url)));

  auto* popup_webcontents =
      static_cast<WebContentsImpl*>(shell_observer.GetShell()->web_contents());
  WaitForLoadStop(popup_webcontents);

  EXPECT_EQ(
      popup_webcontents->GetController().GetLastCommittedEntry()->GetPageType(),
      PAGE_TYPE_ERROR);
}

// Verify that a navigation toward a COOP page, with sandbox flags inherited
// from the initiator ends up as an error page.
IN_PROC_BROWSER_TEST_P(CrossOriginOpenerPolicyBrowserTest,
                       SandboxViaInheritanceNavigationsToCoop) {
  GURL main_page_url = https_server()->GetURL(
      "a.test", "/cross-origin-opener-policy_sandbox_popup.html");
  GURL coop_url = https_server()->GetURL(
      "a.test", "/set-header?Cross-Origin-Opener-Policy: same-origin");
  GURL non_coop_url = https_server()->GetURL("a.test", "/title1.html");

  ASSERT_TRUE(NavigateToURL(shell(), main_page_url));
  ShellAddedObserver shell_observer;
  RenderFrameHostImpl* iframe_rfh =
      current_frame_host()->child_at(0)->current_frame_host();

  ASSERT_TRUE(ExecJs(iframe_rfh, JsReplace("window.open($1);", non_coop_url)));

  auto* popup_webcontents =
      static_cast<WebContentsImpl*>(shell_observer.GetShell()->web_contents());
  WaitForLoadStop(popup_webcontents);
  ASSERT_NE(popup_webcontents->GetPrimaryMainFrame()->active_sandbox_flags(),
            network::mojom::WebSandboxFlags::kNone);

  EXPECT_FALSE(NavigateToURL(popup_webcontents, coop_url));
  EXPECT_EQ(
      popup_webcontents->GetController().GetLastCommittedEntry()->GetPageType(),
      PAGE_TYPE_ERROR);
}

// Verify that a document setting COOP can also set sandbox via CSP.
IN_PROC_BROWSER_TEST_P(CrossOriginOpenerPolicyBrowserTest,
                       SandboxViaCspWithCoop) {
  GURL coop_and_csp_url =
      https_server()->GetURL("a.test",
                             "/set-header?"
                             "Cross-Origin-Opener-Policy: same-origin&"
                             "Content-Security-Policy: sandbox");
  EXPECT_TRUE(NavigateToURL(shell(), coop_and_csp_url));
  EXPECT_EQ(
      web_contents()->GetController().GetLastCommittedEntry()->GetPageType(),
      PAGE_TYPE_NORMAL);
  ASSERT_EQ(current_frame_host()->active_sandbox_flags(),
            network::mojom::WebSandboxFlags::kAll);
  EXPECT_EQ(web_contents()->GetPrimaryMainFrame()->cross_origin_opener_policy(),
            CoopSameOrigin());
}

// Verify that navigating from a document sandboxed via CSP to a COOP document,
// and vice versa, does not end up as an error page.
IN_PROC_BROWSER_TEST_P(CrossOriginOpenerPolicyBrowserTest,
                       SandboxViaCspNavigationsToCoop) {
  GURL csp_url(https_server()->GetURL(
      "a.test", "/set-header?Content-Security-Policy: sandbox"));
  GURL coop_url = https_server()->GetURL(
      "a.test", "/set-header?Cross-Origin-Opener-Policy: same-origin");

  ASSERT_TRUE(NavigateToURL(shell(), csp_url));
  ASSERT_EQ(current_frame_host()->active_sandbox_flags(),
            network::mojom::WebSandboxFlags::kAll);

  EXPECT_TRUE(NavigateToURL(shell(), coop_url));
  EXPECT_EQ(
      web_contents()->GetController().GetLastCommittedEntry()->GetPageType(),
      PAGE_TYPE_NORMAL);

  EXPECT_TRUE(NavigateToURL(shell(), csp_url));
  EXPECT_EQ(
      web_contents()->GetController().GetLastCommittedEntry()->GetPageType(),
      PAGE_TYPE_NORMAL);
}

// Verify that CSP sandbox, which makes the origin opaque, is taken into account
// for the COOP enforcement of the final response.
IN_PROC_BROWSER_TEST_P(CrossOriginOpenerPolicyBrowserTest,
                       SandboxViaCspOpaqueOriginForResponse) {
  GURL coop_url = https_server()->GetURL(
      "a.test", "/set-header?Cross-Origin-Opener-Policy: same-origin");
  GURL coop_and_csp_url =
      https_server()->GetURL("a.test",
                             "/set-header?"
                             "Cross-Origin-Opener-Policy: same-origin&"
                             "Content-Security-Policy: sandbox");

  // Start on a page that sets COOP: same-origin.
  ASSERT_TRUE(NavigateToURL(shell(), coop_url));
  scoped_refptr<SiteInstance> coop_site_instance =
      current_frame_host()->GetSiteInstance();

  // We want to figure out if a BrowsingInstance swap happens because of COOP.
  // To prevent some other types of swaps, such as proactive swaps, we do the
  // navigations in a popup.
  ShellAddedObserver shell_observer;
  ASSERT_TRUE(
      ExecJs(current_frame_host(), JsReplace("window.open($1);", coop_url)));
  auto* popup_webcontents =
      static_cast<WebContentsImpl*>(shell_observer.GetShell()->web_contents());
  WaitForLoadStop(popup_webcontents);
  RenderFrameHostImpl* popup_rfh = popup_webcontents->GetPrimaryMainFrame();
  ASSERT_EQ(popup_rfh->GetSiteInstance(), coop_site_instance.get());

  // Navigate to a same-origin COOP page that sets sandboxing via CSP. The popup
  // should be sandboxed and have an opaque origin.
  ASSERT_TRUE(NavigateToURL(popup_webcontents, coop_and_csp_url));
  scoped_refptr<SiteInstance> coop_and_csp_site_instance =
      popup_webcontents->GetPrimaryMainFrame()->GetSiteInstance();
  ASSERT_EQ(popup_webcontents->GetPrimaryMainFrame()->active_sandbox_flags(),
            network::mojom::WebSandboxFlags::kAll);
  EXPECT_FALSE(coop_site_instance->IsRelatedSiteInstance(
      coop_and_csp_site_instance.get()));

  // Navigate again to the COOP+CSP page. The same should be true in the other
  // direction.
  ASSERT_TRUE(NavigateToURL(popup_webcontents, coop_and_csp_url));
  scoped_refptr<SiteInstance> final_coop_site_instance =
      popup_webcontents->GetPrimaryMainFrame()->GetSiteInstance();
  EXPECT_FALSE(coop_and_csp_site_instance->IsRelatedSiteInstance(
      final_coop_site_instance.get()));
}

// Verify that CSP sandbox, which makes the origin opaque, is not taken into
// account for the COOP enforcement of the final response.
IN_PROC_BROWSER_TEST_P(CrossOriginOpenerPolicyBrowserTest,
                       SandboxViaCspNonOpaqueOriginForRedirect) {
  GURL coop_url = https_server()->GetURL(
      "a.test", "/set-header?Cross-Origin-Opener-Policy: same-origin");
  GURL coop_and_csp_redirect_url = https_server()->GetURL(
      "a.test", "/redirect-with-coop-and-csp-headers?" + coop_url.spec());

  // Start on a page that sets COOP: same-origin.
  ASSERT_TRUE(NavigateToURL(shell(), coop_url));
  scoped_refptr<SiteInstance> coop_site_instance =
      current_frame_host()->GetSiteInstance();

  // We want to figure out if a BrowsingInstance swap happens because of COOP.
  // To prevent some other types of swaps, such as proactive swaps, we do the
  // navigations in a popup.
  ShellAddedObserver shell_observer;
  ASSERT_TRUE(
      ExecJs(current_frame_host(), JsReplace("window.open($1);", coop_url)));
  auto* popup_webcontents =
      static_cast<WebContentsImpl*>(shell_observer.GetShell()->web_contents());
  WaitForLoadStop(popup_webcontents);
  RenderFrameHostImpl* popup_rfh = popup_webcontents->GetPrimaryMainFrame();
  ASSERT_EQ(popup_rfh->GetSiteInstance(), coop_site_instance.get());

  // Navigate to a same-origin redirection url, that sets COOP and sandboxing
  // via CSP. It then redirects to a same-origin COOP page without CSP.
  ASSERT_TRUE(
      NavigateToURL(popup_webcontents, coop_and_csp_redirect_url, coop_url));
  scoped_refptr<SiteInstance> post_redirect_site_instance =
      popup_webcontents->GetPrimaryMainFrame()->GetSiteInstance();
  ASSERT_EQ(popup_webcontents->GetPrimaryMainFrame()->active_sandbox_flags(),
            network::mojom::WebSandboxFlags::kNone);

  // No BrowsingInstance swap should have happened.
  EXPECT_TRUE(coop_site_instance->IsRelatedSiteInstance(
      post_redirect_site_instance.get()));
}

// Verify that a document setting COOP + COEP and CSP: sandbox cannot live in
// the same process as a document setting COOP + COEP with the same (non-opaque)
// origin.
IN_PROC_BROWSER_TEST_P(CrossOriginOpenerPolicyBrowserTest,
                       SandboxViaCspOpaqueOriginForIsolation) {
  GURL coi_url =
      https_server()->GetURL("a.test",
                             "/set-header?"
                             "Cross-Origin-Opener-Policy: same-origin&"
                             "Cross-Origin-Embedder-Policy: require-corp");
  GURL coi_and_csp_url =
      https_server()->GetURL("a.test",
                             "/set-header?"
                             "Cross-Origin-Opener-Policy: same-origin&"
                             "Cross-Origin-Embedder-Policy: require-corp&"
                             "Content-Security-Policy: sandbox");

  // Start on the non opaque page, that does not set CSP: sandbox.
  ASSERT_TRUE(NavigateToURL(shell(), coi_url));
  RenderFrameHostImpl* main_page_rfh = current_frame_host();

  // Open a popup with the same characteristics, but with CSP: sandbox.
  ShellAddedObserver shell_observer;
  ASSERT_TRUE(
      ExecJs(main_page_rfh, JsReplace("window.open($1)", coi_and_csp_url)));
  WebContents* popup_webcontents = shell_observer.GetShell()->web_contents();
  WaitForLoadStop(popup_webcontents);
  RenderFrameHostImpl* popup_rfh =
      static_cast<WebContentsImpl*>(popup_webcontents)->GetPrimaryMainFrame();
  ASSERT_EQ(popup_rfh->active_sandbox_flags(),
            network::mojom::WebSandboxFlags::kAll);
  ASSERT_NE(main_page_rfh->GetLastCommittedOrigin(),
            popup_rfh->GetLastCommittedOrigin());
  ASSERT_TRUE(main_page_rfh->GetSiteInstance()->IsCrossOriginIsolated());
  ASSERT_TRUE(popup_rfh->GetSiteInstance()->IsCrossOriginIsolated());

  // They should be in different BrowsingInstances and processes.
  EXPECT_FALSE(main_page_rfh->GetSiteInstance()->IsRelatedSiteInstance(
      popup_rfh->GetSiteInstance()));
  EXPECT_NE(main_page_rfh->GetSiteInstance()->GetProcess(),
            popup_rfh->GetSiteInstance()->GetProcess());
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
      "a.test", "/cross-origin-opener-policy_redirect_initial.html"));
  GURL redirect_final_page(https_server()->GetURL(
      "a.test", "/cross-origin-opener-policy_redirect_final.html"));

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

  GURL non_coop_page(embedded_test_server()->GetURL("a.test", "/title1.html"));
  GURL coop_page = embedded_test_server()->GetURL(
      "a.test", "/set-header?Cross-Origin-Opener-Policy: same-origin");

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

  ASSERT_TRUE(console_observer.Wait());
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
    const frame = document.createElement('iframe');
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
  RenderFrameHostImpl* popup_rfh = popup_webcontents->GetPrimaryMainFrame();

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
  popup_rfh = popup_webcontents->GetPrimaryMainFrame();

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
  GURL url_b(embedded_test_server()->GetURL("b.test", "/empty.html"));
  ASSERT_TRUE(NavigateToURL(shell(), coop_url));

  // Create child frame.
  ASSERT_TRUE(ExecJs(current_frame_host(), JsReplace(R"(
    const frame = document.createElement('iframe');
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
  RenderFrameHostImpl* popup_rfh = popup_webcontents->GetPrimaryMainFrame();

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
  popup_rfh = popup_webcontents->GetPrimaryMainFrame();

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
  GURL non_coop_page(https_server()->GetURL("a.test", "/title1.html"));
  GURL coop_page = https_server()->GetURL(
      "a.test", "/set-header?Cross-Origin-Opener-Policy: same-origin");

  // Test a crash before the navigation.
  {
    // Navigate to a non coop page.
    EXPECT_TRUE(NavigateToURL(shell(), non_coop_page));
    scoped_refptr<SiteInstance> initial_site_instance(
        current_frame_host()->GetSiteInstance());

    // Ensure it has a RenderFrameProxyHost for another cross-site page.
    GURL non_coop_cross_site_page(
        https_server()->GetURL("b.test", "/title1.html"));
    OpenPopup(current_frame_host(), non_coop_cross_site_page, "");
    EXPECT_EQ(web_contents()
                  ->GetPrimaryMainFrame()
                  ->browsing_context_state()
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
                  ->GetPrimaryMainFrame()
                  ->browsing_context_state()
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
        https_server()->GetURL("b.test", "/title1.html"));

    // Ensure it has a RenderFrameProxyHost for another cross-site page.
    OpenPopup(current_frame_host(), non_coop_cross_site_page, "");
    EXPECT_EQ(web_contents()
                  ->GetPrimaryMainFrame()
                  ->browsing_context_state()
                  ->GetProxyCount(),
              1u);

    // Start navigating to a COOP page.
    TestNavigationManager coop_navigation(web_contents(), coop_page);
    shell()->LoadURL(coop_page);
    if (ShouldCreateNewHostForAllFrames()) {
      // If the feature ResumeNavigationWithSpeculativeRFHProcessGone is
      // enabled, the result of the navigation with a process kill depends on
      // whether the navigation has reached the response stage. We use
      // WaitForResponse rather than WaitForSpeculativeRenderFrameHostCreation
      // to deflake the test.
      EXPECT_TRUE(coop_navigation.WaitForResponse());
    } else {
      EXPECT_TRUE(coop_navigation.WaitForRequestStart());
    }

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
    ASSERT_TRUE(coop_navigation.WaitForNavigationFinished());

    // The navigation will fail if we create speculative RFH when the navigation
    // started (instead of only when the response started), because the renderer
    // process will crash and trigger deletion of the speculative RFH and the
    // navigation using that speculative RFH.
    // TODO(crbug.com/40261276): If the final RenderFrameHost picked for
    // the navigation doesn't use the same process as the crashed process, we
    // can crash the process after the final RenderFrameHost has been picked
    // instead, and the navigation will commit normally.
    if (ShouldCreateNewHostForAllFrames()) {
      EXPECT_FALSE(coop_navigation.was_committed());
      return;
    }

    EXPECT_TRUE(coop_navigation.was_successful());
    EXPECT_FALSE(current_frame_host()->GetSiteInstance()->IsRelatedSiteInstance(
        initial_site_instance.get()));
    EXPECT_EQ(current_frame_host()->cross_origin_opener_policy(),
              CoopSameOrigin());

    // The COOP page should no longer have any RenderFrameHostProxies.
    EXPECT_EQ(web_contents()
                  ->GetPrimaryMainFrame()
                  ->browsing_context_state()
                  ->GetProxyCount(),
              0u);
  }
}

IN_PROC_BROWSER_TEST_P(CrossOriginOpenerPolicyBrowserTest,
                       CoopPageCrashIntoNonCoop) {
  IsolateAllSitesForTesting(base::CommandLine::ForCurrentProcess());
  GURL coop_allow_popups_page(https_server()->GetURL(
      "a.test",
      "/set-header?Cross-Origin-Opener-Policy: same-origin-allow-popups"));
  GURL non_coop_page(https_server()->GetURL(
      "a.test", "/set-header?Cross-Origin-Opener-Policy: unsafe-none"));
  GURL cross_origin_non_coop_page(
      https_server()->GetURL("b.test", "/title1.html"));
  // Test a crash before the navigation.
  {
    // Navigate to a COOP page.
    EXPECT_TRUE(NavigateToURL(shell(), coop_allow_popups_page));
    scoped_refptr<SiteInstance> initial_site_instance(
        current_frame_host()->GetSiteInstance());

    // Ensure it has a RenderFrameProxyHost for another cross-site page.
    OpenPopup(current_frame_host(), cross_origin_non_coop_page, "");
    EXPECT_EQ(web_contents()
                  ->GetPrimaryMainFrame()
                  ->browsing_context_state()
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
                  ->GetPrimaryMainFrame()
                  ->browsing_context_state()
                  ->GetProxyCount(),
              0u);
  }

  // Test a crash during the navigation.
  {
    // Navigate to a COOP page.
    EXPECT_TRUE(NavigateToURL(shell(), coop_allow_popups_page));
    scoped_refptr<SiteInstance> initial_site_instance(
        current_frame_host()->GetSiteInstance());

    // Ensure it has a RenderFrameProxyHost for another cross-site page.
    OpenPopup(current_frame_host(), cross_origin_non_coop_page, "");
    EXPECT_EQ(web_contents()
                  ->GetPrimaryMainFrame()
                  ->browsing_context_state()
                  ->GetProxyCount(),
              1u);

    // Start navigating to a non COOP page.
    TestNavigationManager non_coop_navigation(web_contents(), non_coop_page);
    shell()->LoadURL(non_coop_page);
    if (ShouldCreateNewHostForAllFrames()) {
      // If the feature ResumeNavigationWithSpeculativeRFHProcessGone is
      // enabled, the result of the navigation with a process kill depends on
      // whether the navigation has reached the response stage. We use
      // WaitForResponse rather than WaitForSpeculativeRenderFrameHostCreation
      // to deflake the test.
      EXPECT_TRUE(non_coop_navigation.WaitForResponse());
    } else {
      EXPECT_TRUE(non_coop_navigation.WaitForRequestStart());
    }

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
    ASSERT_TRUE(non_coop_navigation.WaitForNavigationFinished());

    // The navigation will fail if we create speculative RFH when the navigation
    // started (instead of only when the response started), because the renderer
    // process will crash and trigger deletion of the speculative RFH and the
    // navigation using that speculative RFH.
    // TODO(crbug.com/40261276): If the final RenderFrameHost picked for
    // the navigation doesn't use the same process as the crashed process, we
    // can crash the process after the final RenderFrameHost has been picked
    // instead, and the navigation will commit normally.
    if (ShouldCreateNewHostForAllFrames()) {
      EXPECT_FALSE(non_coop_navigation.was_committed());
      return;
    }

    EXPECT_TRUE(non_coop_navigation.was_successful());
    EXPECT_FALSE(current_frame_host()->GetSiteInstance()->IsRelatedSiteInstance(
        initial_site_instance.get()));
    EXPECT_EQ(current_frame_host()->cross_origin_opener_policy(),
              CoopUnsafeNone());

    // The non COOP page should no longer have any RenderFrameHostProxies.
    EXPECT_EQ(web_contents()
                  ->GetPrimaryMainFrame()
                  ->browsing_context_state()
                  ->GetProxyCount(),
              0u);
  }
}

IN_PROC_BROWSER_TEST_P(CrossOriginOpenerPolicyBrowserTest,
                       CoopPageCrashIntoCoop) {
  IsolateAllSitesForTesting(base::CommandLine::ForCurrentProcess());
  GURL coop_allow_popups_page(https_server()->GetURL(
      "a.test",
      "/set-header?Cross-Origin-Opener-Policy: same-origin-allow-popups"));
  GURL cross_origin_non_coop_page(
      https_server()->GetURL("b.test", "/title1.html"));

  // Test a crash before the navigation.
  {
    // Navigate to a COOP page.
    EXPECT_TRUE(NavigateToURL(shell(), coop_allow_popups_page));
    scoped_refptr<SiteInstance> initial_site_instance(
        current_frame_host()->GetSiteInstance());
    EXPECT_EQ(current_frame_host()->cross_origin_opener_policy(),
              CoopSameOriginAllowPopups());

    // Ensure it has a RenderFrameProxyHost for another cross-site page.
    OpenPopup(current_frame_host(), cross_origin_non_coop_page, "");

    EXPECT_EQ(web_contents()
                  ->GetPrimaryMainFrame()
                  ->browsing_context_state()
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
                  ->GetPrimaryMainFrame()
                  ->browsing_context_state()
                  ->GetProxyCount(),
              1u);
  }

  // Test a crash during the navigation.
  {
    // Navigate to a COOP page.
    EXPECT_TRUE(NavigateToURL(shell(), coop_allow_popups_page));
    scoped_refptr<SiteInstance> initial_site_instance(
        current_frame_host()->GetSiteInstance());

    // Ensure it has a RenderFrameProxyHost for another cross-site page.
    OpenPopup(current_frame_host(), cross_origin_non_coop_page, "");
    EXPECT_EQ(web_contents()
                  ->GetPrimaryMainFrame()
                  ->browsing_context_state()
                  ->GetProxyCount(),
              1u);

    // Start navigating to a COOP page.
    TestNavigationManager coop_navigation(web_contents(),
                                          coop_allow_popups_page);
    shell()->LoadURL(coop_allow_popups_page);
    if (ShouldCreateNewHostForAllFrames()) {
      // If the feature ResumeNavigationWithSpeculativeRFHProcessGone is
      // enabled, the result of the navigation with a process kill depends on
      // whether the navigation has reached the response stage. We use
      // WaitForResponse rather than WaitForSpeculativeRenderFrameHostCreation
      // to deflake the test.
      EXPECT_TRUE(coop_navigation.WaitForResponse());
    } else {
      EXPECT_TRUE(coop_navigation.WaitForRequestStart());
    }

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
    ASSERT_TRUE(coop_navigation.WaitForNavigationFinished());

    // The navigation will fail if we create speculative RFH when the navigation
    // started (instead of only when the response started), because the renderer
    // process will crash and trigger deletion of the speculative RFH and the
    // navigation using that speculative RFH.
    // TODO(crbug.com/40261276): If the final RenderFrameHost picked for
    // the navigation doesn't use the same process as the crashed process, we
    // can crash the process after the final RenderFrameHost has been picked
    // instead, and the navigation will commit normally.
    if (ShouldCreateNewHostForAllFrames()) {
      EXPECT_FALSE(coop_navigation.was_committed());
    } else {
      EXPECT_TRUE(coop_navigation.was_committed());
    }

    EXPECT_TRUE(current_frame_host()->GetSiteInstance()->IsRelatedSiteInstance(
        initial_site_instance.get()));
    EXPECT_EQ(current_frame_host()->cross_origin_opener_policy(),
              CoopSameOriginAllowPopups());

    EXPECT_EQ(web_contents()
                  ->GetPrimaryMainFrame()
                  ->browsing_context_state()
                  ->GetProxyCount(),
              1u);
  }
}

// This test is a reproducer for https://crbug.com/1264104.
IN_PROC_BROWSER_TEST_P(CrossOriginOpenerPolicyBrowserTest,
                       BackNavigationCoiToNonCoiAfterCrashReproducer) {
  GURL isolated_page(
      https_server()->GetURL("a.test",
                             "/set-header?"
                             "Cross-Origin-Opener-Policy: same-origin&"
                             "Cross-Origin-Embedder-Policy: require-corp"));
  GURL non_isolated_page(https_server()->GetURL("a.test", "/title1.html"));

  // Put a non isolated page in history.
  EXPECT_TRUE(NavigateToURL(shell(), non_isolated_page));
  scoped_refptr<SiteInstanceImpl> non_isolated_site_instance(
      current_frame_host()->GetSiteInstance());
  RenderFrameHostImplWrapper non_isolated_rfh(current_frame_host());
  EXPECT_FALSE(non_isolated_site_instance->IsCrossOriginIsolated());

  // Keep this alive, simulating not receiving the UnloadACK from the renderer.
  current_frame_host()->DoNotDeleteForTesting();

  // Navigate to an isolated page.
  EXPECT_TRUE(NavigateToURL(shell(), isolated_page));
  scoped_refptr<SiteInstanceImpl> isolated_site_instance(
      current_frame_host()->GetSiteInstance());
  EXPECT_TRUE(isolated_site_instance->IsCrossOriginIsolated());

  // Simulate the renderer process crashing.
  RenderProcessHost* process = isolated_site_instance->GetProcess();
  ASSERT_TRUE(process);
  std::unique_ptr<RenderProcessHostWatcher> crash_observer(
      new RenderProcessHostWatcher(
          process, RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT));
  process->Shutdown(0);
  crash_observer->Wait();
  crash_observer.reset();

  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(web_contents()));
}

IN_PROC_BROWSER_TEST_P(CrossOriginOpenerPolicyBrowserTest,
                       ProxiesAreRemovedWhenCrossingCoopBoundary) {
  GURL non_coop_page(https_server()->GetURL("a.test", "/title1.html"));
  GURL coop_page = https_server()->GetURL(
      "a.test", "/set-header?Cross-Origin-Opener-Policy: same-origin");

  RenderFrameHostManager* main_window_rfhm =
      web_contents()->GetPrimaryFrameTree().root()->render_manager();
  EXPECT_TRUE(NavigateToURL(shell(), non_coop_page));
  EXPECT_EQ(main_window_rfhm->current_frame_host()
                ->browsing_context_state()
                ->GetProxyCount(),
            0u);

  Shell* popup_shell = OpenPopup(shell(), coop_page, "");

  // The main frame should not have the popup referencing it.
  EXPECT_EQ(main_window_rfhm->current_frame_host()
                ->browsing_context_state()
                ->GetProxyCount(),
            0u);

  // It should not have any other related SiteInstance.
  EXPECT_EQ(
      current_frame_host()->GetSiteInstance()->GetRelatedActiveContentsCount(),
      1u);

  // The popup should not have the main frame referencing it.
  FrameTreeNode* popup =
      static_cast<WebContentsImpl*>(popup_shell->web_contents())
          ->GetPrimaryFrameTree()
          .root();
  RenderFrameHostManager* popup_rfhm = popup->render_manager();
  EXPECT_EQ(popup_rfhm->current_frame_host()
                ->browsing_context_state()
                ->GetProxyCount(),
            0u);

  // The popup should have an empty opener.
  EXPECT_FALSE(popup->opener());
}

IN_PROC_BROWSER_TEST_P(CrossOriginOpenerPolicyBrowserTest,
                       ProxiesAreKeptWhenNavigatingFromCoopToCoop) {
  IsolateAllSitesForTesting(base::CommandLine::ForCurrentProcess());
  GURL coop_page = https_server()->GetURL(
      "a.test", "/set-header?Cross-Origin-Opener-Policy: same-origin");

  // Navigate to a COOP page.
  EXPECT_TRUE(NavigateToURL(shell(), coop_page));
  scoped_refptr<SiteInstance> initial_site_instance(
      current_frame_host()->GetSiteInstance());

  // Ensure it has a RenderFrameProxyHost for another cross-site page.
  Shell* popup_shell = OpenPopup(current_frame_host(), coop_page, "");
  GURL cross_site_iframe(https_server()->GetURL("b.test", "/title1.html"));
  TestNavigationManager iframe_navigation(popup_shell->web_contents(),
                                          cross_site_iframe);
  EXPECT_TRUE(
      ExecJs(popup_shell->web_contents(),
             JsReplace("const iframe = document.createElement('iframe');"
                       "iframe.src = $1;"
                       "document.body.appendChild(iframe);",
                       cross_site_iframe)));
  ASSERT_TRUE(iframe_navigation.WaitForNavigationFinished());
  EXPECT_EQ(web_contents()
                ->GetPrimaryMainFrame()
                ->browsing_context_state()
                ->GetProxyCount(),
            1u);

  // Navigate to a COOP page.
  EXPECT_TRUE(NavigateToURL(shell(), coop_page));

  // The COOP page should still have a RenderFrameProxyHost.
  EXPECT_EQ(web_contents()
                ->GetPrimaryMainFrame()
                ->browsing_context_state()
                ->GetProxyCount(),
            1u);
}

IN_PROC_BROWSER_TEST_P(CrossOriginOpenerPolicyBrowserTest,
                       IsolateInNewProcessDespiteLimitReached) {
  // Set a process limit of 1 for testing.
  RenderProcessHostImpl::SetMaxRendererProcessCount(1);

  // Navigate to a starting page.
  GURL starting_page(https_server()->GetURL("a.test", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), starting_page));

  // Open a popup with CrossOriginOpenerPolicy and CrossOriginEmbedderPolicy
  // set.
  GURL url_openee =
      https_server()->GetURL("a.test",
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
            popup_webcontents->GetPrimaryMainFrame()->GetProcess());
}

IN_PROC_BROWSER_TEST_P(CrossOriginOpenerPolicyBrowserTest,
                       NoProcessReuseForCOOPProcesses) {
  // Set a process limit of 1 for testing.
  RenderProcessHostImpl::SetMaxRendererProcessCount(1);

  // Navigate to a starting page with CrossOriginOpenerPolicy and
  // CrossOriginEmbedderPolicy set.
  GURL starting_page =
      https_server()->GetURL("a.test",
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
            popup_webcontents->GetPrimaryMainFrame()->GetProcess());

  // Navigate to a new page without COOP and COEP. Because of process reuse, it
  // is placed in the popup process.
  GURL final_page(https_server()->GetURL("a.test", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), final_page));
  EXPECT_EQ(current_frame_host()->GetProcess(),
            popup_webcontents->GetPrimaryMainFrame()->GetProcess());
}

IN_PROC_BROWSER_TEST_P(CrossOriginOpenerPolicyBrowserTest,
                       SpeculativeRfhsAndCoop) {
  GURL non_coop_page(https_server()->GetURL("a.test", "/title1.html"));
  GURL coop_page =
      https_server()->GetURL("a.test",
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
    if (ShouldCreateNewHostForAllFrames()) {
      non_coop_navigation.WaitForSpeculativeRenderFrameHostCreation();
    } else {
      EXPECT_TRUE(non_coop_navigation.WaitForRequestStart());
    }

    // A speculative RenderFrameHost will only be created if we always use a new
    // RenderFrameHost for all cross-document navigations.
    EXPECT_EQ(ShouldCreateNewHostForAllFrames(),
              !!web_contents()
                    ->GetPrimaryFrameTree()
                    .root()
                    ->render_manager()
                    ->speculative_frame_host());

    ASSERT_TRUE(non_coop_navigation.WaitForNavigationFinished());

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
    if (CanSameSiteMainFrameNavigationsChangeRenderFrameHosts()) {
      coop_navigation.WaitForSpeculativeRenderFrameHostCreation();
    }

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

    ASSERT_TRUE(coop_navigation.WaitForNavigationFinished());

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
    if (CanSameSiteMainFrameNavigationsChangeRenderFrameHosts()) {
      non_coop_navigation.WaitForSpeculativeRenderFrameHostCreation();
    }

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

    ASSERT_TRUE(non_coop_navigation.WaitForNavigationFinished());

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
    if (ShouldCreateNewHostForAllFrames()) {
      coop_navigation.WaitForSpeculativeRenderFrameHostCreation();
    }

    // A speculative RenderFrameHost will only be created if we always use a new
    // RenderFrameHost for all cross-document navigations.
    EXPECT_EQ(ShouldCreateNewHostForAllFrames(),
              !!web_contents()
                    ->GetPrimaryFrameTree()
                    .root()
                    ->render_manager()
                    ->speculative_frame_host());

    ASSERT_TRUE(coop_navigation.WaitForNavigationFinished());

    EXPECT_TRUE(current_frame_host()->GetSiteInstance()->IsRelatedSiteInstance(
        initial_site_instance.get()));
    EXPECT_EQ(
        current_frame_host()->cross_origin_opener_policy().value,
        network::mojom::CrossOriginOpenerPolicyValue::kSameOriginPlusCoep);
  }
}

// https://crbug.com/1266819 suggested that navigating to a cross-origin page
// from a cross-origin isolated page is a good reproducer for potential
// speculative RFHs + crossOriginIsolated issues. Tests from both a regular and
// a crashed frame to also verify with the crash optimization commit.
IN_PROC_BROWSER_TEST_P(CrossOriginOpenerPolicyBrowserTest,
                       SpeculativeSiteInstanceAndCrossOriginIsolation) {
  GURL coop_page_a =
      https_server()->GetURL("a.test",
                             "/set-header?"
                             "Cross-Origin-Opener-Policy: same-origin&"
                             "Cross-Origin-Embedder-Policy: require-corp");
  GURL page_b(https_server()->GetURL("b.test", "/title1.html"));

  // Usual navigation.
  {
    // Start on a COI page.
    EXPECT_TRUE(NavigateToURL(shell(), coop_page_a));
    scoped_refptr<SiteInstanceImpl> main_site_instance(
        current_frame_host()->GetSiteInstance());
    EXPECT_TRUE(main_site_instance->IsCrossOriginIsolated());

    // Popup to a cross-origin page.
    ShellAddedObserver shell_observer;
    EXPECT_TRUE(ExecJs(current_frame_host(),
                       JsReplace("window.open($1, 'windowName')", page_b)));
    WebContents* popup = shell_observer.GetShell()->web_contents();
    WaitForLoadStop(popup);

    RenderFrameHostImpl* popup_frame_host = static_cast<WebContentsImpl*>(popup)
                                                ->GetPrimaryFrameTree()
                                                .root()
                                                ->current_frame_host();
    scoped_refptr<SiteInstanceImpl> popup_site_instance(
        popup_frame_host->GetSiteInstance());
    EXPECT_FALSE(popup_site_instance->IsCrossOriginIsolated());

    // Verify that COOP enforcement was done properly.
    EXPECT_FALSE(
        main_site_instance->IsRelatedSiteInstance(popup_site_instance.get()));
    EXPECT_EQ(true, EvalJs(popup_frame_host, "window.opener == null;"));
    EXPECT_EQ("", EvalJs(popup_frame_host, "window.name"));
    popup->Close();
  }

  // Navigation from a crashed page.
  {
    // Start on a COI page.
    EXPECT_TRUE(NavigateToURL(shell(), coop_page_a));
    scoped_refptr<SiteInstanceImpl> main_site_instance(
        current_frame_host()->GetSiteInstance());
    EXPECT_TRUE(main_site_instance->IsCrossOriginIsolated());

    // Open an empty popup.
    ShellAddedObserver shell_observer;
    EXPECT_TRUE(ExecJs(current_frame_host(),
                       "window.open('about:blank', 'windowName')"));
    WebContents* popup = shell_observer.GetShell()->web_contents();
    WaitForLoadStop(popup);
    RenderFrameHostImpl* popup_frame_host = static_cast<WebContentsImpl*>(popup)
                                                ->GetPrimaryFrameTree()
                                                .root()
                                                ->current_frame_host();
    scoped_refptr<SiteInstanceImpl> popup_site_instance(
        popup_frame_host->GetSiteInstance());

    // Crash it.
    {
      RenderProcessHost* process = popup_site_instance->GetProcess();
      ASSERT_TRUE(process);
      auto crash_observer = std::make_unique<RenderProcessHostWatcher>(
          process, RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
      process->Shutdown(0);
      crash_observer->Wait();
    }

    // Navigate it to a cross-origin page.
    EXPECT_TRUE(NavigateToURL(popup, page_b));
    WaitForLoadStop(popup);
    popup_frame_host = static_cast<WebContentsImpl*>(popup)
                           ->GetPrimaryFrameTree()
                           .root()
                           ->current_frame_host();
    popup_site_instance = popup_frame_host->GetSiteInstance();
    EXPECT_FALSE(popup_site_instance->IsCrossOriginIsolated());

    // Verify that COOP enforcement was done properly.
    EXPECT_FALSE(
        main_site_instance->IsRelatedSiteInstance(popup_site_instance.get()));
    EXPECT_EQ(true, EvalJs(popup_frame_host, "window.opener == null;"));
    EXPECT_EQ("", EvalJs(popup_frame_host, "window.name"));
    popup->Close();
  }
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
          https_server()->GetURL("a.test", "/title1.html"),
          https_server()->GetURL("a.test", "/title2.html"),
          false,
      },
      {
          // different-origin => keep.
          https_server()->GetURL("a.a.test", "/title1.html"),
          https_server()->GetURL("b.a.test", "/title2.html"),
          false,
      },
      {
          // different-site => keep.
          https_server()->GetURL("a.test", "/title1.html"),
          https_server()->GetURL("b.test", "/title2.html"),
          false,
      },

      // non-coop <-> coop.
      {
          // same-origin => change.
          https_server()->GetURL("a.test", "/title1.html"),
          https_server()->GetURL("a.test",
                                 "/set-header?"
                                 "Cross-Origin-Opener-Policy: same-origin&"
                                 "Cross-Origin-Embedder-Policy: require-corp"),
          true,
      },
      {
          // different-origin => change.
          https_server()->GetURL("a.a.test", "/title1.html"),
          https_server()->GetURL("b.a.test",
                                 "/set-header?"
                                 "Cross-Origin-Opener-Policy: same-origin&"
                                 "Cross-Origin-Embedder-Policy: require-corp"),
          true,
      },
      {
          // different-site => change.
          https_server()->GetURL("a.test", "/title1.html"),
          https_server()->GetURL("b.test",
                                 "/set-header?"
                                 "Cross-Origin-Opener-Policy: same-origin&"
                                 "Cross-Origin-Embedder-Policy: require-corp"),
          true,
      },

      // coop <-> coop.
      {
          // same-origin => keep.
          https_server()->GetURL("a.test",
                                 "/set-header?"
                                 "Cross-Origin-Opener-Policy: same-origin&"
                                 "Cross-Origin-Embedder-Policy: require-corp"),
          https_server()->GetURL("a.test",
                                 "/set-header?"
                                 "Cross-Origin-Opener-Policy: same-origin&"
                                 "Cross-Origin-Embedder-Policy: require-corp"),
          false,
      },
      {
          // different-origin => change.
          https_server()->GetURL("a.a.test",
                                 "/set-header?"
                                 "Cross-Origin-Opener-Policy: same-origin&"
                                 "Cross-Origin-Embedder-Policy: require-corp"),
          https_server()->GetURL("b.a.test",
                                 "/set-header?"
                                 "Cross-Origin-Opener-Policy: same-origin&"
                                 "Cross-Origin-Embedder-Policy: require-corp"),
          true,
      },
      {
          // different-site => keep.
          https_server()->GetURL("a.test",
                                 "/set-header?"
                                 "Cross-Origin-Opener-Policy: same-origin&"
                                 "Cross-Origin-Embedder-Policy: require-corp"),
          https_server()->GetURL("b.test",
                                 "/set-header?"
                                 "Cross-Origin-Opener-Policy: same-origin&"
                                 "Cross-Origin-Embedder-Policy: require-corp"),
          true,
      },

      // non-coop <-> coop-ro.
      {
          // same-origin => change.
          https_server()->GetURL("a.test", "/title1.html"),
          https_server()->GetURL(
              "a.test",
              "/set-header?"
              "Cross-Origin-Opener-Policy-Report-Only: same-origin&"
              "Cross-Origin-Embedder-Policy: require-corp"),
          true,
      },
      {
          // different-origin => change.
          https_server()->GetURL("a.a.test", "/title1.html"),
          https_server()->GetURL(
              "b.a.test",
              "/set-header?"
              "Cross-Origin-Opener-Policy-Report-Only: same-origin&"
              "Cross-Origin-Embedder-Policy: require-corp"),
          true,
      },
      {
          // different-site => change.
          https_server()->GetURL("a.test", "/title1.html"),
          https_server()->GetURL(
              "b.test",
              "/set-header?"
              "Cross-Origin-Opener-Policy-Report-Only: same-origin&"
              "Cross-Origin-Embedder-Policy: require-corp"),
          true,
      },

      // coop-ro <-> coop-ro.
      {
          // same-origin => keep.
          https_server()->GetURL(
              "a.test",
              "/set-header?"
              "Cross-Origin-Opener-Policy-Report-Only: same-origin&"
              "Cross-Origin-Embedder-Policy: require-corp"),
          https_server()->GetURL(
              "a.test",
              "/set-header?"
              "Cross-Origin-Opener-Policy-Report-Only: same-origin&"
              "Cross-Origin-Embedder-Policy: require-corp"),
          false,
      },
      {
          // different-origin => change.
          https_server()->GetURL(
              "a.a.test",
              "/set-header?"
              "Cross-Origin-Opener-Policy-Report-Only: same-origin&"
              "Cross-Origin-Embedder-Policy: require-corp"),
          https_server()->GetURL(
              "b.a.test",
              "/set-header?"
              "Cross-Origin-Opener-Policy-Report-Only: same-origin&"
              "Cross-Origin-Embedder-Policy: require-corp"),
          true,
      },
      {
          // different-site => keep.
          https_server()->GetURL(
              "a.test",
              "/set-header?"
              "Cross-Origin-Opener-Policy-Report-Only: same-origin&"
              "Cross-Origin-Embedder-Policy: require-corp"),
          https_server()->GetURL(
              "b.test",
              "/set-header?"
              "Cross-Origin-Opener-Policy-Report-Only: same-origin&"
              "Cross-Origin-Embedder-Policy: require-corp"),
          true,
      },

      // coop <-> coop-ro.
      {
          // same-origin => change.
          https_server()->GetURL("a.test",
                                 "/set-header?"
                                 "Cross-Origin-Opener-Policy: same-origin&"
                                 "Cross-Origin-Embedder-Policy: require-corp"),
          https_server()->GetURL(
              "a.test",
              "/set-header?"
              "Cross-Origin-Opener-Policy-Report-Only: same-origin&"
              "Cross-Origin-Embedder-Policy: require-corp"),
          true,
      },
      {
          // different-origin => change.
          https_server()->GetURL("a.a.test",
                                 "/set-header?"
                                 "Cross-Origin-Opener-Policy: same-origin&"
                                 "Cross-Origin-Embedder-Policy: require-corp"),
          https_server()->GetURL(
              "b.a.test",
              "/set-header?"
              "Cross-Origin-Opener-Policy-Report-Only: same-origin&"
              "Cross-Origin-Embedder-Policy: require-corp"),
          true,
      },
      {
          // different-site => change
          https_server()->GetURL("a.test",
                                 "/set-header?"
                                 "Cross-Origin-Opener-Policy: same-origin&"
                                 "Cross-Origin-Embedder-Policy: require-corp"),
          https_server()->GetURL(
              "b.test",
              "/set-header?"
              "Cross-Origin-Opener-Policy-Report-Only: same-origin&"
              "Cross-Origin-Embedder-Policy: require-corp"),
          true,
      },
      // TODO(crbug.com/40138297). Test with COEP-RO.
      // TODO(crbug.com/40138297). Test with COOP-RO+COOP.
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
          https_server()->GetURL("a.test", "/title1.html"),
          GURL(),
          false,
      },
      {
          // From coop-ro.
          https_server()->GetURL(
              "a.test",
              "/set-header?"
              "Cross-Origin-Opener-Policy-Report-Only: same-origin&"
              "Cross-Origin-Embedder-Policy: require-corp"),
          GURL(),
          false,
      },
      {
          // From coop.
          https_server()->GetURL("a.test",
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
          https_server()->GetURL("a.test", "/title1.html"),
          https_server()->GetURL("a.test", "/title1.html"),
          false,
      },

      // non-coop opens coop-ro.
      {
          https_server()->GetURL("a.test", "/title1.html"),
          https_server()->GetURL(
              "a.test",
              "/set-header?"
              "Cross-Origin-Opener-Policy-Report-Only: same-origin&"
              "Cross-Origin-Embedder-Policy: require-corp"),
          true,
      },

      // non-coop opens coop.
      {
          https_server()->GetURL("a.test", "/title1.html"),
          https_server()->GetURL("a.test",
                                 "/set-header?"
                                 "Cross-Origin-Opener-Policy: same-origin&"
                                 "Cross-Origin-Embedder-Policy: require-corp"),
          true,
      },

      // coop opens non-coop.
      {
          https_server()->GetURL("a.test",
                                 "/set-header?"
                                 "Cross-Origin-Opener-Policy: same-origin&"
                                 "Cross-Origin-Embedder-Policy: require-corp"),
          https_server()->GetURL("a.test", "/title1.html"),
          true,
      },

      // coop-ro opens coop-ro (same-origin).
      {
          https_server()->GetURL(
              "a.test",
              "/set-header?"
              "Cross-Origin-Opener-Policy-Report-Only: same-origin&"
              "Cross-Origin-Embedder-Policy: require-corp"),
          https_server()->GetURL(
              "a.test",
              "/set-header?"
              "Cross-Origin-Opener-Policy-Report-Only: same-origin&"
              "Cross-Origin-Embedder-Policy: require-corp"),
          false,
      },

      // coop-ro opens coop-ro (different-origin).
      {
          https_server()->GetURL(
              "a.test",
              "/set-header?"
              "Cross-Origin-Opener-Policy-Report-Only: same-origin&"
              "Cross-Origin-Embedder-Policy: require-corp"),
          https_server()->GetURL(
              "b.test",
              "/set-header?"
              "Cross-Origin-Opener-Policy-Report-Only: same-origin&"
              "Cross-Origin-Embedder-Policy: require-corp"),
          true,
      },

      // TODO(crbug.com/40138297). Test with COEP-RO.
      // TODO(crbug.com/40138297). Test with COOP-RO+COOP
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

    if (test_case.expect_different_virtual_browsing_context_group) {
      EXPECT_NE(group_opener, group_openee);
    } else {
      EXPECT_EQ(group_opener, group_openee);
    }

    popup->Close();
  }
}

namespace {
// Use two URLs, |url_a| and |url_b|. One of them at least uses
// COOP:same-origin-allow-popups, or COOP-Report-Only:same-origin-allow-popups,
// or both (unless soap_by_default is true).
//
// Test two scenario:
// 1. From |url_a|, opens |url_b|
// 2. From |url_a|, navigates to |url_b|.
//
// In both cases, check whether a new virtual browsing context group has been
// used or not.
//
// If soap_by_default is true, then the test will check the soap by default
// virtual browsing context group.
struct VirtualBcgAllowPopupTestCase {
  GURL url_a;
  GURL url_b;
  bool expect_different_group_window_open;
  bool expect_different_group_navigation;
  int (*get_virtual_browsing_context_group)(WebContents*);
};

void RunTest(const VirtualBcgAllowPopupTestCase& test_case, Shell* shell) {
  SCOPED_TRACE(testing::Message()
               << std::endl
               << "url_a = " << test_case.url_a << std::endl
               << "url_b = " << test_case.url_b << std::endl);
  ASSERT_TRUE(NavigateToURL(shell, test_case.url_a));
  int group_initial =
      test_case.get_virtual_browsing_context_group(shell->web_contents());

  ShellAddedObserver shell_observer;
  EXPECT_TRUE(ExecJs(shell->web_contents()->GetPrimaryMainFrame(),
                     JsReplace("window.open($1)", test_case.url_b)));
  WebContents* popup = shell_observer.GetShell()->web_contents();
  WaitForLoadStop(popup);
  int group_openee = test_case.get_virtual_browsing_context_group(popup);

  ASSERT_TRUE(NavigateToURL(shell, test_case.url_b));
  int group_navigate =
      test_case.get_virtual_browsing_context_group(shell->web_contents());

  if (test_case.expect_different_group_window_open) {
    EXPECT_NE(group_initial, group_openee);
  } else {
    EXPECT_EQ(group_initial, group_openee);
  }

  if (test_case.expect_different_group_navigation) {
    EXPECT_NE(group_initial, group_navigate);
  } else {
    EXPECT_EQ(group_initial, group_navigate);
  }

  popup->Close();
}

}  // namespace

IN_PROC_BROWSER_TEST_P(VirtualBrowsingContextGroupTest,
                       NonCoopToCoopAllowPopup) {
  const VirtualBcgAllowPopupTestCase kTestCases[] = {
      {
          // same-origin.
          https_server()->GetURL("a.test", "/title1.html"),
          https_server()->GetURL(
              "a.test",
              "/set-header?"
              "Cross-Origin-Opener-Policy: same-origin-allow-popups&"
              "Cross-Origin-Embedder-Policy: require-corp"),
          true,
          true,
          VirtualBrowsingContextGroup,
      },
      {
          // cross-origin.
          https_server()->GetURL("a.a.test", "/title1.html"),
          https_server()->GetURL(
              "b.a.test",
              "/set-header?"
              "Cross-Origin-Opener-Policy: same-origin-allow-popups&"
              "Cross-Origin-Embedder-Policy: require-corp"),
          true,
          true,
          VirtualBrowsingContextGroup,
      },
      {
          // cross-site.
          https_server()->GetURL("a.test", "/title1.html"),
          https_server()->GetURL(
              "b.test",
              "/set-header?"
              "Cross-Origin-Opener-Policy: same-origin-allow-popups&"
              "Cross-Origin-Embedder-Policy: require-corp"),
          true,
          true,
          VirtualBrowsingContextGroup,
      },
  };
  for (const auto& test : kTestCases) {
    RunTest(test, shell());
  }
}

// coop:same-origin-allow-popup -> coop:none.
IN_PROC_BROWSER_TEST_P(VirtualBrowsingContextGroupTest,
                       CoopAllowPopup_NonCoop) {
  const VirtualBcgAllowPopupTestCase kTestCases[] = {
      {
          // same-origin.
          https_server()->GetURL(
              "a.test",
              "/set-header?"
              "Cross-Origin-Opener-Policy: same-origin-allow-popups&"
              "Cross-Origin-Embedder-Policy: require-corp"),
          https_server()->GetURL("a.test", "/title1.html"),
          false,
          true,
          VirtualBrowsingContextGroup,
      },
      {
          // cross-origin.
          https_server()->GetURL(
              "b.a.test",
              "/set-header?"
              "Cross-Origin-Opener-Policy: same-origin-allow-popups&"
              "Cross-Origin-Embedder-Policy: require-corp"),
          https_server()->GetURL("a.a.test", "/title1.html"),
          false,
          true,
          VirtualBrowsingContextGroup,
      },
      {
          // cross-site.
          https_server()->GetURL(
              "b.test",
              "/set-header?"
              "Cross-Origin-Opener-Policy: same-origin-allow-popups&"
              "Cross-Origin-Embedder-Policy: require-corp"),
          https_server()->GetURL("a.test", "/title1.html"),
          false,
          true,
          VirtualBrowsingContextGroup,
      },
  };
  for (const auto& test : kTestCases) {
    RunTest(test, shell());
  }
}

// coop:none -> coop:same-origin-allow-popup.
IN_PROC_BROWSER_TEST_P(VirtualBrowsingContextGroupTest,
                       CoopRoAllowPopup_NonCoop) {
  const VirtualBcgAllowPopupTestCase kTestCases[] = {
      {
          // same-origin.
          https_server()->GetURL("a.test",
                                 "/set-header?"
                                 "Cross-Origin-Opener-Policy-Report-Only: "
                                 "same-origin-allow-popups&"
                                 "Cross-Origin-Embedder-Policy: require-corp"),
          https_server()->GetURL("a.test", "/title1.html"),
          false,
          true,
          VirtualBrowsingContextGroup,
      },
      {
          // cross-origin.
          https_server()->GetURL("b.a.test",
                                 "/set-header?"
                                 "Cross-Origin-Opener-Policy-Report-Only: "
                                 "same-origin-allow-popups&"
                                 "Cross-Origin-Embedder-Policy: require-corp"),
          https_server()->GetURL("a.a.test", "/title1.html"),
          false,
          true,
          VirtualBrowsingContextGroup,
      },
      {
          // cross-site.
          https_server()->GetURL("b.test",
                                 "/set-header?"
                                 "Cross-Origin-Opener-Policy-Report-Only: "
                                 "same-origin-allow-popups&"
                                 "Cross-Origin-Embedder-Policy: require-corp"),
          https_server()->GetURL("a.test", "/title1.html"),
          false,
          true,
          VirtualBrowsingContextGroup,
      },
  };
  for (const auto& test : kTestCases) {
    RunTest(test, shell());
  }
}

// coop:same-origin-allow-popup -> coop:same-origin-allow-popup.
IN_PROC_BROWSER_TEST_P(VirtualBrowsingContextGroupTest,
                       CoopAllowPopup_CoopAllowPopup) {
  const VirtualBcgAllowPopupTestCase kTestCases[] = {
      {
          // same-origin.
          https_server()->GetURL(
              "a.test",
              "/set-header?"
              "Cross-Origin-Opener-Policy: same-origin-allow-popups&"
              "Cross-Origin-Embedder-Policy: require-corp"),
          https_server()->GetURL(
              "a.test",
              "/set-header?"
              "Cross-Origin-Opener-Policy: same-origin-allow-popups&"
              "Cross-Origin-Embedder-Policy: require-corp"),
          false,
          false,
          VirtualBrowsingContextGroup,
      },
      {
          // cross-origin.
          https_server()->GetURL(
              "a.a.test",
              "/set-header?"
              "Cross-Origin-Opener-Policy: same-origin-allow-popups&"
              "Cross-Origin-Embedder-Policy: require-corp"),
          https_server()->GetURL(
              "b.a.test",
              "/set-header?"
              "Cross-Origin-Opener-Policy: same-origin-allow-popups&"
              "Cross-Origin-Embedder-Policy: require-corp"),
          true,
          true,
          VirtualBrowsingContextGroup,
      },
      {
          // cross-site.
          https_server()->GetURL(
              "a.test",
              "/set-header?"
              "Cross-Origin-Opener-Policy: same-origin-allow-popups&"
              "Cross-Origin-Embedder-Policy: require-corp"),
          https_server()->GetURL(
              "b.test",
              "/set-header?"
              "Cross-Origin-Opener-Policy: same-origin-allow-popups&"
              "Cross-Origin-Embedder-Policy: require-corp"),
          true,
          true,
          VirtualBrowsingContextGroup,
      },

  };
  for (const auto& test : kTestCases) {
    RunTest(test, shell());
  }
}

// coop:same-origin-allow-popup -> coop-ro:same-origin-allow-popup.
IN_PROC_BROWSER_TEST_P(VirtualBrowsingContextGroupTest,
                       CoopAllowPopup_CoopRoAllowPopup) {
  const VirtualBcgAllowPopupTestCase kTestCases[] = {
      {
          // same-origin.
          https_server()->GetURL("a.test",
                                 "/set-header?"
                                 "Cross-Origin-Opener-Policy: "
                                 "same-origin-allow-popups&"
                                 "Cross-Origin-Embedder-Policy: require-corp"),
          https_server()->GetURL("a.test",
                                 "/set-header?"
                                 "Cross-Origin-Opener-Policy-Report-Only: "
                                 "same-origin-allow-popups&"
                                 "Cross-Origin-Embedder-Policy: require-corp"),
          false,
          true,
          VirtualBrowsingContextGroup,
      },
      {
          // cross-origin.
          https_server()->GetURL("a.a.test",
                                 "/set-header?"
                                 "Cross-Origin-Opener-Policy: "
                                 "same-origin-allow-popups&"
                                 "Cross-Origin-Embedder-Policy: require-corp"),
          https_server()->GetURL("b.a.test",
                                 "/set-header?"
                                 "Cross-Origin-Opener-Policy-Report-Only: "
                                 "same-origin-allow-popups&"
                                 "Cross-Origin-Embedder-Policy: require-corp"),
          true,
          true,
          VirtualBrowsingContextGroup,
      },
      {
          // cross-site.
          https_server()->GetURL("a.test",
                                 "/set-header?"
                                 "Cross-Origin-Opener-Policy: "
                                 "same-origin-allow-popups&"
                                 "Cross-Origin-Embedder-Policy: require-corp"),
          https_server()->GetURL("b.test",
                                 "/set-header?"
                                 "Cross-Origin-Opener-Policy-Report-Only: "
                                 "same-origin-allow-popups&"
                                 "Cross-Origin-Embedder-Policy: require-corp"),
          true,
          true,
          VirtualBrowsingContextGroup,
      },
  };
  for (const auto& test : kTestCases) {
    RunTest(test, shell());
  }
}

// coop-ro:same-origin-allow-popup -> coop:same-origin-allow-popup.
IN_PROC_BROWSER_TEST_P(VirtualBrowsingContextGroupTest,
                       CoopRoAllowPopup_CoopAllowPopup) {
  const VirtualBcgAllowPopupTestCase kTestCases[] = {
      {
          // same-origin.
          https_server()->GetURL("a.test",
                                 "/set-header?"
                                 "Cross-Origin-Opener-Policy-Report-Only: "
                                 "same-origin-allow-popups&"
                                 "Cross-Origin-Embedder-Policy: require-corp"),
          https_server()->GetURL("a.test",
                                 "/set-header?"
                                 "Cross-Origin-Opener-Policy: "
                                 "same-origin-allow-popups&"
                                 "Cross-Origin-Embedder-Policy: require-corp"),
          true,
          true,
          VirtualBrowsingContextGroup,
      },
      {
          // cross-origin.
          https_server()->GetURL("a.a.test",
                                 "/set-header?"
                                 "Cross-Origin-Opener-Policy-Report-Only: "
                                 "same-origin-allow-popups&"
                                 "Cross-Origin-Embedder-Policy: require-corp"),
          https_server()->GetURL(
              "b.a.test",
              "/set-header?"
              "Cross-Origin-Opener-Policy: same-origin-allow-popups&"
              "Cross-Origin-Embedder-Policy: require-corp"),
          true,
          true,
          VirtualBrowsingContextGroup,
      },
      {
          // cross-site.
          https_server()->GetURL(
              "a.test",
              "/set-header?"
              "Cross-Origin-Opener-Policy: same-origin-allow-popups&"
              "Cross-Origin-Embedder-Policy: require-corp"),
          https_server()->GetURL(
              "b.test",
              "/set-header?"
              "Cross-Origin-Opener-Policy: same-origin-allow-popups&"
              "Cross-Origin-Embedder-Policy: require-corp"),
          true,
          true,
          VirtualBrowsingContextGroup,
      },
  };

  for (const auto& test : kTestCases) {
    RunTest(test, shell());
  }
}

// coop:same-origin-allow-popup + coop-ro:same-origin-allow-popup -> coop:none.
IN_PROC_BROWSER_TEST_P(VirtualBrowsingContextGroupTest,
                       CoopPopupRoSameOrigin_NonCoop) {
  const VirtualBcgAllowPopupTestCase kTestCases[] = {
      // coop:allow-popup, coop-ro:same-origin-> no-coop.
      {
          // same-origin.
          https_server()->GetURL(
              "a.test",
              "/set-header?"
              "Cross-Origin-Opener-Policy: same-origin-allow-popups&"
              "Cross-Origin-Opener-Policy-Report-Only: same-origin&"
              "Cross-Origin-Embedder-Policy: require-corp"),
          https_server()->GetURL("a.test", "/title1.html"),
          true,
          true,
          VirtualBrowsingContextGroup,
      },
      {
          // cross-origin.
          https_server()->GetURL(
              "a.a.test",
              "/set-header?"
              "Cross-Origin-Opener-Policy: same-origin-allow-popups&"
              "Cross-Origin-Opener-Policy-Report-Only: same-origin&"
              "Cross-Origin-Embedder-Policy: require-corp"),
          https_server()->GetURL("b.a.test", "/title1.html"),
          true,
          true,
          VirtualBrowsingContextGroup,
      },
      {
          // cross-site.
          https_server()->GetURL(
              "a.test",
              "/set-header?"
              "Cross-Origin-Opener-Policy: same-origin-allow-popups&"
              "Cross-Origin-Opener-Policy-Report-Only: same-origin&"
              "Cross-Origin-Embedder-Policy: require-corp"),
          https_server()->GetURL("b.test", "/title1.html"),
          true,
          true,
          VirtualBrowsingContextGroup,
      },
  };

  for (const auto& test : kTestCases) {
    RunTest(test, shell());
  }
}

// Navigates in between two pages from a different browsing context group. Then
// use the history API to navigate back and forth. Check their virtual browsing
// context group isn't restored.
// The goal is to spot differences when the BackForwardCache is enabled. See
// https://crbug.com/1109648.
IN_PROC_BROWSER_TEST_P(VirtualBrowsingContextGroupTest, HistoryNavigation) {
  GURL url_a = https_server()->GetURL(
      "a.test",
      "/set-header?"
      "Cross-Origin-Opener-Policy-Report-Only: same-origin&"
      "Cross-Origin-Embedder-Policy: require-corp");
  GURL url_b = https_server()->GetURL(
      "b.test",
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

  // TODO(crbug.com/40709606) During history navigation, the virtual
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
  GURL url_a = https_server()->GetURL("a.test", "/title1.html");
  GURL url_b = https_server()->GetURL("b.test", "/title1.html");
  GURL url_c = https_server()->GetURL(
      "c.test",
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

// A subclass for tests incompatible with OriginKeyedProcessesByDefault.
class CrossOriginOpenerPolicyNoOKPBrowserTest
    : public CrossOriginOpenerPolicyBrowserTest {
 public:
  CrossOriginOpenerPolicyNoOKPBrowserTest() {
    feature_list_
        .InitWithFeatures(/*enabled_features=*/
                          {}, /*disabled_features=*/{
                              features::kOriginKeyedProcessesByDefault});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// A test to make sure that loading a page with COOP/COEP headers doesn't set
// is_origin_keyed() on the SiteInstance's SiteInfo. This test should be run
// with OriginKeyedProcessesByDefault disabled, otherwise the SiteInfo will
// be origin-keyed regardless of COOP/COEP.
IN_PROC_BROWSER_TEST_P(CrossOriginOpenerPolicyNoOKPBrowserTest,
                       CoopCoepNotOriginKeyed) {
  GURL isolated_page(
      https_server()->GetURL("a.test",
                             "/set-header?"
                             "Cross-Origin-Opener-Policy: same-origin&"
                             "Cross-Origin-Embedder-Policy: require-corp"));

  EXPECT_TRUE(NavigateToURL(shell(), isolated_page));
  SiteInstanceImpl* current_si = current_frame_host()->GetSiteInstance();
  EXPECT_TRUE(current_si->IsCrossOriginIsolated());
  // Use of COOP/COEP headers should not cause SiteInfo::oac_status() to
  // return origin-keyed. The metrics that track OriginAgentCluster isolation
  // expect oac_status() to refer only to the OriginAgentCluster header.
  EXPECT_EQ(AgentClusterKey::OACStatus::kSiteKeyedByDefault,
            current_si->GetSiteInfo().oac_status());
}

// TODO(crbug.com/40924316): Disable flaky test in Linux.
#if BUILDFLAG(IS_LINUX)
#define MAYBE_CrossOriginIsolatedSiteInstance_MainFrame \
  DISABLED_CrossOriginIsolatedSiteInstance_MainFrame
#else
#define MAYBE_CrossOriginIsolatedSiteInstance_MainFrame \
  CrossOriginIsolatedSiteInstance_MainFrame
#endif
IN_PROC_BROWSER_TEST_P(CrossOriginOpenerPolicyBrowserTest,
                       MAYBE_CrossOriginIsolatedSiteInstance_MainFrame) {
  GURL isolated_page(
      https_server()->GetURL("a.test",
                             "/set-header?"
                             "Cross-Origin-Opener-Policy: same-origin&"
                             "Cross-Origin-Embedder-Policy: require-corp"));
  GURL isolated_page_b(
      https_server()->GetURL("cdn.a.test",
                             "/set-header?"
                             "Cross-Origin-Opener-Policy: same-origin&"
                             "Cross-Origin-Embedder-Policy: require-corp"));
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
    EXPECT_FALSE(current_si->IsRelatedSiteInstance(previous_si.get()));
    EXPECT_NE(current_si->GetProcess(),
              previous_si->GetOrCreateProcessForTesting());
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
    EXPECT_NE(current_si->GetProcess(),
              previous_si->GetOrCreateProcessForTesting());
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
    EXPECT_NE(
        non_cross_origin_isolated_site_instance->GetProcess(),
        cross_origin_isolated_site_instance->GetOrCreateProcessForTesting());
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
    EXPECT_NE(site_instance_1->GetOrCreateProcessForTesting(),
              site_instance_2->GetProcess());
  }
}

IN_PROC_BROWSER_TEST_P(
    CrossOriginOpenerPolicyBrowserTest,
    CrossOriginIsolatedSiteInstance_MainFrameRendererInitiated) {
  GURL isolated_page(
      https_server()->GetURL("a.test",
                             "/set-header?"
                             "Cross-Origin-Opener-Policy: same-origin&"
                             "Cross-Origin-Embedder-Policy: require-corp"));
  GURL isolated_page_b(
      https_server()->GetURL("cdn.a.test",
                             "/set-header?"
                             "Cross-Origin-Opener-Policy: same-origin&"
                             "Cross-Origin-Embedder-Policy: require-corp"));
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
    EXPECT_FALSE(current_si->IsRelatedSiteInstance(previous_si.get()));
    EXPECT_NE(current_si->GetProcess(),
              previous_si->GetOrCreateProcessForTesting());
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
    EXPECT_NE(current_si->GetProcess(),
              previous_si->GetOrCreateProcessForTesting());
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
    EXPECT_NE(site_instance_1->GetOrCreateProcessForTesting(),
              site_instance_2->GetProcess());
  }
}

IN_PROC_BROWSER_TEST_P(CrossOriginOpenerPolicyBrowserTest,
                       CrossOriginIsolatedSiteInstance_IFrame) {
  GURL isolated_page(
      https_server()->GetURL("a.test",
                             "/set-header?"
                             "Cross-Origin-Opener-Policy: same-origin&"
                             "Cross-Origin-Embedder-Policy: require-corp"));
  GURL isolated_page_b(
      https_server()->GetURL("cdn.a.test",
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
               JsReplace("const iframe = document.createElement('iframe'); "
                         "iframe.src = $1; "
                         "document.body.appendChild(iframe);",
                         isolated_page)));

    ASSERT_TRUE(same_origin_iframe_navigation.WaitForNavigationFinished());
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
               JsReplace("const iframe = document.createElement('iframe'); "
                         "iframe.src = $1; "
                         "document.body.appendChild(iframe);",
                         isolated_page_b)));

    ASSERT_TRUE(cross_origin_iframe_navigation.WaitForNavigationFinished());
    EXPECT_TRUE(cross_origin_iframe_navigation.was_successful());
    RenderFrameHostImpl* iframe_rfh =
        current_frame_host()->child_at(1)->current_frame_host();
    SiteInstanceImpl* iframe_si = iframe_rfh->GetSiteInstance();
    EXPECT_TRUE(iframe_si->IsCrossOriginIsolated());
    EXPECT_TRUE(iframe_si->IsRelatedSiteInstance(main_si));
    if (SiteIsolationPolicy::AreOriginKeyedProcessesEnabledByDefault()) {
      // In this case, the main frame and the child frame have different
      // origins, so when OriginKeyedProcessesByDefault is enabled they will
      // be placed into different processes.
      EXPECT_NE(iframe_si->GetProcess(), main_si->GetProcess());
    } else {
      EXPECT_EQ(iframe_si->GetProcess(), main_si->GetProcess());
    }
  }
}

IN_PROC_BROWSER_TEST_P(CrossOriginOpenerPolicyBrowserTest,
                       CrossOriginIsolatedSiteInstance_Popup) {
  GURL isolated_page(
      https_server()->GetURL("a.test",
                             "/set-header?"
                             "Cross-Origin-Opener-Policy: same-origin&"
                             "Cross-Origin-Embedder-Policy: require-corp"));
  GURL isolated_page_b(
      https_server()->GetURL("cdn.a.test",
                             "/set-header?"
                             "Cross-Origin-Opener-Policy: same-origin&"
                             "Cross-Origin-Embedder-Policy: require-corp"));
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
    EXPECT_FALSE(popup_rfh->GetSiteInstance()->IsRelatedSiteInstance(
        current_frame_host()->GetSiteInstance()));
    EXPECT_FALSE(popup_rfh->frame_tree_node()->opener());
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
      https_server()->GetURL("a.test",
                             "/set-header?"
                             "Cross-Origin-Opener-Policy: same-origin&"
                             "Cross-Origin-Embedder-Policy: require-corp"));
  GURL non_coep_page(https_server()->GetURL("b.test",
                                            "/set-header?"
                                            "Access-Control-Allow-Origin: *"));

  GURL invalid_url(
      https_server()->GetURL("a.test", "/this_page_does_not_exist.html"));

  GURL error_url(https_server()->GetURL("a.test", "/page404.html"));

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
    EXPECT_TRUE(iframe_si->IsCrossOriginIsolated());
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
    EXPECT_TRUE(iframe_si->IsCrossOriginIsolated());
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
    EXPECT_TRUE(iframe_si->IsCrossOriginIsolated());
  }

  // Top frame.
  {
    scoped_refptr<SiteInstanceImpl> previous_si =
        current_frame_host()->GetSiteInstance();
    EXPECT_FALSE(NavigateToURL(shell(), invalid_url));
    SiteInstanceImpl* current_si = current_frame_host()->GetSiteInstance();
    EXPECT_FALSE(current_si->IsRelatedSiteInstance(previous_si.get()));
    EXPECT_NE(current_si->GetProcess(),
              previous_si->GetOrCreateProcessForTesting());
    EXPECT_FALSE(current_si->IsCrossOriginIsolated());
  }
}

// Regression test for https://crbug.com/1226909.
IN_PROC_BROWSER_TEST_P(CrossOriginOpenerPolicyBrowserTest,
                       NavigatePopupToErrorAndCrash) {
  GURL isolated_page(
      https_server()->GetURL("a.test",
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
  EXPECT_FALSE(popup_web_contents->GetPrimaryMainFrame()
                   ->GetSiteInstance()
                   ->IsCrossOriginIsolated());
  EXPECT_EQ(
      CoopUnsafeNone(),
      popup_web_contents->GetPrimaryMainFrame()->cross_origin_opener_policy());

  url::Origin error_origin =
      popup_web_contents->GetPrimaryMainFrame()->GetLastCommittedOrigin();

  // Simulate the popup renderer process crashing.
  RenderProcessHost* popup_process =
      popup_web_contents->GetPrimaryMainFrame()->GetProcess();
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
  EXPECT_EQ(
      error_origin,
      popup_web_contents->GetPrimaryMainFrame()->GetLastCommittedOrigin());
  EXPECT_FALSE(popup_web_contents->GetPrimaryMainFrame()
                   ->GetSiteInstance()
                   ->IsCrossOriginIsolated());
  EXPECT_EQ(
      CoopUnsafeNone(),
      popup_web_contents->GetPrimaryMainFrame()->cross_origin_opener_policy());
}

// Regression test for https://crbug.com/1239540.
IN_PROC_BROWSER_TEST_P(CrossOriginOpenerPolicyBrowserTest,
                       ReloadCrossOriginIsolatedPageWhileOffline) {
  GURL isolated_page(
      https_server()->GetURL("a.test",
                             "/set-header?"
                             "Cross-Origin-Opener-Policy: same-origin&"
                             "Cross-Origin-Embedder-Policy: require-corp"));

  // Initial cross origin isolated page.
  EXPECT_TRUE(NavigateToURL(shell(), isolated_page));
  SiteInstanceImpl* main_si = current_frame_host()->GetSiteInstance();
  EXPECT_TRUE(main_si->IsCrossOriginIsolated());

  // Simulate being offline by failing all network requests.
  auto url_loader_interceptor =
      std::make_unique<content::URLLoaderInterceptor>(base::BindRepeating(
          [](content::URLLoaderInterceptor::RequestParams* params) {
            network::URLLoaderCompletionStatus status;
            status.error_code = net::Error::ERR_CONNECTION_FAILED;
            params->client->OnComplete(status);
            return true;
          }));

  // Reload and end up with an error page to verify we do not violate any cross
  // origin isolation invariant.
  ReloadBlockUntilNavigationsComplete(shell(), 1);
}

// Regression test for https://crbug.com/1239540.
IN_PROC_BROWSER_TEST_P(CrossOriginOpenerPolicyBrowserTest,
                       ReloadCoopPageWhileOffline) {
  GURL isolated_page(
      https_server()->GetURL("a.test",
                             "/set-header?"
                             "Cross-Origin-Opener-Policy: same-origin"));

  // Initial coop isolated page.
  EXPECT_TRUE(NavigateToURL(shell(), isolated_page));
  RenderFrameHostImpl* main_rfh = current_frame_host();
  EXPECT_EQ(main_rfh->cross_origin_opener_policy(), CoopSameOrigin());

  // Simulate being offline by failing all network requests.
  auto url_loader_interceptor =
      std::make_unique<content::URLLoaderInterceptor>(base::BindRepeating(
          [](content::URLLoaderInterceptor::RequestParams* params) {
            network::URLLoaderCompletionStatus status;
            status.error_code = net::Error::ERR_CONNECTION_FAILED;
            params->client->OnComplete(status);
            return true;
          }));

  // Reload and end up with an error page to verify we do not violate any cross
  // origin isolation invariant.
  ReloadBlockUntilNavigationsComplete(shell(), 1);
}

// Regression test for https://crbug.com/1239540.
IN_PROC_BROWSER_TEST_P(CrossOriginOpenerPolicyBrowserTest,
                       BackNavigationToCrossOriginIsolatedPageWhileOffline) {
  GURL isolated_page(
      https_server()->GetURL("a.test",
                             "/set-header?"
                             "Cross-Origin-Opener-Policy: same-origin&"
                             "Cross-Origin-Embedder-Policy: require-corp"));

  GURL same_origin_isolated_page(
      https_server()->GetURL("a.test", "/cross-origin-isolated.html"));

  // Put the initial isolated page in history.
  EXPECT_TRUE(NavigateToURL(shell(), isolated_page));
  SiteInstanceImpl* main_si = current_frame_host()->GetSiteInstance();
  EXPECT_TRUE(main_si->IsCrossOriginIsolated());

  // This test relies on actually doing the back navigation from network.
  // We disable BFCache on the initial to ensure that happens.
  DisableBFCacheForRFHForTesting(current_frame_host()->GetGlobalId());

  // Navigate to a same origin isolated page, staying in the same
  // BrowsingInstance. This is also ensured by having the BFCache disabled on
  // the initial page, avoiding special same-site proactive swaps.
  EXPECT_TRUE(NavigateToURL(shell(), same_origin_isolated_page));
  main_si = current_frame_host()->GetSiteInstance();
  EXPECT_TRUE(main_si->IsCrossOriginIsolated());

  // Simulate being offline by failing all network requests.
  auto url_loader_interceptor =
      std::make_unique<content::URLLoaderInterceptor>(base::BindRepeating(
          [](content::URLLoaderInterceptor::RequestParams* params) {
            network::URLLoaderCompletionStatus status;
            status.error_code = net::Error::ERR_CONNECTION_FAILED;
            params->client->OnComplete(status);
            return true;
          }));

  // Go back and end up with an error page to verify we do not violate any cross
  // origin isolation invariant.
  web_contents()->GetController().GoBack();
  EXPECT_FALSE(WaitForLoadStop(web_contents()));
}

// Regression test for https://crbug.com/1374705.
IN_PROC_BROWSER_TEST_P(CrossOriginOpenerPolicyBrowserTest,
                       ReloadRedirectsToCoopPage) {
  GURL coop_page(
      https_server()->GetURL("a.test",
                             "/set-header?"
                             "Cross-Origin-Opener-Policy: same-origin"));
  GURL redirect_page(https_server()->GetURL(
      "a.test", "/redirect-on-second-navigation?" + coop_page.spec()));

  // Navigate to the redirect page. On the first navigation, this is a simple
  // empty page with no headers.
  EXPECT_TRUE(NavigateToURL(shell(), redirect_page));
  scoped_refptr<SiteInstanceImpl> main_si =
      current_frame_host()->GetSiteInstance();
  EXPECT_EQ(current_frame_host()->GetLastCommittedURL(), redirect_page);

  // Reload. This time we should be redirected to a COOP: same-origin page.
  ReloadBlockUntilNavigationsComplete(shell(), 1);
  EXPECT_EQ(current_frame_host()->GetLastCommittedURL(), coop_page);

  // We should have swapped BrowsingInstance.
  EXPECT_FALSE(
      main_si->IsRelatedSiteInstance(current_frame_host()->GetSiteInstance()));
}

IN_PROC_BROWSER_TEST_P(CrossOriginOpenerPolicyBrowserTest,
                       ReloadPageWithUpdatedCoopHeader) {
  GURL changing_coop_page(
      https_server()->GetURL("a.test", "/serve-coop-on-second-navigation"));

  // Navigate to the page. On the first navigation, this is a simple empty page
  // with no headers.
  EXPECT_TRUE(NavigateToURL(shell(), changing_coop_page));
  scoped_refptr<SiteInstanceImpl> main_si =
      current_frame_host()->GetSiteInstance();

  // Reload. This time the page should be served with COOP: same-origin.
  ReloadBlockUntilNavigationsComplete(shell(), 1);

  // We should have swapped BrowsingInstance.
  EXPECT_FALSE(
      main_si->IsRelatedSiteInstance(current_frame_host()->GetSiteInstance()));
}

IN_PROC_BROWSER_TEST_P(CrossOriginOpenerPolicyBrowserTest,
                       CrossOriginRedirectHasProperCrossOriginIsolatedState) {
  GURL non_isolated_page(
      embedded_test_server()->GetURL("a.test", "/title1.html"));

  GURL isolated_page(
      https_server()->GetURL("c.test",
                             "/set-header?"
                             "Cross-Origin-Opener-Policy: same-origin&"
                             "Cross-Origin-Embedder-Policy: require-corp"));

  GURL redirect_isolated_page(https_server()->GetURL(
      "b.test", "/redirect-with-coop-coep-headers?" + isolated_page.spec()));

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
      https_server()->GetURL("a.test",
                             "/set-header?"
                             "Cross-Origin-Opener-Policy: same-origin&"
                             "Cross-Origin-Embedder-Policy: require-corp"));
  GURL isolated_page_b(
      https_server()->GetURL("cdn.a.test",
                             "/set-header?"
                             "Cross-Origin-Embedder-Policy: require-corp&"
                             "Cross-Origin-Resource-Policy: cross-origin"));

  // Initial cross-origin isolated page.
  EXPECT_TRUE(NavigateToURL(shell(), isolated_page));
  SiteInstanceImpl* main_si = current_frame_host()->GetSiteInstance();
  EXPECT_TRUE(main_si->IsCrossOriginIsolated());

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
      current_frame_host()->child_at(0)->current_frame_host();
  SiteInstanceImpl* iframe_si = iframe_rfh->GetSiteInstance();
  EXPECT_TRUE(iframe_si->IsCrossOriginIsolated());
  EXPECT_TRUE(iframe_si->IsRelatedSiteInstance(main_si));
  if (SiteIsolationPolicy::AreOriginKeyedProcessesEnabledByDefault()) {
    // The main frame and the child frame have different origins, so when
    // OriginKeyedProcessesByDefault is enabled they will be placed in different
    // processes.
    EXPECT_NE(iframe_si->GetProcess(), main_si->GetProcess());
  } else {
    EXPECT_EQ(iframe_si->GetProcess(), main_si->GetProcess());
  }

  // Open an isolated popup, but cross-origin.
  {
    RenderFrameHostImpl* popup_rfh =
        static_cast<WebContentsImpl*>(
            OpenPopup(iframe_rfh, isolated_page, "", "", false)->web_contents())
            ->GetPrimaryMainFrame();

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
      "a.test",
      "/set-header?Cross-Origin-Opener-Policy-Report-Only: same-origin"));
  GURL b_url(https_server()->GetURL("b.test", "/empty.html"));
  GURL c_url(https_server()->GetURL("c.test", "/empty.html"));

  // 1. Start from COOP-Report-Only:same-origin. (a.test COOP-RO)
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

  // 3. Insert a cross-origin iframe. (b.test)
  EXPECT_TRUE(ExecJs(opener_rfh, JsReplace(R"(
    const iframe = document.createElement("iframe");
    iframe.src = $1;
    document.body.appendChild(iframe);
  )",
                                           b_url)));
  WaitForLoadStop(web_contents());
  RenderFrameHostImpl* opener_child_rfh =
      opener_rfh->child_at(0)->current_frame_host();

  // 4. Insert a grand-child iframe (b.test).
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

// This test is a reproducer for https://crbug.com/1305394.
IN_PROC_BROWSER_TEST_P(CrossOriginOpenerPolicyBrowserTest,
                       CrossOriginIframeCoopBypass) {
  // This test requires that a cross-origin iframe be placed in its own
  // process. It is irrelevant without strict site isolation.
  if (!SiteIsolationPolicy::UseDedicatedProcessesForAllSites()) {
    return;
  }

  GURL non_coop_page(https_server()->GetURL("a.test", "/title1.html"));
  GURL cross_origin_non_coop_page(
      https_server()->GetURL("b.test", "/title1.html"));
  GURL coop_page(https_server()->GetURL(
      "a.test", "/set-header?cross-origin-opener-policy: same-origin"));

  // Get an initial non-COOP page with an empty popup.
  ASSERT_TRUE(NavigateToURL(shell(), non_coop_page));
  RenderFrameHostImplWrapper initial_main_rfh(current_frame_host());

  ShellAddedObserver shell_observer;
  ASSERT_TRUE(ExecJs(initial_main_rfh.get(),
                     JsReplace("window.open($1)", non_coop_page)));
  WebContentsImpl* popup =
      static_cast<WebContentsImpl*>(shell_observer.GetShell()->web_contents());
  RenderFrameHostImpl* popup_rfh = popup->GetPrimaryMainFrame();

  // At this stage we have a single SiteInstance used both for the main page and
  // the same-site popup.
  SiteInstanceImpl* initial_main_si = initial_main_rfh->GetSiteInstance();
  SiteInstanceImpl* popup_si = popup_rfh->GetSiteInstance();
  ASSERT_EQ(initial_main_si, popup_si);
  RenderProcessHost* process_A = initial_main_si->GetProcess();

  // The popup then navigates the opener to a COOP page.
  EXPECT_TRUE(popup_rfh->frame_tree_node()->opener());
  ASSERT_TRUE(ExecJs(popup_rfh, JsReplace("opener.location = $1", coop_page)));
  ASSERT_TRUE(WaitForLoadStop(web_contents()));
  ASSERT_TRUE(initial_main_rfh.WaitUntilRenderFrameDeleted());

  // This should trigger a BrowsingInstance swap. The main frame gets a new
  // unrelated BrowsingInstance.
  RenderFrameHostImpl* main_rfh = current_frame_host();
  SiteInstanceImpl* main_si = main_rfh->GetSiteInstance();
  RenderProcessHost* process_B = main_si->GetProcess();
  ASSERT_FALSE(popup_si->IsRelatedSiteInstance(main_si));

  // The popup still uses process A, but the opener link should be cut and no
  // proxy should remain between the two site instances.
  EXPECT_EQ(process_A, popup_si->GetProcess());
  if (ShouldCreateNewHostForAllFrames()) {
    // When RenderDocument is enabled, we will create a new RenderFrameHost
    // using the same SiteInstance from the start of the navigation where we
    // don't have the COOP information yet. Then when we receive the final
    // response, we will try to reuse the process used by the speculative RFH,
    // which is the same process as before.
    // TODO(crbug.com/40261276): This is unexpected. Fix this so that the
    // process won't be reused.
    EXPECT_EQ(process_B, process_A);
  } else {
    // When RenderDocument is enabled, we will only create a new RenderFrameHost
    // when the final response for the COOP page is created. In this case, a new
    // process will be created for the final RenderFrameHost.
    EXPECT_NE(process_B, process_A);
  }
  EXPECT_FALSE(popup_rfh->frame_tree_node()->opener());
  EXPECT_TRUE(popup_rfh->frame_tree_node()
                  ->render_manager()
                  ->GetAllProxyHostsForTesting()
                  .empty());
  EXPECT_TRUE(main_rfh->frame_tree_node()
                  ->render_manager()
                  ->GetAllProxyHostsForTesting()
                  .empty());

  // Load an iframe that is cross-origin to the top frame's opener.
  ASSERT_TRUE(ExecJs(popup_rfh, JsReplace(R"(
    const frame = document.createElement('iframe');
    frame.src = $1;
    document.body.appendChild(frame);
  )",
                                          cross_origin_non_coop_page)));
  EXPECT_TRUE(WaitForLoadStop(popup));
  RenderFrameHostImpl* iframe_rfh =
      popup_rfh->child_at(0)->current_frame_host();
  SiteInstanceImpl* iframe_si = iframe_rfh->GetSiteInstance();

  // The iframe being cross-origin, it is put in a different but related
  // SiteInstance.
  EXPECT_TRUE(iframe_si->IsRelatedSiteInstance(popup_si));
  EXPECT_FALSE(iframe_si->IsRelatedSiteInstance(main_si));

  // We end up with the main window, the main popup frame and the iframe all
  // living in their own process. We should only have proxies from the popup
  // main frame to iframe and vice versa. Opener links should stay severed.
  RenderProcessHost* process_C = iframe_si->GetProcess();
  EXPECT_NE(process_C, process_A);
  EXPECT_NE(process_C, process_B);
  EXPECT_EQ(1u, iframe_rfh->frame_tree_node()
                    ->render_manager()
                    ->GetAllProxyHostsForTesting()
                    .size());
  EXPECT_EQ(1u, popup_rfh->frame_tree_node()
                    ->render_manager()
                    ->GetAllProxyHostsForTesting()
                    .size());

  // The opener should not be reachable from the popup iframe.
  EXPECT_EQ(true, EvalJs(iframe_rfh, "parent.opener == null"));
}

// Check whether not using COOP causes a RenderProcessHost change during
// same-origin navigations. This is a control test for the subsequent tests.
IN_PROC_BROWSER_TEST_P(CrossOriginOpenerPolicyBrowserTest,
                       Process_CoopUnsafeNone_SameOrigin) {
  GURL url_1(https_server()->GetURL("a.test", "/empty.html?1"));
  GURL url_2(https_server()->GetURL("a.test", "/empty.html?2"));
  GURL url_3(https_server()->GetURL("a.test", "/empty.html?3"));

  EXPECT_TRUE(NavigateToURL(shell(), url_1));
  int rph_id_1 = current_frame_host()->GetProcess()->GetDeprecatedID();
  EXPECT_TRUE(NavigateToURL(shell(), url_2));
  int rph_id_2 = current_frame_host()->GetProcess()->GetDeprecatedID();
  EXPECT_TRUE(NavigateToURL(shell(), url_3));
  int rph_id_3 = current_frame_host()->GetProcess()->GetDeprecatedID();

  EXPECT_EQ(rph_id_1, rph_id_2);
  EXPECT_EQ(rph_id_2, rph_id_3);
  EXPECT_EQ(rph_id_3, rph_id_1);
}

// Check whether using COOP causes a RenderProcessHost change during
// same-origin navigations.
IN_PROC_BROWSER_TEST_P(CrossOriginOpenerPolicyBrowserTest,
                       Process_CoopSameOrigin_SameOrigin) {
  GURL url_1(https_server()->GetURL(
      "a.test", "/set-header?Cross-Origin-Opener-Policy: same-origin&1"));
  GURL url_2(https_server()->GetURL(
      "a.test", "/set-header?Cross-Origin-Opener-Policy: same-origin&2"));
  GURL url_3(https_server()->GetURL(
      "a.test", "/set-header?Cross-Origin-Opener-Policy: same-origin&3"));

  EXPECT_TRUE(NavigateToURL(shell(), url_1));
  int rph_id_1 = current_frame_host()->GetProcess()->GetDeprecatedID();
  EXPECT_TRUE(NavigateToURL(shell(), url_2));
  int rph_id_2 = current_frame_host()->GetProcess()->GetDeprecatedID();
  EXPECT_TRUE(NavigateToURL(shell(), url_3));
  int rph_id_3 = current_frame_host()->GetProcess()->GetDeprecatedID();

  EXPECT_EQ(rph_id_1, rph_id_2);
  EXPECT_EQ(rph_id_2, rph_id_3);
  EXPECT_EQ(rph_id_3, rph_id_1);
}

// Check that a COOP mismatch does not cause a RenderProcessHost change during
// same-origin navigations, unless COOP triggers the site isolation heuristic
// of requiring a dedicated process, which would force a process swap.
IN_PROC_BROWSER_TEST_P(CrossOriginOpenerPolicyBrowserTest,
                       Process_CoopAlternate_SameOrigin) {
  GURL url_1(https_server()->GetURL("a.test", "/empty.html"));
  GURL url_2(https_server()->GetURL(
      "a.test", "/set-header?Cross-Origin-Opener-Policy: same-origin"));
  GURL url_3(https_server()->GetURL("a.test", "/empty.html"));

  EXPECT_TRUE(NavigateToURL(shell(), url_1));
  int rph_id_1 = current_frame_host()->GetProcess()->GetDeprecatedID();
  EXPECT_TRUE(NavigateToURL(shell(), url_2));
  int rph_id_2 = current_frame_host()->GetProcess()->GetDeprecatedID();
  EXPECT_TRUE(NavigateToURL(shell(), url_3));
  int rph_id_3 = current_frame_host()->GetProcess()->GetDeprecatedID();

  // If we're using the COOP site isolation heuristic (e.g., on Android), we
  // have to swap processes since we're going from an unlocked process to a
  // locked process.
  if (SiteIsolationPolicy::IsSiteIsolationForCOOPEnabled()) {
    EXPECT_NE(rph_id_1, rph_id_2);
    // COOP isolation only applies to the current BrowsingInstance if there was
    // no user gesture.  Since NavigateToURL forced a BrowsingInstance swap,
    // and since there was no user gesture on url_2, we'll be going from a
    // locked process back to an unlocked process, and hence require a process
    // swap.
    EXPECT_NE(rph_id_2, rph_id_3);
  } else {
    EXPECT_EQ(rph_id_1, rph_id_2);
    EXPECT_EQ(rph_id_2, rph_id_3);
  }
}

// Check whether COOP causes a RenderProcessHost change during same-site
// navigations.
IN_PROC_BROWSER_TEST_P(CrossOriginOpenerPolicyBrowserTest,
                       Process_CoopAlternate_SameSite) {
  GURL url_1(https_server()->GetURL("a.a.test", "/empty.html"));
  GURL url_2(https_server()->GetURL(
      "b.a.test", "/set-header?Cross-Origin-Opener-Policy: same-origin"));
  GURL url_3(https_server()->GetURL("c.a.test", "/empty.html"));

  EXPECT_TRUE(NavigateToURL(shell(), url_1));
  int rph_id_1 = current_frame_host()->GetProcess()->GetDeprecatedID();
  EXPECT_TRUE(NavigateToURL(shell(), url_2));
  int rph_id_2 = current_frame_host()->GetProcess()->GetDeprecatedID();
  EXPECT_TRUE(NavigateToURL(shell(), url_3));
  int rph_id_3 = current_frame_host()->GetProcess()->GetDeprecatedID();

  // If we're using the COOP site isolation heuristic (e.g., on Android), we
  // have to swap processes since we're going from an unlocked process to a
  // locked process.
  if (SiteIsolationPolicy::IsSiteIsolationForCOOPEnabled()) {
    EXPECT_NE(rph_id_1, rph_id_2);
    // COOP isolation only applies to the current BrowsingInstance if there was
    // no user gesture.  Since NavigateToURL forced a BrowsingInstance swap,
    // and since there was no user gesture on url_2, we'll be going from a
    // locked process back to an unlocked process, and hence require a process
    // swap.
    EXPECT_NE(rph_id_2, rph_id_3);
  } else if (SiteIsolationPolicy::AreOriginKeyedProcessesEnabledByDefault()) {
    // With OriginKeyedProcessesByDefault, each unique origin will be placed in
    // a separate process.
    EXPECT_NE(rph_id_1, rph_id_2);
    EXPECT_NE(rph_id_2, rph_id_3);
    EXPECT_NE(rph_id_1, rph_id_3);
  } else {
    EXPECT_EQ(rph_id_1, rph_id_2);
    EXPECT_EQ(rph_id_2, rph_id_3);
  }
}

// Check whether COOP causes a RenderProcessHost change during cross-origin
// navigations.
IN_PROC_BROWSER_TEST_P(CrossOriginOpenerPolicyBrowserTest,
                       Process_CoopSameOrigin_CrossOrigin) {
  GURL url_1(https_server()->GetURL("a.test", "/empty.html"));
  GURL url_2(https_server()->GetURL(
      "b.test", "/set-header?Cross-Origin-Opener-Policy: same-origin"));
  GURL url_3(https_server()->GetURL("c.test", "/empty.html"));

  EXPECT_TRUE(NavigateToURL(shell(), url_1));
  int rph_id_1 = current_frame_host()->GetProcess()->GetDeprecatedID();
  EXPECT_TRUE(NavigateToURL(shell(), url_2));
  int rph_id_2 = current_frame_host()->GetProcess()->GetDeprecatedID();
  EXPECT_TRUE(NavigateToURL(shell(), url_3));
  int rph_id_3 = current_frame_host()->GetProcess()->GetDeprecatedID();

  EXPECT_NE(rph_id_1, rph_id_2);
  EXPECT_NE(rph_id_2, rph_id_3);
  EXPECT_NE(rph_id_3, rph_id_1);
}

// Smoke test for an iframe in a crossOriginIsolated page doing a same-document
// history navigation. Added to prevent regression of https://crbug.com/1413081.
IN_PROC_BROWSER_TEST_P(CrossOriginOpenerPolicyBrowserTest,
                       SmokeTest_CoopCoepSameDocumentIframeHistoryNavigation) {
  GURL main_page_url(
      https_server()->GetURL("a.test",
                             "/set-header?"
                             "Cross-origin-opener-policy: same-origin&"
                             "Cross-origin-embedder-policy: require-corp"));
  GURL iframe_url(
      https_server()->GetURL("a.test",
                             "/set-header?"
                             "Cross-Origin-Embedder-Policy: require-corp&"
                             "Cross-Origin-Resource-Policy: cross-origin"));

  // Start with a cross-origin isolated document.
  ASSERT_TRUE(NavigateToURL(shell(), main_page_url));

  // Add an iframe that has the appropriate COEP and CORP headers.
  ASSERT_TRUE(ExecJs(current_frame_host(), JsReplace(R"(
    const frame = document.createElement('iframe');
    frame.src = $1;
    document.body.appendChild(frame);
  )",
                                                     iframe_url)));
  ASSERT_TRUE(WaitForLoadStop(web_contents()));

  // Do a pushState/popState in the iframe. This will generate a same-document
  // history navigation.
  RenderFrameHostImpl* child_rfh =
      current_frame_host()->child_at(0)->current_frame_host();
  ASSERT_TRUE(ExecJs(child_rfh, "history.pushState({}, '', '');"));
  ASSERT_TRUE(ExecJs(child_rfh, "history.go(-1)"));

  // We should commit and gracefully finish loading.
  EXPECT_TRUE(WaitForLoadStop(web_contents()));
}

// Ensure that when navigating from a non-COOP site to a site with COOP that
// also requires a dedicated process, there's only one new process created, and
// the BrowsingInstance swap required by COOP doesn't trigger an unneeded
// second process swap at response time.  In other words, the process created
// for the speculative RenderFrameHost at navigation start time ought to be
// reused by the speculative RenderFrameHost that's recomputed at
// OnResponseStarted response time (where it's recomputed due to the
// BrowsingInstance swap required by COOP).
IN_PROC_BROWSER_TEST_P(CrossOriginOpenerPolicyBrowserTest,
                       NoExtraProcessSwapFromDiscardedSpeculativeRFH) {
  base::HistogramTester histogram_tester;
  if (IsIsolatedOriginRequiredToGuaranteeDedicatedProcess()) {
    IsolateOriginsForTesting(embedded_test_server(), shell()->web_contents(),
                             {url::Origin::Create(GURL("https://b.test/"))});
  }

  GURL url_1(https_server()->GetURL("a.test", "/empty.html"));
  GURL url_2(https_server()->GetURL(
      "b.test", "/set-header?Cross-Origin-Opener-Policy: same-origin"));

  // Navigate to a non-COOP URL.  Note that on Android this will be in a
  // default SiteInstance and in a process that's not locked to a specific
  // site, and on desktop it'll be in a process that's locked to a.test.  We're
  // interested in covering both cases.
  EXPECT_TRUE(NavigateToURL(shell(), url_1));
  int rph_id_1 = current_frame_host()->GetProcess()->GetDeprecatedID();

  // Start a navigation to b.test, which will have COOP headers, but this isn't
  // known until response time.  This creates a speculative RFH and process
  // that's locked to b.test.
  TestNavigationManager navigation(web_contents(), url_2);
  EXPECT_TRUE(BeginNavigateToURLFromRenderer(web_contents(), url_2));
  navigation.WaitForSpeculativeRenderFrameHostCreation();
  RenderFrameHostWrapper speculative_rfh(web_contents()
                                             ->GetPrimaryFrameTree()
                                             .root()
                                             ->render_manager()
                                             ->speculative_frame_host());
  ASSERT_TRUE(speculative_rfh.get());
  int rph_id_2 = speculative_rfh->GetProcess()->GetDeprecatedID();
  EXPECT_NE(rph_id_1, rph_id_2);

  // Allow the navigation to receive the response and commit.
  EXPECT_TRUE(navigation.WaitForNavigationFinished());
  EXPECT_TRUE(navigation.was_successful());

  // When the response for `url_2` was received, we should have learned about
  // the COOP headers and swapped BrowsingInstances. This should've recreated
  // the speculative RFH in a new SiteInstance/BrowsingInstance, but note that
  // since `url_2` only has COOP but no COEP (and hence no process isolation
  // requirement due to cross-origin isolation), it still just needs a regular
  // process locked to b.test, which is exactly the process that we created for
  // the original speculative RFH. Ensure that this process gets reused and not
  // wasted.
  int rph_id_3 = current_frame_host()->GetProcess()->GetDeprecatedID();
  EXPECT_EQ(rph_id_2, rph_id_3);

  // This test is parameterized on whether the bfcache is enabled.  With
  // bfcache, we force a BrowsingInstance swap at the very beginning when the
  // navigation to `url_2` starts, so there's no need to create a new
  // SiteInstance when we learn about COOP at response time, since the
  // candidate (speculative RFH's) SiteInstance is already in a fresh
  // BrowsingInstance.  Therefore, with bfcache, the original speculative RFH
  // will be the RFH that eventually commits.  Otherwise, the original
  // speculative RFH should be destroyed and replaced by another RFH.
  //
  // When BackForwardCache is not enabled, the speculative RFH is created in the
  // current BrowsingInstance, and is not appropriate for a navigation to the
  // COOP page. So we destroy it, but we can still reuse its process for the new
  // SiteInstance we create for the navigation.
  if (IsBackForwardCacheEnabled()) {
    EXPECT_FALSE(speculative_rfh.IsDestroyed());
  } else {
    EXPECT_TRUE(speculative_rfh.IsDestroyed());

    histogram_tester.ExpectUniqueSample(
        "Navigation.ProcessReuseOnCOOP.DifferentSiteInstance", true, 1);
  }
}

// Ensure that same-site navigations that result in a COOP mismatch avoid an
// unnecessary process swap when those navigations happen in a
// BrowsingContextGroup of size 1 (in this case, in the same WebContents).
IN_PROC_BROWSER_TEST_P(CrossOriginOpenerPolicyBrowserTest,
                       NoExtraProcessSwapFromSameSiteCOOPMismatch) {
  base::HistogramTester histogram_tester;
  GURL url_1(https_server()->GetURL("a.test", "/empty.html"));
  GURL url_2(https_server()->GetURL(
      "a.test", "/set-header?Cross-Origin-Opener-Policy: same-origin"));

  // Navigate to a non-COOP URL.
  EXPECT_TRUE(NavigateToURL(shell(), url_1));
  int rph_id_1 = current_frame_host()->GetProcess()->GetDeprecatedID();
  bool rph_1_is_locked =
      current_frame_host()->GetProcess()->GetProcessLock().IsLockedToSite();

  // Start a navigation to a page on a.test that will have COOP headers.
  TestNavigationManager navigation(web_contents(), url_2);
  EXPECT_TRUE(BeginNavigateToURLFromRenderer(web_contents(), url_2));
  EXPECT_TRUE(navigation.WaitForRequestStart());

  // When the back-forward cache is enabled, or when RenderDocument is used, we
  // will get a speculative RenderFrameHost, which should reuse the existing
  // process because the navigation is same-site.  Otherwise, the navigation
  // should stay in the current RenderFrameHost.
  int rph_id_2;
  RenderFrameHost* speculative_rfh = nullptr;
  if (IsBackForwardCacheEnabled() || ShouldCreateNewHostForAllFrames()) {
    navigation.WaitForSpeculativeRenderFrameHostCreation();
    speculative_rfh = web_contents()
                          ->GetPrimaryFrameTree()
                          .root()
                          ->render_manager()
                          ->speculative_frame_host();
    ASSERT_TRUE(speculative_rfh);
    rph_id_2 = speculative_rfh->GetProcess()->GetDeprecatedID();
    EXPECT_EQ(rph_id_1, rph_id_2);
  } else {
    ASSERT_FALSE(web_contents()
                     ->GetPrimaryFrameTree()
                     .root()
                     ->render_manager()
                     ->speculative_frame_host());
    rph_id_2 = rph_id_1;
  }

  RenderFrameHostWrapper speculative_rfh_checker(speculative_rfh);

  // Allow the navigation to receive the response commit.
  EXPECT_TRUE(navigation.WaitForNavigationFinished());
  EXPECT_TRUE(navigation.was_successful());

  // When the response for `url_2` was received, we should have learned about
  // the COOP headers and swapped BrowsingInstances. This should've created a
  // new speculative RFH in a new SiteInstance/BrowsingInstance, but it should
  // reuse the old a.com process since `url_2` only has COOP but no COEP (and
  // hence no process isolation requirement due to cross-origin isolation).  An
  // exception to this is if COOP triggers site isolation (e.g., on Android),
  // and the old process wasn't already locked to a.test.  In that case, a
  // process swap is required, since we are going from an unlocked process to a
  // locked process.
  //
  // When BFCache is enabled, we do a proactive BrowsingInstance swap at the
  // beginning of navigation, which has created a brand new speculative
  // RenderFrameHost. This speculative RenderFrameHost in a new BrowsingInstance
  // can be used to commit the navigation to the COOP page, so it is not
  // destroyed.
  int rph_id_3 = current_frame_host()->GetProcess()->GetDeprecatedID();
  if (SiteIsolationPolicy::IsSiteIsolationForCOOPEnabled()) {
    EXPECT_NE(rph_id_2, rph_id_3);
    EXPECT_FALSE(rph_1_is_locked);
    EXPECT_TRUE(
        current_frame_host()->GetProcess()->GetProcessLock().IsLockedToSite());

    // The metric for why we failed to reuse the process will be reported
    // differently depending on whether default SiteInstanceGroups are enabled.
    // For more information, see how "Navigation.ProcessReuseOnCOOP" is emitted
    // in `RenderFrameHostManager::GetSiteInstanceForNavigation()`. Importantly,
    // the sample (i.e., whether actual process reuse happened) should be
    // reported as false in both cases.
    if (ShouldUseDefaultSiteInstanceGroup()) {
      // With default SiteInstanceGroups, if we created a new candidate
      // SiteInstance at the start of the navigation to `url_2`, then the reason
      // will be recorded as "DifferentSiteInstance". This is the case when a
      // speculative RFH is created, which requires either bfcache or
      // RenderDocument (see also `speculative_rfh` expectations above).
      // Otherwise, the reason will be recorded as
      // "SameSiteNavigationInSingleWebContents".
      if (IsBackForwardCacheEnabled() || ShouldCreateNewHostForAllFrames()) {
        histogram_tester.ExpectUniqueSample(
            "Navigation.ProcessReuseOnCOOP.DifferentSiteInstance", false, 1);
      } else {
        histogram_tester.ExpectUniqueSample(
            "Navigation.ProcessReuseOnCOOP."
            "SameSiteNavigationInSingleWebContents",
            false, 1);
      }
    } else {
      histogram_tester.ExpectUniqueSample("Navigation.ProcessReuseOnCOOP.None",
                                          false, 1);
    }
  } else {
    EXPECT_EQ(rph_id_2, rph_id_3);

    if (IsBackForwardCacheEnabled()) {
      EXPECT_FALSE(speculative_rfh_checker.IsDestroyed());
    } else if (ShouldCreateNewHostForAllFrames()) {
      histogram_tester.ExpectUniqueSample(
          "Navigation.ProcessReuseOnCOOP.DifferentSiteInstance", true, 1);
    } else {
      histogram_tester.ExpectUniqueSample(
          "Navigation.ProcessReuseOnCOOP.SameSiteNavigationInSingleWebContents",
          true, 1);
    }
  }
}

// Verify that there's no extra process swap during a same-site navigation from
// one COOP page to another COOP page.
IN_PROC_BROWSER_TEST_P(CrossOriginOpenerPolicyBrowserTest,
                       NavigatingFromCOOPToCOOPHasNoExtraProcessCreation) {
  base::HistogramTester histogram_tester;
  GURL url_1(https_server()->GetURL(
      "a.test", "/set-header?Cross-Origin-Opener-Policy: same-origin"));
  GURL url_2(https_server()->GetURL(
      "a.test", "/set-header?Cross-Origin-Opener-Policy: same-origin&2"));

  // Navigate to a COOP URL.
  EXPECT_TRUE(NavigateToURL(shell(), url_1));
  int rph_id_1 = current_frame_host()->GetProcess()->GetDeprecatedID();
  histogram_tester.ExpectUniqueSample("Navigation.ProcessReuseOnCOOP.None",
                                      false, 1);

  // Start a navigation to another same-site COOP URL.
  TestNavigationManager navigation(web_contents(), url_2);
  EXPECT_TRUE(BeginNavigateToURLFromRenderer(web_contents(), url_2));
  // Wait for response to ensure any speculative RFH has already been created,
  // if necessary.
  ASSERT_TRUE(navigation.WaitForResponse());
  RenderFrameHostImpl* speculative_rfh = web_contents()
                                             ->GetPrimaryFrameTree()
                                             .root()
                                             ->render_manager()
                                             ->speculative_frame_host();

  // When the back-forward cache is enabled, or when RenderDocument is used, we
  // will get a speculative RenderFrameHost, which should reuse the existing
  // process because the navigation is same-site.  Otherwise, the navigation
  // should stay in the current RenderFrameHost.  The else path verifies that
  // we don't assume no COOP when initially making the request to `url_2` and
  // place the candidate SiteInstance in a new BrowsingInstance, and later come
  // back to the original BrowsingInstance after realizing at response time
  // that COOP hasn't changed.
  int rph_id_2;
  if (IsBackForwardCacheEnabled() || ShouldCreateNewHostForAllFrames()) {
    ASSERT_TRUE(speculative_rfh);
    rph_id_2 = speculative_rfh->GetProcess()->GetDeprecatedID();
    EXPECT_EQ(rph_id_1, rph_id_2);
  } else {
    ASSERT_FALSE(speculative_rfh);
    rph_id_2 = rph_id_1;
  }

  // Allow the navigation to commit.
  navigation.ResumeNavigation();
  EXPECT_TRUE(navigation.WaitForNavigationFinished());
  EXPECT_TRUE(navigation.was_successful());

  // When the response for `url_2` was received, we should verify that COOP
  // status hasn't changed, so no BrowsingInstance swap is needed, and we
  // should stay in the same process.
  int rph_id_3 = current_frame_host()->GetProcess()->GetDeprecatedID();
  EXPECT_EQ(rph_id_2, rph_id_3);
}

// Ensure that a same-site COOP mismatch that happens in a popup does *not*
// reuse the existing process, unlike in the
// NoExtraProcessSwapFromSameSiteCOOPMismatch test above.  This ensures that
// same-site COOP mismatch reuses the old process only in single-window
// BrowsingInstances, and noopener-like popups with a COOP mismatch still get a
// fresh process.
IN_PROC_BROWSER_TEST_P(CrossOriginOpenerPolicyBrowserTest,
                       NoProcessReuseForSameSiteCOOPMismatchInPopup) {
  base::HistogramTester histogram_tester;
  GURL url_1(https_server()->GetURL("a.test", "/empty.html"));
  GURL url_2(https_server()->GetURL(
      "a.test", "/set-header?Cross-Origin-Opener-Policy: same-origin"));

  // Navigate to a non-COOP URL.
  EXPECT_TRUE(NavigateToURL(shell(), url_1));
  int rph_id_1 = current_frame_host()->GetProcess()->GetDeprecatedID();

  // Open a same-site popup with COOP.
  Shell* new_shell = OpenPopup(web_contents(), url_2, "");
  EXPECT_TRUE(new_shell);
  WebContentsImpl* popup_contents =
      static_cast<WebContentsImpl*>(new_shell->web_contents());

  // When the response for `url_2` was received, we should have learned about
  // the COOP headers and swapped BrowsingInstances. This should've created a
  // new speculative RFH in a new SiteInstance/BrowsingInstance, and it should
  // create a fresh process rather than reuse the old a.com process, since
  // there was more than one active window in the old BrowsingInstance.
  int rph_id_2 =
      popup_contents->GetPrimaryMainFrame()->GetProcess()->GetDeprecatedID();
  EXPECT_NE(rph_id_1, rph_id_2);
  histogram_tester.ExpectUniqueSample("Navigation.ProcessReuseOnCOOP.None",
                                      false, 1);
}

// Tests the behavior around COOP BrowsingInstance swap when prerendering a COOP
// page.
// Regression test for crbug.com/1519131.
IN_PROC_BROWSER_TEST_P(ProcessReuseOnPrerenderCOOPSwapBrowserTest,
                       COOPSwapForPrerenderingCOOPPage) {
  base::HistogramTester histogram_tester;
  GURL initial_page(https_server()->GetURL("a.test", "/empty.html"));
  GURL prerender_page(https_server()->GetURL(
      "a.test", "/set-header?Cross-Origin-Opener-Policy: same-origin"));

  // Navigate to an initial non-COOP page.
  ASSERT_TRUE(NavigateToURL(shell(), initial_page));
  RenderFrameHostImpl* rfh_1 = current_frame_host();
  scoped_refptr<SiteInstanceImpl> si_1 = rfh_1->GetSiteInstance();
  base::UnguessableToken bi_token_1 =
      rfh_1->GetSiteInstance()->browsing_instance_token();
  int rph_id_1 = rfh_1->GetProcess()->GetDeprecatedID();

  // Start prerendering a COOP page.
  TestNavigationManager navigation_manager(web_contents(), prerender_page);
  prerender_helper().AddPrerenderAsync(prerender_page);

  // Up to this stage, PrerenderHost has been created and prerender navigation
  // has been started. Per PrerenderHost creation, new FrameTree is initialized
  // with new BrowsingInstance / SiteInstance, and a new process will be
  // assigned to it accordingly.
  ASSERT_TRUE(navigation_manager.WaitForRequestStart());
  FrameTreeNodeId prerender_host_id =
      prerender_helper().GetHostForUrl(prerender_page);
  RenderFrameHostImpl* rfh_2 =
      web_contents()->UnsafeFindFrameByFrameTreeNodeId(prerender_host_id);
  ASSERT_TRUE(rfh_2);
  scoped_refptr<SiteInstanceImpl> si_2 = rfh_2->GetSiteInstance();
  base::UnguessableToken bi_token_2 =
      rfh_2->GetSiteInstance()->browsing_instance_token();
  int rph_id_2 = rfh_2->GetProcess()->GetDeprecatedID();
  ASSERT_NE(rfh_1, rfh_2);
  ASSERT_NE(si_1, si_2);
  ASSERT_NE(bi_token_1, bi_token_2);
  ASSERT_NE(rph_id_1, rph_id_2);

  // After receiving a response, BrowsingInstance swap occurs by COOP and it
  // will result in the forced recreation of another new RenderFrameHost in a
  // new SiteInstance and renderer process.
  // With the current short-term fix under kProcessReuseOnPrerenderCOOPSwap, we
  // try to reuse the unlocked renderer process for the SiteInstance that was
  // recreated.
  ASSERT_TRUE(navigation_manager.WaitForNavigationFinished());
  ASSERT_TRUE(navigation_manager.was_successful());
  RenderFrameHostImpl* rfh_3 =
      web_contents()->UnsafeFindFrameByFrameTreeNodeId(prerender_host_id);
  ASSERT_TRUE(rfh_3);
  scoped_refptr<SiteInstanceImpl> si_3 = rfh_3->GetSiteInstance();
  base::UnguessableToken bi_token_3 =
      rfh_3->GetSiteInstance()->browsing_instance_token();
  int rph_id_3 = rfh_3->GetProcess()->GetDeprecatedID();
  EXPECT_NE(rfh_2, rfh_3);
  EXPECT_NE(si_2, si_3);
  EXPECT_NE(bi_token_2, bi_token_3);
  // Renderer process is reused.
  EXPECT_EQ(rph_id_2, rph_id_3);
  histogram_tester.ExpectUniqueSample("Navigation.ProcessReuseOnCOOP.Prerender",
                                      true, 1);
}

// TODO(crbug.com/40138297). Test inheritance of the virtual browsing
// context group when using window.open from an iframe, same-origin and
// cross-origin.

static auto kTestParams =
    testing::Combine(testing::ValuesIn(RenderDocumentFeatureLevelValues()),
                     testing::Bool());
INSTANTIATE_TEST_SUITE_P(All,
                         CrossOriginOpenerPolicyBrowserTest,
                         kTestParams,
                         CrossOriginOpenerPolicyBrowserTest::DescribeParams);
INSTANTIATE_TEST_SUITE_P(All,
                         CrossOriginOpenerPolicyNoOKPBrowserTest,
                         kTestParams,
                         CrossOriginOpenerPolicyBrowserTest::DescribeParams);
INSTANTIATE_TEST_SUITE_P(All,
                         VirtualBrowsingContextGroupTest,
                         kTestParams,
                         CrossOriginOpenerPolicyBrowserTest::DescribeParams);
INSTANTIATE_TEST_SUITE_P(All,
                         NoSharedArrayBufferByDefault,
                         kTestParams,
                         CrossOriginOpenerPolicyBrowserTest::DescribeParams);
INSTANTIATE_TEST_SUITE_P(All,
                         SoapByDefaultVirtualBrowsingContextGroupTest,
                         kTestParams,
                         CrossOriginOpenerPolicyBrowserTest::DescribeParams);
INSTANTIATE_TEST_SUITE_P(All,
                         NoSiteIsolationCrossOriginIsolationBrowserTest,
                         kTestParams,
                         CrossOriginOpenerPolicyBrowserTest::DescribeParams);
INSTANTIATE_TEST_SUITE_P(
    All,
    DefaultSiteInstanceGroupCrossOriginIsolationBrowserTest,
    kTestParams,
    CrossOriginOpenerPolicyBrowserTest::DescribeParams);
INSTANTIATE_TEST_SUITE_P(All,
                         ProcessReuseOnPrerenderCOOPSwapBrowserTest,
                         kTestParams,
                         CrossOriginOpenerPolicyBrowserTest::DescribeParams);

IN_PROC_BROWSER_TEST_P(NoSharedArrayBufferByDefault, BaseCase) {
  GURL url = https_server()->GetURL("a.test", "/empty.html");
  EXPECT_TRUE(NavigateToURL(shell(), url));
  EXPECT_EQ(false, EvalJs(current_frame_host(), "self.crossOriginIsolated"));
  EXPECT_EQ(false,
            EvalJs(current_frame_host(), "'SharedArrayBuffer' in globalThis"));
}

IN_PROC_BROWSER_TEST_P(NoSharedArrayBufferByDefault, CoopCoepIsolated) {
  GURL url =
      https_server()->GetURL("a.test",
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
      https_server()->GetURL("a.test",
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
    const sab = new SharedArrayBuffer(1234);
    g_iframe.contentWindow.postMessage(sab, "*");
  )"));

  EXPECT_EQ(1234, EvalJs(sub_document, "g_sab_size"));
}

IN_PROC_BROWSER_TEST_P(NoSharedArrayBufferByDefault,
                       CoopCoepTransferSharedArrayBufferToAboutBlankIframe) {
  CHECK(!base::FeatureList::IsEnabled(features::kSharedArrayBuffer));
  GURL url =
      https_server()->GetURL("a.test",
                             "/set-header?"
                             "Cross-Origin-Opener-Policy: same-origin&"
                             "Cross-Origin-Embedder-Policy: require-corp");
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

IN_PROC_BROWSER_TEST_P(
    NoSharedArrayBufferByDefault,
    CoopCoepTransferSharedArrayBufferToAboutBlankIframeWithoutWaiting) {
  CHECK(!base::FeatureList::IsEnabled(features::kSharedArrayBuffer));
  GURL url =
      https_server()->GetURL("a.test",
                             "/set-header?"
                             "Cross-Origin-Opener-Policy: same-origin&"
                             "Cross-Origin-Embedder-Policy: require-corp");
  EXPECT_TRUE(NavigateToURL(shell(), url));
  EXPECT_EQ(true, EvalJs(current_frame_host(),
                         "const iframe = document.createElement('iframe');"
                         "document.body.appendChild(iframe);"
                         "iframe.contentWindow.crossOriginIsolated;"));
}

// Transfer a SharedArrayBuffer in between two COOP+COEP document with a
// parent/child relationship. The child has set Permissions-Policy:
// cross-origin-isolated=(). As a result, it can't receive the object.
IN_PROC_BROWSER_TEST_P(
    NoSharedArrayBufferByDefault,
    CoopCoepTransferSharedArrayBufferToNoCrossOriginIsolatedIframe) {
  CHECK(!base::FeatureList::IsEnabled(features::kSharedArrayBuffer));
  GURL main_url =
      https_server()->GetURL("a.test",
                             "/set-header?"
                             "Cross-Origin-Opener-Policy: same-origin&"
                             "Cross-Origin-Embedder-Policy: require-corp");
  GURL iframe_url =
      https_server()->GetURL("a.test",
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
    const sab = new SharedArrayBuffer(1234);
    g_iframe.contentWindow.postMessage(sab,"*");
  )");

  EXPECT_THAT(postSharedArrayBuffer,
              EvalJsResult::ErrorIs(
                  HasSubstr("Failed to execute 'postMessage' on 'Window':")));
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
      https_server()->GetURL("a.test",
                             "/set-header?"
                             "Cross-Origin-Opener-Policy: same-origin&"
                             "Cross-Origin-Embedder-Policy: require-corp");
  GURL iframe_url =
      https_server()->GetURL("a.test",
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

  // TODO(crbug.com/40155614): Being able to share SharedArrayBuffer from
  // a document with self.crossOriginIsolated == false sounds wrong.
  EXPECT_TRUE(ExecJs(sub_document, R"(
    // Create a WebAssembly Memory to bypass the SAB constructor restriction.
    const sab = new (new WebAssembly.Memory(
        { shared:true, initial:1, maximum:1 }).buffer.constructor)(1234);
    parent.postMessage(sab, "*");
  )"));

  EXPECT_EQ(1234, EvalJs(main_document, "g_sab_size"));
}

class OriginTrialBrowserTest : public ContentBrowserTest {
 public:
  // The OriginTrial token is bound to a given origin. Since the
  // EmbeddedTestServer's port changes after every test run, it can't be used.
  // As a result, response must be served using a URLLoaderInterceptor.
  GURL OriginTrialURL() { return GURL("https://coop.security:9999"); }

  WebContentsImpl* web_contents() const {
    return static_cast<WebContentsImpl*>(shell()->web_contents());
  }

  RenderFrameHostImpl* current_frame_host() {
    return web_contents()->GetPrimaryMainFrame();
  }

  net::EmbeddedTestServer* https_server() { return &https_server_; }

 private:
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
  net::EmbeddedTestServer https_server_;
};

// Ensure the UnrestrictedSharedArrayBuffer reverse origin trial is correctly
// implemented.
class UnrestrictedSharedArrayBufferOriginTrialBrowserTest
    : public OriginTrialBrowserTest {
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

 private:
  base::test::ScopedFeatureList feature_list_;
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
#if !BUILDFLAG(IS_ANDROID)
  EXPECT_EQ(true,
            EvalJs(current_frame_host(), "'SharedArrayBuffer' in globalThis"));
#else   // !BUILDFLAG(IS_ANDROID)
  EXPECT_EQ(false,
            EvalJs(current_frame_host(), "'SharedArrayBuffer' in globalThis"));
#endif  // !BUILDFLAG(IS_ANDROID)
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

#if BUILDFLAG(IS_ANDROID)
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

#if BUILDFLAG(IS_ANDROID)
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

#if !BUILDFLAG(IS_ANDROID)
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
#else   // !BUILDFLAG(IS_ANDROID)
  auto postSharedArrayBuffer = EvalJs(main_document, R"(
    // Create a WebAssembly Memory to bypass the SAB constructor restriction.
    const sab =
        new WebAssembly.Memory({ shared:true, initial:1, maximum:1 }).buffer;
    g_iframe.contentWindow.postMessage(sab,"*");
  )");

  EXPECT_THAT(postSharedArrayBuffer,
              EvalJsResult::ErrorIs(
                  HasSubstr("Failed to execute 'postMessage' on 'Window'")));
#endif  // !BUILDFLAG(IS_ANDROID)
}

// Enable the reverse OriginTrial via a <meta> tag. Then send a Webassembly's
// SharedArrayBuffer toward the iframe.
// Regression test for https://crbug.com/1201589).
// The SAB reverse origin trial only work on Desktop.
#if !BUILDFLAG(IS_ANDROID)
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
    const wasm_shared_memory = new WebAssembly.Memory({
      shared:true, initial:0, maximum:0 });
    g_iframe.contentWindow.postMessage(wasm_shared_memory.buffer, "*");
  )"));
  EXPECT_EQ(0, EvalJs(sub_document, "g_sab_size"));
}
#endif

IN_PROC_BROWSER_TEST_P(SoapByDefaultVirtualBrowsingContextGroupTest, NoHeader) {
  const VirtualBcgAllowPopupTestCase kTestCases[] = {
      {
          // same-origin.
          https_server()->GetURL("a.test", "/title1.html"),
          https_server()->GetURL("a.test", "/title1.html"),
          false,
          false,
          SoapByDefaultVirtualBrowsingContextGroup,
      },
      {
          // cross-origin.
          https_server()->GetURL("a.a.test", "/title1.html"),
          https_server()->GetURL("b.a.test", "/title1.html"),
          true,
          true,
          SoapByDefaultVirtualBrowsingContextGroup,
      },
      {
          // cross-site.
          https_server()->GetURL("a.test", "/title1.html"),
          https_server()->GetURL("b.test", "/title1.html"),
          true,
          true,
          SoapByDefaultVirtualBrowsingContextGroup,
      },
  };
  for (const auto& test : kTestCases) {
    RunTest(test, shell());
  }
}

IN_PROC_BROWSER_TEST_P(SoapByDefaultVirtualBrowsingContextGroupTest,
                       ToUnsafeNone) {
  const VirtualBcgAllowPopupTestCase kTestCases[] = {
      {
          // same-origin.
          https_server()->GetURL("a.test", "/title1.html"),
          https_server()->GetURL("a.test",
                                 "/set-header?"
                                 "Cross-Origin-Opener-Policy: unsafe-none"),
          false,
          true,
          SoapByDefaultVirtualBrowsingContextGroup,
      },
      {
          // cross-origin.
          https_server()->GetURL("a.a.test", "/title1.html"),
          https_server()->GetURL("b.a.test",
                                 "/set-header?"
                                 "Cross-Origin-Opener-Policy: unsafe-none"),
          false,
          true,
          SoapByDefaultVirtualBrowsingContextGroup,
      },
      {
          // cross-site.
          https_server()->GetURL("a.test", "/title1.html"),
          https_server()->GetURL("b.test",
                                 "/set-header?"
                                 "Cross-Origin-Opener-Policy: unsafe-none"),
          false,
          true,
          SoapByDefaultVirtualBrowsingContextGroup,
      },
  };
  for (const auto& test : kTestCases) {
    RunTest(test, shell());
  }
}

IN_PROC_BROWSER_TEST_P(SoapByDefaultVirtualBrowsingContextGroupTest,
                       FromUnsafeNone) {
  const VirtualBcgAllowPopupTestCase kTestCases[] = {
      {
          // same-origin.
          https_server()->GetURL("a.test",
                                 "/set-header?"
                                 "Cross-Origin-Opener-Policy: unsafe-none"),
          https_server()->GetURL("a.test", "/title1.html"),
          true,
          true,
          SoapByDefaultVirtualBrowsingContextGroup,
      },
      {
          // cross-origin.
          https_server()->GetURL("a.test",
                                 "/set-header?"
                                 "Cross-Origin-Opener-Policy: unsafe-none"),
          https_server()->GetURL("b.a.test", "/title1.html"),
          true,
          true,
          SoapByDefaultVirtualBrowsingContextGroup,
      },
      {
          // cross-site.
          https_server()->GetURL("a.test",
                                 "/set-header?"
                                 "Cross-Origin-Opener-Policy: unsafe-none"),
          https_server()->GetURL("b.test", "/title1.html"),
          true,
          true,
          SoapByDefaultVirtualBrowsingContextGroup,
      },
  };
  for (const auto& test : kTestCases) {
    RunTest(test, shell());
  }
}

IN_PROC_BROWSER_TEST_P(SoapByDefaultVirtualBrowsingContextGroupTest,
                       ToSameOriginAllowPopups) {
  const VirtualBcgAllowPopupTestCase kTestCases[] = {
      {
          // same-origin.
          https_server()->GetURL("a.test", "/title1.html"),
          https_server()->GetURL(
              "a.test",
              "/set-header?"
              "Cross-Origin-Opener-Policy: same-origin-allow-popups"),
          false,
          false,
          SoapByDefaultVirtualBrowsingContextGroup,
      },
      {
          // cross-origin.
          https_server()->GetURL("a.a.test", "/title1.html"),
          https_server()->GetURL(
              "b.a.test",
              "/set-header?"
              "Cross-Origin-Opener-Policy: same-origin-allow-popups"),
          true,
          true,
          SoapByDefaultVirtualBrowsingContextGroup,
      },
      {
          // cross-site.
          https_server()->GetURL("a.test", "/title1.html"),
          https_server()->GetURL(
              "b.test",
              "/set-header?"
              "Cross-Origin-Opener-Policy: same-origin-allow-popups"),
          true,
          true,
          SoapByDefaultVirtualBrowsingContextGroup,
      },
  };
  for (const auto& test : kTestCases) {
    RunTest(test, shell());
  }
}

IN_PROC_BROWSER_TEST_P(SoapByDefaultVirtualBrowsingContextGroupTest,
                       FromSameOriginAllowPopus) {
  const VirtualBcgAllowPopupTestCase kTestCases[] = {
      {
          // same-origin.
          https_server()->GetURL(
              "a.test",
              "/set-header?"
              "Cross-Origin-Opener-Policy: same-origin-allow-popups"),
          https_server()->GetURL("a.test", "/title1.html"),
          false,
          false,
          SoapByDefaultVirtualBrowsingContextGroup,
      },
      {
          // cross-origin.
          https_server()->GetURL(
              "a.test",
              "/set-header?"
              "Cross-Origin-Opener-Policy: same-origin-allow-popups"),
          https_server()->GetURL("b.a.test", "/title1.html"),
          true,
          true,
          SoapByDefaultVirtualBrowsingContextGroup,
      },
      {
          // cross-site.
          https_server()->GetURL(
              "a.test",
              "/set-header?"
              "Cross-Origin-Opener-Policy: same-origin-allow-popups"),
          https_server()->GetURL("b.test", "/title1.html"),
          true,
          true,
          SoapByDefaultVirtualBrowsingContextGroup,
      },
  };
  for (const auto& test : kTestCases) {
    RunTest(test, shell());
  }
}

IN_PROC_BROWSER_TEST_P(SoapByDefaultVirtualBrowsingContextGroupTest,
                       ToSameOrigin) {
  const VirtualBcgAllowPopupTestCase kTestCases[] = {
      {
          // same-origin.
          https_server()->GetURL("a.test", "/title1.html"),
          https_server()->GetURL("a.test",
                                 "/set-header?"
                                 "Cross-Origin-Opener-Policy: same-origin"),
          true,
          true,
          SoapByDefaultVirtualBrowsingContextGroup,
      },
      {
          // cross-origin.
          https_server()->GetURL("a.a.test", "/title1.html"),
          https_server()->GetURL("b.a.test",
                                 "/set-header?"
                                 "Cross-Origin-Opener-Policy: same-origin"),
          true,
          true,
          SoapByDefaultVirtualBrowsingContextGroup,
      },
      {
          // cross-site.
          https_server()->GetURL("a.test", "/title1.html"),
          https_server()->GetURL("b.test",
                                 "/set-header?"
                                 "Cross-Origin-Opener-Policy: same-origin"),
          true,
          true,
          SoapByDefaultVirtualBrowsingContextGroup,
      },
  };
  for (const auto& test : kTestCases) {
    RunTest(test, shell());
  }
}

IN_PROC_BROWSER_TEST_P(SoapByDefaultVirtualBrowsingContextGroupTest,
                       FromSameOrigin) {
  const VirtualBcgAllowPopupTestCase kTestCases[] = {
      {
          // same-origin.
          https_server()->GetURL("a.test",
                                 "/set-header?"
                                 "Cross-Origin-Opener-Policy: same-origin"),
          https_server()->GetURL("a.test", "/title1.html"),
          true,
          true,
          SoapByDefaultVirtualBrowsingContextGroup,
      },
      {
          // cross-origin.
          https_server()->GetURL("a.test",
                                 "/set-header?"
                                 "Cross-Origin-Opener-Policy: same-origin"),
          https_server()->GetURL("b.a.test", "/title1.html"),
          true,
          true,
          SoapByDefaultVirtualBrowsingContextGroup,
      },
      {
          // cross-site.
          https_server()->GetURL("a.test",
                                 "/set-header?"
                                 "Cross-Origin-Opener-Policy: same-origin"),
          https_server()->GetURL("b.test", "/title1.html"),
          true,
          true,
          SoapByDefaultVirtualBrowsingContextGroup,
      },
  };
  for (const auto& test : kTestCases) {
    RunTest(test, shell());
  }
}

// Navigates in between two pages from a different browsing context group. Then
// use the history API to navigate back and forth. Check their virtual browsing
// context group isn't restored.
// The goal is to spot differences when the BackForwardCache is enabled. See
// https://crbug.com/1109648.
IN_PROC_BROWSER_TEST_P(SoapByDefaultVirtualBrowsingContextGroupTest,
                       HistoryNavigation) {
  GURL url_a = https_server()->GetURL("a.test", "/title1.html");
  GURL url_b = https_server()->GetURL("b.test", "/title1.html");

  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  int group_1 = SoapByDefaultVirtualBrowsingContextGroup(web_contents());

  EXPECT_TRUE(NavigateToURL(shell(), url_b));
  int group_2 = SoapByDefaultVirtualBrowsingContextGroup(web_contents());

  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(web_contents()));
  int group_3 = SoapByDefaultVirtualBrowsingContextGroup(web_contents());

  web_contents()->GetController().GoForward();
  EXPECT_TRUE(WaitForLoadStop(web_contents()));
  int group_4 = SoapByDefaultVirtualBrowsingContextGroup(web_contents());

  // No matter whether the BackForwardCache is enabled or not, the navigation in
  // between the two URLs must always cross a virtual browsing context group.
  EXPECT_NE(group_1, group_2);
  EXPECT_NE(group_2, group_3);
  EXPECT_NE(group_3, group_4);
  EXPECT_NE(group_1, group_4);

  // TODO(crbug.com/40709606) During history navigation, the virtual
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

// 1. A1 opens A2 (same virtual browsing context group).
// 2. A2 navigates to B3 (different virtual browsing context group).
// 3. B3 navigates back to A4 using the history (different virtual browsing
//    context group).
//
// A1 and A4 must not be in the same browsing context group.
IN_PROC_BROWSER_TEST_P(SoapByDefaultVirtualBrowsingContextGroupTest,
                       HistoryNavigationWithPopup) {
  GURL url_a = https_server()->GetURL("a.test", "/title1.html");
  GURL url_b = https_server()->GetURL("b.test", "/title1.html");

  // Navigate to A1.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  int group_1 = SoapByDefaultVirtualBrowsingContextGroup(web_contents());

  // A1 opens A2.
  ShellAddedObserver shell_observer;
  EXPECT_TRUE(
      ExecJs(current_frame_host(), JsReplace("window.open($1)", url_a)));
  WebContents* popup = shell_observer.GetShell()->web_contents();
  EXPECT_TRUE(WaitForLoadStop(popup));
  int group_2 = SoapByDefaultVirtualBrowsingContextGroup(popup);

  // A2 navigates to B3.
  EXPECT_TRUE(ExecJs(popup, JsReplace("location.href = $1;", url_b)));
  EXPECT_TRUE(WaitForLoadStop(popup));
  int group_3 = SoapByDefaultVirtualBrowsingContextGroup(popup);

  // B3 navigates back to A4.
  EXPECT_TRUE(ExecJs(popup, JsReplace("history.back()")));
  EXPECT_TRUE(WaitForLoadStop(popup));
  int group_4 = SoapByDefaultVirtualBrowsingContextGroup(popup);

  EXPECT_EQ(group_1, group_2);
  EXPECT_NE(group_2, group_3);
  EXPECT_NE(group_3, group_4);
  EXPECT_NE(group_4, group_1);
}

IN_PROC_BROWSER_TEST_P(NoSiteIsolationCrossOriginIsolationBrowserTest,
                       COICanLiveInDefaultSI) {
  GURL isolated_page(
      https_server()->GetURL("a.test",
                             "/set-header"
                             "?cross-origin-opener-policy: same-origin"
                             "&cross-origin-embedder-policy: require-corp"));
  GURL non_isolated_page(https_server()->GetURL("a.test", "/title1.html"));

  EXPECT_TRUE(NavigateToURL(shell(), isolated_page));
  SiteInstanceImpl* main_frame_si = current_frame_host()->GetSiteInstance();
  EXPECT_TRUE(main_frame_si->IsCrossOriginIsolated());
  EXPECT_TRUE(main_frame_si->IsDefaultSiteInstance());

  {
    // Open a popup to a page with similar isolation. Pages that have compatible
    // cross origin isolation should be put in the same default SiteInstance.
    ShellAddedObserver shell_observer;
    EXPECT_TRUE(ExecJs(current_frame_host(),
                       JsReplace("window.open($1);", isolated_page)));
    WebContentsImpl* popup = static_cast<WebContentsImpl*>(
        shell_observer.GetShell()->web_contents());
    EXPECT_TRUE(WaitForLoadStop(popup));

    SiteInstanceImpl* popup_si =
        popup->GetPrimaryMainFrame()->GetSiteInstance();
    EXPECT_TRUE(popup_si->IsCrossOriginIsolated());
    EXPECT_TRUE(popup_si->IsDefaultSiteInstance());
    EXPECT_EQ(popup_si, main_frame_si);

    popup->Close();
  }

  {
    // Open a popup to a same origin non-isolated page. This page should live in
    // a different BrowsingInstance in the default non-isolated SiteInstance.
    ShellAddedObserver shell_observer;
    EXPECT_TRUE(ExecJs(current_frame_host(),
                       JsReplace("window.open($1);", non_isolated_page)));
    WebContentsImpl* popup = static_cast<WebContentsImpl*>(
        shell_observer.GetShell()->web_contents());
    EXPECT_TRUE(WaitForLoadStop(popup));

    SiteInstanceImpl* popup_si =
        popup->GetPrimaryMainFrame()->GetSiteInstance();
    EXPECT_FALSE(popup_si->IsCrossOriginIsolated());
    EXPECT_TRUE(popup_si->IsDefaultSiteInstance());
    EXPECT_NE(popup_si, main_frame_si);

    popup->Close();
  }
}

IN_PROC_BROWSER_TEST_P(DefaultSiteInstanceGroupCrossOriginIsolationBrowserTest,
                       COICanLiveInDefaultSiteInstanceGroup) {
  GURL isolated_page(
      https_server()->GetURL("a.test",
                             "/set-header"
                             "?cross-origin-opener-policy: same-origin"
                             "&cross-origin-embedder-policy: require-corp"));
  GURL non_isolated_page(https_server()->GetURL("a.test", "/title1.html"));

  EXPECT_TRUE(NavigateToURL(shell(), isolated_page));
  SiteInstanceImpl* main_frame_si = current_frame_host()->GetSiteInstance();
  EXPECT_TRUE(main_frame_si->IsCrossOriginIsolated());
  EXPECT_EQ(main_frame_si->group(),
            main_frame_si->DefaultSiteInstanceGroupForBrowsingInstance());

  {
    // Open a popup to a page with similar isolation. Pages that have compatible
    // cross origin isolation should be put in the same default
    // SiteInstanceGroup.
    ShellAddedObserver shell_observer;
    EXPECT_TRUE(ExecJs(current_frame_host(),
                       JsReplace("window.open($1);", isolated_page)));
    WebContentsImpl* popup = static_cast<WebContentsImpl*>(
        shell_observer.GetShell()->web_contents());
    EXPECT_TRUE(WaitForLoadStop(popup));

    SiteInstanceImpl* popup_si =
        popup->GetPrimaryMainFrame()->GetSiteInstance();
    EXPECT_TRUE(popup_si->IsCrossOriginIsolated());
    EXPECT_EQ(popup_si->group(),
              popup_si->DefaultSiteInstanceGroupForBrowsingInstance());
    EXPECT_EQ(popup_si, main_frame_si);

    popup->Close();
  }

  {
    // Open a popup to a same origin non-isolated page. This page should live in
    // a different BrowsingInstance in the default non-isolated
    // SiteInstanceGroup.
    ShellAddedObserver shell_observer;
    EXPECT_TRUE(ExecJs(current_frame_host(),
                       JsReplace("window.open($1);", non_isolated_page)));
    WebContentsImpl* popup = static_cast<WebContentsImpl*>(
        shell_observer.GetShell()->web_contents());
    EXPECT_TRUE(WaitForLoadStop(popup));

    SiteInstanceImpl* popup_si =
        popup->GetPrimaryMainFrame()->GetSiteInstance();
    EXPECT_FALSE(popup_si->IsCrossOriginIsolated());
    EXPECT_EQ(popup_si->group(),
              popup_si->DefaultSiteInstanceGroupForBrowsingInstance());
    EXPECT_NE(popup_si, main_frame_si);

    popup->Close();
  }
}

IN_PROC_BROWSER_TEST_P(CrossOriginOpenerPolicyBrowserTest,
                       ConsoleErrorOnWindowLocationAccess) {
  const GURL non_coop_page = https_server()->GetURL("a.test", "/title1.html");
  const GURL coop_page = https_server()->GetURL(
      "b.test",
      "/set-header?Cross-Origin-Opener-Policy-Report-Only: same-origin");

  EXPECT_TRUE(NavigateToURL(shell(), non_coop_page));

  ShellAddedObserver shell_observer;
  ASSERT_TRUE(ExecJs(current_frame_host(),
                     JsReplace("window.popup = window.open($1)", coop_page)));
  EXPECT_TRUE(WaitForLoadStop(shell_observer.GetShell()->web_contents()));

  WebContentsConsoleObserver console_observer(shell()->web_contents());
  console_observer.SetPattern(
      "Cross-Origin-Opener-Policy policy would block the window.location "
      "call.");
  ASSERT_TRUE(ExecJs(current_frame_host(), "window.popup.location"));
  ASSERT_TRUE(console_observer.Wait());
}

IN_PROC_BROWSER_TEST_P(CrossOriginOpenerPolicyBrowserTest,
                       ConsoleErrorOnWindowIndexedAccess) {
  const GURL non_coop_page = https_server()->GetURL("a.test", "/title1.html");
  const GURL coop_page = https_server()->GetURL(
      "b.test",
      "/set-header?Cross-Origin-Opener-Policy-Report-Only: same-origin");

  EXPECT_TRUE(NavigateToURL(shell(), non_coop_page));

  ShellAddedObserver shell_observer;
  ASSERT_TRUE(ExecJs(current_frame_host(),
                     JsReplace("window.popup = window.open($1)", coop_page)));
  EXPECT_TRUE(WaitForLoadStop(shell_observer.GetShell()->web_contents()));
  ASSERT_TRUE(
      ExecJs(shell_observer.GetShell()->web_contents(),
             JsReplace("const iframe = document.createElement('iframe');"
                       "iframe.src = $1;"
                       "document.body.appendChild(iframe);",
                       non_coop_page)));
  EXPECT_TRUE(WaitForLoadStop(shell_observer.GetShell()->web_contents()));

  WebContentsConsoleObserver console_observer(shell()->web_contents());
  console_observer.SetPattern(
      "Cross-Origin-Opener-Policy policy would block the window[i] call.");
  ASSERT_TRUE(ExecJs(current_frame_host(), "window.popup[0]"));
  ASSERT_TRUE(console_observer.Wait());
}

IN_PROC_BROWSER_TEST_P(CrossOriginOpenerPolicyBrowserTest,
                       ConsoleErrorOnWindowNamedAccess) {
  const GURL non_coop_page = https_server()->GetURL("a.test", "/title1.html");
  const GURL coop_page = https_server()->GetURL(
      "a.test",
      "/set-header?Cross-Origin-Opener-Policy-Report-Only: same-origin");

  EXPECT_TRUE(NavigateToURL(shell(), non_coop_page));

  ShellAddedObserver shell_observer;
  ASSERT_TRUE(ExecJs(current_frame_host(),
                     JsReplace("window.popup = window.open($1)", coop_page)));
  EXPECT_TRUE(WaitForLoadStop(shell_observer.GetShell()->web_contents()));
  ASSERT_TRUE(ExecJs(shell_observer.GetShell()->web_contents(), R"(
    const div = document.createElement("div");
    div.id = "divID";
    document.body.appendChild(div);
  )"));

  WebContentsConsoleObserver console_observer(shell()->web_contents());
  console_observer.SetPattern(
      "Cross-Origin-Opener-Policy policy would block the window[\"name\"] "
      "call.");
  EXPECT_TRUE(ExecJs(current_frame_host(), "window.popup['divID']"));
  ASSERT_TRUE(console_observer.Wait());
}

// Navigate in between two documents. Check the virtual browsing context group
// is properly updated.
IN_PROC_BROWSER_TEST_P(CrossOriginOpenerPolicyBrowserTest,
                       NavigationVirtualBrowsingContextGroupNoopener) {
  GURL::Replacements cross_origin;
  cross_origin.SetHostStr("cross-origin.example.com");

  const struct {
    GURL url_a;
    GURL url_b;
    bool expect_different_group_a_to_b;
    bool expect_different_group_b_to_a;
  } kTestCases[] = {
      {
          // unsafe-none, noopener => no change
          https_server()->GetURL("a.test",
                                 "/set-header?"
                                 "Cross-Origin-Opener-Policy: unsafe-none"),
          https_server()->GetURL(
              "a.test",
              "/set-header?"
              "Cross-Origin-Opener-Policy: noopener-allow-popups"),
          true,
          false,
      },
      {
          // Same origin, noopener => change
          https_server()->GetURL("a.test",
                                 "/set-header?"
                                 "Cross-Origin-Opener-Policy: same-origin"),
          https_server()->GetURL(
              "a.test",
              "/set-header?"
              "Cross-Origin-Opener-Policy: noopener-allow-popups"),
          true,
          true,
      },
      {
          // Same origin allow popups, noopener => change
          https_server()->GetURL(
              "a.test",
              "/set-header?"
              "Cross-Origin-Opener-Policy: same-origin-allow-popups"),
          https_server()->GetURL(
              "a.test",
              "/set-header?"
              "Cross-Origin-Opener-Policy: noopener-allow-popups"),
          true,
          true,
      },
      {
          // noopener allow popups, noopener allow popups => no change
          https_server()->GetURL(
              "a.test",
              "/set-header?"
              "Cross-Origin-Opener-Policy: noopener-allow-popups"),
          https_server()->GetURL(
              "a.test",
              "/set-header?"
              "Cross-Origin-Opener-Policy: noopener-allow-popups"),
          false,
          false,
      },
      {
          // noopener allow popups, cross-origin noopener allow popups => change
          https_server()->GetURL(
              "a.test",
              "/set-header?"
              "Cross-Origin-Opener-Policy: noopener-allow-popups"),
          https_server()
              ->GetURL("a.test",
                       "/set-header?"
                       "Cross-Origin-Opener-Policy: noopener-allow-popups")
              .ReplaceComponents(cross_origin),
          true,
          true,
      },
      {
          // unsafe-none, noopener => no change
          https_server()->GetURL("a.test",
                                 "/set-header?"
                                 "Cross-Origin-Opener-Policy: unsafe-none"),
          https_server()->GetURL(
              "a.test",
              "/set-header?"
              "Cross-Origin-Opener-Policy-Report-Only: noopener-allow-popups"),
          true,
          false,
      },
      {
          // Same origin, noopener => change
          https_server()->GetURL("a.test",
                                 "/set-header?"
                                 "Cross-Origin-Opener-Policy: same-origin"),
          https_server()->GetURL(
              "a.test",
              "/set-header?"
              "Cross-Origin-Opener-Policy-Report-Only: noopener-allow-popups"),
          true,
          true,
      },
      {
          // Same origin allow popups, noopener => change
          https_server()->GetURL(
              "a.test",
              "/set-header?"
              "Cross-Origin-Opener-Policy: same-origin-allow-popups"),
          https_server()->GetURL(
              "a.test",
              "/set-header?"
              "Cross-Origin-Opener-Policy-Report-Only: noopener-allow-popups"),
          true,
          true,
      },
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

    if (test_case.expect_different_group_a_to_b) {
      EXPECT_NE(group_1, group_2);  // url_a -> url_b.
    } else {
      EXPECT_EQ(group_1, group_2);  // url_a -> url_b.
    }
    if (test_case.expect_different_group_b_to_a) {
      EXPECT_NE(group_2, group_3);  // url_a <- url_b.
    } else {
      EXPECT_EQ(group_2, group_3);  // url_b <- url_b.
    }
  }
}

}  // namespace content
