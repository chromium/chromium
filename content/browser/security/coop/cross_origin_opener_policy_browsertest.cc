// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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

network::CrossOriginOpenerPolicy CoopSameOrigin(
    const std::optional<url::Origin>& origin = std::nullopt) {
  network::CrossOriginOpenerPolicy coop;
  coop.value = network::mojom::CrossOriginOpenerPolicyValue::kSameOrigin;
  coop.soap_by_default_value =
      network::mojom::CrossOriginOpenerPolicyValue::kSameOrigin;
  coop.origin = origin;
  return coop;
}

network::CrossOriginOpenerPolicy CoopSameOriginPlusCoep(
    const std::optional<url::Origin>& origin = std::nullopt) {
  network::CrossOriginOpenerPolicy coop;
  coop.value =
      network::mojom::CrossOriginOpenerPolicyValue::kSameOriginPlusCoep;
  coop.soap_by_default_value =
      network::mojom::CrossOriginOpenerPolicyValue::kSameOriginPlusCoep;
  coop.origin = origin;
  return coop;
}

network::CrossOriginOpenerPolicy CoopSameOriginAllowPopups(
    const std::optional<url::Origin>& origin = std::nullopt) {
  network::CrossOriginOpenerPolicy coop;
  coop.value =
      network::mojom::CrossOriginOpenerPolicyValue::kSameOriginAllowPopups;
  coop.soap_by_default_value =
      network::mojom::CrossOriginOpenerPolicyValue::kSameOriginAllowPopups;
  coop.origin = origin;
  return coop;
}

network::CrossOriginOpenerPolicy CoopRestrictProperties(
    const std::optional<url::Origin>& origin = std::nullopt) {
  network::CrossOriginOpenerPolicy coop;
  coop.value =
      network::mojom::CrossOriginOpenerPolicyValue::kRestrictProperties;
  coop.soap_by_default_value =
      network::mojom::CrossOriginOpenerPolicyValue::kRestrictProperties;
  coop.origin = origin;
  return coop;
}

network::CrossOriginOpenerPolicy CoopRestrictPropertiesPlusCoep(
    const std::optional<url::Origin>& origin = std::nullopt) {
  network::CrossOriginOpenerPolicy coop;
  coop.value =
      network::mojom::CrossOriginOpenerPolicyValue::kRestrictPropertiesPlusCoep;
  coop.soap_by_default_value =
      network::mojom::CrossOriginOpenerPolicyValue::kRestrictPropertiesPlusCoep;
  coop.origin = origin;
  return coop;
}

network::CrossOriginOpenerPolicy
CoopReportOnlyRestrictPropertiesWithSoapByDefault(
    const std::optional<url::Origin>& origin = std::nullopt) {
  network::CrossOriginOpenerPolicy coop;
  coop.report_only_value =
      network::mojom::CrossOriginOpenerPolicyValue::kRestrictProperties;
  coop.soap_by_default_value =
      network::mojom::CrossOriginOpenerPolicyValue::kSameOriginAllowPopups;
  coop.origin = origin;
  return coop;
}

network::CrossOriginOpenerPolicy
CoopReportOnlyRestrictPropertiesPlusCoepWithSoapByDefault(
    const std::optional<url::Origin>& origin = std::nullopt) {
  network::CrossOriginOpenerPolicy coop;
  coop.report_only_value =
      network::mojom::CrossOriginOpenerPolicyValue::kRestrictPropertiesPlusCoep;
  coop.soap_by_default_value =
      network::mojom::CrossOriginOpenerPolicyValue::kSameOriginAllowPopups;
  coop.origin = origin;
  return coop;
}

// This is the value of COOP when navigating to a page without COOP set:
//  - value is kUnsafeNone
//  - soap_by_default_value is kSameOriginAllowPopups
network::CrossOriginOpenerPolicy CoopUnsafeNoneWithSoapByDefault(
    const std::optional<url::Origin>& origin = std::nullopt) {
  network::CrossOriginOpenerPolicy coop;
  coop.soap_by_default_value =
      network::mojom::CrossOriginOpenerPolicyValue::kSameOriginAllowPopups;
  coop.origin = origin;
  return coop;
}

network::CrossOriginOpenerPolicy CoopUnsafeNone(
    const std::optional<url::Origin>& origin = std::nullopt) {
  network::CrossOriginOpenerPolicy coop;
  // Using the default value.
  coop.origin = origin;
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

std::unique_ptr<net::test_server::HttpResponse>
CoopAndCspSandboxRedirectHandler(const net::test_server::HttpRequest& request) {
  std::string dest =
      base::UnescapeBinaryURLComponent(request.GetURL().query_piece());
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

std::unique_ptr<net::test_server::HttpResponse>
ServeDifferentCoopOnSecondNavigation(
    unsigned int& navigation_counter,
    const net::test_server::HttpRequest& request) {
  ++navigation_counter;
  auto http_response = std::make_unique<net::test_server::BasicHttpResponse>();
  http_response->set_code(net::HttpStatusCode::HTTP_OK);
  http_response->AddCustomHeader("Cache-Control", "no-store, must-revalidate");
  if (navigation_counter > 1) {
    http_response->AddCustomHeader("Cross-Origin-Opener-Policy", "same-origin");
  } else {
    http_response->AddCustomHeader("Cross-Origin-Opener-Policy",
                                   "restrict-properties");
  }
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
    https_server_.RegisterDefaultHandler(base::BindRepeating(
        &net::test_server::HandlePrefixedRequest,
        "/serve-different-coop-on-second-navigation",
        base::BindRepeating(&ServeDifferentCoopOnSecondNavigation,
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

// Same as CrossOriginOpenerPolicyBrowserTest, but enables COOP:
// restrict-properties.
class CoopRestrictPropertiesBrowserTest
    : public CrossOriginOpenerPolicyBrowserTest {
 public:
  CoopRestrictPropertiesBrowserTest() {
    feature_list_.InitWithFeatures({network::features::kCoopRestrictProperties},
                                   {});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Same as CoopRestrictPropertiesBrowserTest, but skips on platforms not
// providing full site isolation, to help test the existence of proxies. Also
// provides helper functions to leverage FrameTreeVisualizer. Inherits its
// parametrization for RenderDocument and BackForwardCache.
class CoopRestrictPropertiesProxiesBrowserTest
    : public CoopRestrictPropertiesBrowserTest {
 public:
  void SetUpOnMainThread() override {
    // These tests verify what proxies exist using DepictFrameTree and exact
    // string comparison. Return early if we would not put cross-origin
    // iframes and popups in their own processes, which would modify the proxy
    // structure.
    if (!AreAllSitesIsolatedForTesting()) {
      GTEST_SKIP();
    }
    CoopRestrictPropertiesBrowserTest::SetUpOnMainThread();
  }

  std::string DepictFrameTree(FrameTreeNode* node) {
    return visualizer_.DepictFrameTree(node);
  }

  WebContentsImpl* OpenPopupAndWaitForInitialRFHDeletion(
      RenderFrameHostImpl* opener_rfh,
      const GURL& url,
      const std::string& name) {
    // First open a popup to the initial empty document, and then navigate it to
    // the final url. This allows waiting on the deletion of the initial empty
    // document proxies and having a clean state for proxy checking.
    ShellAddedObserver shell_observer;
    CHECK(ExecJs(opener_rfh, JsReplace("window.open('', $1);", name)));
    WebContentsImpl* popup_window = static_cast<WebContentsImpl*>(
        shell_observer.GetShell()->web_contents());
    RenderFrameHostWrapper initial_popup_rfh(
        popup_window->GetPrimaryMainFrame());
    CHECK(NavigateToURLFromRenderer(initial_popup_rfh.get(), url));
    CHECK(initial_popup_rfh.WaitUntilRenderFrameDeleted());
    return popup_window;
  }

 private:
  FrameTreeVisualizer visualizer_;
};

// Same as CoopRestrictPropertiesBrowserTest, but uses the new
// BrowsingContextState mode that swaps BrowsingContextState when navigating
// cross BrowsingInstance. Inherits its parametrization for RenderDocument and
// BackForwardCache.
class CoopRestrictPropertiesWithNewBrowsingContextStateModeBrowserTest
    : public CoopRestrictPropertiesBrowserTest {
 public:
  CoopRestrictPropertiesWithNewBrowsingContextStateModeBrowserTest() {
    feature_list_.InitWithFeatures(
        {features::kNewBrowsingContextStateOnBrowsingContextGroupSwap}, {});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

using CoopRestrictPropertiesAccessBrowserTest =
    CoopRestrictPropertiesBrowserTest;

static constexpr char kCoopRpErrorMessageRegex[] =
    ".*Cross-Origin-Opener-Policy: 'restrict-properties' blocked the access.";

using CoopRestrictPropertiesReportingBrowserTest =
    CoopRestrictPropertiesBrowserTest;

// Certain features are only active when SiteIsolation is off or restricted.
// This is the case for example for Default SiteInstances that are used on
// Android to limit the number of processes. Testing these particularities of
// the process model and their interaction with cross-origin isolation requires
// to disable SiteIsolation.
class NoSiteIsolationCrossOriginIsolationBrowserTest
    : public CrossOriginOpenerPolicyBrowserTest {
 public:
  NoSiteIsolationCrossOriginIsolationBrowserTest() {
    // Disable the heuristic to isolate COOP pages from the default
    // SiteInstance. This is otherwise on by default on Android.
    feature_list_.InitWithFeatures(
        {}, {features::kSiteIsolationForCrossOriginOpenerPolicy});
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

  EXPECT_EQ(main_rfh->cross_origin_opener_policy(),
            CoopSameOrigin(url::Origin::Create(starting_page)));
  EXPECT_EQ(popup_rfh->cross_origin_opener_policy(),
            CoopSameOrigin(url::Origin::Create(starting_page)));

  EXPECT_TRUE(popup_rfh->policy_container_host()
                  ->policies()
                  .allow_cross_origin_isolation);
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
            CoopSameOriginAllowPopups(url::Origin::Create(starting_page)));
  EXPECT_EQ(popup_rfh->cross_origin_opener_policy(),
            CoopSameOriginAllowPopups(url::Origin::Create(starting_page)));

  EXPECT_TRUE(popup_rfh->policy_container_host()
                  ->policies()
                  .allow_cross_origin_isolation);
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

  EXPECT_EQ(main_rfh->cross_origin_opener_policy(),
            CoopSameOrigin(url::Origin::Create(starting_page)));
  EXPECT_EQ(popup_rfh->cross_origin_opener_policy(), CoopUnsafeNone());

  EXPECT_FALSE(popup_rfh->policy_container_host()
                   ->policies()
                   .allow_cross_origin_isolation);
}

IN_PROC_BROWSER_TEST_P(CoopRestrictPropertiesBrowserTest,
                       NewPopupCOOP_CrossOriginInheritsRestrictProperties) {
  GURL starting_page(https_server()->GetURL(
      "a.test", "/set-header?cross-origin-opener-policy: restrict-properties"));
  GURL url_b(https_server()->GetURL("b.test", "/empty.html"));
  GURL url_b_with_headers(https_server()->GetURL(
      "b.test", "/set-header?cross-origin-opener-policy: restrict-properties"));

  EXPECT_TRUE(NavigateToURL(shell(), starting_page));

  RenderFrameHostImpl* main_rfh = current_frame_host();

  // Create a cross origin child frame.
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

  auto* popup_webcontents =
      static_cast<WebContentsImpl*>(shell_observer.GetShell()->web_contents());
  RenderFrameHostImpl* popup_rfh = popup_webcontents->GetPrimaryMainFrame();

  EXPECT_EQ(main_rfh->cross_origin_opener_policy(),
            CoopRestrictProperties(url::Origin::Create(starting_page)));
  EXPECT_EQ(popup_rfh->cross_origin_opener_policy(),
            CoopRestrictProperties(url::Origin::Create(starting_page)));

  EXPECT_FALSE(popup_rfh->policy_container_host()
                   ->policies()
                   .allow_cross_origin_isolation);

  ASSERT_TRUE(NavigateToURL(popup_webcontents, url_b_with_headers));

  popup_rfh = popup_webcontents->GetPrimaryMainFrame();
  EXPECT_EQ(popup_rfh->cross_origin_opener_policy(),
            CoopRestrictProperties(url::Origin::Create(url_b)));
  EXPECT_TRUE(popup_webcontents->GetPrimaryMainFrame()
                  ->policy_container_host()
                  ->policies()
                  .allow_cross_origin_isolation);
}

IN_PROC_BROWSER_TEST_P(
    CoopRestrictPropertiesBrowserTest,
    NewPopupCOOP_CrossOriginInheritsRestrictPropertiesPlusCoep) {
  GURL starting_page(
      https_server()->GetURL("a.test",
                             "/set-header"
                             "?cross-origin-opener-policy: restrict-properties"
                             "&cross-origin-embedder-policy: credentialless"));
  GURL url_b(https_server()->GetURL("b.test", "/empty.html"));
  GURL url_b_with_headers(
      https_server()->GetURL("b.test",
                             "/set-header"
                             "?cross-origin-opener-policy: restrict-properties"
                             "&cross-origin-embedder-policy: credentialless"));

  EXPECT_TRUE(NavigateToURL(shell(), starting_page));

  RenderFrameHostImpl* main_rfh = current_frame_host();

  // Create a cross origin child frame.
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

  auto* popup_webcontents =
      static_cast<WebContentsImpl*>(shell_observer.GetShell()->web_contents());
  RenderFrameHostImpl* popup_rfh = popup_webcontents->GetPrimaryMainFrame();

  EXPECT_EQ(main_rfh->cross_origin_opener_policy(),
            CoopRestrictPropertiesPlusCoep(url::Origin::Create(starting_page)));
  EXPECT_EQ(popup_rfh->cross_origin_opener_policy(),
            CoopRestrictPropertiesPlusCoep(url::Origin::Create(starting_page)));

  EXPECT_FALSE(popup_rfh->policy_container_host()
                   ->policies()
                   .allow_cross_origin_isolation);

  ASSERT_TRUE(NavigateToURL(popup_webcontents, url_b_with_headers));

  popup_rfh = popup_webcontents->GetPrimaryMainFrame();
  EXPECT_EQ(popup_rfh->cross_origin_opener_policy(),
            CoopRestrictPropertiesPlusCoep(url::Origin::Create(url_b)));
  EXPECT_TRUE(popup_webcontents->GetPrimaryMainFrame()
                  ->policy_container_host()
                  ->policies()
                  .allow_cross_origin_isolation);
}

IN_PROC_BROWSER_TEST_P(
    CoopRestrictPropertiesBrowserTest,
    NewPopupCOOP_CrossOriginInheritsReportOnlyRestrictProperties) {
  GURL starting_page(https_server()->GetURL(
      "a.test",
      "/set-header"
      "?cross-origin-opener-policy-report-only: restrict-properties"));
  GURL url_b(https_server()->GetURL("b.test", "/empty.html"));
  GURL url_b_with_headers(https_server()->GetURL(
      "b.test",
      "/set-header"
      "?cross-origin-opener-policy-report-only: restrict-properties"));

  EXPECT_TRUE(NavigateToURL(shell(), starting_page));

  RenderFrameHostImpl* main_rfh = current_frame_host();

  // Create a cross origin child frame.
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

  auto* popup_webcontents =
      static_cast<WebContentsImpl*>(shell_observer.GetShell()->web_contents());
  RenderFrameHostImpl* popup_rfh = popup_webcontents->GetPrimaryMainFrame();

  EXPECT_EQ(main_rfh->cross_origin_opener_policy(),
            CoopReportOnlyRestrictPropertiesWithSoapByDefault(
                url::Origin::Create(starting_page)));
  EXPECT_EQ(popup_rfh->cross_origin_opener_policy(),
            CoopReportOnlyRestrictPropertiesWithSoapByDefault(
                url::Origin::Create(starting_page)));

  EXPECT_FALSE(popup_rfh->policy_container_host()
                   ->policies()
                   .allow_cross_origin_isolation);

  ASSERT_TRUE(NavigateToURL(popup_webcontents, url_b_with_headers));

  popup_rfh = popup_webcontents->GetPrimaryMainFrame();
  EXPECT_EQ(popup_rfh->cross_origin_opener_policy(),
            CoopReportOnlyRestrictPropertiesWithSoapByDefault(
                url::Origin::Create(url_b)));
  EXPECT_TRUE(popup_webcontents->GetPrimaryMainFrame()
                  ->policy_container_host()
                  ->policies()
                  .allow_cross_origin_isolation);
}

IN_PROC_BROWSER_TEST_P(
    CoopRestrictPropertiesBrowserTest,
    NewPopupCOOP_CrossOriginInheritsReportOnlyRestrictPropertiesPlusCoep) {
  GURL starting_page(https_server()->GetURL(
      "a.test",
      "/set-header"
      "?cross-origin-opener-policy-report-only: restrict-properties"
      "&cross-origin-embedder-policy: credentialless"));
  GURL url_b(https_server()->GetURL("b.test", "/empty.html"));
  GURL url_b_with_headers(https_server()->GetURL(
      "b.test",
      "/set-header"
      "?cross-origin-opener-policy-report-only: restrict-properties"
      "&cross-origin-embedder-policy: credentialless"));

  EXPECT_TRUE(NavigateToURL(shell(), starting_page));

  RenderFrameHostImpl* main_rfh = current_frame_host();

  // Create a cross origin child frame.
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

  auto* popup_webcontents =
      static_cast<WebContentsImpl*>(shell_observer.GetShell()->web_contents());
  RenderFrameHostImpl* popup_rfh = popup_webcontents->GetPrimaryMainFrame();

  EXPECT_EQ(main_rfh->cross_origin_opener_policy(),
            CoopReportOnlyRestrictPropertiesPlusCoepWithSoapByDefault(
                url::Origin::Create(starting_page)));
  EXPECT_EQ(popup_rfh->cross_origin_opener_policy(),
            CoopReportOnlyRestrictPropertiesPlusCoepWithSoapByDefault(
                url::Origin::Create(starting_page)));

  EXPECT_FALSE(popup_rfh->policy_container_host()
                   ->policies()
                   .allow_cross_origin_isolation);

  ASSERT_TRUE(NavigateToURL(popup_webcontents, url_b_with_headers));

  popup_rfh = popup_webcontents->GetPrimaryMainFrame();
  EXPECT_EQ(popup_rfh->cross_origin_opener_policy(),
            CoopReportOnlyRestrictPropertiesPlusCoepWithSoapByDefault(
                url::Origin::Create(url_b)));
  EXPECT_TRUE(popup_webcontents->GetPrimaryMainFrame()
                  ->policy_container_host()
                  ->policies()
                  .allow_cross_origin_isolation);
}

IN_PROC_BROWSER_TEST_P(
    CoopRestrictPropertiesBrowserTest,
    NewPopupCOOP_SameOriginSubframeCanNavigatePopupOpenedByMainFrame) {
  GURL starting_page(https_server()->GetURL(
      "a.test",
      "/set-header"
      "?cross-origin-opener-policy: restrict-properties"));
  GURL url_a(https_server()->GetURL("a.test", "/empty.html"));
  GURL url_b(https_server()->GetURL("b.test", "/empty.html"));

  ASSERT_TRUE(NavigateToURL(shell(), starting_page));

  RenderFrameHostImpl* main_rfh = current_frame_host();

  // a.test embeds a.test
  ASSERT_TRUE(ExecJs(main_rfh, JsReplace(R"(
    const frame = document.createElement('iframe');
    frame.src = $1;
    document.body.appendChild(frame);
  )",
                                         url_a)));
  ASSERT_TRUE(WaitForLoadStop(web_contents()));

  ShellAddedObserver shell_observer;
  // a.test opens a popup to about:blank.
  ASSERT_TRUE(ExecJs(main_rfh, "window.open('about:blank', 'popup')"));

  auto* popup_webcontents =
      static_cast<WebContentsImpl*>(shell_observer.GetShell()->web_contents());
  RenderFrameHostImpl* popup_rfh = popup_webcontents->GetPrimaryMainFrame();

  // Expect popup's origin to be a.test.
  EXPECT_EQ(popup_rfh->GetLastCommittedOrigin(),
            url::Origin::Create(starting_page));

  RenderFrameHostImpl* iframe_rfh = main_rfh->child_at(0)->current_frame_host();
  // a.test subframe navigates the popup to b.test (using named targeting)
  ASSERT_TRUE(ExecJs(iframe_rfh, JsReplace("window.open($1, 'popup')", url_b)));

  ASSERT_TRUE(WaitForLoadStop(popup_webcontents));

  popup_rfh = popup_webcontents->GetPrimaryMainFrame();
  // Expect popup doesn't navigate, and its origin is still a.test.
  EXPECT_EQ(popup_rfh->GetLastCommittedOrigin(), url::Origin::Create(url_b));
  EXPECT_TRUE(popup_rfh->policy_container_host()
                  ->policies()
                  .allow_cross_origin_isolation);
}

IN_PROC_BROWSER_TEST_P(
    CoopRestrictPropertiesBrowserTest,
    NewPopupCOOP_CrossOriginSubframeCannotNavigatePopupOpenedByMainFrame) {
  GURL starting_page(https_server()->GetURL(
      "a.test",
      "/set-header"
      "?cross-origin-opener-policy: restrict-properties"));
  GURL url_b(https_server()->GetURL("b.test", "/empty.html"));

  ASSERT_TRUE(NavigateToURL(shell(), starting_page));

  RenderFrameHostImpl* main_rfh = current_frame_host();

  // a.test embeds b.test
  ASSERT_TRUE(ExecJs(main_rfh, JsReplace(R"(
    const frame = document.createElement('iframe');
    frame.src = $1;
    document.body.appendChild(frame);
  )",
                                         url_b)));
  ASSERT_TRUE(WaitForLoadStop(web_contents()));

  ShellAddedObserver shell_observer;
  // a.test opens a popup to about:blank.
  ASSERT_TRUE(ExecJs(main_rfh, "window.open('about:blank', 'popup')"));

  auto* popup_webcontents =
      static_cast<WebContentsImpl*>(shell_observer.GetShell()->web_contents());
  RenderFrameHostImpl* popup_rfh = popup_webcontents->GetPrimaryMainFrame();

  // Expect popup's origin to be a.test.
  EXPECT_EQ(popup_rfh->GetLastCommittedOrigin(),
            url::Origin::Create(starting_page));

  RenderFrameHostImpl* iframe_rfh = main_rfh->child_at(0)->current_frame_host();
  // b.test navigates the popup to b.test (using named targeting)
  ASSERT_TRUE(ExecJs(iframe_rfh, JsReplace("window.open($1, 'popup')", url_b)));

  ASSERT_TRUE(WaitForLoadStop(popup_webcontents));

  popup_rfh = popup_webcontents->GetPrimaryMainFrame();
  // Expect popup doesn't navigate, and its origin is still a.test.
  EXPECT_EQ(popup_rfh->GetLastCommittedOrigin(),
            url::Origin::Create(starting_page));
  EXPECT_TRUE(popup_rfh->policy_container_host()
                  ->policies()
                  .allow_cross_origin_isolation);
}

IN_PROC_BROWSER_TEST_P(
    CoopRestrictPropertiesBrowserTest,
    NewPopupCOOP_CrossOriginSubframeCannotNavigatePopupOpenedByMainFrameToAboutBlank) {
  GURL starting_page(https_server()->GetURL(
      "a.test",
      "/set-header"
      "?cross-origin-opener-policy: restrict-properties"));
  GURL url_b(https_server()->GetURL("b.test", "/empty.html"));

  ASSERT_TRUE(NavigateToURL(shell(), starting_page));

  RenderFrameHostImpl* main_rfh = current_frame_host();

  // a.test embeds b.test
  ASSERT_TRUE(ExecJs(main_rfh, JsReplace(R"(
    const frame = document.createElement('iframe');
    frame.src = $1;
    document.body.appendChild(frame);
  )",
                                         url_b)));
  ASSERT_TRUE(WaitForLoadStop(web_contents()));

  ShellAddedObserver shell_observer;
  // a.test opens a popup to about:blank.
  ASSERT_TRUE(ExecJs(main_rfh, "window.open('about:blank', 'popup')"));

  auto* popup_webcontents =
      static_cast<WebContentsImpl*>(shell_observer.GetShell()->web_contents());
  RenderFrameHostImpl* popup_rfh = popup_webcontents->GetPrimaryMainFrame();

  // Expect popup's origin to be a.test.
  EXPECT_EQ(popup_rfh->GetLastCommittedOrigin(),
            url::Origin::Create(starting_page));

  RenderFrameHostImpl* iframe_rfh = main_rfh->child_at(0)->current_frame_host();
  // b.test navigates the popup to about:blank (using named targeting)
  ASSERT_TRUE(ExecJs(iframe_rfh, "window.open('about:blank', 'popup')"));

  ASSERT_TRUE(WaitForLoadStop(popup_webcontents));

  popup_rfh = popup_webcontents->GetPrimaryMainFrame();
  // Expect popup doesn't navigate, and its origin is still a.test.
  EXPECT_EQ(popup_rfh->GetLastCommittedOrigin(),
            url::Origin::Create(starting_page));
  EXPECT_TRUE(popup_rfh->policy_container_host()
                  ->policies()
                  .allow_cross_origin_isolation);
}

IN_PROC_BROWSER_TEST_P(
    CoopRestrictPropertiesBrowserTest,
    NewPopupCOOP_CrossOriginSubframeCannotNavigatePopupOpenedByMainFrameWithCoopRpToAboutBlank) {
  GURL starting_page(https_server()->GetURL(
      "a.test",
      "/set-header"
      "?cross-origin-opener-policy: restrict-properties"));
  GURL url_b(https_server()->GetURL("b.test", "/empty.html"));

  ASSERT_TRUE(NavigateToURL(shell(), starting_page));

  RenderFrameHostImpl* main_rfh = current_frame_host();

  // a.test embeds b.test
  ASSERT_TRUE(ExecJs(main_rfh, JsReplace(R"(
    const frame = document.createElement('iframe');
    frame.src = $1;
    document.body.appendChild(frame);
  )",
                                         url_b)));
  ASSERT_TRUE(WaitForLoadStop(web_contents()));

  ShellAddedObserver shell_observer;
  // a.test opens a popup to a.test with COOP RP.
  ASSERT_TRUE(
      ExecJs(main_rfh, JsReplace("window.open($1, 'popup')", starting_page)));

  auto* popup_webcontents =
      static_cast<WebContentsImpl*>(shell_observer.GetShell()->web_contents());
  RenderFrameHostImpl* popup_rfh = popup_webcontents->GetPrimaryMainFrame();

  // Expect popup's origin to be a.test.
  EXPECT_EQ(popup_rfh->GetLastCommittedOrigin(),
            url::Origin::Create(starting_page));

  RenderFrameHostImpl* iframe_rfh = main_rfh->child_at(0)->current_frame_host();
  // b.test navigates the popup to about:blank (using named targeting)
  ASSERT_TRUE(ExecJs(iframe_rfh, "window.open('about:blank', 'popup')"));

  ASSERT_TRUE(WaitForLoadStop(popup_webcontents));

  popup_rfh = popup_webcontents->GetPrimaryMainFrame();
  // Expect popup doesn't navigate, and its origin is still a.test.
  EXPECT_EQ(popup_rfh->GetLastCommittedOrigin(),
            url::Origin::Create(starting_page));
  EXPECT_TRUE(popup_rfh->policy_container_host()
                  ->policies()
                  .allow_cross_origin_isolation);
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
  EXPECT_EQ(popup_rfh->cross_origin_opener_policy(),
            CoopSameOrigin(url::Origin::Create(starting_page)));
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
  EXPECT_EQ(popup_rfh->cross_origin_opener_policy(),
            CoopSameOriginPlusCoep(url::Origin::Create(starting_page)));
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
  EXPECT_EQ(popup_rfh->cross_origin_opener_policy(),
            CoopSameOriginPlusCoep(url::Origin::Create(starting_page)));
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
            CoopSameOriginAllowPopups(url::Origin::Create(starting_page)));
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
  EXPECT_EQ(popup_rfh->cross_origin_opener_policy(),
            CoopSameOrigin(url::Origin::Create(starting_page)));
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
  EXPECT_EQ(
      second_popup_rfh->cross_origin_opener_policy(),
      CoopUnsafeNoneWithSoapByDefault(url::Origin::Create(starting_page)));
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
  EXPECT_TRUE(web_contents()
                  ->GetPrimaryMainFrame()
                  ->cross_origin_opener_policy()
                  .IsEqualExcludingOrigin(CoopSameOrigin()));
  EXPECT_TRUE(web_contents()
                  ->GetPrimaryMainFrame()
                  ->cross_origin_opener_policy()
                  .origin->opaque());
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
    CHECK(
        navigation_request->response()
            ->parsed_headers->cross_origin_opener_policy.IsEqualExcludingOrigin(
                expected_coop_));
    CHECK(!navigation_request->response()
               ->parsed_headers->cross_origin_opener_policy.origin.has_value());
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
      CoopSameOriginPlusCoep(url::Origin::Create(redirect_final_page)));

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
            CoopUnsafeNone(url::Origin::Create(non_coop_page)));

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
              CoopSameOrigin(url::Origin::Create(coop_page)));

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
      coop_navigation.WaitForSpeculativeRenderFrameHostCreation();
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
              CoopSameOrigin(url::Origin::Create(non_coop_page)));

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
              CoopUnsafeNone(url::Origin::Create(non_coop_page)));

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
      non_coop_navigation.WaitForSpeculativeRenderFrameHostCreation();
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
              CoopUnsafeNone(url::Origin::Create(non_coop_page)));

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
    EXPECT_EQ(
        current_frame_host()->cross_origin_opener_policy(),
        CoopSameOriginAllowPopups(url::Origin::Create(coop_allow_popups_page)));

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
    EXPECT_EQ(
        current_frame_host()->cross_origin_opener_policy(),
        CoopSameOriginAllowPopups(url::Origin::Create(coop_allow_popups_page)));

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
      coop_navigation.WaitForSpeculativeRenderFrameHostCreation();
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
    EXPECT_EQ(
        current_frame_host()->cross_origin_opener_policy(),
        CoopSameOriginAllowPopups(url::Origin::Create(coop_allow_popups_page)));

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
  for (const auto& test : kTestCases)
    RunTest(test, shell());
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
  for (const auto& test : kTestCases)
    RunTest(test, shell());
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
  for (const auto& test : kTestCases)
    RunTest(test, shell());
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
  // Use of COOP/COEP headers should not cause SiteInfo::is_origin_keyed() to
  // return true. The metrics that track OriginAgentCluster isolation expect
  // is_origin_keyed() to refer only to the OriginAgentCluster header.
  EXPECT_FALSE(current_si->GetSiteInfo().requires_origin_keyed_process());
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
    EXPECT_NE(current_si->GetProcess(), previous_si->GetProcess());
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
  EXPECT_TRUE(CoopUnsafeNone().IsEqualExcludingOrigin(
      popup_web_contents->GetPrimaryMainFrame()->cross_origin_opener_policy()));

  EXPECT_TRUE(popup_web_contents->GetPrimaryMainFrame()
                  ->cross_origin_opener_policy()
                  .origin->opaque());

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
  EXPECT_TRUE(CoopUnsafeNone().IsEqualExcludingOrigin(
      popup_web_contents->GetPrimaryMainFrame()->cross_origin_opener_policy()));

  EXPECT_TRUE(popup_web_contents->GetPrimaryMainFrame()
                  ->cross_origin_opener_policy()
                  .origin->opaque());
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
  EXPECT_EQ(main_rfh->cross_origin_opener_policy(),
            CoopSameOrigin(url::Origin::Create(isolated_page)));

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
  if (!SiteIsolationPolicy::UseDedicatedProcessesForAllSites())
    return;

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
  int rph_id_1 = current_frame_host()->GetProcess()->GetID();
  EXPECT_TRUE(NavigateToURL(shell(), url_2));
  int rph_id_2 = current_frame_host()->GetProcess()->GetID();
  EXPECT_TRUE(NavigateToURL(shell(), url_3));
  int rph_id_3 = current_frame_host()->GetProcess()->GetID();

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
  int rph_id_1 = current_frame_host()->GetProcess()->GetID();
  EXPECT_TRUE(NavigateToURL(shell(), url_2));
  int rph_id_2 = current_frame_host()->GetProcess()->GetID();
  EXPECT_TRUE(NavigateToURL(shell(), url_3));
  int rph_id_3 = current_frame_host()->GetProcess()->GetID();

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
  int rph_id_1 = current_frame_host()->GetProcess()->GetID();
  EXPECT_TRUE(NavigateToURL(shell(), url_2));
  int rph_id_2 = current_frame_host()->GetProcess()->GetID();
  EXPECT_TRUE(NavigateToURL(shell(), url_3));
  int rph_id_3 = current_frame_host()->GetProcess()->GetID();

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
  int rph_id_1 = current_frame_host()->GetProcess()->GetID();
  EXPECT_TRUE(NavigateToURL(shell(), url_2));
  int rph_id_2 = current_frame_host()->GetProcess()->GetID();
  EXPECT_TRUE(NavigateToURL(shell(), url_3));
  int rph_id_3 = current_frame_host()->GetProcess()->GetID();

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
  int rph_id_1 = current_frame_host()->GetProcess()->GetID();
  EXPECT_TRUE(NavigateToURL(shell(), url_2));
  int rph_id_2 = current_frame_host()->GetProcess()->GetID();
  EXPECT_TRUE(NavigateToURL(shell(), url_3));
  int rph_id_3 = current_frame_host()->GetProcess()->GetID();

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
  int rph_id_1 = current_frame_host()->GetProcess()->GetID();

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
  int rph_id_2 = speculative_rfh->GetProcess()->GetID();
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
  int rph_id_3 = current_frame_host()->GetProcess()->GetID();
  EXPECT_EQ(rph_id_2, rph_id_3);

  // The original speculative RFH should always be destroyed.
  //
  // Subtle note: this happens even when bfcache is enabled. With bfcache,
  // we force a BrowsingInstance swap at the very beginning when the navigation
  // to `url_2` starts.  So when we learn about COOP at response time, the
  // candidate (speculative RFH's) SiteInstance is already in a fresh
  // BrowsingInstance. However, it cannot be reused, because COOP requires a
  // BrowsingInstance with b.test as its common_coop_origin(), and the
  // candidate SiteInstance's BrowsingInstance has no common_coop_origin(), so
  // it cannot be reused, and we end up creating a new speculative RFH and
  // destroying the original one.
  EXPECT_TRUE(speculative_rfh.IsDestroyed());
}

// Ensure that same-site navigations that result in a COOP mismatch avoid an
// unnecessary process swap when those navigations happen in a
// BrowsingContextGroup of size 1 (in this case, in the same WebContents).
IN_PROC_BROWSER_TEST_P(CrossOriginOpenerPolicyBrowserTest,
                       NoExtraProcessSwapFromSameSiteCOOPMismatch) {
  GURL url_1(https_server()->GetURL("a.test", "/empty.html"));
  GURL url_2(https_server()->GetURL(
      "a.test", "/set-header?Cross-Origin-Opener-Policy: same-origin"));

  // Navigate to a non-COOP URL.
  EXPECT_TRUE(NavigateToURL(shell(), url_1));
  int rph_id_1 = current_frame_host()->GetProcess()->GetID();
  bool rph_1_is_locked =
      current_frame_host()->GetProcess()->GetProcessLock().is_locked_to_site();

  // Start a navigation to a page on a.test that will have COOP headers.
  TestNavigationManager navigation(web_contents(), url_2);
  EXPECT_TRUE(BeginNavigateToURLFromRenderer(web_contents(), url_2));
  EXPECT_TRUE(navigation.WaitForRequestStart());

  // When the back-forward cache is enabled, or when RenderDocument is used, we
  // will get a speculative RenderFrameHost, which should reuse the existing
  // process because the navigation is same-site.  Otherwise, the navigation
  // should stay in the current RenderFrameHost.
  int rph_id_2;
  if (IsBackForwardCacheEnabled() || ShouldCreateNewHostForAllFrames()) {
    navigation.WaitForSpeculativeRenderFrameHostCreation();
    RenderFrameHost* speculative_rfh = web_contents()
                                           ->GetPrimaryFrameTree()
                                           .root()
                                           ->render_manager()
                                           ->speculative_frame_host();
    ASSERT_TRUE(speculative_rfh);
    rph_id_2 = speculative_rfh->GetProcess()->GetID();
    EXPECT_EQ(rph_id_1, rph_id_2);
  } else {
    ASSERT_FALSE(web_contents()
                     ->GetPrimaryFrameTree()
                     .root()
                     ->render_manager()
                     ->speculative_frame_host());
    rph_id_2 = rph_id_1;
  }

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
  int rph_id_3 = current_frame_host()->GetProcess()->GetID();
  if (SiteIsolationPolicy::IsSiteIsolationForCOOPEnabled()) {
    EXPECT_NE(rph_id_2, rph_id_3);
    EXPECT_FALSE(rph_1_is_locked);
    EXPECT_TRUE(current_frame_host()
                    ->GetProcess()
                    ->GetProcessLock()
                    .is_locked_to_site());
  } else {
    EXPECT_EQ(rph_id_2, rph_id_3);
  }
}

// Verify that there's no extra process swap during a same-site navigation from
// one COOP page to another COOP page.
IN_PROC_BROWSER_TEST_P(CrossOriginOpenerPolicyBrowserTest,
                       NavigatingFromCOOPToCOOPHasNoExtraProcessCreation) {
  GURL url_1(https_server()->GetURL(
      "a.test", "/set-header?Cross-Origin-Opener-Policy: same-origin"));
  GURL url_2(https_server()->GetURL(
      "a.test", "/set-header?Cross-Origin-Opener-Policy: same-origin&2"));

  // Navigate to a COOP URL.
  EXPECT_TRUE(NavigateToURL(shell(), url_1));
  int rph_id_1 = current_frame_host()->GetProcess()->GetID();

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
    rph_id_2 = speculative_rfh->GetProcess()->GetID();
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
  int rph_id_3 = current_frame_host()->GetProcess()->GetID();
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
  GURL url_1(https_server()->GetURL("a.test", "/empty.html"));
  GURL url_2(https_server()->GetURL(
      "a.test", "/set-header?Cross-Origin-Opener-Policy: same-origin"));

  // Navigate to a non-COOP URL.
  EXPECT_TRUE(NavigateToURL(shell(), url_1));
  int rph_id_1 = current_frame_host()->GetProcess()->GetID();

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
  int rph_id_2 = popup_contents->GetPrimaryMainFrame()->GetProcess()->GetID();
  EXPECT_NE(rph_id_1, rph_id_2);
}

// Tests the behavior around COOP BrowsingInstance swap when prerendering a COOP
// page.
// Regression test for crbug.com/1519131.
IN_PROC_BROWSER_TEST_P(ProcessReuseOnPrerenderCOOPSwapBrowserTest,
                       COOPSwapForPrerenderingCOOPPage) {
  GURL initial_page(https_server()->GetURL("a.test", "/empty.html"));
  GURL prerender_page(https_server()->GetURL(
      "a.test", "/set-header?Cross-Origin-Opener-Policy: same-origin"));

  // Navigate to an initial non-COOP page.
  ASSERT_TRUE(NavigateToURL(shell(), initial_page));
  RenderFrameHostImpl* rfh_1 = current_frame_host();
  scoped_refptr<SiteInstanceImpl> si_1 = rfh_1->GetSiteInstance();
  base::UnguessableToken bi_token_1 =
      rfh_1->GetSiteInstance()->browsing_instance_token();
  int rph_id_1 = rfh_1->GetProcess()->GetID();

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
  int rph_id_2 = rfh_2->GetProcess()->GetID();
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
  int rph_id_3 = rfh_3->GetProcess()->GetID();
  EXPECT_NE(rfh_2, rfh_3);
  EXPECT_NE(si_2, si_3);
  EXPECT_NE(bi_token_2, bi_token_3);
  // Renderer process is reused.
  EXPECT_EQ(rph_id_2, rph_id_3);
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
                         CoopRestrictPropertiesBrowserTest,
                         kTestParams,
                         CrossOriginOpenerPolicyBrowserTest::DescribeParams);
INSTANTIATE_TEST_SUITE_P(All,
                         CoopRestrictPropertiesProxiesBrowserTest,
                         kTestParams,
                         CrossOriginOpenerPolicyBrowserTest::DescribeParams);
INSTANTIATE_TEST_SUITE_P(
    All,
    CoopRestrictPropertiesWithNewBrowsingContextStateModeBrowserTest,
    kTestParams,
    CrossOriginOpenerPolicyBrowserTest::DescribeParams);
INSTANTIATE_TEST_SUITE_P(All,
                         CoopRestrictPropertiesAccessBrowserTest,
                         kTestParams,
                         CrossOriginOpenerPolicyBrowserTest::DescribeParams);
INSTANTIATE_TEST_SUITE_P(All,
                         NoSiteIsolationCrossOriginIsolationBrowserTest,
                         kTestParams,
                         CrossOriginOpenerPolicyBrowserTest::DescribeParams);
INSTANTIATE_TEST_SUITE_P(All,
                         CoopRestrictPropertiesReportingBrowserTest,
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

  EXPECT_THAT(postSharedArrayBuffer.error,
              HasSubstr("Failed to execute 'postMessage' on 'Window'"));
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
                         kTestParams,
                         CrossOriginOpenerPolicyBrowserTest::DescribeParams);

IN_PROC_BROWSER_TEST_P(SharedArrayBufferOnDesktopBrowserTest,
                       DesktopHasSharedArrayBuffer) {
  CHECK(!base::FeatureList::IsEnabled(features::kSharedArrayBuffer));
  GURL url = https_server()->GetURL("a.test", "/empty.html");
  EXPECT_TRUE(NavigateToURL(shell(), url));
  EXPECT_EQ(false, EvalJs(current_frame_host(), "self.crossOriginIsolated"));
#if !BUILDFLAG(IS_ANDROID)
  EXPECT_EQ(true,
            EvalJs(current_frame_host(), "'SharedArrayBuffer' in globalThis"));
#else   // !BUILDFLAG(IS_ANDROID)
  EXPECT_EQ(false,
            EvalJs(current_frame_host(), "'SharedArrayBuffer' in globalThis"));
#endif  // !BUILDFLAG(IS_ANDROID)
}

IN_PROC_BROWSER_TEST_P(SharedArrayBufferOnDesktopBrowserTest,
                       DesktopTransferSharedArrayBuffer) {
  CHECK(!base::FeatureList::IsEnabled(features::kSharedArrayBuffer));
  GURL main_url = https_server()->GetURL("a.test", "/empty.html");
  GURL iframe_url = https_server()->GetURL("a.test", "/empty.html");
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

#if !BUILDFLAG(IS_ANDROID)
  EXPECT_TRUE(ExecJs(sub_document, R"(
    const sab = new SharedArrayBuffer(1234);
    parent.postMessage(sab, "*");
  )"));

  EXPECT_EQ(1234, EvalJs(main_document, "g_sab_size"));
#else   // !BUILDFLAG(IS_ANDROID)
  EXPECT_FALSE(ExecJs(sub_document, R"(
    const sab = new SharedArrayBuffer(1234);
    parent.postMessage(sab, "*");
  )"));
#endif  // !BUILDFLAG(IS_ANDROID)
}

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
  for (const auto& test : kTestCases)
    RunTest(test, shell());
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
  for (const auto& test : kTestCases)
    RunTest(test, shell());
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
  for (const auto& test : kTestCases)
    RunTest(test, shell());
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
  for (const auto& test : kTestCases)
    RunTest(test, shell());
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
  for (const auto& test : kTestCases)
    RunTest(test, shell());
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
  for (const auto& test : kTestCases)
    RunTest(test, shell());
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
  for (const auto& test : kTestCases)
    RunTest(test, shell());
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

IN_PROC_BROWSER_TEST_P(CoopRestrictPropertiesBrowserTest,
                       CoopRestrictPropertiesIsParsed) {
  GURL starting_page(https_server()->GetURL(
      "a.test",
      "/set-header"
      "?cross-origin-opener-policy: restrict-properties"));
  EXPECT_TRUE(NavigateToURL(shell(), starting_page));

  // Verify that COOP: restrict-properties was parsed.
  EXPECT_EQ(current_frame_host()->cross_origin_opener_policy(),
            CoopRestrictProperties(url::Origin::Create(starting_page)));
  EXPECT_FALSE(
      current_frame_host()->GetSiteInstance()->IsCrossOriginIsolated());
}

IN_PROC_BROWSER_TEST_P(CoopRestrictPropertiesBrowserTest,
                       CoopRestrictPropertiesPlusCoepIsParsed) {
  GURL starting_page(
      https_server()->GetURL("a.test",
                             "/set-header"
                             "?cross-origin-opener-policy: restrict-properties"
                             "&cross-origin-embedder-policy: require-corp"));
  EXPECT_TRUE(NavigateToURL(shell(), starting_page));

  // Verify that COOP: restrict-properties was parsed along COEP, and that it
  // correctly enabled cross origin isolation.
  EXPECT_EQ(current_frame_host()->cross_origin_opener_policy(),
            CoopRestrictPropertiesPlusCoep(url::Origin::Create(starting_page)));
  EXPECT_TRUE(current_frame_host()->GetSiteInstance()->IsCrossOriginIsolated());
}

class CoopRestrictPropertiesOriginTrialBrowserTest
    : public OriginTrialBrowserTest,
      public ::testing::WithParamInterface<bool> {
 public:
  CoopRestrictPropertiesOriginTrialBrowserTest()
      : is_origin_trial_enabled(GetParam()) {
    if (is_origin_trial_enabled) {
      feature_list_.InitWithFeatures(
          {
              network::features::kCoopRestrictPropertiesOriginTrial,
          },  // Enabled
          {}  // Disabled
      );
    } else {
      feature_list_.InitWithFeatures(
          {},  // Enabled
          {
              network::features::kCoopRestrictPropertiesOriginTrial,
          }  // Disabled
      );
    }
  }

  // Origin Trials key generated with:
  //
  // tools/origin_trials/generate_token.py --expire-days 5000 --version 3
  // https://coop.security:9999 CoopRestrictProperties
  static std::string OriginTrialToken() {
    return "A8Yj3ElroyqJKJPrXAbAcR7e4oZZo978guRoJqwghGM0nnOI8PM8Ay1y1TRlAajef7o"
           "CHH+lahsRWglSKSy+"
           "Wg8AAABjeyJvcmlnaW4iOiAiaHR0cHM6Ly9jb29wLnNlY3VyaXR5Ojk5OTkiLCAiZmV"
           "hdHVyZSI6ICJDb29wUmVzdHJpY3RQcm9wZXJ0aWVzIiwgImV4cGlyeSI6IDIxMTY1MT"
           "cwMTd9";
  }
  GURL OtherURL() { return GURL("https://a.test"); }
  bool is_origin_trial_enabled;

 private:
  base::test::ScopedFeatureList feature_list_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         CoopRestrictPropertiesOriginTrialBrowserTest,
                         testing::Bool());

IN_PROC_BROWSER_TEST_P(CoopRestrictPropertiesOriginTrialBrowserTest,
                       CoopRestrictPropertiesValidToken) {
  URLLoaderInterceptor interceptor(base::BindLambdaForTesting(
      [&](URLLoaderInterceptor::RequestParams* params) {
        DCHECK_EQ(params->url_request.url, OriginTrialURL());
        URLLoaderInterceptor::WriteResponse(
            "HTTP/1.1 200 OK\n"
            "Content-type: text/html\n"
            "Cross-Origin-Opener-Policy: restrict-properties\n"
            "Origin-Trial: " +
                OriginTrialToken() + "\n\n",
            "", params->client.get());
        return true;
      }));
  EXPECT_TRUE(NavigateToURL(shell(), OriginTrialURL()));
  EXPECT_EQ(
      current_frame_host()->cross_origin_opener_policy().value,
      is_origin_trial_enabled
          ? network::mojom::CrossOriginOpenerPolicyValue::kRestrictProperties
          : network::mojom::CrossOriginOpenerPolicyValue::kUnsafeNone);
}

IN_PROC_BROWSER_TEST_P(CoopRestrictPropertiesOriginTrialBrowserTest,
                       CoopRestrictPropertiesTokenOriginMismatched) {
  URLLoaderInterceptor interceptor(base::BindLambdaForTesting(
      [&](URLLoaderInterceptor::RequestParams* params) {
        DCHECK_EQ(params->url_request.url, OtherURL());
        URLLoaderInterceptor::WriteResponse(
            "HTTP/1.1 200 OK\n"
            "Content-type: text/html\n"
            "Cross-Origin-Opener-Policy: restrict-properties\n"
            "Origin-Trial: " +
                OriginTrialToken() + "\n\n",
            "", params->client.get());
        return true;
      }));
  EXPECT_TRUE(NavigateToURL(shell(), OtherURL()));
  EXPECT_EQ(current_frame_host()->cross_origin_opener_policy().value,
            network::mojom::CrossOriginOpenerPolicyValue::kUnsafeNone);
}

IN_PROC_BROWSER_TEST_P(CoopRestrictPropertiesOriginTrialBrowserTest,
                       CoopRestrictPropertiesPlusCoepValidToken) {
  URLLoaderInterceptor interceptor(base::BindLambdaForTesting(
      [&](URLLoaderInterceptor::RequestParams* params) {
        DCHECK_EQ(params->url_request.url, OriginTrialURL());
        URLLoaderInterceptor::WriteResponse(
            "HTTP/1.1 200 OK\n"
            "Content-type: text/html\n"
            "Cross-Origin-Opener-Policy: restrict-properties\n"
            "Cross-Origin-Embedder-Policy: require-corp\n"
            "Origin-Trial: " +
                OriginTrialToken() + "\n\n",
            "", params->client.get());
        return true;
      }));
  EXPECT_TRUE(NavigateToURL(shell(), OriginTrialURL()));
  EXPECT_EQ(current_frame_host()->cross_origin_opener_policy().value,
            is_origin_trial_enabled
                ? network::mojom::CrossOriginOpenerPolicyValue::
                      kRestrictPropertiesPlusCoep
                : network::mojom::CrossOriginOpenerPolicyValue::kUnsafeNone);
}

IN_PROC_BROWSER_TEST_P(CoopRestrictPropertiesOriginTrialBrowserTest,
                       CoopRestrictPropertiesPlusCoepTokenOriginMismatched) {
  URLLoaderInterceptor interceptor(base::BindLambdaForTesting(
      [&](URLLoaderInterceptor::RequestParams* params) {
        DCHECK_EQ(params->url_request.url, OtherURL());
        URLLoaderInterceptor::WriteResponse(
            "HTTP/1.1 200 OK\n"
            "Content-type: text/html\n"
            "Cross-Origin-Opener-Policy: restrict-properties\n"
            "Cross-Origin-Embedder-Policy: require-corp\n"
            "Origin-Trial: " +
                OriginTrialToken() + "\n\n",
            "", params->client.get());
        return true;
      }));
  EXPECT_TRUE(NavigateToURL(shell(), OtherURL()));
  EXPECT_EQ(current_frame_host()->cross_origin_opener_policy().value,
            network::mojom::CrossOriginOpenerPolicyValue::kUnsafeNone);
}

IN_PROC_BROWSER_TEST_P(CoopRestrictPropertiesOriginTrialBrowserTest,
                       CoopReportOnlyRestrictPropertiesValidToken) {
  URLLoaderInterceptor interceptor(base::BindLambdaForTesting(
      [&](URLLoaderInterceptor::RequestParams* params) {
        DCHECK_EQ(params->url_request.url, OriginTrialURL());
        URLLoaderInterceptor::WriteResponse(
            "HTTP/1.1 200 OK\n"
            "Content-type: text/html\n"
            "Cross-Origin-Opener-Policy-Report-Only: restrict-properties\n"
            "Origin-Trial: " +
                OriginTrialToken() + "\n\n",
            "", params->client.get());
        return true;
      }));
  EXPECT_TRUE(NavigateToURL(shell(), OriginTrialURL()));
  EXPECT_EQ(
      current_frame_host()->cross_origin_opener_policy().report_only_value,
      is_origin_trial_enabled
          ? network::mojom::CrossOriginOpenerPolicyValue::kRestrictProperties
          : network::mojom::CrossOriginOpenerPolicyValue::kUnsafeNone);
}

IN_PROC_BROWSER_TEST_P(CoopRestrictPropertiesOriginTrialBrowserTest,
                       CoopReportOnlyRestrictPropertiesTokenOriginMismatched) {
  URLLoaderInterceptor interceptor(base::BindLambdaForTesting(
      [&](URLLoaderInterceptor::RequestParams* params) {
        DCHECK_EQ(params->url_request.url, OtherURL());
        URLLoaderInterceptor::WriteResponse(
            "HTTP/1.1 200 OK\n"
            "Content-type: text/html\n"
            "Cross-Origin-Opener-Policy-Report-Only: restrict-properties\n"
            "Origin-Trial: " +
                OriginTrialToken() + "\n\n",
            "", params->client.get());
        return true;
      }));
  EXPECT_TRUE(NavigateToURL(shell(), OtherURL()));
  EXPECT_EQ(
      current_frame_host()->cross_origin_opener_policy().report_only_value,
      network::mojom::CrossOriginOpenerPolicyValue::kUnsafeNone);
}

IN_PROC_BROWSER_TEST_P(CoopRestrictPropertiesOriginTrialBrowserTest,
                       CoopReportOnlyRestrictPropertiesPlusCoepValidToken) {
  URLLoaderInterceptor interceptor(base::BindLambdaForTesting(
      [&](URLLoaderInterceptor::RequestParams* params) {
        DCHECK_EQ(params->url_request.url, OriginTrialURL());
        URLLoaderInterceptor::WriteResponse(
            "HTTP/1.1 200 OK\n"
            "Content-type: text/html\n"
            "Cross-Origin-Opener-Policy-Report-Only: restrict-properties\n"
            "Cross-Origin-Embedder-Policy: require-corp\n"
            "Origin-Trial: " +
                OriginTrialToken() + "\n\n",
            "", params->client.get());
        return true;
      }));
  EXPECT_TRUE(NavigateToURL(shell(), OriginTrialURL()));
  EXPECT_EQ(
      current_frame_host()->cross_origin_opener_policy().report_only_value,
      is_origin_trial_enabled
          ? network::mojom::CrossOriginOpenerPolicyValue::
                kRestrictPropertiesPlusCoep
          : network::mojom::CrossOriginOpenerPolicyValue::kUnsafeNone);
}

IN_PROC_BROWSER_TEST_P(
    CoopRestrictPropertiesOriginTrialBrowserTest,
    CoopReportOnlyRestrictPropertiesPlusCoepTokenOriginMismatched) {
  URLLoaderInterceptor interceptor(base::BindLambdaForTesting(
      [&](URLLoaderInterceptor::RequestParams* params) {
        DCHECK_EQ(params->url_request.url, OtherURL());
        URLLoaderInterceptor::WriteResponse(
            "HTTP/1.1 200 OK\n"
            "Content-type: text/html\n"
            "Cross-Origin-Opener-Policy-Report-Only: restrict-properties\n"
            "Cross-Origin-Embedder-Policy: require-corp\n"
            "Origin-Trial: " +
                OriginTrialToken() + "\n\n",
            "", params->client.get());
        return true;
      }));
  EXPECT_TRUE(NavigateToURL(shell(), OtherURL()));
  EXPECT_EQ(
      current_frame_host()->cross_origin_opener_policy().report_only_value,
      network::mojom::CrossOriginOpenerPolicyValue::kUnsafeNone);
}

// Verify that a simple navigation from a regular page to a COOP:
// restrict-properties page puts the two pages in different BrowsingInstances in
// the same CoopRelatedGroup.
IN_PROC_BROWSER_TEST_P(CoopRestrictPropertiesBrowserTest,
                       NavigateNonCoopToCoopRp) {
  GURL regular_page(https_server()->GetURL("a.test", "/title1.html"));
  GURL coop_rp_page(https_server()->GetURL(
      "a.test",
      "/set-header"
      "?cross-origin-opener-policy: restrict-properties"));

  ASSERT_TRUE(NavigateToURL(shell(), regular_page));
  scoped_refptr<SiteInstanceImpl> initial_si(
      current_frame_host()->GetSiteInstance());
  ASSERT_TRUE(NavigateToURL(shell(), coop_rp_page));
  scoped_refptr<SiteInstanceImpl> final_si(
      current_frame_host()->GetSiteInstance());

  EXPECT_FALSE(initial_si->IsRelatedSiteInstance(final_si.get()));
  EXPECT_TRUE(initial_si->IsCoopRelatedSiteInstance(final_si.get()));
}

// Verify that a simple navigation from a COOP: restrict-properties page to a
// regular page puts the two pages in BrowsingInstances in the same
// CoopRelatedGroup.
IN_PROC_BROWSER_TEST_P(CoopRestrictPropertiesBrowserTest,
                       NavigateCoopRpToNonCoop) {
  GURL coop_rp_page(https_server()->GetURL(
      "a.test",
      "/set-header"
      "?cross-origin-opener-policy: restrict-properties"));
  GURL regular_page(https_server()->GetURL("a.test", "/title1.html"));

  ASSERT_TRUE(NavigateToURL(shell(), coop_rp_page));
  scoped_refptr<SiteInstanceImpl> initial_si(
      current_frame_host()->GetSiteInstance());
  ASSERT_TRUE(NavigateToURL(shell(), regular_page));
  scoped_refptr<SiteInstanceImpl> final_si(
      current_frame_host()->GetSiteInstance());

  EXPECT_FALSE(initial_si->IsRelatedSiteInstance(final_si.get()));
  EXPECT_TRUE(initial_si->IsCoopRelatedSiteInstance(final_si.get()));
}

// Verify that a simple navigation from a COOP: restrict-properties page to
// another same-origin COOP: restrict-properties page puts the two pages in the
// same SiteInstance.
IN_PROC_BROWSER_TEST_P(CoopRestrictPropertiesBrowserTest,
                       NavigateCoopRpToCoopRpSameOrigin) {
  GURL coop_rp_page(https_server()->GetURL(
      "a.test",
      "/set-header"
      "?cross-origin-opener-policy: restrict-properties"));
  GURL coop_rp_page_2(https_server()->GetURL(
      "a.test",
      "/set-header"
      "?cross-origin-opener-policy: restrict-properties&1"));

  ASSERT_TRUE(NavigateToURL(shell(), coop_rp_page));
  scoped_refptr<SiteInstanceImpl> initial_si(
      current_frame_host()->GetSiteInstance());
  ASSERT_TRUE(NavigateToURL(shell(), coop_rp_page_2));
  scoped_refptr<SiteInstanceImpl> final_si(
      current_frame_host()->GetSiteInstance());

  // BFCache can force a proactive BrowsingInstance swap, since we're not
  // dealing with popups.
  if (IsBackForwardCacheEnabled()) {
    EXPECT_FALSE(initial_si->IsRelatedSiteInstance(final_si.get()));
    EXPECT_FALSE(initial_si->IsCoopRelatedSiteInstance(final_si.get()));
  } else {
    EXPECT_EQ(initial_si.get(), final_si.get());
  }
}

// Verify that a simple navigation from a COOP: restrict-properties page to
// another cross-origin COOP: restrict-properties page puts the two pages in
// different SiteInstances and BrowsingInstances in the same CoopRelatedGroup.
IN_PROC_BROWSER_TEST_P(CoopRestrictPropertiesBrowserTest,
                       NavigateCoopRpToCoopRpCrossOrigin) {
  GURL coop_rp_page(https_server()->GetURL(
      "a.test",
      "/set-header"
      "?cross-origin-opener-policy: restrict-properties"));
  GURL coop_rp_page_2(https_server()->GetURL(
      "b.test",
      "/set-header"
      "?cross-origin-opener-policy: restrict-properties"));

  ASSERT_TRUE(NavigateToURL(shell(), coop_rp_page));
  scoped_refptr<SiteInstanceImpl> initial_si(
      current_frame_host()->GetSiteInstance());
  ASSERT_TRUE(NavigateToURL(shell(), coop_rp_page_2));
  scoped_refptr<SiteInstanceImpl> final_si(
      current_frame_host()->GetSiteInstance());

  EXPECT_NE(initial_si, final_si);
  EXPECT_FALSE(initial_si->IsRelatedSiteInstance(final_si.get()));
  EXPECT_TRUE(initial_si->IsCoopRelatedSiteInstance(final_si.get()));
}

// Verify that a simple navigation from a COOP: restrict-properties page to
// another same-origin COOP: restrict-properties page that also sets COEP puts
// the two pages in different SiteInstances and BrowsingInstances in the same
// CoopRelatedGroup.
IN_PROC_BROWSER_TEST_P(CoopRestrictPropertiesBrowserTest,
                       NavigateCoopRpToCoopRpPlusCoep) {
  GURL coop_rp_page(https_server()->GetURL(
      "a.test",
      "/set-header"
      "?cross-origin-opener-policy: restrict-properties"));
  GURL coop_rp_with_coep_page(
      https_server()->GetURL("a.test",
                             "/set-header?"
                             "cross-origin-opener-policy: restrict-properties&"
                             "cross-origin-embedder-policy: require-corp"));

  ASSERT_TRUE(NavigateToURL(shell(), coop_rp_page));
  scoped_refptr<SiteInstanceImpl> initial_si(
      current_frame_host()->GetSiteInstance());
  ASSERT_TRUE(NavigateToURL(shell(), coop_rp_with_coep_page));
  scoped_refptr<SiteInstanceImpl> final_si(
      current_frame_host()->GetSiteInstance());

  EXPECT_NE(initial_si, final_si);
  EXPECT_FALSE(initial_si->IsRelatedSiteInstance(final_si.get()));
  EXPECT_TRUE(initial_si->IsCoopRelatedSiteInstance(final_si.get()));
}

// Verify that a navigation from a regular page to a COOP: restrict-properties
// and then to another regular page reuses the initial BrowsingInstance.
IN_PROC_BROWSER_TEST_P(CoopRestrictPropertiesBrowserTest,
                       NavigateNonCoopToCoopRpToNonCoop) {
  GURL regular_page(https_server()->GetURL("a.test", "/title1.html"));
  GURL coop_rp_page(https_server()->GetURL(
      "a.test",
      "/set-header"
      "?cross-origin-opener-policy: restrict-properties"));
  GURL regular_page_2(https_server()->GetURL("a.test", "/title2.html"));

  ASSERT_TRUE(NavigateToURL(shell(), regular_page));
  scoped_refptr<SiteInstanceImpl> initial_si(
      current_frame_host()->GetSiteInstance());
  ASSERT_TRUE(NavigateToURL(shell(), coop_rp_page));
  ASSERT_TRUE(NavigateToURL(shell(), regular_page_2));
  scoped_refptr<SiteInstanceImpl> final_si(
      current_frame_host()->GetSiteInstance());

  EXPECT_EQ(initial_si.get(), final_si.get());
}

// Verify that a navigation from a security sensitive page to a COOP:
// restrict-properties changes the CoopRelatedGroup.
IN_PROC_BROWSER_TEST_P(CoopRestrictPropertiesBrowserTest,
                       NavigateWebUiToCoopRp) {
  GURL webui_page("chrome://ukm");
  GURL coop_rp_page(https_server()->GetURL(
      "a.test",
      "/set-header"
      "?cross-origin-opener-policy: restrict-properties"));

  ASSERT_TRUE(NavigateToURL(shell(), webui_page));
  scoped_refptr<SiteInstanceImpl> initial_si(
      current_frame_host()->GetSiteInstance());
  ASSERT_TRUE(NavigateToURL(shell(), coop_rp_page));
  scoped_refptr<SiteInstanceImpl> final_si(
      current_frame_host()->GetSiteInstance());

  EXPECT_NE(initial_si.get(), final_si.get());
  EXPECT_FALSE(initial_si->IsRelatedSiteInstance(final_si.get()));
  EXPECT_FALSE(initial_si->IsCoopRelatedSiteInstance(final_si.get()));
}

// Verify that a popup opened with matching COOP: restrict-properties value and
// origin stays in the same SiteInstance.
IN_PROC_BROWSER_TEST_P(CoopRestrictPropertiesBrowserTest,
                       NoSwapForMatchingPopupAndMainPage) {
  GURL coop_rp_page(https_server()->GetURL(
      "a.test",
      "/set-header"
      "?cross-origin-opener-policy: restrict-properties"));
  GURL coop_rp_page_2(https_server()->GetURL(
      "a.test",
      "/set-header"
      "?cross-origin-opener-policy: restrict-properties&1"));

  // Start with a page that sets COOP: restrict-properties.
  ASSERT_TRUE(NavigateToURL(shell(), coop_rp_page));
  scoped_refptr<SiteInstanceImpl> main_page_si(
      current_frame_host()->GetSiteInstance());

  // Open a same-origin page that also sets COOP: restrict-properties.
  WebContentsImpl* popup_window = static_cast<WebContentsImpl*>(
      OpenPopup(current_frame_host(), coop_rp_page_2, "")->web_contents());
  scoped_refptr<SiteInstanceImpl> popup_si(
      popup_window->GetPrimaryMainFrame()->GetSiteInstance());
  EXPECT_EQ(main_page_si.get(), popup_si.get());
  EXPECT_TRUE(main_page_si->IsRelatedSiteInstance(popup_si.get()));
  EXPECT_TRUE(main_page_si->IsCoopRelatedSiteInstance(popup_si.get()));
}

// Verify that a popup in a different BrowsingInstance within the same
// CoopRelatedGroup can come back to the main page SiteInstance if navigating to
// a compatible page.
IN_PROC_BROWSER_TEST_P(CoopRestrictPropertiesBrowserTest,
                       ReuseBrowsingInstanceInCoopGroupPopupAndMainPage) {
  GURL regular_page(https_server()->GetURL("a.test", "/title1.html"));
  GURL coop_rp_page(https_server()->GetURL(
      "a.test",
      "/set-header"
      "?cross-origin-opener-policy: restrict-properties"));

  // We start with a simple page which opens a COOP: restrict-properties popup.
  ASSERT_TRUE(NavigateToURL(shell(), regular_page));
  scoped_refptr<SiteInstanceImpl> main_page_si(
      current_frame_host()->GetSiteInstance());

  WebContentsImpl* popup_window = static_cast<WebContentsImpl*>(
      OpenPopup(current_frame_host(), coop_rp_page, "")->web_contents());
  scoped_refptr<SiteInstanceImpl> initial_popup_si(
      popup_window->GetPrimaryMainFrame()->GetSiteInstance());
  EXPECT_NE(main_page_si.get(), initial_popup_si.get());
  EXPECT_FALSE(main_page_si->IsRelatedSiteInstance(initial_popup_si.get()));
  EXPECT_TRUE(main_page_si->IsCoopRelatedSiteInstance(initial_popup_si.get()));

  // Navigate the popup to the same url as the main page. It should reuse the
  // main page BrowsingInstance and SiteInstance.
  ASSERT_TRUE(NavigateToURL(popup_window, regular_page));
  scoped_refptr<SiteInstanceImpl> final_popup_si(
      popup_window->GetPrimaryMainFrame()->GetSiteInstance());
  EXPECT_EQ(main_page_si.get(), final_popup_si.get());
}

// Verify that a popup a in a different BrowsingInstance within the same
// CoopRelatedGroup can come back to the main page SiteInstance if navigating to
// a compatible page, initiated by the renderer.
IN_PROC_BROWSER_TEST_P(
    CoopRestrictPropertiesBrowserTest,
    ReuseBrowsingInstanceInCoopGroupPopupAndMainPageRenderInitiated) {
  GURL regular_page(https_server()->GetURL("a.test", "/title1.html"));
  GURL coop_rp_page(https_server()->GetURL(
      "a.test",
      "/set-header"
      "?cross-origin-opener-policy: restrict-properties"));

  // We start with a simple page which opens a COOP: restrict-properties popup.
  ASSERT_TRUE(NavigateToURL(shell(), regular_page));
  scoped_refptr<SiteInstanceImpl> main_page_si(
      current_frame_host()->GetSiteInstance());

  WebContentsImpl* popup_window = static_cast<WebContentsImpl*>(
      OpenPopup(current_frame_host(), coop_rp_page, "")->web_contents());
  scoped_refptr<SiteInstanceImpl> initial_popup_si(
      popup_window->GetPrimaryMainFrame()->GetSiteInstance());
  EXPECT_NE(main_page_si.get(), initial_popup_si.get());
  EXPECT_FALSE(main_page_si->IsRelatedSiteInstance(initial_popup_si.get()));
  EXPECT_TRUE(main_page_si->IsCoopRelatedSiteInstance(initial_popup_si.get()));

  // Navigate the popup to the same url as the main page, from the renderer. It
  // should reuse the main page BrowsingInstance and SiteInstance.
  ASSERT_TRUE(ExecJs(popup_window->GetPrimaryMainFrame(),
                     JsReplace("location.href = $1", regular_page)));
  ASSERT_TRUE(WaitForLoadStop(popup_window));
  scoped_refptr<SiteInstanceImpl> final_popup_si(
      popup_window->GetPrimaryMainFrame()->GetSiteInstance());
  EXPECT_EQ(main_page_si.get(), final_popup_si.get());
}

// Verify that two pages in different BrowsingInstances within the same
// CoopRelatedGroup can both navigate to a third page, and end up in the same
// SiteInstance.
IN_PROC_BROWSER_TEST_P(CoopRestrictPropertiesBrowserTest,
                       ReuseBrowsingInstanceInCoopGroupTwoPopups) {
  GURL coop_rp_page(https_server()->GetURL(
      "a.test",
      "/set-header"
      "?cross-origin-opener-policy: restrict-properties"));
  GURL coop_rp_page_2(https_server()->GetURL(
      "b.test",
      "/set-header"
      "?cross-origin-opener-policy: restrict-properties"));
  GURL regular_page(https_server()->GetURL("a.test", "/title1.html"));

  // We start with a COOP: restrict-properties page which opens a popup to a
  // cross-origin COOP: restrict-properties page. They end up in different
  // BrowsingInstances but in the same CoopRelatedGroup.
  ASSERT_TRUE(NavigateToURL(shell(), coop_rp_page));
  scoped_refptr<SiteInstanceImpl> initial_main_page_si(
      current_frame_host()->GetSiteInstance());

  WebContentsImpl* popup_window = static_cast<WebContentsImpl*>(
      OpenPopup(current_frame_host(), coop_rp_page_2, "")->web_contents());
  scoped_refptr<SiteInstanceImpl> initial_popup_si(
      popup_window->GetPrimaryMainFrame()->GetSiteInstance());
  EXPECT_NE(initial_main_page_si.get(), initial_popup_si.get());
  EXPECT_FALSE(
      initial_main_page_si->IsRelatedSiteInstance(initial_popup_si.get()));
  EXPECT_TRUE(
      initial_main_page_si->IsCoopRelatedSiteInstance(initial_popup_si.get()));

  // Navigate both COOP: restrict-properties pages to the same unsafe-none page.
  ASSERT_TRUE(NavigateToURL(shell(), regular_page));
  ASSERT_TRUE(NavigateToURL(popup_window, regular_page));

  // They should both use the same newly created BrowsingInstance and
  // SiteInstance.
  scoped_refptr<SiteInstanceImpl> final_main_page_si(
      current_frame_host()->GetSiteInstance());
  scoped_refptr<SiteInstanceImpl> final_popup_si(
      popup_window->GetPrimaryMainFrame()->GetSiteInstance());
  EXPECT_EQ(final_main_page_si.get(), final_popup_si.get());
}

// Verify that CSP: sandbox is taken into account for the common coop origin
// computation.
// TODO(crbug.com/40879437): This is not currently the case. Enable once
// COOP is bundled with the appropriate origin.
IN_PROC_BROWSER_TEST_P(CoopRestrictPropertiesBrowserTest,
                       DoNotReuseBrowsingInstanceInCoopGroupOpaqueOrigin) {
  GURL coop_rp_page(https_server()->GetURL(
      "a.test",
      "/set-header"
      "?cross-origin-opener-policy: restrict-properties"));
  GURL coop_rp_and_csp_page(
      https_server()->GetURL("a.test",
                             "/set-header"
                             "?cross-origin-opener-policy: restrict-properties&"
                             "Content-Security-Policy: sandbox"));

  // We start with a COOP: restrict-properties page which opens a popup to a
  // same-origin COOP: restrict-properties page, but which sets CSP, making its
  // origin opaque. They should end up in different BrowsingInstances in the
  // same CoopRelatedGroup.
  ASSERT_TRUE(NavigateToURL(shell(), coop_rp_page));
  scoped_refptr<SiteInstanceImpl> main_page_si(
      current_frame_host()->GetSiteInstance());

  WebContentsImpl* popup_window = static_cast<WebContentsImpl*>(
      OpenPopup(current_frame_host(), coop_rp_and_csp_page, "")
          ->web_contents());
  scoped_refptr<SiteInstanceImpl> popup_si(
      popup_window->GetPrimaryMainFrame()->GetSiteInstance());
  EXPECT_NE(main_page_si.get(), popup_si.get());
  EXPECT_FALSE(main_page_si->IsRelatedSiteInstance(popup_si.get()));
  EXPECT_TRUE(main_page_si->IsCoopRelatedSiteInstance(popup_si.get()));

  // The recorded common COOP origin should differ, because CSP forces an opaque
  // origin.
  EXPECT_NE(main_page_si->GetCommonCoopOrigin(),
            popup_si->GetCommonCoopOrigin());
}

// Verify that active WebContents counting works across different
// BrowsingInstances in the same CoopRelatedGroup.
IN_PROC_BROWSER_TEST_P(CoopRestrictPropertiesBrowserTest,
                       ActiveWebContentsCountInCoopRelatedGroup) {
  GURL regular_page(https_server()->GetURL("a.test", "/title1.html"));
  GURL coop_rp_page(https_server()->GetURL(
      "a.test",
      "/set-header"
      "?cross-origin-opener-policy: restrict-properties"));
  GURL coop_so_page(
      https_server()->GetURL("a.test",
                             "/set-header"
                             "?cross-origin-opener-policy: same-origin"));

  // We start with a simple page which opens a COOP: restrict-properties popup.
  ASSERT_TRUE(NavigateToURL(shell(), regular_page));
  scoped_refptr<SiteInstanceImpl> main_page_si(
      current_frame_host()->GetSiteInstance());
  EXPECT_EQ(1u, main_page_si->GetRelatedActiveContentsCount());

  // Open a popup in the same BrowsingInstance and SiteInstance.
  WebContentsImpl* first_popup_window = static_cast<WebContentsImpl*>(
      OpenPopup(current_frame_host(), regular_page, "")->web_contents());
  scoped_refptr<SiteInstanceImpl> first_popup_si(
      first_popup_window->GetPrimaryMainFrame()->GetSiteInstance());
  EXPECT_EQ(2u, main_page_si->GetRelatedActiveContentsCount());
  EXPECT_EQ(2u, first_popup_si->GetRelatedActiveContentsCount());

  // Open a popup in the same CoopRelatedGroup in another BrowsingInstance.
  WebContentsImpl* second_popup_window = static_cast<WebContentsImpl*>(
      OpenPopup(current_frame_host(), coop_rp_page, "")->web_contents());
  scoped_refptr<SiteInstanceImpl> second_popup_si(
      second_popup_window->GetPrimaryMainFrame()->GetSiteInstance());
  EXPECT_EQ(3u, main_page_si->GetRelatedActiveContentsCount());
  EXPECT_EQ(3u, first_popup_si->GetRelatedActiveContentsCount());
  EXPECT_EQ(3u, second_popup_si->GetRelatedActiveContentsCount());

  // Have each of these popups open a new COOP: restrict-properties popup.
  WebContentsImpl* third_popup_window = static_cast<WebContentsImpl*>(
      OpenPopup(first_popup_window->GetPrimaryMainFrame(), coop_rp_page, "")
          ->web_contents());
  scoped_refptr<SiteInstanceImpl> third_popup_si(
      third_popup_window->GetPrimaryMainFrame()->GetSiteInstance());
  WebContentsImpl* fourth_popup_window = static_cast<WebContentsImpl*>(
      OpenPopup(second_popup_window->GetPrimaryMainFrame(), coop_rp_page, "")
          ->web_contents());
  scoped_refptr<SiteInstanceImpl> fourth_popup_si(
      fourth_popup_window->GetPrimaryMainFrame()->GetSiteInstance());
  EXPECT_EQ(5u, main_page_si->GetRelatedActiveContentsCount());
  EXPECT_EQ(5u, first_popup_si->GetRelatedActiveContentsCount());
  EXPECT_EQ(5u, second_popup_si->GetRelatedActiveContentsCount());
  EXPECT_EQ(5u, third_popup_si->GetRelatedActiveContentsCount());
  EXPECT_EQ(5u, fourth_popup_si->GetRelatedActiveContentsCount());

  // Open an extra popup from the root, that does not belong to the COOP group,
  // and verify that the count is not increased.
  WebContentsImpl* fifth_popup_window = static_cast<WebContentsImpl*>(
      OpenPopup(current_frame_host(), coop_so_page, "")->web_contents());
  scoped_refptr<SiteInstanceImpl> fifth_popup_si(
      fifth_popup_window->GetPrimaryMainFrame()->GetSiteInstance());
  EXPECT_EQ(5u, main_page_si->GetRelatedActiveContentsCount());
  EXPECT_EQ(5u, first_popup_si->GetRelatedActiveContentsCount());
  EXPECT_EQ(5u, second_popup_si->GetRelatedActiveContentsCount());
  EXPECT_EQ(5u, third_popup_si->GetRelatedActiveContentsCount());
  EXPECT_EQ(5u, fourth_popup_si->GetRelatedActiveContentsCount());
  EXPECT_EQ(1u, fifth_popup_si->GetRelatedActiveContentsCount());

  fifth_popup_window->Close();
  EXPECT_EQ(5u, main_page_si->GetRelatedActiveContentsCount());
  EXPECT_EQ(5u, first_popup_si->GetRelatedActiveContentsCount());
  EXPECT_EQ(5u, second_popup_si->GetRelatedActiveContentsCount());
  EXPECT_EQ(5u, third_popup_si->GetRelatedActiveContentsCount());
  EXPECT_EQ(5u, fourth_popup_si->GetRelatedActiveContentsCount());

  // Close all the popups one by one and verify that the web contents decreases
  // accordingly. Purposefully close the middle popups before the leaf popups,
  // to verify counting works without the root window.
  first_popup_window->Close();
  EXPECT_EQ(4u, main_page_si->GetRelatedActiveContentsCount());
  EXPECT_EQ(4u, second_popup_si->GetRelatedActiveContentsCount());
  EXPECT_EQ(4u, third_popup_si->GetRelatedActiveContentsCount());
  EXPECT_EQ(4u, fourth_popup_si->GetRelatedActiveContentsCount());

  second_popup_window->Close();
  EXPECT_EQ(3u, main_page_si->GetRelatedActiveContentsCount());
  EXPECT_EQ(3u, third_popup_si->GetRelatedActiveContentsCount());
  EXPECT_EQ(3u, fourth_popup_si->GetRelatedActiveContentsCount());

  third_popup_window->Close();
  EXPECT_EQ(2u, main_page_si->GetRelatedActiveContentsCount());
  EXPECT_EQ(2u, fourth_popup_si->GetRelatedActiveContentsCount());

  fourth_popup_window->Close();
  EXPECT_EQ(1u, main_page_si->GetRelatedActiveContentsCount());
}

// Verify that the COOP: restrict-properties origin is inherited by a subframe.
IN_PROC_BROWSER_TEST_P(CoopRestrictPropertiesBrowserTest,
                       CommonCoopOriginInheritedBySubframe) {
  GURL coop_rp_page(https_server()->GetURL(
      "b.test",
      "/set-header"
      "?cross-origin-opener-policy: restrict-properties"));
  GURL regular_page(https_server()->GetURL("a.test", "/title1.html"));

  ASSERT_TRUE(NavigateToURL(shell(), coop_rp_page));
  ASSERT_EQ(current_frame_host()->cross_origin_opener_policy(),
            CoopRestrictProperties(url::Origin::Create(coop_rp_page)));

  // Create a cross origin child frame.
  ASSERT_TRUE(ExecJs(current_frame_host(), JsReplace(R"(
    const frame = document.createElement('iframe');
    frame.src = $1;
    document.body.appendChild(frame);
  )",
                                                     regular_page)));
  ASSERT_TRUE(WaitForLoadStop(web_contents()));
  RenderFrameHostImpl* iframe_rfh =
      current_frame_host()->child_at(0)->current_frame_host();

  EXPECT_EQ(iframe_rfh->GetSiteInstance()->GetCommonCoopOrigin(),
            current_frame_host()->GetSiteInstance()->GetCommonCoopOrigin());
}

// Verify that the COOP: restrict-properties origin is inherited by a subframe
// even when it specifies its own COOP header, which should be ignored.
IN_PROC_BROWSER_TEST_P(
    CoopRestrictPropertiesBrowserTest,
    CommonCoopOriginInheritedBySubframeOverridesIgnoredCoopHeader) {
  GURL coop_rp_page(https_server()->GetURL(
      "a.test",
      "/set-header"
      "?cross-origin-opener-policy: restrict-properties"));
  GURL coop_rp_page_2(https_server()->GetURL(
      "b.test",
      "/set-header"
      "?cross-origin-opener-policy: restrict-properties"));

  ASSERT_TRUE(NavigateToURL(shell(), coop_rp_page));
  ASSERT_EQ(current_frame_host()->cross_origin_opener_policy(),
            CoopRestrictProperties(url::Origin::Create(coop_rp_page)));

  // Create cross origin child frame.
  ASSERT_TRUE(ExecJs(current_frame_host(), JsReplace(R"(
    const frame = document.createElement('iframe');
    frame.src = $1;
    document.body.appendChild(frame);
  )",
                                                     coop_rp_page_2)));
  ASSERT_TRUE(WaitForLoadStop(web_contents()));
  RenderFrameHostImpl* iframe_rfh =
      current_frame_host()->child_at(0)->current_frame_host();

  EXPECT_EQ(iframe_rfh->GetSiteInstance()->GetCommonCoopOrigin(),
            current_frame_host()->GetSiteInstance()->GetCommonCoopOrigin());
}

// Verify that the COOP: restrict-properties origin is inherited by a subframe
// even when it is in a popup.
IN_PROC_BROWSER_TEST_P(CoopRestrictPropertiesBrowserTest,
                       CommonCoopOriginInheritedBySubframeInPopup) {
  GURL regular_page(https_server()->GetURL("a.test", "/title1.html"));
  GURL coop_rp_page(https_server()->GetURL(
      "a.test",
      "/set-header"
      "?cross-origin-opener-policy: restrict-properties"));
  GURL regular_page_2(https_server()->GetURL("b.test", "/title1.html"));

  // Start by opening a popup to a COOP: restrict-properties page from a regular
  // page.
  ASSERT_TRUE(NavigateToURL(shell(), regular_page));
  WebContentsImpl* popup_window = static_cast<WebContentsImpl*>(
      OpenPopup(current_frame_host(), coop_rp_page, "")->web_contents());
  RenderFrameHostImpl* main_popup_rfh = popup_window->GetPrimaryMainFrame();
  EXPECT_NE(current_frame_host()->GetSiteInstance(),
            main_popup_rfh->GetSiteInstance());
  EXPECT_FALSE(current_frame_host()->GetSiteInstance()->IsRelatedSiteInstance(
      main_popup_rfh->GetSiteInstance()));
  EXPECT_TRUE(
      current_frame_host()->GetSiteInstance()->IsCoopRelatedSiteInstance(
          main_popup_rfh->GetSiteInstance()));

  // Now create a cross origin child frame in the popup.
  ASSERT_TRUE(ExecJs(main_popup_rfh, JsReplace(R"(
    const frame = document.createElement('iframe');
    frame.src = $1;
    document.body.appendChild(frame);
  )",
                                               regular_page_2)));
  ASSERT_TRUE(WaitForLoadStop(popup_window));
  RenderFrameHostImpl* iframe_rfh =
      main_popup_rfh->child_at(0)->current_frame_host();
  EXPECT_EQ(iframe_rfh->GetSiteInstance()->GetCommonCoopOrigin(),
            main_popup_rfh->GetSiteInstance()->GetCommonCoopOrigin());
}

// This test verifies that navigating to a COOP: restrict-properties page and
// back uses the appropriate BrowsingInstance.
IN_PROC_BROWSER_TEST_P(CoopRestrictPropertiesBrowserTest,
                       HistoryNavigationBackToCoopRpFromNonCoop) {
  GURL regular_page(https_server()->GetURL("a.test", "/title1.html"));
  GURL coop_rp_page(https_server()->GetURL(
      "a.test",
      "/set-header"
      "?cross-origin-opener-policy: restrict-properties"));

  ASSERT_TRUE(NavigateToURL(shell(), regular_page));
  scoped_refptr<SiteInstanceImpl> initial_si(
      current_frame_host()->GetSiteInstance());

  ASSERT_TRUE(NavigateToURL(shell(), coop_rp_page));
  scoped_refptr<SiteInstanceImpl> coop_rp_si(
      current_frame_host()->GetSiteInstance());
  EXPECT_NE(initial_si.get(), coop_rp_si.get());
  EXPECT_FALSE(initial_si->IsRelatedSiteInstance(coop_rp_si.get()));
  EXPECT_TRUE(initial_si->IsCoopRelatedSiteInstance(coop_rp_si.get()));

  // Navigate back. The correct SiteInstance should be reused.
  web_contents()->GetController().GoBack();
  ASSERT_TRUE(WaitForLoadStop(web_contents()));
  scoped_refptr<SiteInstanceImpl> back_si(
      current_frame_host()->GetSiteInstance());
  EXPECT_EQ(initial_si.get(), back_si.get());
}

// This test verifies that navigating to a regular page from a COOP:
// restrict-properties page and then back, puts the initial page in the
// appropriate BrowsingInstance.
IN_PROC_BROWSER_TEST_P(CoopRestrictPropertiesBrowserTest,
                       HistoryNavigationBackToNonCoopFromCoopRp) {
  GURL coop_rp_page(https_server()->GetURL(
      "a.test",
      "/set-header"
      "?cross-origin-opener-policy: restrict-properties"));
  GURL regular_page(https_server()->GetURL("a.test", "/title1.html"));

  ASSERT_TRUE(NavigateToURL(shell(), coop_rp_page));
  scoped_refptr<SiteInstanceImpl> initial_si(
      current_frame_host()->GetSiteInstance());

  ASSERT_TRUE(NavigateToURL(shell(), regular_page));
  scoped_refptr<SiteInstanceImpl> regular_si(
      current_frame_host()->GetSiteInstance());
  EXPECT_NE(initial_si.get(), regular_si.get());
  EXPECT_FALSE(initial_si->IsRelatedSiteInstance(regular_si.get()));
  EXPECT_TRUE(initial_si->IsCoopRelatedSiteInstance(regular_si.get()));

  // Navigate the popup back. The correct SiteInstance should be reused.
  web_contents()->GetController().GoBack();
  ASSERT_TRUE(WaitForLoadStop(web_contents()));
  scoped_refptr<SiteInstanceImpl> back_si(
      current_frame_host()->GetSiteInstance());
  EXPECT_EQ(initial_si.get(), back_si.get());
}

// This test verifies that a popup initially on a regular page navigates to a
// COOP: restrict-properties page and back gets put in the appropriate
// BrowsingInstance.
IN_PROC_BROWSER_TEST_P(CoopRestrictPropertiesBrowserTest,
                       HistoryNavigationBackToCoopRpFromNonCoopInPopup) {
  GURL regular_page(https_server()->GetURL("a.test", "/title1.html"));
  GURL coop_rp_page(https_server()->GetURL(
      "a.test",
      "/set-header"
      "?cross-origin-opener-policy: restrict-properties"));

  // We start with a simple page which opens a popup.
  ASSERT_TRUE(NavigateToURL(shell(), regular_page));
  scoped_refptr<SiteInstanceImpl> main_page_si(
      current_frame_host()->GetSiteInstance());

  WebContentsImpl* popup_window = static_cast<WebContentsImpl*>(
      OpenPopup(current_frame_host(), regular_page, "")->web_contents());
  scoped_refptr<SiteInstanceImpl> initial_popup_si(
      popup_window->GetPrimaryMainFrame()->GetSiteInstance());
  ASSERT_EQ(main_page_si.get(), initial_popup_si.get());

  // Navigate the popup to a COOP: restrict-properties page and then back. It
  // should reuse the original SiteInstance.
  ASSERT_TRUE(NavigateToURL(popup_window, coop_rp_page));
  scoped_refptr<SiteInstanceImpl> second_popup_si(
      popup_window->GetPrimaryMainFrame()->GetSiteInstance());
  EXPECT_NE(second_popup_si.get(), initial_popup_si.get());
  EXPECT_FALSE(second_popup_si->IsRelatedSiteInstance(initial_popup_si.get()));
  EXPECT_TRUE(
      second_popup_si->IsCoopRelatedSiteInstance(initial_popup_si.get()));

  TestNavigationManager nav_manager(popup_window, regular_page);
  popup_window->GetController().GoBack();

  // Check that the proper speculative SiteInstance was selected.
  nav_manager.WaitForSpeculativeRenderFrameHostCreation();
  RenderFrameHostImpl* speculative_rfh = popup_window->GetPrimaryFrameTree()
                                             .root()
                                             ->render_manager()
                                             ->speculative_frame_host();
  ASSERT_TRUE(speculative_rfh);
  EXPECT_EQ(initial_popup_si.get(), speculative_rfh->GetSiteInstance());
  ASSERT_TRUE(nav_manager.WaitForNavigationFinished());

  // Check that the speculative SiteInstance was then committed.
  scoped_refptr<SiteInstanceImpl> back_si(
      popup_window->GetPrimaryMainFrame()->GetSiteInstance());
  EXPECT_EQ(initial_popup_si.get(), back_si.get());
}

// This test verifies that a popup initially on a COOP: restrict-properties page
// that navigates to a regular page and then back, gets put in the appropriate
// original BrowsingInstance.
IN_PROC_BROWSER_TEST_P(CoopRestrictPropertiesBrowserTest,
                       HistoryNavigationBackToNonCoopFromCoopRpInPopup) {
  GURL coop_rp_page(https_server()->GetURL(
      "a.test",
      "/set-header"
      "?cross-origin-opener-policy: restrict-properties"));
  GURL regular_page(https_server()->GetURL("a.test", "/title1.html"));

  // We start with a COOP: restrict-properties page which opens a popup to a
  // same-origin COOP: restrict-properties page.
  ASSERT_TRUE(NavigateToURL(shell(), coop_rp_page));
  scoped_refptr<SiteInstanceImpl> main_page_si(
      current_frame_host()->GetSiteInstance());

  WebContentsImpl* popup_window = static_cast<WebContentsImpl*>(
      OpenPopup(current_frame_host(), coop_rp_page, "")->web_contents());
  scoped_refptr<SiteInstanceImpl> initial_popup_si(
      popup_window->GetPrimaryMainFrame()->GetSiteInstance());
  ASSERT_EQ(main_page_si.get(), initial_popup_si.get());

  // Navigate the popup to a regular page and then back. It should reuse the
  // original SiteInstance.
  ASSERT_TRUE(NavigateToURL(popup_window, regular_page));
  scoped_refptr<SiteInstanceImpl> second_popup_si(
      popup_window->GetPrimaryMainFrame()->GetSiteInstance());
  EXPECT_NE(second_popup_si.get(), initial_popup_si.get());
  EXPECT_FALSE(second_popup_si->IsRelatedSiteInstance(initial_popup_si.get()));
  EXPECT_TRUE(
      second_popup_si->IsCoopRelatedSiteInstance(initial_popup_si.get()));

  popup_window->GetController().GoBack();
  ASSERT_TRUE(WaitForLoadStop(popup_window));
  scoped_refptr<SiteInstanceImpl> back_si(
      popup_window->GetPrimaryMainFrame()->GetSiteInstance());
  EXPECT_EQ(initial_popup_si.get(), back_si.get());
}

// This test verifies that the reload of a COOP: restrict-properties page ends
// up in the appropriate BrowsingInstance.
IN_PROC_BROWSER_TEST_P(CoopRestrictPropertiesBrowserTest,
                       HistoryNavigationReloadOfCoopRp) {
  GURL coop_rp_page(https_server()->GetURL(
      "a.test",
      "/set-header"
      "?cross-origin-opener-policy: restrict-properties"));

  // Start on a COOP: restrict-properties page.
  ASSERT_TRUE(NavigateToURL(shell(), coop_rp_page));
  scoped_refptr<SiteInstanceImpl> initial_si(
      current_frame_host()->GetSiteInstance());

  // Reload the page. It should end up in the same SiteInstance.
  ReloadBlockUntilNavigationsComplete(shell(), 1);
  scoped_refptr<SiteInstanceImpl> final_si(
      current_frame_host()->GetSiteInstance());
  EXPECT_EQ(initial_si.get(), final_si.get());
}

// This test verifies that the failed reload of a COOP: restrict-properties page
// ends up in the appropriate BrowsingInstance.
IN_PROC_BROWSER_TEST_P(CoopRestrictPropertiesBrowserTest,
                       HistoryNavigationFailedReloadOfCoopRp) {
  GURL coop_rp_page(https_server()->GetURL(
      "a.test",
      "/set-header"
      "?cross-origin-opener-policy: restrict-properties"));

  // Start on a COOP: restrict-properties page.
  ASSERT_TRUE(NavigateToURL(shell(), coop_rp_page));
  scoped_refptr<SiteInstanceImpl> initial_si(
      current_frame_host()->GetSiteInstance());

  // Simulate being offline by failing all network requests.
  auto url_loader_interceptor =
      std::make_unique<content::URLLoaderInterceptor>(base::BindRepeating(
          [](content::URLLoaderInterceptor::RequestParams* params) {
            network::URLLoaderCompletionStatus status;
            status.error_code = net::Error::ERR_CONNECTION_FAILED;
            params->client->OnComplete(status);
            return true;
          }));

  // Reload the page. It will end up as an error page.
  ReloadBlockUntilNavigationsComplete(shell(), 1);
  scoped_refptr<SiteInstanceImpl> error_si(
      current_frame_host()->GetSiteInstance());

  // Error pages have COOP: unsafe-none, so it should end up in a different
  // BrowsingInstance in the same CoopRelatedGroup.
  EXPECT_NE(initial_si.get(), error_si.get());
  EXPECT_FALSE(initial_si->IsRelatedSiteInstance(error_si.get()));
  EXPECT_TRUE(initial_si->IsCoopRelatedSiteInstance(error_si.get()));
}

// This test verifies that a back navigation supposed to be in the same
// CoopRelatedGroup, but that ends up in a different one due a change in header
// is handled properly.
IN_PROC_BROWSER_TEST_P(CoopRestrictPropertiesBrowserTest,
                       HistoryNavigationsBackToChangedCoopHeader) {
  GURL changing_coop_page(https_server()->GetURL(
      "a.test", "/serve-different-coop-on-second-navigation"));
  GURL regular_page(https_server()->GetURL("a.test", "/title1.html"));

  // Start on a changing COOP headers page. It is first served with COOP:
  // restrict-properties.
  ASSERT_TRUE(NavigateToURL(shell(), changing_coop_page));
  scoped_refptr<SiteInstanceImpl> initial_si(
      current_frame_host()->GetSiteInstance());

  ASSERT_TRUE(NavigateToURL(shell(), regular_page));
  scoped_refptr<SiteInstanceImpl> intermediate_si(
      current_frame_host()->GetSiteInstance());
  EXPECT_FALSE(initial_si->IsRelatedSiteInstance(intermediate_si.get()));
  EXPECT_TRUE(initial_si->IsCoopRelatedSiteInstance(intermediate_si.get()));

  // When going back, the page is now served with COOP: same-origin. This should
  // force a different CoopRelatedGroup, and not only a different
  // BrowsingInstance.
  web_contents()->GetController().GoBack();
  ASSERT_TRUE(WaitForLoadStop(web_contents()));
  scoped_refptr<SiteInstanceImpl> final_si(
      current_frame_host()->GetSiteInstance());
  EXPECT_FALSE(initial_si->IsRelatedSiteInstance(final_si.get()));
  EXPECT_FALSE(initial_si->IsCoopRelatedSiteInstance(final_si.get()));
}

// This test verifies that after a simple page opens a popup to a COOP:
// restrict-properties page, we have two cross-BrowsingInstance proxies.
IN_PROC_BROWSER_TEST_P(CoopRestrictPropertiesProxiesBrowserTest,
                       SimpleCrossBrowsingInstanceProxy) {
  GURL regular_page(https_server()->GetURL("a.test", "/title1.html"));
  GURL coop_rp_page(https_server()->GetURL(
      "a.test",
      "/set-header"
      "?cross-origin-opener-policy: restrict-properties"));

  // Start by opening a popup to a COOP: rp page from a regular page.
  ASSERT_TRUE(NavigateToURL(shell(), regular_page));
  WebContentsImpl* popup_window = OpenPopupAndWaitForInitialRFHDeletion(
      current_frame_host(), coop_rp_page, "");
  RenderFrameHostImpl* popup_rfh = popup_window->GetPrimaryMainFrame();
  ASSERT_FALSE(current_frame_host()->GetSiteInstance()->IsRelatedSiteInstance(
      popup_rfh->GetSiteInstance()));
  ASSERT_TRUE(
      current_frame_host()->GetSiteInstance()->IsCoopRelatedSiteInstance(
          popup_rfh->GetSiteInstance()));

  // Verify that the opener and openee frames exist as proxies in each other's
  // SiteInstanceGroup. Note that the actual sites are the same, but they exist
  // in different SiteInstanceGroups because they are in different
  // BrowsingInstances.
  EXPECT_EQ(
      " Site A ------------ proxies for B\n"
      "Where A = https://a.test/\n"
      "      B = https://a.test/",
      DepictFrameTree(current_frame_host()->frame_tree_node()));
  EXPECT_EQ(
      " Site B ------------ proxies for A\n"
      "Where A = https://a.test/\n"
      "      B = https://a.test/",
      DepictFrameTree(popup_rfh->frame_tree_node()));
}

// This test verifies that a new iframe in a page that opened a popup in a
// different BrowsingInstance in the same CoopRelatedGroup is not visible to
// the popup.
IN_PROC_BROWSER_TEST_P(CoopRestrictPropertiesProxiesBrowserTest,
                       SubframeInMainPageCrossBrowsingInstanceProxy) {
  GURL regular_page(https_server()->GetURL("a.test", "/title1.html"));
  GURL coop_rp_page(https_server()->GetURL(
      "a.test",
      "/set-header"
      "?cross-origin-opener-policy: restrict-properties"));
  GURL regular_page_2(https_server()->GetURL("b.test", "/title1.html"));

  // Start with a page that opens a COOP: restrict-properties popup.
  ASSERT_TRUE(NavigateToURL(shell(), regular_page));
  WebContentsImpl* popup_window = OpenPopupAndWaitForInitialRFHDeletion(
      current_frame_host(), coop_rp_page, "");
  RenderFrameHostImpl* popup_rfh = popup_window->GetPrimaryMainFrame();
  ASSERT_FALSE(current_frame_host()->GetSiteInstance()->IsRelatedSiteInstance(
      popup_rfh->GetSiteInstance()));
  ASSERT_TRUE(
      current_frame_host()->GetSiteInstance()->IsCoopRelatedSiteInstance(
          popup_rfh->GetSiteInstance()));

  // Now add a cross-origin iframe in the main page.
  ASSERT_TRUE(ExecJs(current_frame_host(), JsReplace(R"(
    const frame = document.createElement('iframe');
    frame.src = $1;
    document.body.appendChild(frame);
  )",
                                                     regular_page_2)));
  ASSERT_TRUE(WaitForLoadStop(web_contents()));

  // The main frame should have proxies in both the popup's and iframe's
  // SiteInstanceGroup. The iframe should not have a proxy in the popup's
  // SiteInstanceGroup.
  EXPECT_EQ(
      " Site A ------------ proxies for B C\n"
      "   +--Site B ------- proxies for A\n"
      "Where A = https://a.test/\n"
      "      B = https://b.test/\n"
      "      C = https://a.test/",
      DepictFrameTree(current_frame_host()->frame_tree_node()));
  // The popup should only have a proxy in the main frame's SiteInstanceGroup,
  // but not the iframe's SiteInstanceGroup.
  EXPECT_EQ(
      " Site C ------------ proxies for A\n"
      "Where A = https://a.test/\n"
      "      C = https://a.test/",
      DepictFrameTree(popup_rfh->frame_tree_node()));
}

// This test verifies that a new iframe in a popup that lives in a different
// BrowsingInstance in the same CoopRelatedGroup has visibility of the opener
// frame and of no other frame in the other BrowsingInstance.
IN_PROC_BROWSER_TEST_P(CoopRestrictPropertiesProxiesBrowserTest,
                       SubframeInPopupCrossBrowsingInstanceProxy) {
  GURL regular_page(https_server()->GetURL("a.test", "/title1.html"));
  GURL regular_page_2(https_server()->GetURL("b.test", "/title1.html"));
  GURL coop_rp_page(https_server()->GetURL(
      "a.test",
      "/set-header"
      "?cross-origin-opener-policy: restrict-properties"));

  // Start with a page with a cross-origin iframe.
  ASSERT_TRUE(NavigateToURL(shell(), regular_page));
  ASSERT_TRUE(ExecJs(current_frame_host(), JsReplace(R"(
    const frame = document.createElement('iframe');
    frame.src = $1;
    document.body.appendChild(frame);
  )",
                                                     regular_page_2)));
  ASSERT_TRUE(WaitForLoadStop(web_contents()));
  RenderFrameHostImpl* main_page_iframe_rfh =
      current_frame_host()->child_at(0)->current_frame_host();

  // Verify that we have simple parent/child proxies.
  EXPECT_EQ(
      " Site A ------------ proxies for B\n"
      "   +--Site B ------- proxies for A\n"
      "Where A = https://a.test/\n"
      "      B = https://b.test/",
      DepictFrameTree(current_frame_host()->frame_tree_node()));

  // Now open a COOP: restrict-properties popup in another BrowsingInstance in
  // the same CoopRelatedGroup.
  WebContentsImpl* popup_window = OpenPopupAndWaitForInitialRFHDeletion(
      current_frame_host(), coop_rp_page, "");
  RenderFrameHostImpl* popup_rfh = popup_window->GetPrimaryMainFrame();
  ASSERT_FALSE(current_frame_host()->GetSiteInstance()->IsRelatedSiteInstance(
      popup_rfh->GetSiteInstance()));
  ASSERT_TRUE(
      current_frame_host()->GetSiteInstance()->IsCoopRelatedSiteInstance(
          popup_rfh->GetSiteInstance()));

  // The main frame should have proxies in both the popup's and iframe's
  // SiteInstanceGroups. The iframe should not have a proxy in the popup's
  // SiteInstanceGroup.
  EXPECT_EQ(
      " Site A ------------ proxies for B C\n"
      "   +--Site B ------- proxies for A\n"
      "Where A = https://a.test/\n"
      "      B = https://b.test/\n"
      "      C = https://a.test/",
      DepictFrameTree(current_frame_host()->frame_tree_node()));
  // The popup should only have a proxy in the main frame's SiteInstanceGroup,
  // but not the iframe's SiteInstanceGroup.
  EXPECT_EQ(
      " Site C ------------ proxies for A\n"
      "Where A = https://a.test/\n"
      "      C = https://a.test/",
      DepictFrameTree(popup_rfh->frame_tree_node()));

  // Now create a cross-origin subframe in the popup. We reuse the same url as
  // for the main page's iframe, but it should not matter since they are in
  // different BrowsingInstances.
  ASSERT_TRUE(ExecJs(popup_rfh, JsReplace(R"(
    const frame = document.createElement('iframe');
    frame.src = $1;
    document.body.appendChild(frame);
  )",
                                          regular_page_2)));
  ASSERT_TRUE(WaitForLoadStop(popup_window));
  RenderFrameHostImpl* popup_iframe_rfh =
      popup_rfh->child_at(0)->current_frame_host();
  ASSERT_FALSE(main_page_iframe_rfh->GetSiteInstance()->IsRelatedSiteInstance(
      popup_iframe_rfh->GetSiteInstance()));
  ASSERT_TRUE(
      main_page_iframe_rfh->GetSiteInstance()->IsCoopRelatedSiteInstance(
          popup_iframe_rfh->GetSiteInstance()));

  // The popup's iframe should only have a proxy in its parent's
  // SiteInstanceGroup. The popup's iframe's SiteInstanceGroup should have
  // proxies for the parent frame and the opener, but not the opener's iframe.
  EXPECT_EQ(
      " Site A ------------ proxies for B C D\n"
      "   +--Site B ------- proxies for A\n"
      "Where A = https://a.test/\n"
      "      B = https://b.test/\n"
      "      C = https://a.test/\n"
      "      D = https://b.test/",
      DepictFrameTree(current_frame_host()->frame_tree_node()));
  EXPECT_EQ(
      " Site C ------------ proxies for A D\n"
      "   +--Site D ------- proxies for C\n"
      "Where A = https://a.test/\n"
      "      C = https://a.test/\n"
      "      D = https://b.test/",
      DepictFrameTree(popup_rfh->frame_tree_node()));
}

// This test verifies that a subframe opening a popup in another
// BrowsingInstance in the same CoopRelatedGroup gets the appropriate proxies.
IN_PROC_BROWSER_TEST_P(CoopRestrictPropertiesProxiesBrowserTest,
                       SubframeOpenerCrossBrowsingInstanceProxy) {
  GURL regular_page(https_server()->GetURL("a.test", "/title1.html"));
  GURL regular_page_2(https_server()->GetURL("b.test", "/title1.html"));
  GURL coop_rp_page(https_server()->GetURL(
      "b.test",
      "/set-header"
      "?cross-origin-opener-policy: restrict-properties"));

  // Start with a page with a cross-origin iframe.
  ASSERT_TRUE(NavigateToURL(shell(), regular_page));
  ASSERT_TRUE(ExecJs(current_frame_host(), JsReplace(R"(
    const frame = document.createElement('iframe');
    frame.src = $1;
    document.body.appendChild(frame);
  )",
                                                     regular_page_2)));
  ASSERT_TRUE(WaitForLoadStop(web_contents()));
  RenderFrameHostImpl* iframe_rfh =
      current_frame_host()->child_at(0)->current_frame_host();

  // Verify that we have simple parent/child proxies.
  EXPECT_EQ(
      " Site A ------------ proxies for B\n"
      "   +--Site B ------- proxies for A\n"
      "Where A = https://a.test/\n"
      "      B = https://b.test/",
      DepictFrameTree(current_frame_host()->frame_tree_node()));

  // Now open a COOP: restrict-properties popup from the iframe.
  WebContentsImpl* popup_window =
      OpenPopupAndWaitForInitialRFHDeletion(iframe_rfh, coop_rp_page, "");
  RenderFrameHostImpl* popup_rfh = popup_window->GetPrimaryMainFrame();
  ASSERT_FALSE(iframe_rfh->GetSiteInstance()->IsRelatedSiteInstance(
      popup_rfh->GetSiteInstance()));
  ASSERT_TRUE(iframe_rfh->GetSiteInstance()->IsCoopRelatedSiteInstance(
      popup_rfh->GetSiteInstance()));

  // The main frame should have proxies in the iframe and the popup's
  // SiteInstanceGroup. The popup cannot reach the main frame, but
  // we still need a main frame proxy to have the iframe proxy, which cannot
  // exist by itself. The iframe should have a proxy in the main frame's and the
  // popup's SiteInstanceGroups.
  EXPECT_EQ(
      " Site A ------------ proxies for B C\n"
      "   +--Site B ------- proxies for A C\n"
      "Where A = https://a.test/\n"
      "      B = https://b.test/\n"
      "      C = https://b.test/",
      DepictFrameTree(current_frame_host()->frame_tree_node()));
  // The popup should have a proxy in the iframe's SiteInstanceGroup.
  EXPECT_EQ(
      " Site C ------------ proxies for B\n"
      "Where B = https://b.test/\n"
      "      C = https://b.test/",
      DepictFrameTree(popup_rfh->frame_tree_node()));
}

// This test verifies that a popup opened from a popup already in a different
// BrowsingInstance but same CoopRelatedGroup as its opener, cannot see its
// opener's opener.
IN_PROC_BROWSER_TEST_P(CoopRestrictPropertiesProxiesBrowserTest,
                       ChainedPopupsCrossBrowsingInstanceProxies) {
  GURL regular_page(https_server()->GetURL("a.test", "/title1.html"));
  GURL coop_rp_page(https_server()->GetURL(
      "a.test",
      "/set-header"
      "?cross-origin-opener-policy: restrict-properties"));
  GURL coop_rp_page_2(https_server()->GetURL(
      "b.test",
      "/set-header"
      "?cross-origin-opener-policy: restrict-properties"));

  // Start with a regular page that opens a COOP: restrict-properties popup.
  ASSERT_TRUE(NavigateToURL(shell(), regular_page));
  WebContentsImpl* first_popup_window = OpenPopupAndWaitForInitialRFHDeletion(
      current_frame_host(), coop_rp_page, "");
  RenderFrameHostImpl* first_popup_rfh =
      first_popup_window->GetPrimaryMainFrame();
  ASSERT_FALSE(current_frame_host()->GetSiteInstance()->IsRelatedSiteInstance(
      first_popup_rfh->GetSiteInstance()));
  ASSERT_TRUE(
      current_frame_host()->GetSiteInstance()->IsCoopRelatedSiteInstance(
          first_popup_rfh->GetSiteInstance()));

  // Verify that the opener and openee frames exist as proxies in each other's
  // SiteInstanceGroup.
  EXPECT_EQ(
      " Site A ------------ proxies for B\n"
      "Where A = https://a.test/\n"
      "      B = https://a.test/",
      DepictFrameTree(current_frame_host()->frame_tree_node()));
  EXPECT_EQ(
      " Site B ------------ proxies for A\n"
      "Where A = https://a.test/\n"
      "      B = https://a.test/",
      DepictFrameTree(first_popup_rfh->frame_tree_node()));

  // Open another popup from the first popup. The three pages live in different
  // BrowsingInstances.
  WebContentsImpl* second_popup_window = OpenPopupAndWaitForInitialRFHDeletion(
      first_popup_rfh, coop_rp_page_2, "");
  RenderFrameHostImpl* second_popup_rfh =
      second_popup_window->GetPrimaryMainFrame();
  ASSERT_FALSE(current_frame_host()->GetSiteInstance()->IsRelatedSiteInstance(
      second_popup_rfh->GetSiteInstance()));
  ASSERT_TRUE(
      current_frame_host()->GetSiteInstance()->IsCoopRelatedSiteInstance(
          second_popup_rfh->GetSiteInstance()));
  ASSERT_FALSE(first_popup_rfh->GetSiteInstance()->IsRelatedSiteInstance(
      second_popup_rfh->GetSiteInstance()));
  ASSERT_TRUE(first_popup_rfh->GetSiteInstance()->IsCoopRelatedSiteInstance(
      second_popup_rfh->GetSiteInstance()));

  // The main frame should not have a proxy in the second popup's
  // SiteInstanceGroup and vice versa. Only the first popup should have two
  // proxies, one in the main frame's and one in the second popup's
  // SiteInstanceGroup.
  EXPECT_EQ(
      " Site A ------------ proxies for B\n"
      "Where A = https://a.test/\n"
      "      B = https://a.test/",
      DepictFrameTree(current_frame_host()->frame_tree_node()));
  EXPECT_EQ(
      " Site B ------------ proxies for A C\n"
      "Where A = https://a.test/\n"
      "      B = https://a.test/\n"
      "      C = https://b.test/",
      DepictFrameTree(first_popup_rfh->frame_tree_node()));
  EXPECT_EQ(
      " Site C ------------ proxies for B\n"
      "Where B = https://a.test/\n"
      "      C = https://b.test/",
      DepictFrameTree(second_popup_rfh->frame_tree_node()));

  // Verify that it is not possible for the second popup to reach the main page,
  // as means of accessing it should be restricted.
  std::string result =
      EvalJs(second_popup_rfh,
             "try { window.opener.opener } catch (e) { e.toString(); }")
          .ExtractString();
  EXPECT_THAT(result, ::testing::MatchesRegex(kCoopRpErrorMessageRegex));
}

// This test verifies that a new popup opened from a popup in the same
// BrowsingInstance will have visibility of all its BrowsingInstance frames, but
// will only have visibility of the direct opener frame in a different
// BrowsingInstance in the same CoopRelatedGroup.
// TODO(crbug.com/40286486): Failing on Mac bots
#if BUILDFLAG(IS_MAC)
#define MAYBE_ChainedPopupsMixedBrowsingInstanceProxies DISABLED_ChainedPopupsMixedBrowsingInstanceProxies
#else
#define MAYBE_ChainedPopupsMixedBrowsingInstanceProxies ChainedPopupsMixedBrowsingInstanceProxies
#endif
IN_PROC_BROWSER_TEST_P(CoopRestrictPropertiesProxiesBrowserTest,
                       MAYBE_ChainedPopupsMixedBrowsingInstanceProxies) {
  GURL coop_rp_page(https_server()->GetURL(
      "a.test",
      "/set-header"
      "?cross-origin-opener-policy: restrict-properties"));
  GURL regular_page(https_server()->GetURL("a.test", "/title1.html"));
  GURL regular_page_2(https_server()->GetURL("b.test", "/title1.html"));

  // Start with a COOP: restrict-properties page that opens a regular popup.
  ASSERT_TRUE(NavigateToURL(shell(), coop_rp_page));
  WebContentsImpl* first_popup_window = OpenPopupAndWaitForInitialRFHDeletion(
      current_frame_host(), regular_page, "");
  RenderFrameHostImpl* first_popup_rfh =
      first_popup_window->GetPrimaryMainFrame();
  ASSERT_FALSE(current_frame_host()->GetSiteInstance()->IsRelatedSiteInstance(
      first_popup_rfh->GetSiteInstance()));
  ASSERT_TRUE(
      current_frame_host()->GetSiteInstance()->IsCoopRelatedSiteInstance(
          first_popup_rfh->GetSiteInstance()));

  // Verify that the opener and openee frames exist as proxies in each other's
  // SiteInstanceGroup.
  EXPECT_EQ(
      " Site A ------------ proxies for B\n"
      "Where A = https://a.test/\n"
      "      B = https://a.test/",
      DepictFrameTree(current_frame_host()->frame_tree_node()));
  EXPECT_EQ(
      " Site B ------------ proxies for A\n"
      "Where A = https://a.test/\n"
      "      B = https://a.test/",
      DepictFrameTree(first_popup_rfh->frame_tree_node()));

  // Open a cross-origin popup from the first popup. It should live in a
  // different SiteInstance in the same BrowsingInstance.
  WebContentsImpl* second_popup_window = OpenPopupAndWaitForInitialRFHDeletion(
      first_popup_rfh, regular_page_2, "");
  RenderFrameHostImpl* second_popup_rfh =
      second_popup_window->GetPrimaryMainFrame();
  ASSERT_FALSE(current_frame_host()->GetSiteInstance()->IsRelatedSiteInstance(
      second_popup_rfh->GetSiteInstance()));
  ASSERT_TRUE(
      current_frame_host()->GetSiteInstance()->IsCoopRelatedSiteInstance(
          second_popup_rfh->GetSiteInstance()));
  ASSERT_TRUE(first_popup_rfh->GetSiteInstance()->IsRelatedSiteInstance(
      second_popup_rfh->GetSiteInstance()));

  // The original frame should have proxies in the first and second popup's
  // SiteInstanceGroups, because they can respectively use opener and
  // opener.opener to reach the original frame.
  EXPECT_EQ(
      " Site A ------------ proxies for B C\n"
      "Where A = https://a.test/\n"
      "      B = https://a.test/\n"
      "      C = https://b.test/",
      DepictFrameTree(current_frame_host()->frame_tree_node()));
  // The first popup frame should have proxies in the original frame's and the
  // second popup's SiteInstanceGroups, which can both reach it.
  EXPECT_EQ(
      " Site B ------------ proxies for A C\n"
      "Where A = https://a.test/\n"
      "      B = https://a.test/\n"
      "      C = https://b.test/",
      DepictFrameTree(first_popup_rfh->frame_tree_node()));
  // Finally the second popup frame should only have a proxy in the first
  // popup's SiteInstanceGroup, because the original frame has no way to reach
  // it.
  EXPECT_EQ(
      " Site C ------------ proxies for B\n"
      "Where B = https://a.test/\n"
      "      C = https://b.test/",
      DepictFrameTree(second_popup_rfh->frame_tree_node()));
}

// Allows waiting until a frame name change is effective in the Browser process.
class FrameNameChangedWaiter : public WebContentsObserver {
 public:
  explicit FrameNameChangedWaiter(WebContents* web_contents,
                                  RenderFrameHostImpl* frame,
                                  const std::string& expected_name)
      : WebContentsObserver(web_contents),
        frame_(frame),
        expected_name_(expected_name) {}

  // This will wait until the given frame, in the given WebContents, changes its
  // name to the expected name, all given during construction.
  void Wait() { run_loop_.Run(); }

 private:
  void FrameNameChanged(RenderFrameHost* render_frame_host,
                        const std::string& name) override {
    if (render_frame_host == frame_.get() && name == expected_name_) {
      run_loop_.Quit();
    }
  }

  raw_ptr<RenderFrameHostImpl> frame_;
  std::string expected_name_;
  base::RunLoop run_loop_;
};

// This test verifies that proxies usually created to support named targeting
// are not created for cross-BrowsingInstance frames.
// TODO(crbug.com/40276662): This test will likely need to change if we
// implement per-BrowsingInstance names. In that case, named targeting would be
// possible using the per-BrowsingContextGroup names, and proxies should be
// created.
IN_PROC_BROWSER_TEST_P(CoopRestrictPropertiesProxiesBrowserTest,
                       NamedTargetingCrossBrowsingInstanceProxies) {
  GURL regular_page(https_server()->GetURL("a.test", "/title1.html"));
  GURL regular_page_2(https_server()->GetURL("b.test", "/title1.html"));
  GURL coop_rp_page(https_server()->GetURL(
      "a.test",
      "/set-header"
      "?cross-origin-opener-policy: restrict-properties"));

  // Start with a regular page, with a cross-origin subframe.
  ASSERT_TRUE(NavigateToURL(shell(), regular_page));
  ASSERT_TRUE(ExecJs(current_frame_host(), JsReplace(R"(
    const frame = document.createElement('iframe');
    frame.src = $1;
    document.body.appendChild(frame);
  )",
                                                     regular_page_2)));
  ASSERT_TRUE(WaitForLoadStop(web_contents()));

  // Now open a COOP: restrict-properties popup with a name. The name should be
  // cleared and trigger no extra proxy creation.
  WebContentsImpl* popup_window = OpenPopupAndWaitForInitialRFHDeletion(
      current_frame_host(), coop_rp_page, "test_name");
  RenderFrameHostImpl* popup_rfh = popup_window->GetPrimaryMainFrame();
  ASSERT_FALSE(current_frame_host()->GetSiteInstance()->IsRelatedSiteInstance(
      popup_rfh->GetSiteInstance()));
  ASSERT_TRUE(
      current_frame_host()->GetSiteInstance()->IsCoopRelatedSiteInstance(
          popup_rfh->GetSiteInstance()));

  // Verify that the popup frame is not proxied in the iframe's
  // SiteInstanceGroup.
  EXPECT_EQ(
      " Site A ------------ proxies for B C\n"
      "   +--Site B ------- proxies for A\n"
      "Where A = https://a.test/\n"
      "      B = https://b.test/\n"
      "      C = https://a.test/",
      DepictFrameTree(current_frame_host()->frame_tree_node()));
  EXPECT_EQ(
      " Site C ------------ proxies for A\n"
      "Where A = https://a.test/\n"
      "      C = https://a.test/",
      DepictFrameTree(popup_rfh->frame_tree_node()));

  // Manually update the popup name. By the time the WebContentsObserver gets
  // notified of a frame name change, we've run the proxy creation code, so this
  // should be enough to wait for.
  FrameNameChangedWaiter frame_name_changed(popup_window, popup_rfh,
                                            "another_name");
  ASSERT_TRUE(ExecJs(popup_rfh, "window.name = 'another_name';"));
  frame_name_changed.Wait();

  // No extra proxy should be created when a name is set.
  EXPECT_EQ(
      " Site A ------------ proxies for B C\n"
      "   +--Site B ------- proxies for A\n"
      "Where A = https://a.test/\n"
      "      B = https://b.test/\n"
      "      C = https://a.test/",
      DepictFrameTree(current_frame_host()->frame_tree_node()));
  EXPECT_EQ(
      " Site C ------------ proxies for A\n"
      "Where A = https://a.test/\n"
      "      C = https://a.test/",
      DepictFrameTree(popup_rfh->frame_tree_node()));
}

// This test verifies that proxies are created on demand to support postMessage
// event.source, even cross-BrowsingInstance.
IN_PROC_BROWSER_TEST_P(CoopRestrictPropertiesProxiesBrowserTest,
                       PostMessageProxiesCrossBrowsingInstance) {
  GURL regular_page(https_server()->GetURL("a.test", "/title1.html"));
  GURL coop_rp_page(https_server()->GetURL(
      "a.test",
      "/set-header"
      "?cross-origin-opener-policy: restrict-properties"));
  GURL regular_page_2(https_server()->GetURL("b.test", "/title1.html"));

  // Start from a regular page and open a COOP: restrict-properties popup.
  ASSERT_TRUE(NavigateToURL(shell(), regular_page));
  WebContentsImpl* popup_window = OpenPopupAndWaitForInitialRFHDeletion(
      current_frame_host(), coop_rp_page, "");
  RenderFrameHostImpl* popup_rfh = popup_window->GetPrimaryMainFrame();
  ASSERT_FALSE(current_frame_host()->GetSiteInstance()->IsRelatedSiteInstance(
      popup_rfh->GetSiteInstance()));
  ASSERT_TRUE(
      current_frame_host()->GetSiteInstance()->IsCoopRelatedSiteInstance(
          popup_rfh->GetSiteInstance()));

  // Verify that the opener and openee frames exist as proxies in each other's
  // SiteInstanceGroup.
  EXPECT_EQ(
      " Site A ------------ proxies for B\n"
      "Where A = https://a.test/\n"
      "      B = https://a.test/",
      DepictFrameTree(current_frame_host()->frame_tree_node()));
  EXPECT_EQ(
      " Site B ------------ proxies for A\n"
      "Where A = https://a.test/\n"
      "      B = https://a.test/",
      DepictFrameTree(popup_rfh->frame_tree_node()));

  // Add a cross-origin iframe to the popup.
  ASSERT_TRUE(ExecJs(popup_rfh, JsReplace(R"(
    const frame = document.createElement('iframe');
    frame.src = $1;
    document.body.appendChild(frame);
  )",
                                          regular_page_2)));
  ASSERT_TRUE(WaitForLoadStop(popup_window));
  RenderFrameHostImpl* iframe_rfh =
      popup_rfh->child_at(0)->current_frame_host();

  // The iframe can see the original frame via parent.opener, but there should
  // be no proxy for the iframe in the original frame's SiteInstanceGroup,
  // because the original frame should not be able to access it at this point.
  EXPECT_EQ(
      " Site A ------------ proxies for B C\n"
      "Where A = https://a.test/\n"
      "      B = https://a.test/\n"
      "      C = https://b.test/",
      DepictFrameTree(current_frame_host()->frame_tree_node()));
  EXPECT_EQ(
      " Site B ------------ proxies for A C\n"
      "   +--Site C ------- proxies for B\n"
      "Where A = https://a.test/\n"
      "      B = https://a.test/\n"
      "      C = https://b.test/",
      DepictFrameTree(popup_rfh->frame_tree_node()));

  // Now send a postMessage from the iframe to the main frame, and wait for it
  // to be received.
  ASSERT_TRUE(ExecJs(current_frame_host(), R"(
      window.future_message = new Promise(r => {
        onmessage = (event) => {
          if (event.data == 'test') {
            window.post_message_source = event.source;
            r();
          }
        }
      }); 0;)"));  // This avoids waiting on the promise right now.
  ASSERT_TRUE(ExecJs(iframe_rfh, "window.top.opener.postMessage('test', '*')"));
  ASSERT_TRUE(ExecJs(current_frame_host(), "window.future_message"));

  // Verify that an iframe proxy was created in the main frame's
  // SiteInstanceGroup to support event.source.
  EXPECT_EQ(
      " Site A ------------ proxies for B C\n"
      "Where A = https://a.test/\n"
      "      B = https://a.test/\n"
      "      C = https://b.test/",
      DepictFrameTree(current_frame_host()->frame_tree_node()));
  EXPECT_EQ(
      " Site B ------------ proxies for A C\n"
      "   +--Site C ------- proxies for A B\n"
      "Where A = https://a.test/\n"
      "      B = https://a.test/\n"
      "      C = https://b.test/",
      DepictFrameTree(popup_rfh->frame_tree_node()));

  // Finally postMessage to event.source to make sure the proxy is functional.
  ASSERT_TRUE(ExecJs(iframe_rfh, R"(
      window.future_message = new Promise(r => {
        onmessage = (event) => {
          if (event.data == 'test') r();
        }
      }); 0;)"));  // This avoids waiting on the promise right now.
  ASSERT_TRUE(ExecJs(current_frame_host(),
                     "window.post_message_source.postMessage('test', '*')"));
  ASSERT_TRUE(ExecJs(iframe_rfh, "window.future_message"));
}

// This test verifies that proxies are created on demand to support postMessage
// event.source, even cross-BrowsingInstance, even when the source is an iframe
// for which the target frame's SiteInstanceGroup does not have a main frame
// proxy yet.
// TODO(crbug.com/40286486) Failing on mac bots
#if BUILDFLAG(IS_MAC)
#define MAYBE_SubframePostMessageProxiesCrossBrowsingInstance DISABLED_SubframePostMessageProxiesCrossBrowsingInstance
#else
#define MAYBE_SubframePostMessageProxiesCrossBrowsingInstance SubframePostMessageProxiesCrossBrowsingInstance
#endif
IN_PROC_BROWSER_TEST_P(CoopRestrictPropertiesProxiesBrowserTest,
                       MAYBE_SubframePostMessageProxiesCrossBrowsingInstance) {
  GURL coop_rp_page(https_server()->GetURL(
      "a.test",
      "/set-header"
      "?cross-origin-opener-policy: restrict-properties"));
  GURL regular_page(https_server()->GetURL("a.test", "/title1.html"));
  GURL regular_page_2(https_server()->GetURL("b.test", "/title1.html"));
  GURL regular_page_3(https_server()->GetURL("c.test", "/title1.html"));

  // Start from a COOP: restrict-properties opening a regular popup.
  ASSERT_TRUE(NavigateToURL(shell(), coop_rp_page));
  WebContentsImpl* first_popup_window = OpenPopupAndWaitForInitialRFHDeletion(
      current_frame_host(), regular_page, "");
  RenderFrameHostImpl* first_popup_rfh =
      first_popup_window->GetPrimaryMainFrame();
  ASSERT_FALSE(current_frame_host()->GetSiteInstance()->IsRelatedSiteInstance(
      first_popup_rfh->GetSiteInstance()));
  ASSERT_TRUE(
      current_frame_host()->GetSiteInstance()->IsCoopRelatedSiteInstance(
          first_popup_rfh->GetSiteInstance()));

  // Verify that the opener and openee frames exist as proxies in each other's
  // SiteInstanceGroup.
  EXPECT_EQ(
      " Site A ------------ proxies for B\n"
      "Where A = https://a.test/\n"
      "      B = https://a.test/",
      DepictFrameTree(current_frame_host()->frame_tree_node()));
  EXPECT_EQ(
      " Site B ------------ proxies for A\n"
      "Where A = https://a.test/\n"
      "      B = https://a.test/",
      DepictFrameTree(first_popup_rfh->frame_tree_node()));

  // Then open a popup from the popup, in the same BrowsingInstance and add
  // a cross-origin iframe to it. This setup makes sure that we have an iframe
  // and a main frame that are unknown to the main page.
  WebContentsImpl* second_popup_window = OpenPopupAndWaitForInitialRFHDeletion(
      first_popup_rfh, regular_page_2, "");
  RenderFrameHostImpl* second_popup_rfh =
      second_popup_window->GetPrimaryMainFrame();
  ASSERT_TRUE(first_popup_rfh->GetSiteInstance()->IsRelatedSiteInstance(
      second_popup_rfh->GetSiteInstance()));

  ASSERT_TRUE(ExecJs(second_popup_rfh, JsReplace(R"(
    const frame = document.createElement('iframe');
    frame.src = $1;
    document.body.appendChild(frame);
  )",
                                                 regular_page_3)));
  ASSERT_TRUE(WaitForLoadStop(second_popup_window));
  RenderFrameHostImpl* iframe_rfh =
      second_popup_rfh->child_at(0)->current_frame_host();

  // The main frame should have proxies in the first popup's, second popup's and
  // iframe's SiteInstanceGroups.
  EXPECT_EQ(
      " Site A ------------ proxies for B C D\n"
      "Where A = https://a.test/\n"
      "      B = https://a.test/\n"
      "      C = https://b.test/\n"
      "      D = https://c.test/",
      DepictFrameTree(current_frame_host()->frame_tree_node()));
  // The first popup should have proxies in the main frame's, second popup's and
  // iframe's SiteInstanceGroups.
  EXPECT_EQ(
      " Site B ------------ proxies for A C D\n"
      "Where A = https://a.test/\n"
      "      B = https://a.test/\n"
      "      C = https://b.test/\n"
      "      D = https://c.test/",
      DepictFrameTree(first_popup_rfh->frame_tree_node()));
  // The second popup should have proxies in the first popup's and iframe's
  // SiteInstanceGroups. The iframe popup should have proxies in the first and
  // second popup's SiteInstanceGroup. Note that the main frame does not know
  // about the second popup nor its iframe.
  EXPECT_EQ(
      " Site C ------------ proxies for B D\n"
      "   +--Site D ------- proxies for B C\n"
      "Where B = https://a.test/\n"
      "      C = https://b.test/\n"
      "      D = https://c.test/",
      DepictFrameTree(second_popup_rfh->frame_tree_node()));

  // Now send a postMessage from the iframe to the main frame, and wait for it
  // to be received.
  ASSERT_TRUE(ExecJs(current_frame_host(), R"(
      window.future_message = new Promise(r => {
        onmessage = (event) => {
          if (event.data == 'test') r();
        }
      }); 0;)"));  // This avoids waiting on the promise right now.
  ASSERT_TRUE(
      ExecJs(iframe_rfh, "window.top.opener.opener.postMessage('test', '*')"));
  ASSERT_TRUE(ExecJs(current_frame_host(), "window.future_message"));

  // Verify that an iframe proxy and a second popup proxy were created in the
  // main frame's SiteInstanceGroup to support event.source, and to make sure
  // the iframe proxy does not float around without a main frame proxy.
  EXPECT_EQ(
      " Site C ------------ proxies for A B D\n"
      "   +--Site D ------- proxies for A B C\n"
      "Where A = https://a.test/\n"
      "      B = https://a.test/\n"
      "      C = https://b.test/\n"
      "      D = https://c.test/",
      DepictFrameTree(second_popup_rfh->frame_tree_node()));
}

// Smoke test for the case where a proxy for a given subframe is created before
// other subframe proxies, that might be below it in the indexed order.
// TODO(1495328,40269878): Failing on bots in multiple platforms
IN_PROC_BROWSER_TEST_P(CoopRestrictPropertiesProxiesBrowserTest,
                       DISABLED_SubframesProxiesInWrongOrderSmokeTest) {
  GURL coop_rp_page(https_server()->GetURL(
      "a.test",
      "/set-header"
      "?cross-origin-opener-policy: restrict-properties"));
  GURL regular_page(https_server()->GetURL("b.test", "/title1.html"));
  GURL regular_page_2(https_server()->GetURL("c.test", "/title1.html"));
  GURL regular_page_3(https_server()->GetURL("d.test", "/title1.html"));
  GURL regular_page_4(https_server()->GetURL("e.test", "/title1.html"));

  // Start from a COOP: restrict-properties opening a regular popup.
  ASSERT_TRUE(NavigateToURL(shell(), coop_rp_page));
  WebContentsImpl* first_popup_window = OpenPopupAndWaitForInitialRFHDeletion(
      current_frame_host(), regular_page, "");
  RenderFrameHostImpl* first_popup_rfh =
      first_popup_window->GetPrimaryMainFrame();
  ASSERT_FALSE(current_frame_host()->GetSiteInstance()->IsRelatedSiteInstance(
      first_popup_rfh->GetSiteInstance()));
  ASSERT_TRUE(
      current_frame_host()->GetSiteInstance()->IsCoopRelatedSiteInstance(
          first_popup_rfh->GetSiteInstance()));

  // Verify that the opener and openee frames exist as proxies in each other's
  // SiteInstanceGroup.
  EXPECT_EQ(
      " Site A ------------ proxies for B\n"
      "Where A = https://a.test/\n"
      "      B = https://b.test/",
      DepictFrameTree(current_frame_host()->frame_tree_node()));
  EXPECT_EQ(
      " Site B ------------ proxies for A\n"
      "Where A = https://a.test/\n"
      "      B = https://b.test/",
      DepictFrameTree(first_popup_rfh->frame_tree_node()));

  // Then open a popup from the popup, in the same BrowsingInstance and add
  // a two cross-origin iframes to it. This setup makes sure that we have two
  // iframes and a main frame that are unknown to the main page.
  WebContentsImpl* second_popup_window = OpenPopupAndWaitForInitialRFHDeletion(
      first_popup_rfh, regular_page_2, "");
  RenderFrameHostImpl* second_popup_rfh =
      second_popup_window->GetPrimaryMainFrame();
  ASSERT_TRUE(first_popup_rfh->GetSiteInstance()->IsRelatedSiteInstance(
      second_popup_rfh->GetSiteInstance()));

  ASSERT_TRUE(
      ExecJs(second_popup_rfh, JsReplace(R"(
    const frame1 = document.createElement('iframe');
    const frame2 = document.createElement('iframe');
    frame1.src = $1;
    frame2.src = $2;
    document.body.appendChild(frame1);
    document.body.appendChild(frame2);
  )",
                                         regular_page_3, regular_page_4)));
  ASSERT_TRUE(WaitForLoadStop(second_popup_window));
  RenderFrameHostImpl* first_iframe_rfh =
      second_popup_rfh->child_at(0)->current_frame_host();
  RenderFrameHostImpl* second_iframe_rfh =
      second_popup_rfh->child_at(1)->current_frame_host();

  // Both iframes should have proxies in their parent's, parent's opener's and
  // other subframe's SiteInstanceGroup. The original frame should not know
  // about them at this stage.
  EXPECT_EQ(
      " Site A ------------ proxies for B C D E\n"
      "Where A = https://a.test/\n"
      "      B = https://b.test/\n"
      "      C = https://c.test/\n"
      "      D = https://d.test/\n"
      "      E = https://e.test/",
      DepictFrameTree(current_frame_host()->frame_tree_node()));
  EXPECT_EQ(
      " Site B ------------ proxies for A C D E\n"
      "Where A = https://a.test/\n"
      "      B = https://b.test/\n"
      "      C = https://c.test/\n"
      "      D = https://d.test/\n"
      "      E = https://e.test/",
      DepictFrameTree(first_popup_rfh->frame_tree_node()));
  EXPECT_EQ(
      " Site C ------------ proxies for B D E\n"
      "   |--Site D ------- proxies for B C E\n"
      "   +--Site E ------- proxies for B C D\n"
      "Where B = https://b.test/\n"
      "      C = https://c.test/\n"
      "      D = https://d.test/\n"
      "      E = https://e.test/",
      DepictFrameTree(second_popup_rfh->frame_tree_node()));

  // Now send a postMessage from the second iframe to the main frame, and wait
  // for it to be received.
  ASSERT_TRUE(ExecJs(current_frame_host(), R"(
      window.future_message = new Promise(r => {
        onmessage = (event) => {
          if (event.data == 'test') r();
        }
      }); 0;)"));  // This avoids waiting on the promise right now.
  ASSERT_TRUE(ExecJs(second_iframe_rfh,
                     "window.top.opener.opener.postMessage('test', '*')"));
  ASSERT_TRUE(ExecJs(current_frame_host(), "window.future_message"));

  // The second iframe should now have a proxy in the main frame's
  // SiteInstanceGroup, but the first iframe should not yet.
  EXPECT_EQ(
      " Site C ------------ proxies for A B D E\n"
      "   |--Site D ------- proxies for B C E\n"
      "   +--Site E ------- proxies for A B C D\n"
      "Where A = https://a.test/\n"
      "      B = https://b.test/\n"
      "      C = https://c.test/\n"
      "      D = https://d.test/\n"
      "      E = https://e.test/",
      DepictFrameTree(second_popup_rfh->frame_tree_node()));

  // Now send a postMessage from the first iframe to the main frame, and wait
  // for it to be received.
  ASSERT_TRUE(ExecJs(current_frame_host(), R"(
      window.future_message = new Promise(r => {
        onmessage = (event) => {
          if (event.data == 'test') r();
        }
      }); 0;)"));  // This avoids waiting on the promise right now.
  ASSERT_TRUE(ExecJs(first_iframe_rfh,
                     "window.top.opener.opener.postMessage('test', '*')"));
  ASSERT_TRUE(ExecJs(current_frame_host(), "window.future_message"));

  // The first iframe should now have a proxy in the main frame's
  // SiteInstanceGroup. Creating proxies in the wrong order should not crash or
  // cause problems.
  EXPECT_EQ(
      " Site C ------------ proxies for A B D E\n"
      "   |--Site D ------- proxies for A B C E\n"
      "   +--Site E ------- proxies for A B C D\n"
      "Where A = https://a.test/\n"
      "      B = https://b.test/\n"
      "      C = https://c.test/\n"
      "      D = https://d.test/\n"
      "      E = https://e.test/",
      DepictFrameTree(second_popup_rfh->frame_tree_node()));
}

// This test verifies that a BrowsingInstance swap to a different
// CoopRelatedGroup clears preexisting proxies to other BrowsingInstances.
IN_PROC_BROWSER_TEST_P(
    CoopRestrictPropertiesProxiesBrowserTest,
    StrictBrowsingInstanceSwapDeletesCrossBrowsingInstanceProxies) {
  GURL regular_page(https_server()->GetURL("a.test", "/title1.html"));
  GURL coop_rp_page(https_server()->GetURL(
      "a.test",
      "/set-header"
      "?cross-origin-opener-policy: restrict-properties"));
  GURL coop_so_page(
      https_server()->GetURL("a.test",
                             "/set-header"
                             "?cross-origin-opener-policy: same-origin"));

  // Start by opening a popup to a COOP: restrict-properties page from a regular
  // page.
  ASSERT_TRUE(NavigateToURL(shell(), regular_page));
  WebContentsImpl* popup_window = OpenPopupAndWaitForInitialRFHDeletion(
      current_frame_host(), coop_rp_page, "");
  RenderFrameHostImpl* coop_rp_rfh = popup_window->GetPrimaryMainFrame();
  scoped_refptr<SiteInstanceImpl> coop_rp_si = coop_rp_rfh->GetSiteInstance();
  ASSERT_FALSE(current_frame_host()->GetSiteInstance()->IsRelatedSiteInstance(
      coop_rp_si.get()));
  ASSERT_TRUE(
      current_frame_host()->GetSiteInstance()->IsCoopRelatedSiteInstance(
          coop_rp_si.get()));

  // Verify that the opener and openee frames exist as proxies in each other's
  // SiteInstanceGroup.
  EXPECT_EQ(
      " Site A ------------ proxies for B\n"
      "Where A = https://a.test/\n"
      "      B = https://a.test/",
      DepictFrameTree(current_frame_host()->frame_tree_node()));
  EXPECT_EQ(
      " Site B ------------ proxies for A\n"
      "Where A = https://a.test/\n"
      "      B = https://a.test/",
      DepictFrameTree(coop_rp_rfh->frame_tree_node()));

  // Navigate the popup to a COOP: same-origin page. This should trigger a swap
  // to a BrowsingInstance in a different CoopRelatedGroup.
  RenderFrameDeletedObserver popup_deleted_observer_1(coop_rp_rfh);
  ASSERT_TRUE(NavigateToURL(popup_window, coop_so_page));
  RenderFrameHostImpl* coop_so_rfh = popup_window->GetPrimaryMainFrame();
  scoped_refptr<SiteInstanceImpl> coop_so_si = coop_so_rfh->GetSiteInstance();
  ASSERT_FALSE(current_frame_host()->GetSiteInstance()->IsRelatedSiteInstance(
      coop_so_si.get()));
  ASSERT_FALSE(current_frame_host()->GetSiteInstance()->IsRelatedSiteInstance(
      coop_so_si.get()));

  // Wait for the previous RFH to be deleted so that the proxy count does not
  // flake.
  popup_deleted_observer_1.WaitUntilDeleted();

  // The cross-BrowsingInstance proxies should be gone.
  EXPECT_EQ(0u, current_frame_host()->GetProxyCount());
  EXPECT_EQ(0u, coop_so_rfh->GetProxyCount());

  // Finally go back. The original COOP: restrict-properties SiteInstance will
  // be reused.
  RenderFrameDeletedObserver popup_deleted_observer_2(coop_so_rfh);
  popup_window->GetController().GoBack();
  WaitForLoadStop(popup_window);
  RenderFrameHostImpl* back_rfh = popup_window->GetPrimaryMainFrame();
  ASSERT_EQ(back_rfh->GetSiteInstance(), coop_rp_si.get());
  EXPECT_FALSE(
      back_rfh->GetSiteInstance()->IsCoopRelatedSiteInstance(coop_so_si.get()));

  // BackForwardCache will kick in and store the RenderFrameHost, preventing its
  // deletion.
  if (!IsBackForwardCacheEnabled()) {
    popup_deleted_observer_2.WaitUntilDeleted();
  }

  // Proxies are not re-created, because the opener was removed by going to
  // COOP: same-origin, and is not restored when going back, despite the
  // SiteInstance reuse.
  EXPECT_EQ(0u, current_frame_host()->GetProxyCount());
  EXPECT_EQ(0u, back_rfh->GetProxyCount());
}

// This test verifies that proxies are as expected after a navigation. Start on
// a page with an existing SiteInstance before navigating to a COOP:
// restrict-properties page.
IN_PROC_BROWSER_TEST_P(CoopRestrictPropertiesProxiesBrowserTest,
                       ExistingSiteInstanceNavigationProxies) {
  GURL regular_page(https_server()->GetURL("a.test", "/title1.html"));
  GURL regular_page_2(https_server()->GetURL("b.test", "/title1.html"));
  GURL coop_rp_page(https_server()->GetURL(
      "c.test",
      "/set-header"
      "?cross-origin-opener-policy: restrict-properties"));

  // Start from a regular page and open a regular cross-origin popup.
  ASSERT_TRUE(NavigateToURL(shell(), regular_page));
  WebContentsImpl* first_popup_window = OpenPopupAndWaitForInitialRFHDeletion(
      current_frame_host(), regular_page_2, "");
  RenderFrameHostImpl* first_popup_rfh =
      first_popup_window->GetPrimaryMainFrame();
  ASSERT_TRUE(current_frame_host()->GetSiteInstance()->IsRelatedSiteInstance(
      first_popup_rfh->GetSiteInstance()));

  // Verify that the opener and openee frames exist as proxies in each other's
  // SiteInstanceGroup.
  EXPECT_EQ(
      " Site A ------------ proxies for B\n"
      "Where A = https://a.test/\n"
      "      B = https://b.test/",
      DepictFrameTree(current_frame_host()->frame_tree_node()));
  EXPECT_EQ(
      " Site B ------------ proxies for A\n"
      "Where A = https://a.test/\n"
      "      B = https://b.test/",
      DepictFrameTree(first_popup_rfh->frame_tree_node()));

  // Now open from the first popup a second popup with the same url as the main
  // page. It should reuse its SiteInstance.
  WebContentsImpl* second_popup_window =
      OpenPopupAndWaitForInitialRFHDeletion(first_popup_rfh, regular_page, "");
  RenderFrameHostImpl* second_popup_rfh =
      second_popup_window->GetPrimaryMainFrame();
  ASSERT_EQ(current_frame_host()->GetSiteInstance(),
            second_popup_rfh->GetSiteInstance());

  // The main frame should have a proxy in the first popup's SiteInstanceGroup.
  EXPECT_EQ(
      " Site A ------------ proxies for B\n"
      "Where A = https://a.test/\n"
      "      B = https://b.test/",
      DepictFrameTree(current_frame_host()->frame_tree_node()));
  // The first popup should have a proxy in the main frame's and second popup's
  // SiteInstanceGroup (which are the same).
  EXPECT_EQ(
      " Site B ------------ proxies for A\n"
      "Where A = https://a.test/\n"
      "      B = https://b.test/",
      DepictFrameTree(first_popup_rfh->frame_tree_node()));
  // The second popup should have a proxy in the first popup's
  // SiteInstanceGroup.
  EXPECT_EQ(
      " Site A ------------ proxies for B\n"
      "Where A = https://a.test/\n"
      "      B = https://b.test/",
      DepictFrameTree(second_popup_rfh->frame_tree_node()));

  // Finally, navigate the second popup to a COOP: restrict-properties page.
  RenderFrameDeletedObserver initial_popup_rfh_observer(second_popup_rfh);
  ASSERT_TRUE(NavigateToURL(second_popup_window, coop_rp_page));
  RenderFrameHostImpl* final_second_popup_rfh =
      second_popup_window->GetPrimaryMainFrame();
  initial_popup_rfh_observer.WaitUntilDeleted();

  // The main frame should have a proxy in the first popup's SiteInstanceGroup.
  EXPECT_EQ(
      " Site A ------------ proxies for B\n"
      "Where A = https://a.test/\n"
      "      B = https://b.test/",
      DepictFrameTree(current_frame_host()->frame_tree_node()));
  // The first popup should have a proxy in the main frame's and second popup's
  // SiteInstanceGroups (which are now different).
  EXPECT_EQ(
      " Site B ------------ proxies for A C\n"
      "Where A = https://a.test/\n"
      "      B = https://b.test/\n"
      "      C = https://c.test/",
      DepictFrameTree(first_popup_rfh->frame_tree_node()));
  // The second popup should have a proxy in the first popup's
  // SiteInstanceGroup.
  //
  // It also exists as a proxy in the main frame's SiteInstanceGroup, because
  // the page was initially in the same SiteInstance as the main page. When the
  // cross-site navigation starts, a proxy of the second popup is created in
  // its own SiteInstanceGroup, which happens to be the same as another frame.
  // This proxy is never deleted because there is still a frame using the
  // SiteInstanceGroup after the navigation is finished. This should be fine
  // because being in the same SiteInstanceGroup in the first place means that
  // the frame retaining the proxy knew about this frame's existence.
  EXPECT_EQ(
      " Site C ------------ proxies for A B\n"
      "Where A = https://a.test/\n"
      "      B = https://b.test/\n"
      "      C = https://c.test/",
      DepictFrameTree(final_second_popup_rfh->frame_tree_node()));

  // To confirm that the second popup is not leaking extra information in the
  // main frame's SiteInstanceGroup, add an iframe in it and check that it does
  // not have a proxy in the main frame's SiteInstanceGroup.
  ASSERT_TRUE(ExecJs(final_second_popup_rfh, JsReplace(R"(
    const frame = document.createElement('iframe');
    frame.src = $1;
    document.body.appendChild(frame);
  )",
                                                       regular_page)));
  ASSERT_TRUE(WaitForLoadStop(second_popup_window));

  // The iframe should not have a proxy in the main frame's SiteInstanceGroup.
  EXPECT_EQ(
      " Site C ------------ proxies for A B D\n"
      "   +--Site D ------- proxies for C\n"
      "Where A = https://a.test/\n"
      "      B = https://b.test/\n"
      "      C = https://c.test/\n"
      "      D = https://a.test/",
      DepictFrameTree(final_second_popup_rfh->frame_tree_node()));
}

// This test verifies that proxies are as expected after a navigation. Start on
// a page in a related SiteInstance before navigating to a COOP:
// restrict-properties page.
IN_PROC_BROWSER_TEST_P(CoopRestrictPropertiesProxiesBrowserTest,
                       RelatedSiteInstanceNavigationProxies) {
  GURL regular_page(https_server()->GetURL("a.test", "/title1.html"));
  GURL regular_page_2(https_server()->GetURL("b.test", "/title1.html"));
  GURL regular_page_3(https_server()->GetURL("c.test", "/title1.html"));
  GURL coop_rp_page(https_server()->GetURL(
      "d.test",
      "/set-header"
      "?cross-origin-opener-policy: restrict-properties"));

  // Start from a regular page and open a regular cross-origin popup.
  ASSERT_TRUE(NavigateToURL(shell(), regular_page));
  WebContentsImpl* first_popup_window = OpenPopupAndWaitForInitialRFHDeletion(
      current_frame_host(), regular_page_2, "");
  RenderFrameHostImpl* first_popup_rfh =
      first_popup_window->GetPrimaryMainFrame();
  ASSERT_TRUE(current_frame_host()->GetSiteInstance()->IsRelatedSiteInstance(
      first_popup_rfh->GetSiteInstance()));

  // Verify that the opener and openee frames exist as proxies in each other's
  // SiteInstanceGroup.
  EXPECT_EQ(
      " Site A ------------ proxies for B\n"
      "Where A = https://a.test/\n"
      "      B = https://b.test/",
      DepictFrameTree(current_frame_host()->frame_tree_node()));
  EXPECT_EQ(
      " Site B ------------ proxies for A\n"
      "Where A = https://a.test/\n"
      "      B = https://b.test/",
      DepictFrameTree(first_popup_rfh->frame_tree_node()));

  // Now open from the first popup a second popup with a third origin. It should
  // use a new related SiteInstance.
  WebContentsImpl* second_popup_window = OpenPopupAndWaitForInitialRFHDeletion(
      first_popup_rfh, regular_page_3, "");
  RenderFrameHostImpl* second_popup_rfh =
      second_popup_window->GetPrimaryMainFrame();
  ASSERT_TRUE(first_popup_rfh->GetSiteInstance()->IsRelatedSiteInstance(
      second_popup_rfh->GetSiteInstance()));

  // The main frame should have a proxy in the first popup's and the second
  // popup's SiteInstanceGroups.
  EXPECT_EQ(
      " Site A ------------ proxies for B C\n"
      "Where A = https://a.test/\n"
      "      B = https://b.test/\n"
      "      C = https://c.test/",
      DepictFrameTree(current_frame_host()->frame_tree_node()));
  // The first popup should have a proxy in the main frame's and second popup's
  // SiteInstanceGroups.
  EXPECT_EQ(
      " Site B ------------ proxies for A C\n"
      "Where A = https://a.test/\n"
      "      B = https://b.test/\n"
      "      C = https://c.test/",
      DepictFrameTree(first_popup_rfh->frame_tree_node()));
  // The second popup should have a proxy in the first popup's
  // SiteInstanceGroup. It does not exist as a proxy in the main frame's
  // SiteInstanceGroup, because the main frame does not have a way to reference
  // it.
  EXPECT_EQ(
      " Site C ------------ proxies for B\n"
      "Where B = https://b.test/\n"
      "      C = https://c.test/",
      DepictFrameTree(second_popup_rfh->frame_tree_node()));

  // Finally, navigate the second popup to a COOP: restrict-properties page.
  RenderFrameDeletedObserver initial_popup_rfh_observer(second_popup_rfh);
  ASSERT_TRUE(NavigateToURL(second_popup_window, coop_rp_page));
  RenderFrameHostImpl* final_second_popup_rfh =
      second_popup_window->GetPrimaryMainFrame();
  initial_popup_rfh_observer.WaitUntilDeleted();

  // The main frame should have a proxy in the first popup's SiteInstanceGroup,
  EXPECT_EQ(
      " Site A ------------ proxies for B\n"
      "Where A = https://a.test/\n"
      "      B = https://b.test/",
      DepictFrameTree(current_frame_host()->frame_tree_node()));
  // The first popup should have a proxy in the main frame's and second popup's
  // SiteInstanceGroups.
  EXPECT_EQ(
      " Site B ------------ proxies for A D\n"
      "Where A = https://a.test/\n"
      "      B = https://b.test/\n"
      "      D = https://d.test/",
      DepictFrameTree(first_popup_rfh->frame_tree_node()));
  // The second popup should have a proxy in the first popup's
  // SiteInstanceGroup. The main frame's SiteInstanceGroup still does not have a
  // proxy of the second popup's frame, as opposed to the case where they
  // initially share the same SiteInstance.
  EXPECT_EQ(
      " Site D ------------ proxies for B\n"
      "Where B = https://b.test/\n"
      "      D = https://d.test/",
      DepictFrameTree(final_second_popup_rfh->frame_tree_node()));
}

// This test verifies that proxies are as expected after a navigation. Start on
// a page in an unrelated SiteInstance before navigating to a COOP:
// restrict-properties page.
IN_PROC_BROWSER_TEST_P(CoopRestrictPropertiesProxiesBrowserTest,
                       UnrelatedSiteInstanceNavigationProxies) {
  GURL regular_page(https_server()->GetURL("a.test", "/title1.html"));
  GURL regular_page_2(https_server()->GetURL("b.test", "/title1.html"));
  GURL coop_so_page(
      https_server()->GetURL("c.test",
                             "/set-header"
                             "?cross-origin-opener-policy: same-origin"));
  GURL coop_rp_page(https_server()->GetURL(
      "d.test",
      "/set-header"
      "?cross-origin-opener-policy: restrict-properties"));

  // Start from a regular page and open a regular cross-origin popup.
  ASSERT_TRUE(NavigateToURL(shell(), regular_page));
  WebContentsImpl* first_popup_window = OpenPopupAndWaitForInitialRFHDeletion(
      current_frame_host(), regular_page_2, "");
  RenderFrameHostImpl* first_popup_rfh =
      first_popup_window->GetPrimaryMainFrame();
  ASSERT_TRUE(current_frame_host()->GetSiteInstance()->IsRelatedSiteInstance(
      first_popup_rfh->GetSiteInstance()));

  // Verify that the opener and openee frames exist as proxies in each other's
  // SiteInstanceGroup.
  EXPECT_EQ(
      " Site A ------------ proxies for B\n"
      "Where A = https://a.test/\n"
      "      B = https://b.test/",
      DepictFrameTree(current_frame_host()->frame_tree_node()));
  EXPECT_EQ(
      " Site B ------------ proxies for A\n"
      "Where A = https://a.test/\n"
      "      B = https://b.test/",
      DepictFrameTree(first_popup_rfh->frame_tree_node()));

  // Open a second popup to a COOP: same-origin page. This should trigger a swap
  // to a BrowsingInstance in a different CoopRelatedGroup.
  WebContentsImpl* second_popup_window =
      OpenPopupAndWaitForInitialRFHDeletion(first_popup_rfh, coop_so_page, "");
  RenderFrameHostImpl* second_popup_rfh =
      second_popup_window->GetPrimaryMainFrame();
  ASSERT_FALSE(first_popup_rfh->GetSiteInstance()->IsRelatedSiteInstance(
      second_popup_rfh->GetSiteInstance()));

  // The main frame should have a proxy in the first popup's SiteInstanceGroup.
  EXPECT_EQ(
      " Site A ------------ proxies for B\n"
      "Where A = https://a.test/\n"
      "      B = https://b.test/",
      DepictFrameTree(current_frame_host()->frame_tree_node()));
  // The first popup should have a proxy in the main frame's SiteInstanceGroup.
  EXPECT_EQ(
      " Site B ------------ proxies for A\n"
      "Where A = https://a.test/\n"
      "      B = https://b.test/",
      DepictFrameTree(first_popup_rfh->frame_tree_node()));
  // The second popup should have no proxies.
  EXPECT_EQ(
      " Site C\n"
      "Where C = https://c.test/",
      DepictFrameTree(second_popup_rfh->frame_tree_node()));

  // Finally, navigate the second popup to a COOP: restrict-properties page.
  RenderFrameDeletedObserver initial_popup_rfh_observer(second_popup_rfh);
  ASSERT_TRUE(NavigateToURL(second_popup_window, coop_rp_page));
  RenderFrameHostImpl* final_second_popup_rfh =
      second_popup_window->GetPrimaryMainFrame();

  // BackForwardCache will kick in and store the RenderFrameHost, preventing its
  // deletion.
  if (!IsBackForwardCacheEnabled()) {
    initial_popup_rfh_observer.WaitUntilDeleted();
  }

  // No new proxy should have been created.
  EXPECT_EQ(
      " Site A ------------ proxies for B\n"
      "Where A = https://a.test/\n"
      "      B = https://b.test/",
      DepictFrameTree(current_frame_host()->frame_tree_node()));
  EXPECT_EQ(
      " Site B ------------ proxies for A\n"
      "Where A = https://a.test/\n"
      "      B = https://b.test/",
      DepictFrameTree(first_popup_rfh->frame_tree_node()));
  EXPECT_EQ(
      " Site D\n"
      "Where D = https://d.test/",
      DepictFrameTree(final_second_popup_rfh->frame_tree_node()));
}

// This test verifies that an opener update does not create extra proxies in
// SiteInstanceGroups in other BrowsingInstances.
IN_PROC_BROWSER_TEST_P(CoopRestrictPropertiesProxiesBrowserTest,
                       NoExtraProxyDiscoveredByOpenerUpdate) {
  GURL regular_page(https_server()->GetURL("a.test", "/title1.html"));
  GURL coop_rp_page(https_server()->GetURL(
      "b.test",
      "/set-header"
      "?cross-origin-opener-policy: restrict-properties"));

  // Set up a main page with two same-origin popups.
  ASSERT_TRUE(NavigateToURL(shell(), regular_page));
  WebContentsImpl* first_popup_window = static_cast<WebContentsImpl*>(
      OpenPopup(current_frame_host(), regular_page, "")->web_contents());
  RenderFrameHostImpl* first_popup_rfh =
      first_popup_window->GetPrimaryMainFrame();
  ASSERT_EQ(current_frame_host()->GetSiteInstance(),
            first_popup_rfh->GetSiteInstance());

  WebContentsImpl* second_popup_window = static_cast<WebContentsImpl*>(
      OpenPopup(current_frame_host(), regular_page, "second_popup_name")
          ->web_contents());
  RenderFrameHostImpl* second_popup_rfh =
      second_popup_window->GetPrimaryMainFrame();
  ASSERT_EQ(current_frame_host()->GetSiteInstance(),
            second_popup_rfh->GetSiteInstance());

  // From the second popup, open a final popup to a COOP: restrict-properties
  // page.
  WebContentsImpl* third_popup_window =
      OpenPopupAndWaitForInitialRFHDeletion(second_popup_rfh, coop_rp_page, "");
  RenderFrameHostImpl* third_popup_rfh =
      third_popup_window->GetPrimaryMainFrame();
  ASSERT_FALSE(second_popup_rfh->GetSiteInstance()->IsRelatedSiteInstance(
      third_popup_rfh->GetSiteInstance()));
  ASSERT_TRUE(second_popup_rfh->GetSiteInstance()->IsCoopRelatedSiteInstance(
      third_popup_rfh->GetSiteInstance()));

  // The main page should not be visible by the third popup's SiteInstanceGroup.
  EXPECT_EQ(
      " Site A\n"
      "Where A = https://a.test/",
      DepictFrameTree(current_frame_host()->frame_tree_node()));
  // Neither should the first popup's SiteInstanceGroup.
  EXPECT_EQ(
      " Site A\n"
      "Where A = https://a.test/",
      DepictFrameTree(first_popup_rfh->frame_tree_node()));
  // On the other hand, the third popup's SiteInstanceGroup should know about
  // the second popup.
  EXPECT_EQ(
      " Site A ------------ proxies for B\n"
      "Where A = https://a.test/\n"
      "      B = https://b.test/",
      DepictFrameTree(second_popup_rfh->frame_tree_node()));

  // To begin with, window.opener.opener should return null in the second popup,
  // because its opener is the main page which itself does not have an opener.
  ASSERT_EQ(true, EvalJs(second_popup_rfh, "window.opener.opener == null;"));

  // Now update the opener of the second popup using named targeting. The second
  // popup's opener is now the first popup.
  ASSERT_TRUE(ExecJs(first_popup_rfh,
                     "window.w = window.open('', 'second_popup_name');"));

  // Verify the opener was properly updated in the second popup.
  ASSERT_EQ(true,
            EvalJs(second_popup_rfh, "window.opener.opener.opener == null;"));

  // The COOP: restrict-properties SiteInstanceGroup in the third popup should
  // still be unaware of the main page and the first popup.
  EXPECT_EQ(
      " Site A\n"
      "Where A = https://a.test/",
      DepictFrameTree(current_frame_host()->frame_tree_node()));
  EXPECT_EQ(
      " Site A\n"
      "Where A = https://a.test/",
      DepictFrameTree(first_popup_rfh->frame_tree_node()));
  EXPECT_EQ(
      " Site A ------------ proxies for B\n"
      "Where A = https://a.test/\n"
      "      B = https://b.test/",
      DepictFrameTree(second_popup_rfh->frame_tree_node()));
}

// This test verifies that named targeting does not resolve across
// BrowsingInstances.
// TODO(crbug.com/40276662): Named targeting might evolve in the future,
// when we're able to have per-BrowsingInstance names. For now, we're simply
// blocking all named targeting.
IN_PROC_BROWSER_TEST_P(CoopRestrictPropertiesBrowserTest,
                       NamedTargetingIsBlockedAcrossBrowsingInstances) {
  GURL regular_page(https_server()->GetURL("a.test", "/title1.html"));
  GURL coop_rp_page(https_server()->GetURL(
      "a.test",
      "/set-header"
      "?cross-origin-opener-policy: restrict-properties"));
  GURL regular_page_2(https_server()->GetURL("b.test", "/title1.html"));

  // 1. Verify that the set name gets cleared when opening a popup in a
  // different BrowsingInstance.
  ASSERT_TRUE(NavigateToURL(shell(), regular_page));
  WebContentsImpl* popup_window = static_cast<WebContentsImpl*>(
      OpenPopup(current_frame_host(), coop_rp_page, "name1")->web_contents());
  RenderFrameHostImpl* popup_rfh = popup_window->GetPrimaryMainFrame();
  ASSERT_FALSE(current_frame_host()->GetSiteInstance()->IsRelatedSiteInstance(
      popup_rfh->GetSiteInstance()));
  ASSERT_TRUE(
      current_frame_host()->GetSiteInstance()->IsCoopRelatedSiteInstance(
          popup_rfh->GetSiteInstance()));

  EXPECT_EQ("", popup_rfh->GetFrameName());
  EXPECT_EQ(true, EvalJs(popup_rfh, "window.name == '';"));

  // 2. Verify that setting a new name to the frame still doesn't make the popup
  // targetable.
  FrameNameChangedWaiter frame_name_changed(popup_window, popup_rfh, "name2");
  ASSERT_TRUE(ExecJs(popup_rfh, "window.name = 'name2';"));

  // Note: This waits for the name update to reach the browser, which will send
  // replication state updates to the renderers processes keeping proxies of
  // this frame. Because the interfaces are associated, we expect the proxy
  // update to happen before the script execution below.
  frame_name_changed.Wait();

  ShellAddedObserver main_page_targeting_observer;
  ASSERT_TRUE(ExecJs(current_frame_host(), "window.open('', 'name2')"));
  main_page_targeting_observer.GetShell();

  // We should have 3 different windows: the main page, the first popup and the
  // second popup that was just opened because named targeting did not resolve.
  EXPECT_EQ(3u, Shell::windows().size());

  // 3. Verify that a named subframe is similarly not targetable by the opening
  // context.
  ASSERT_TRUE(ExecJs(popup_rfh, JsReplace(R"(
    const frame = document.createElement('iframe');
    frame.name = 'name3';
    frame.src = $1;
    document.body.appendChild(frame);
  )",
                                          regular_page_2)));
  ASSERT_TRUE(WaitForLoadStop(popup_window));

  // The iframe should not even have a proxy in the main page's process, and no
  // matching frame should be returned. A new popup is created instead.
  ShellAddedObserver iframe_targeting_observer;
  ASSERT_TRUE(ExecJs(current_frame_host(), "window.open('', 'name3')"));
  iframe_targeting_observer.GetShell();

  // We should have all 3 preceding windows, and another one that was opened
  // because the subframe targeting did not resolve.
  EXPECT_EQ(4u, Shell::windows().size());
}

// Smoke test with kNewBrowsingContextStateOnBrowsingContextGroupSwap enabled.
// Verifies that nothing breaks when we're dealing with proxies across different
// BrowsingInstances with COOP: restrict-properties.
// TODO(crbug.com/40061970): Enable once BrowsingContextState new mode
// implementation is further down the line. Currently this test crashes even
// with COOP: same-origin.
IN_PROC_BROWSER_TEST_P(
    CoopRestrictPropertiesWithNewBrowsingContextStateModeBrowserTest,
    DISABLED_BrowsingContextStateNewModeSmokeTest) {
  GURL regular_page(https_server()->GetURL("a.test", "/title1.html"));
  GURL coop_rp_page(https_server()->GetURL(
      "a.test",
      "/set-header"
      "?cross-origin-opener-policy: restrict-properties"));

  // Start by opening a popup to a COOP: rp page from a regular page.
  // Note: This currently causes a crash in the renderer.
  ASSERT_TRUE(NavigateToURL(shell(), regular_page));
  WebContentsImpl* popup_window = static_cast<WebContentsImpl*>(
      OpenPopup(current_frame_host(), coop_rp_page, "")->web_contents());
  RenderFrameHostImpl* popup_rfh = popup_window->GetPrimaryMainFrame();
  ASSERT_FALSE(current_frame_host()->GetSiteInstance()->IsRelatedSiteInstance(
      popup_rfh->GetSiteInstance()));

  popup_window->Close();
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
IN_PROC_BROWSER_TEST_P(CoopRestrictPropertiesReportingBrowserTest,
                       NavigationVirtualBrowsingContextGroup) {
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
          https_server()->GetURL(
              "a.test",
              "/set-header?"
              "Cross-Origin-Opener-Policy: restrict-properties&"
              "Cross-Origin-Embedder-Policy: require-corp"),
          true,
      },
      {
          // different-origin => change.
          https_server()->GetURL("a.a.test", "/title1.html"),
          https_server()->GetURL(
              "b.a.test",
              "/set-header?"
              "Cross-Origin-Opener-Policy: restrict-properties&"
              "Cross-Origin-Embedder-Policy: require-corp"),
          true,
      },
      {
          // different-site => change.
          https_server()->GetURL("a.test", "/title1.html"),
          https_server()->GetURL(
              "b.test",
              "/set-header?"
              "Cross-Origin-Opener-Policy: restrict-properties&"
              "Cross-Origin-Embedder-Policy: require-corp"),
          true,
      },

      // coop <-> coop.
      {
          // same-origin => keep.
          https_server()->GetURL(
              "a.test",
              "/set-header?"
              "Cross-Origin-Opener-Policy: restrict-properties&"
              "Cross-Origin-Embedder-Policy: require-corp"),
          https_server()->GetURL(
              "a.test",
              "/set-header?"
              "Cross-Origin-Opener-Policy: restrict-properties&"
              "Cross-Origin-Embedder-Policy: require-corp"),
          false,
      },
      {
          // different-origin => change.
          https_server()->GetURL(
              "a.a.test",
              "/set-header?"
              "Cross-Origin-Opener-Policy: restrict-properties&"
              "Cross-Origin-Embedder-Policy: require-corp"),
          https_server()->GetURL(
              "b.a.test",
              "/set-header?"
              "Cross-Origin-Opener-Policy: restrict-properties&"
              "Cross-Origin-Embedder-Policy: require-corp"),
          true,
      },
      {
          // different-site => change.
          https_server()->GetURL(
              "a.test",
              "/set-header?"
              "Cross-Origin-Opener-Policy: restrict-properties&"
              "Cross-Origin-Embedder-Policy: require-corp"),
          https_server()->GetURL(
              "b.test",
              "/set-header?"
              "Cross-Origin-Opener-Policy: restrict-properties&"
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
              "Cross-Origin-Opener-Policy-Report-Only: restrict-properties&"
              "Cross-Origin-Embedder-Policy: require-corp"),
          true,
      },
      {
          // different-origin => change.
          https_server()->GetURL("a.a.test", "/title1.html"),
          https_server()->GetURL(
              "b.a.test",
              "/set-header?"
              "Cross-Origin-Opener-Policy-Report-Only: restrict-properties&"
              "Cross-Origin-Embedder-Policy: require-corp"),
          true,
      },
      {
          // different-site => change.
          https_server()->GetURL("a.test", "/title1.html"),
          https_server()->GetURL(
              "b.test",
              "/set-header?"
              "Cross-Origin-Opener-Policy-Report-Only: restrict-properties&"
              "Cross-Origin-Embedder-Policy: require-corp"),
          true,
      },

      // coop-ro <-> coop-ro.
      {
          // same-origin => keep.
          https_server()->GetURL(
              "a.test",
              "/set-header?"
              "Cross-Origin-Opener-Policy-Report-Only: restrict-properties&"
              "Cross-Origin-Embedder-Policy: require-corp"),
          https_server()->GetURL(
              "a.test",
              "/set-header?"
              "Cross-Origin-Opener-Policy-Report-Only: restrict-properties&"
              "Cross-Origin-Embedder-Policy: require-corp"),
          false,
      },
      {
          // different-origin => change.
          https_server()->GetURL(
              "a.a.test",
              "/set-header?"
              "Cross-Origin-Opener-Policy-Report-Only: restrict-properties&"
              "Cross-Origin-Embedder-Policy: require-corp"),
          https_server()->GetURL(
              "b.a.test",
              "/set-header?"
              "Cross-Origin-Opener-Policy-Report-Only: restrict-properties&"
              "Cross-Origin-Embedder-Policy: require-corp"),
          true,
      },
      {
          // different-site => keep.
          https_server()->GetURL(
              "a.test",
              "/set-header?"
              "Cross-Origin-Opener-Policy-Report-Only: restrict-properties&"
              "Cross-Origin-Embedder-Policy: require-corp"),
          https_server()->GetURL(
              "b.test",
              "/set-header?"
              "Cross-Origin-Opener-Policy-Report-Only: restrict-properties&"
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
              "Cross-Origin-Opener-Policy-Report-Only: restrict-properties&"
              "Cross-Origin-Embedder-Policy: require-corp"),
          true,
      },
      {
          // different-origin => change.
          https_server()->GetURL(
              "a.a.test",
              "/set-header?"
              "Cross-Origin-Opener-Policy: restrict-properties&"
              "Cross-Origin-Embedder-Policy: require-corp"),
          https_server()->GetURL(
              "b.a.test",
              "/set-header?"
              "Cross-Origin-Opener-Policy-Report-Only: restrict-properties&"
              "Cross-Origin-Embedder-Policy: require-corp"),
          true,
      },
      {
          // different-site => change
          https_server()->GetURL(
              "a.test",
              "/set-header?"
              "Cross-Origin-Opener-Policy: restrict-properties&"
              "Cross-Origin-Embedder-Policy: require-corp"),
          https_server()->GetURL(
              "b.test",
              "/set-header?"
              "Cross-Origin-Opener-Policy-Report-Only: restrict-properties&"
              "Cross-Origin-Embedder-Policy: require-corp"),
          true,
      },
      // TODO(crbug.com/40260406): Test with COEP-RO.
      // TODO(crbug.com/40260406): Test interactions with COOP: SO.
      // TODO(crbug.com/40260406): Test interactions with COOP: SOAP.
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

// Use window.open(url). Check the virtual browsing context group of the two
// window.
IN_PROC_BROWSER_TEST_P(CoopRestrictPropertiesReportingBrowserTest,
                       WindowOpenVirtualBrowsingContextGroup) {
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
              "Cross-Origin-Opener-Policy-Report-Only: restrict-properties&"
              "Cross-Origin-Embedder-Policy: require-corp"),
          GURL(),
          false,
      },
      {
          // From coop.
          https_server()->GetURL(
              "a.test",
              "/set-header?"
              "Cross-Origin-Opener-Policy: restrict-properties&"
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
              "Cross-Origin-Opener-Policy-Report-Only: restrict-properties&"
              "Cross-Origin-Embedder-Policy: require-corp"),
          true,
      },

      // non-coop opens coop.
      {
          https_server()->GetURL("a.test", "/title1.html"),
          https_server()->GetURL(
              "a.test",
              "/set-header?"
              "Cross-Origin-Opener-Policy: restrict-properties&"
              "Cross-Origin-Embedder-Policy: require-corp"),
          true,
      },

      // coop opens non-coop.
      {
          https_server()->GetURL(
              "a.test",
              "/set-header?"
              "Cross-Origin-Opener-Policy: restrict-properties&"
              "Cross-Origin-Embedder-Policy: require-corp"),
          https_server()->GetURL("a.test", "/title1.html"),
          true,
      },

      // coop-ro opens coop-ro (same-origin).
      {
          https_server()->GetURL(
              "a.test",
              "/set-header?"
              "Cross-Origin-Opener-Policy-Report-Only: restrict-properties&"
              "Cross-Origin-Embedder-Policy: require-corp"),
          https_server()->GetURL(
              "a.test",
              "/set-header?"
              "Cross-Origin-Opener-Policy-Report-Only: restrict-properties&"
              "Cross-Origin-Embedder-Policy: require-corp"),
          false,
      },

      // coop-ro opens coop-ro (different-origin).
      {
          https_server()->GetURL(
              "a.test",
              "/set-header?"
              "Cross-Origin-Opener-Policy-Report-Only: restrict-properties&"
              "Cross-Origin-Embedder-Policy: require-corp"),
          https_server()->GetURL(
              "b.test",
              "/set-header?"
              "Cross-Origin-Opener-Policy-Report-Only: restrict-properties&"
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

// Verify that two documents in different browsing context groups in the same
// CoopRelatedGroup only have access to window.closed and window.postMessage().
IN_PROC_BROWSER_TEST_P(CoopRestrictPropertiesAccessBrowserTest,
                       PropertiesAreBlockedAcrossBrowsingContextGroup) {
  GURL regular_page(https_server()->GetURL("a.test", "/title1.html"));
  GURL coop_rp_page(https_server()->GetURL(
      "b.test",
      "/set-header"
      "?cross-origin-opener-policy: restrict-properties"));
  GURL same_origin_iframe(https_server()->GetURL("a.test", "/title1.html"));

  // Start from a regular page and open a cross-origin popup. Open it manually
  // to store the returned popup handle.
  ASSERT_TRUE(NavigateToURL(shell(), regular_page));
  ShellAddedObserver shell_observer;
  EXPECT_TRUE(ExecJs(current_frame_host(),
                     JsReplace("window.w = window.open($1)", coop_rp_page)));
  WebContentsImpl* popup_window =
      static_cast<WebContentsImpl*>(shell_observer.GetShell()->web_contents());
  ASSERT_TRUE(WaitForLoadStop(popup_window));
  RenderFrameHostImpl* popup_rfh = popup_window->GetPrimaryMainFrame();
  ASSERT_FALSE(current_frame_host()->GetSiteInstance()->IsRelatedSiteInstance(
      popup_rfh->GetSiteInstance()));
  ASSERT_TRUE(
      current_frame_host()->GetSiteInstance()->IsCoopRelatedSiteInstance(
          popup_rfh->GetSiteInstance()));

  // Try to access always-authorized properties. They should return as usual.
  EXPECT_EQ(false, EvalJs(current_frame_host(), "window.w.closed"));
  EXPECT_EQ(nullptr,
            EvalJs(current_frame_host(), "window.w.postMessage('', '*')"));

  // Then poke at restricted properties and verify that we return a COOP:
  // restrict-properties SecurityError.

  // window.window
  std::string result =
      EvalJs(current_frame_host(),
             "try { window.w.window } catch (e) { e.toString(); }")
          .ExtractString();
  EXPECT_THAT(result, ::testing::MatchesRegex(kCoopRpErrorMessageRegex));

  // window.self
  result = EvalJs(current_frame_host(),
                  "try { window.w.self } catch (e) { e.toString(); }")
               .ExtractString();
  EXPECT_THAT(result, ::testing::MatchesRegex(kCoopRpErrorMessageRegex));

  // window.location
  result = EvalJs(current_frame_host(),
                  "try { window.w.location } catch (e) { e.toString(); }")
               .ExtractString();
  EXPECT_THAT(result, ::testing::MatchesRegex(kCoopRpErrorMessageRegex));

  // window.focus()
  result = EvalJs(current_frame_host(),
                  "try { window.w.focus() } catch (e) { e.toString(); }")
               .ExtractString();
  EXPECT_THAT(result, ::testing::MatchesRegex(kCoopRpErrorMessageRegex));

  // window.blur()
  result = EvalJs(current_frame_host(),
                  "try { window.w.blur() } catch (e) { e.toString(); }")
               .ExtractString();
  EXPECT_THAT(result, ::testing::MatchesRegex(kCoopRpErrorMessageRegex));

  // window.frames
  result = EvalJs(current_frame_host(),
                  "try { window.w.frames } catch (e) { e.toString(); }")
               .ExtractString();
  EXPECT_THAT(result, ::testing::MatchesRegex(kCoopRpErrorMessageRegex));

  // window.length
  result = EvalJs(current_frame_host(),
                  "try { window.w.length } catch (e) { e.toString(); }")
               .ExtractString();
  EXPECT_THAT(result, ::testing::MatchesRegex(kCoopRpErrorMessageRegex));

  // window.top
  result = EvalJs(current_frame_host(),
                  "try { window.w.top } catch (e) { e.toString(); }")
               .ExtractString();
  EXPECT_THAT(result, ::testing::MatchesRegex(kCoopRpErrorMessageRegex));

  // window.opener
  result = EvalJs(current_frame_host(),
                  "try { window.w.opener } catch (e) { e.toString(); }")
               .ExtractString();
  EXPECT_THAT(result, ::testing::MatchesRegex(kCoopRpErrorMessageRegex));

  // window.parent
  result = EvalJs(current_frame_host(),
                  "try { window.w.parent } catch (e) { e.toString(); }")
               .ExtractString();
  EXPECT_THAT(result, ::testing::MatchesRegex(kCoopRpErrorMessageRegex));

  // window indexed getter
  result = EvalJs(current_frame_host(),
                  "try { window.w[0] } catch (e) { e.toString(); }")
               .ExtractString();
  EXPECT_THAT(result, ::testing::MatchesRegex(kCoopRpErrorMessageRegex));

  // window named getter
  result = EvalJs(current_frame_host(),
                  "try { window.w['iframe_name'] } catch (e) { e.toString(); }")
               .ExtractString();
  EXPECT_THAT(result, ::testing::MatchesRegex(kCoopRpErrorMessageRegex));

  // Verify that getting window["then"] uses the special cross-origin fallback.
  // See https://html.spec.whatwg.org/#crossoriginpropertyfallback-(-p-)
  // This makes sure windowProxy is thenable, see the original discussion here:
  // https://github.com/whatwg/dom/issues/536.
  EXPECT_TRUE(ExecJs(current_frame_host(), "window.w['then']"));

  // window.close()
  result = EvalJs(current_frame_host(),
                  "try { window.w.close() } catch (e) { e.toString(); }")
               .ExtractString();
  EXPECT_THAT(result, ::testing::MatchesRegex(kCoopRpErrorMessageRegex));
}

// Verifies that the BrowsingContextGroupInfo is properly propagated when
// opening a popup in the same SiteInstance.
IN_PROC_BROWSER_TEST_P(CoopRestrictPropertiesAccessBrowserTest,
                       SimpleLocalPopup) {
  GURL regular_page(https_server()->GetURL("a.test", "/title1.html"));

  // Start from a regular page and open a popup in the same SiteInstance.
  ASSERT_TRUE(NavigateToURL(shell(), regular_page));
  ShellAddedObserver shell_observer;
  ASSERT_TRUE(ExecJs(current_frame_host(),
                     JsReplace("window.w = open($1);", regular_page)));
  WebContentsImpl* popup_window =
      static_cast<WebContentsImpl*>(shell_observer.GetShell()->web_contents());
  ASSERT_TRUE(WaitForLoadStop(popup_window));
  RenderFrameHostImpl* popup_rfh = popup_window->GetPrimaryMainFrame();
  ASSERT_EQ(current_frame_host()->GetSiteInstance(),
            popup_rfh->GetSiteInstance());

  // Because they are in the same SiteInstance, their browsing context group
  // should match and access should be possible.
  EXPECT_TRUE(ExecJs(current_frame_host(), "window.w.blur()"));
  EXPECT_TRUE(ExecJs(popup_rfh, "opener.blur()"));
}

// Verifies that the BrowsingContextGroupInfo is properly propagated when
// opening a popup in the same browsing context group in another SiteInstance.
IN_PROC_BROWSER_TEST_P(CoopRestrictPropertiesAccessBrowserTest,
                       SimpleRemotePopup) {
  GURL regular_page(https_server()->GetURL("a.test", "/title1.html"));
  GURL regular_page_2(https_server()->GetURL("b.test", "/title1.html"));

  // Start from a regular page and open a popup in the same browsing context
  // group.
  ASSERT_TRUE(NavigateToURL(shell(), regular_page));
  ShellAddedObserver shell_observer;
  ASSERT_TRUE(ExecJs(current_frame_host(),
                     JsReplace("window.w = open($1);", regular_page_2)));
  WebContentsImpl* popup_window =
      static_cast<WebContentsImpl*>(shell_observer.GetShell()->web_contents());

  ASSERT_TRUE(WaitForLoadStop(popup_window));
  RenderFrameHostImpl* popup_rfh = popup_window->GetPrimaryMainFrame();
  ASSERT_TRUE(current_frame_host()->GetSiteInstance()->IsRelatedSiteInstance(
      popup_rfh->GetSiteInstance()));

  // Because they are in the same browsing context group access should be
  // possible.
  EXPECT_TRUE(ExecJs(current_frame_host(), "window.w.blur()"));
  EXPECT_TRUE(ExecJs(popup_rfh, "opener.blur()"));
}

// Verifies that the BrowsingContextGroupInfo is properly propagated when
// opening a popup in another browsing context group in the same
// CoopRelatedGroup.
IN_PROC_BROWSER_TEST_P(CoopRestrictPropertiesAccessBrowserTest,
                       SimpleCoopPopup) {
  GURL regular_page(https_server()->GetURL("a.test", "/title1.html"));
  GURL coop_rp_page(https_server()->GetURL(
      "b.test",
      "/set-header"
      "?cross-origin-opener-policy: restrict-properties"));
  GURL regular_page_same_document(
      https_server()->GetURL("a.test", "/title1.html#fragment"));

  // Start from a regular page and open a popup in another browsing context
  // group in the same CoopRelatedGroup.
  ASSERT_TRUE(NavigateToURL(shell(), regular_page));
  ShellAddedObserver shell_observer;
  ASSERT_TRUE(ExecJs(current_frame_host(),
                     JsReplace("window.w = open($1);", coop_rp_page)));
  WebContentsImpl* popup_window =
      static_cast<WebContentsImpl*>(shell_observer.GetShell()->web_contents());
  ASSERT_TRUE(WaitForLoadStop(popup_window));
  RenderFrameHostImpl* popup_rfh = popup_window->GetPrimaryMainFrame();
  ASSERT_NE(current_frame_host()->GetSiteInstance(),
            popup_rfh->GetSiteInstance());
  ASSERT_FALSE(current_frame_host()->GetSiteInstance()->IsRelatedSiteInstance(
      popup_rfh->GetSiteInstance()));
  ASSERT_TRUE(
      current_frame_host()->GetSiteInstance()->IsCoopRelatedSiteInstance(
          popup_rfh->GetSiteInstance()));

  // Because they are in different browsing context groups in the same
  // CoopRelatedGroup, access to cross-origin properties should be restricted.
  std::string result =
      EvalJs(current_frame_host(),
             "try { window.w.blur() } catch (e) { e.toString(); }")
          .ExtractString();
  EXPECT_THAT(result, ::testing::MatchesRegex(kCoopRpErrorMessageRegex));

  result =
      EvalJs(popup_rfh, "try { opener.blur() } catch (e) { e.toString(); }")
          .ExtractString();
  EXPECT_THAT(result, ::testing::MatchesRegex(kCoopRpErrorMessageRegex));

  // Always-allowed properties should still be accessible, and trying to access
  // them should not throw an exception.
  EXPECT_EQ(true, EvalJs(popup_rfh, "opener.closed == false"));
  EXPECT_EQ(true, EvalJs(current_frame_host(), "window.w.closed == false"));

  // Finally, close the popup and verify that window.closed reflects the update.
  // To make sure the update is propagated, run a quick same-document navigation
  // which should rely on the same underlying interface pipe.
  popup_window->Close();
  ASSERT_TRUE(NavigateToURL(shell(), regular_page_same_document));
  EXPECT_EQ(true, EvalJs(current_frame_host(), "window.w.closed == true"));
}

// Verifies in more details how the BrowsingContextGroupInfo is propagated when
// opening a popup in another browsing context group in the same
// CoopRelatedGroup.
IN_PROC_BROWSER_TEST_P(CoopRestrictPropertiesAccessBrowserTest,
                       SimpleCoopPopupDetailed) {
  // This test verifies details about RenderViewHosts, so make sure we're using
  // different processes for different pages.
  if (!AreAllSitesIsolatedForTesting()) {
    return;
  }

  GURL regular_page(https_server()->GetURL("a.test", "/title1.html"));
  GURL coop_rp_page(https_server()->GetURL(
      "a.test",
      "/set-header"
      "?cross-origin-opener-policy: restrict-properties"));

  // Start from a regular page and open a popup in another browsing context
  // group in the same CoopRelatedGroup.
  ASSERT_TRUE(NavigateToURL(shell(), regular_page));
  SiteInstanceImpl* main_page_si = current_frame_host()->GetSiteInstance();
  base::UnguessableToken main_page_bi_token =
      main_page_si->browsing_instance_token();
  base::UnguessableToken main_page_coop_token =
      main_page_si->coop_related_group_token();

  // Then open a popup in the same SiteInstance. The popup starts with the same
  // tokens as the main page since it belong to the same SiteInstance.
  ShellAddedObserver shell_observer;
  ASSERT_TRUE(ExecJs(current_frame_host(), "window.w = open('');"));
  WebContentsImpl* popup_window =
      static_cast<WebContentsImpl*>(shell_observer.GetShell()->web_contents());
  ASSERT_TRUE(WaitForLoadStop(popup_window));
  RenderFrameHostImpl* popup_rfh = popup_window->GetPrimaryMainFrame();
  RenderFrameHostManager* popup_rfhm =
      popup_rfh->frame_tree_node()->render_manager();
  ASSERT_EQ(current_frame_host()->GetSiteInstance(),
            popup_rfh->GetSiteInstance());

  // At this stage, two RenderViewHosts exist, in the same process, one for each
  // page. In both, the frames are local.
  RenderViewHostImpl* rvh1 = static_cast<RenderViewHostImpl*>(
      current_frame_host()->GetRenderViewHost());
  RenderViewHostImpl* rvh2 =
      static_cast<RenderViewHostImpl*>(popup_rfh->GetRenderViewHost());
  ASSERT_NE(rvh1, rvh2);
  ASSERT_NE(rvh1->frame_tree(), rvh2->frame_tree());
  ASSERT_EQ(rvh1->site_instance_group(),
            rvh1->frame_tree()->GetMainFrame()->GetSiteInstance()->group());
  ASSERT_EQ(rvh2->site_instance_group(),
            rvh2->frame_tree()->GetMainFrame()->GetSiteInstance()->group());

  // Now, start a navigation to a COOP: restrict-properties page, in another
  // browsing context group in the same CoopRelatedGroup.
  TestNavigationManager navigation_manager(popup_window, coop_rp_page);
  NavigationController::LoadURLParams params(coop_rp_page);
  popup_window->GetController().LoadURLWithParams(params);

  // Stop when we've started the request. At this stage, we should have no
  // speculative frame, because we still think we can reuse the same
  // RenderFrameHost.
  ASSERT_TRUE(navigation_manager.WaitForRequestStart());
  ASSERT_FALSE(popup_rfhm->speculative_frame_host());

  // After receiving the response, we realize that COOP headers do not match. We
  // should have created a new RenderFrameHost in another browsing context
  // group.
  ASSERT_TRUE(navigation_manager.WaitForResponse());
  RenderFrameHostImpl* new_rfh = popup_rfhm->speculative_frame_host();
  ASSERT_TRUE(new_rfh);
  ASSERT_FALSE(new_rfh->GetSiteInstance()->IsRelatedSiteInstance(
      popup_rfh->GetSiteInstance()));
  ASSERT_TRUE(new_rfh->GetSiteInstance()->IsCoopRelatedSiteInstance(
      popup_rfh->GetSiteInstance()));

  base::UnguessableToken popup_bi_token =
      new_rfh->GetSiteInstance()->browsing_instance_token();
  base::UnguessableToken popup_coop_token =
      new_rfh->GetSiteInstance()->coop_related_group_token();
  EXPECT_NE(main_page_bi_token, popup_bi_token);
  EXPECT_EQ(main_page_coop_token, popup_coop_token);

  // At this point, we should have 4 RenderViewHosts, one for each page in each
  // process. Grab the ones created for the new process.
  RenderFrameProxyHost* proxy_for_main_page_in_popup =
      current_frame_host()
          ->browsing_context_state()
          ->proxy_hosts()[new_rfh->GetSiteInstance()->group()->GetId()]
          .get();
  RenderViewHostImpl* rvh3 = proxy_for_main_page_in_popup->GetRenderViewHost();
  RenderViewHostImpl* rvh4 =
      static_cast<RenderViewHostImpl*>(new_rfh->GetRenderViewHost());

  // The first RenderViewHost represents the main page, in the main page
  // process.
  ASSERT_EQ(rvh1->frame_tree(), &(web_contents()->GetPrimaryFrameTree()));
  EXPECT_EQ(rvh1->site_instance_group(),
            rvh1->frame_tree()->GetMainFrame()->GetSiteInstance()->group());
  EXPECT_EQ(rvh1->site_instance_group()->browsing_instance_token(),
            main_page_bi_token);
  EXPECT_EQ(rvh1->site_instance_group()->coop_related_group_token(),
            main_page_coop_token);

  // The second RenderViewHost represents the popup, in the main page process.
  // At this stage, the new popup frame has not yet been committed, and it
  // should still be for the old popup frame.
  ASSERT_EQ(rvh2->frame_tree(), &(popup_window->GetPrimaryFrameTree()));
  EXPECT_EQ(rvh2->site_instance_group(),
            rvh2->frame_tree()->GetMainFrame()->GetSiteInstance()->group());
  EXPECT_EQ(rvh2->site_instance_group()->browsing_instance_token(),
            main_page_bi_token);
  EXPECT_EQ(rvh2->site_instance_group()->coop_related_group_token(),
            main_page_coop_token);
  EXPECT_EQ(rvh2->frame_tree()
                ->GetMainFrame()
                ->GetSiteInstance()
                ->browsing_instance_token(),
            main_page_bi_token);
  EXPECT_EQ(rvh2->frame_tree()
                ->GetMainFrame()
                ->GetSiteInstance()
                ->coop_related_group_token(),
            main_page_coop_token);

  // The third RenderViewHost represents the main page, in the popup process. It
  // should have a proxy as its main frame, with the final BrowsingContextGroup
  // information. We sent the renderer process that information at RenderView
  // creation time.
  ASSERT_EQ(rvh3->frame_tree(), &(web_contents()->GetPrimaryFrameTree()));
  EXPECT_NE(rvh3->site_instance_group(),
            rvh3->frame_tree()->GetMainFrame()->GetSiteInstance()->group());
  EXPECT_EQ(rvh3->site_instance_group()->browsing_instance_token(),
            popup_bi_token);
  EXPECT_EQ(rvh3->site_instance_group()->coop_related_group_token(),
            popup_coop_token);
  EXPECT_EQ(rvh3->frame_tree()
                ->GetMainFrame()
                ->GetSiteInstance()
                ->browsing_instance_token(),
            main_page_bi_token);
  EXPECT_EQ(rvh3->frame_tree()
                ->GetMainFrame()
                ->GetSiteInstance()
                ->coop_related_group_token(),
            main_page_coop_token);

  // The fourth RenderViewHost represents the popup, in the popup process.
  // Before commit, the main frame should be a proxy. We sent the renderer
  // process the current frame's BrowsingContextGroup information at RenderView
  // creation time.
  ASSERT_EQ(rvh4->frame_tree(), &(popup_window->GetPrimaryFrameTree()));
  EXPECT_NE(rvh4->site_instance_group(),
            rvh4->frame_tree()->GetMainFrame()->GetSiteInstance()->group());
  EXPECT_EQ(rvh4->site_instance_group()->browsing_instance_token(),
            popup_bi_token);
  EXPECT_EQ(rvh4->site_instance_group()->coop_related_group_token(),
            popup_coop_token);
  EXPECT_EQ(rvh4->frame_tree()
                ->GetMainFrame()
                ->GetSiteInstance()
                ->browsing_instance_token(),
            main_page_bi_token);
  EXPECT_EQ(rvh4->frame_tree()
                ->GetMainFrame()
                ->GetSiteInstance()
                ->coop_related_group_token(),
            main_page_coop_token);

  // Commit the navigation. The speculative RenderFrameHost is now the current
  // RenderFrameHost.
  ASSERT_TRUE(navigation_manager.WaitForNavigationFinished());
  ASSERT_EQ(new_rfh, popup_window->GetPrimaryMainFrame());

  // At commit time, two things happened:
  // (1) We sent the popup's renderer (rvh4) the new RenderFrameHost tokens as
  // part of the commit. They should be in line with the currently active frame,
  // which is now local. Note that we cannot verify the information sent to the
  // renderer, but at least make sure that the browser side holds the correct
  // information.
  EXPECT_EQ(rvh4->site_instance_group(),
            rvh4->frame_tree()->GetMainFrame()->GetSiteInstance()->group());
  EXPECT_EQ(rvh4->site_instance_group()->browsing_instance_token(),
            popup_bi_token);
  EXPECT_EQ(rvh4->site_instance_group()->coop_related_group_token(),
            popup_coop_token);

  // (2) We've broadcasted the BrowsingContextGroupInfo update to
  // RenderViewHosts that have a proxy of the navigated frame as their main
  // frame. In this case, rvh2, which now has a proxy of the popup frame as its
  // main frame.
  EXPECT_NE(rvh2->site_instance_group(),
            rvh2->frame_tree()->GetMainFrame()->GetSiteInstance()->group());
  EXPECT_EQ(rvh2->site_instance_group()->browsing_instance_token(),
            main_page_bi_token);
  EXPECT_EQ(rvh2->site_instance_group()->coop_related_group_token(),
            main_page_coop_token);
  EXPECT_EQ(rvh2->frame_tree()
                ->GetMainFrame()
                ->GetSiteInstance()
                ->browsing_instance_token(),
            popup_bi_token);
  EXPECT_EQ(rvh2->frame_tree()
                ->GetMainFrame()
                ->GetSiteInstance()
                ->coop_related_group_token(),
            popup_coop_token);

  // Finally, make sure the right properties are blocked, and the right
  // properties can be accessed.
  std::string result =
      EvalJs(current_frame_host(),
             "try { window.w.blur() } catch (e) { e.toString(); }")
          .ExtractString();
  EXPECT_THAT(result, ::testing::MatchesRegex(kCoopRpErrorMessageRegex));

  result = EvalJs(new_rfh, "try { opener.blur() } catch (e) { e.toString(); }")
               .ExtractString();
  EXPECT_THAT(result, ::testing::MatchesRegex(kCoopRpErrorMessageRegex));

  // Always-allowed properties should still be accessible, and trying to access
  // them should not throw any exception.
  EXPECT_EQ(true, EvalJs(new_rfh, "opener.closed == false"));
  EXPECT_EQ(true, EvalJs(current_frame_host(), "window.w.closed == false"));
}

// Verifies that BrowsingContextGroupInfo is properly propagated to an iframe
// in the same SiteInstance.
IN_PROC_BROWSER_TEST_P(CoopRestrictPropertiesAccessBrowserTest, LocalSubframe) {
  GURL regular_page(https_server()->GetURL("a.test", "/title1.html"));

  // Navigate to a regular page, with a subframe in the same SiteInstance.
  ASSERT_TRUE(NavigateToURL(shell(), regular_page));
  ASSERT_TRUE(ExecJs(current_frame_host(), JsReplace(R"(
    const frame = document.createElement('iframe');
    frame.src = $1;
    document.body.appendChild(frame);
  )",
                                                     regular_page)));
  ASSERT_TRUE(WaitForLoadStop(web_contents()));
  RenderFrameHostImpl* iframe_rfh =
      current_frame_host()->child_at(0)->current_frame_host();

  // The iframe is in the same SiteInstance, and access should be possible.
  EXPECT_TRUE(ExecJs(current_frame_host(), "window[0].blur()"));
  EXPECT_TRUE(ExecJs(iframe_rfh, "top.blur()"));
}

// Verifies that BrowsingContextGroupInfo is properly propagated to an iframe
// in the same browsing context group in another SiteInstance.
IN_PROC_BROWSER_TEST_P(CoopRestrictPropertiesAccessBrowserTest,
                       RemoteSubframe) {
  GURL regular_page(https_server()->GetURL("a.test", "/title1.html"));
  GURL regular_page_2(https_server()->GetURL("b.test", "/title1.html"));

  // Navigate to a regular page, with a subframe in another SiteInstance.
  ASSERT_TRUE(NavigateToURL(shell(), regular_page));
  ASSERT_TRUE(ExecJs(current_frame_host(), JsReplace(R"(
    const frame = document.createElement('iframe');
    frame.src = $1;
    document.body.appendChild(frame);
  )",
                                                     regular_page)));
  ASSERT_TRUE(WaitForLoadStop(web_contents()));
  RenderFrameHostImpl* iframe_rfh =
      current_frame_host()->child_at(0)->current_frame_host();

  // The iframe is in the same browsing context group, and access should be
  // possible.
  EXPECT_TRUE(ExecJs(current_frame_host(), "window[0].blur()"));
  EXPECT_TRUE(ExecJs(iframe_rfh, "top.blur()"));
}

// Verifies that BrowsingContextGroupInfo is properly propagated to iframes and
// iframes in popups, all living in the same SiteInstance.
IN_PROC_BROWSER_TEST_P(CoopRestrictPropertiesAccessBrowserTest,
                       LocalSubframesInPopup) {
  GURL regular_page(https_server()->GetURL("a.test", "/title1.html"));

  // Start from a regular page with a subframe and open a popup with a subframe,
  // all in the same SiteInstance.
  ASSERT_TRUE(NavigateToURL(shell(), regular_page));
  ASSERT_TRUE(ExecJs(current_frame_host(), JsReplace(R"(
    const frame = document.createElement('iframe');
    frame.src = $1;
    document.body.appendChild(frame);
  )",
                                                     regular_page)));
  ASSERT_TRUE(WaitForLoadStop(web_contents()));
  RenderFrameHostImpl* iframe_rfh =
      current_frame_host()->child_at(0)->current_frame_host();

  ShellAddedObserver shell_observer;
  ASSERT_TRUE(ExecJs(current_frame_host(),
                     JsReplace("window.w = open($1);", regular_page)));
  WebContentsImpl* popup_window =
      static_cast<WebContentsImpl*>(shell_observer.GetShell()->web_contents());
  ASSERT_TRUE(WaitForLoadStop(popup_window));
  RenderFrameHostImpl* popup_rfh = popup_window->GetPrimaryMainFrame();
  ASSERT_EQ(current_frame_host()->GetSiteInstance(),
            popup_rfh->GetSiteInstance());

  ASSERT_TRUE(ExecJs(popup_rfh, JsReplace(R"(
    const frame = document.createElement('iframe');
    frame.src = $1;
    document.body.appendChild(frame);
  )",
                                          regular_page)));
  ASSERT_TRUE(WaitForLoadStop(popup_window));
  RenderFrameHostImpl* popup_iframe_rfh =
      popup_rfh->child_at(0)->current_frame_host();

  // All frames are in the same SiteInstance, and access should be possible.
  EXPECT_TRUE(ExecJs(current_frame_host(), "window[0].blur()"));
  EXPECT_TRUE(ExecJs(current_frame_host(), "window.w.blur()"));
  EXPECT_TRUE(ExecJs(current_frame_host(), "window.w[0].blur()"));

  EXPECT_TRUE(ExecJs(iframe_rfh, "top.blur()"));
  EXPECT_TRUE(ExecJs(iframe_rfh, "top.w.blur()"));
  EXPECT_TRUE(ExecJs(iframe_rfh, "top.w[0].blur()"));

  EXPECT_TRUE(ExecJs(popup_rfh, "opener.blur()"));
  EXPECT_TRUE(ExecJs(popup_rfh, "opener[0].blur()"));
  EXPECT_TRUE(ExecJs(popup_rfh, "window[0].blur()"));

  EXPECT_TRUE(ExecJs(popup_iframe_rfh, "top.blur()"));
  EXPECT_TRUE(ExecJs(popup_iframe_rfh, "top.opener.blur()"));
  EXPECT_TRUE(ExecJs(popup_iframe_rfh, "top.opener[0].blur()"));
}

// Verifies that BrowsingContextGroupInfo is properly propagated to iframes and
// iframes in popups, all living in the same browsing context group.
IN_PROC_BROWSER_TEST_P(CoopRestrictPropertiesAccessBrowserTest,
                       RemoteSubframesInPopup) {
  GURL regular_page(https_server()->GetURL("a.test", "/title1.html"));
  GURL regular_page_2(https_server()->GetURL("b.test", "/title1.html"));
  GURL regular_page_3(https_server()->GetURL("c.test", "/title1.html"));
  GURL regular_page_4(https_server()->GetURL("d.test", "/title1.html"));

  // Start from a regular page with a subframe and open a popup with a subframe,
  // all in the same browsing context group, but in different SiteInstances.
  ASSERT_TRUE(NavigateToURL(shell(), regular_page));
  ASSERT_TRUE(ExecJs(current_frame_host(), JsReplace(R"(
    const frame = document.createElement('iframe');
    frame.src = $1;
    document.body.appendChild(frame);
  )",
                                                     regular_page_2)));
  ASSERT_TRUE(WaitForLoadStop(web_contents()));
  RenderFrameHostImpl* iframe_rfh =
      current_frame_host()->child_at(0)->current_frame_host();

  ShellAddedObserver shell_observer;
  ASSERT_TRUE(ExecJs(current_frame_host(),
                     JsReplace("window.w = open($1);", regular_page_3)));
  WebContentsImpl* popup_window =
      static_cast<WebContentsImpl*>(shell_observer.GetShell()->web_contents());

  ASSERT_TRUE(WaitForLoadStop(popup_window));
  RenderFrameHostImpl* popup_rfh = popup_window->GetPrimaryMainFrame();
  ASSERT_TRUE(current_frame_host()->GetSiteInstance()->IsRelatedSiteInstance(
      popup_rfh->GetSiteInstance()));

  ASSERT_TRUE(ExecJs(popup_rfh, JsReplace(R"(
    const frame = document.createElement('iframe');
    frame.src = $1;
    document.body.appendChild(frame);
  )",
                                          regular_page_4)));
  ASSERT_TRUE(WaitForLoadStop(popup_window));
  RenderFrameHostImpl* popup_iframe_rfh =
      popup_rfh->child_at(0)->current_frame_host();

  // All frames are in the same browsing context group and access should be
  // possible.
  EXPECT_TRUE(ExecJs(current_frame_host(), "window[0].blur()"));
  EXPECT_TRUE(ExecJs(current_frame_host(), "window.w.blur()"));
  EXPECT_TRUE(ExecJs(current_frame_host(), "window.w[0].blur()"));

  // The iframe in the main page can only access its top frame, because it has
  // no way to grab the window.w handle as a cross-origin frame.
  EXPECT_TRUE(ExecJs(iframe_rfh, "top.blur()"));

  EXPECT_TRUE(ExecJs(popup_rfh, "opener.blur()"));
  EXPECT_TRUE(ExecJs(popup_rfh, "opener[0].blur()"));
  EXPECT_TRUE(ExecJs(popup_rfh, "window[0].blur()"));

  EXPECT_TRUE(ExecJs(popup_iframe_rfh, "top.blur()"));
  EXPECT_TRUE(ExecJs(popup_iframe_rfh, "top.opener.blur()"));
  EXPECT_TRUE(ExecJs(popup_iframe_rfh, "top.opener[0].blur()"));
}

// Verifies that BrowsingContextGroupInfo is properly propagated to iframes and
// iframes in popups living in a different browsing context group in the same
// CoopRelatedGroup.
IN_PROC_BROWSER_TEST_P(CoopRestrictPropertiesAccessBrowserTest,
                       SubframesInCoopPopup) {
  GURL regular_page(https_server()->GetURL("a.test", "/title1.html"));
  GURL regular_page_2(https_server()->GetURL("b.test", "/title1.html"));
  GURL coop_rp_page(https_server()->GetURL(
      "c.test",
      "/set-header"
      "?cross-origin-opener-policy: restrict-properties"));
  GURL regular_page_3(https_server()->GetURL("d.test", "/title1.html"));

  // Start from a regular page with a subframe and open a popup in another
  // browsing context group in the same CoopRelatedGroup, itself with an iframe.
  ASSERT_TRUE(NavigateToURL(shell(), regular_page));
  ASSERT_TRUE(ExecJs(current_frame_host(), JsReplace(R"(
    const frame = document.createElement('iframe');
    frame.src = $1;
    document.body.appendChild(frame);
  )",
                                                     regular_page_2)));
  ASSERT_TRUE(WaitForLoadStop(web_contents()));
  RenderFrameHostImpl* iframe_rfh =
      current_frame_host()->child_at(0)->current_frame_host();

  ShellAddedObserver shell_observer;
  ASSERT_TRUE(ExecJs(current_frame_host(),
                     JsReplace("window.w = open($1);", coop_rp_page)));
  WebContentsImpl* popup_window =
      static_cast<WebContentsImpl*>(shell_observer.GetShell()->web_contents());

  ASSERT_TRUE(WaitForLoadStop(popup_window));
  RenderFrameHostImpl* popup_rfh = popup_window->GetPrimaryMainFrame();
  ASSERT_FALSE(current_frame_host()->GetSiteInstance()->IsRelatedSiteInstance(
      popup_rfh->GetSiteInstance()));
  ASSERT_TRUE(
      current_frame_host()->GetSiteInstance()->IsCoopRelatedSiteInstance(
          popup_rfh->GetSiteInstance()));

  ASSERT_TRUE(ExecJs(popup_rfh, JsReplace(R"(
    const frame = document.createElement('iframe');
    frame.src = $1;
    document.body.appendChild(frame);
  )",
                                          regular_page_3)));
  ASSERT_TRUE(WaitForLoadStop(popup_window));
  RenderFrameHostImpl* popup_iframe_rfh =
      popup_rfh->child_at(0)->current_frame_host();

  // Different pages are in different browsing context groups and access should
  // be restricted. Access within a page should not.
  EXPECT_TRUE(ExecJs(current_frame_host(), "window[0].blur()"));
  std::string result =
      EvalJs(current_frame_host(),
             "try { window.w.blur() } catch (e) { e.toString(); }")
          .ExtractString();
  EXPECT_THAT(result, ::testing::MatchesRegex(kCoopRpErrorMessageRegex));
  EXPECT_EQ(true, EvalJs(current_frame_host(), "window.w.closed == false"));

  EXPECT_TRUE(ExecJs(iframe_rfh, "top.blur()"));

  EXPECT_TRUE(ExecJs(popup_rfh, "window[0].blur()"));
  result =
      EvalJs(popup_rfh, "try { opener.blur() } catch (e) { e.toString(); }")
          .ExtractString();
  EXPECT_THAT(result, ::testing::MatchesRegex(kCoopRpErrorMessageRegex));
  EXPECT_EQ(true, EvalJs(popup_rfh, "opener.closed == false"));

  EXPECT_TRUE(ExecJs(popup_iframe_rfh, "top.blur()"));
  result = EvalJs(popup_iframe_rfh,
                  "try { top.opener.blur() } catch (e) { e.toString(); }")
               .ExtractString();
  EXPECT_THAT(result, ::testing::MatchesRegex(kCoopRpErrorMessageRegex));
  EXPECT_EQ(true, EvalJs(popup_iframe_rfh, "top.opener.closed == false"));
}

// Verify that navigating to another browsing context group in the same
// CoopRelatedGroup and ending up in an error page propagates the
// BrowsingContextGroupInfo properly.
IN_PROC_BROWSER_TEST_P(CoopRestrictPropertiesAccessBrowserTest,
                       NavigationToError) {
  GURL regular_page(https_server()->GetURL("a.test", "/title1.html"));
  GURL coop_rp_page(https_server()->GetURL(
      "b.test",
      "/set-header"
      "?cross-origin-opener-policy: restrict-properties"));
  GURL error_page(https_server()->GetURL("b.test", "/page_not_found"));

  // Start from a regular page and a popup in different browsing context groups
  // in the same CoopRelatedGroup.
  ASSERT_TRUE(NavigateToURL(shell(), regular_page));
  ShellAddedObserver shell_observer;
  ASSERT_TRUE(ExecJs(current_frame_host(),
                     JsReplace("window.w = open($1);", coop_rp_page)));
  WebContentsImpl* popup_window =
      static_cast<WebContentsImpl*>(shell_observer.GetShell()->web_contents());
  ASSERT_TRUE(WaitForLoadStop(popup_window));
  RenderFrameHostImpl* popup_rfh = popup_window->GetPrimaryMainFrame();
  ASSERT_FALSE(current_frame_host()->GetSiteInstance()->IsRelatedSiteInstance(
      popup_rfh->GetSiteInstance()));
  ASSERT_TRUE(
      current_frame_host()->GetSiteInstance()->IsCoopRelatedSiteInstance(
          popup_rfh->GetSiteInstance()));

  // Navigate the popup to an error page. It should reuse the original browsing
  // context group.
  ASSERT_FALSE(NavigateToURL(popup_window, error_page));
  RenderFrameHostImpl* error_popup_rfh = popup_window->GetPrimaryMainFrame();
  ASSERT_TRUE(current_frame_host()->GetSiteInstance()->IsRelatedSiteInstance(
      error_popup_rfh->GetSiteInstance()));

  // We've come back to the original browsing context group, so access should be
  // possible.
  EXPECT_TRUE(ExecJs(current_frame_host(), "window.w.blur()"));
  EXPECT_TRUE(ExecJs(error_popup_rfh, "opener.blur()"));
}

// Verify that navigating to another browsing context group in the same
// CoopRelatedGroup and going back propagates the BrowsingContextGroupInfo
// properly.
IN_PROC_BROWSER_TEST_P(CoopRestrictPropertiesAccessBrowserTest,
                       HistoryNavigation) {
  GURL regular_page(https_server()->GetURL("a.test", "/title1.html"));
  GURL regular_page_2(https_server()->GetURL("b.test", "/title1.html"));
  GURL coop_rp_page(https_server()->GetURL(
      "c.test",
      "/set-header"
      "?cross-origin-opener-policy: restrict-properties"));

  // Start from a regular page and a popup in the same browsing context group.
  ASSERT_TRUE(NavigateToURL(shell(), regular_page));
  ShellAddedObserver shell_observer;
  ASSERT_TRUE(ExecJs(current_frame_host(),
                     JsReplace("window.w = open($1);", regular_page_2)));
  WebContentsImpl* popup_window =
      static_cast<WebContentsImpl*>(shell_observer.GetShell()->web_contents());
  ASSERT_TRUE(WaitForLoadStop(popup_window));
  RenderFrameHostImpl* popup_rfh = popup_window->GetPrimaryMainFrame();
  ASSERT_TRUE(current_frame_host()->GetSiteInstance()->IsRelatedSiteInstance(
      popup_rfh->GetSiteInstance()));

  // Navigate the popup to another browsing context group in the same
  // CoopRelatedGroup.
  ASSERT_TRUE(NavigateToURL(popup_window, coop_rp_page));
  RenderFrameHostImpl* second_popup_rfh = popup_window->GetPrimaryMainFrame();
  ASSERT_FALSE(current_frame_host()->GetSiteInstance()->IsRelatedSiteInstance(
      second_popup_rfh->GetSiteInstance()));
  ASSERT_TRUE(
      current_frame_host()->GetSiteInstance()->IsCoopRelatedSiteInstance(
          second_popup_rfh->GetSiteInstance()));

  // Navigate back. The browsing context group information should properly be
  // updated.
  popup_window->GetController().GoBack();
  ASSERT_TRUE(WaitForLoadStop(popup_window));
  RenderFrameHostImpl* back_popup_rfh = popup_window->GetPrimaryMainFrame();
  ASSERT_TRUE(current_frame_host()->GetSiteInstance()->IsRelatedSiteInstance(
      back_popup_rfh->GetSiteInstance()));

  // We've come back to the original browsing context group, access should be
  // possible.
  EXPECT_TRUE(ExecJs(current_frame_host(), "window.w.blur()"));
  EXPECT_TRUE(ExecJs(back_popup_rfh, "opener.blur()"));
}

// Verify that activating a BackForwardCache entry in another browsing context
// group propagates the BrowsingContextGroupInfo properly.
IN_PROC_BROWSER_TEST_P(CoopRestrictPropertiesAccessBrowserTest,
                       BackForwardCacheNavigation) {
  GURL regular_page_1(https_server()->GetURL("a.test", "/title1.html"));
  GURL regular_page_2(https_server()->GetURL("b.test", "/title1.html"));
  GURL coop_rp_page(https_server()->GetURL(
      "c.test",
      "/set-header"
      "?cross-origin-opener-policy: restrict-properties"));
  GURL coop_rp_page_with_fragment(coop_rp_page.spec() + "#fragment");

  // Start on a first page, then navigate to a cross-origin page. If BFCache is
  // enabled, we'll get a proactive swap and the page will be saved in the
  // BFCache.
  ASSERT_TRUE(NavigateToURL(shell(), regular_page_1));
  RenderFrameHostImpl* initial_rfh = current_frame_host();
  scoped_refptr<SiteInstanceImpl> initial_si =
      current_frame_host()->GetSiteInstance();
  base::UnguessableToken initial_bi_token =
      initial_si->browsing_instance_token();
  base::UnguessableToken initial_coop_token =
      initial_si->coop_related_group_token();

  ASSERT_TRUE(NavigateToURL(shell(), regular_page_2));
  scoped_refptr<SiteInstanceImpl> second_si =
      current_frame_host()->GetSiteInstance();
  base::UnguessableToken second_bi_token = second_si->browsing_instance_token();
  base::UnguessableToken second_coop_token =
      second_si->coop_related_group_token();
  if (IsBackForwardCacheEnabled()) {
    ASSERT_FALSE(second_si->IsCoopRelatedSiteInstance(initial_si.get()));
    EXPECT_NE(initial_bi_token, second_bi_token);
    EXPECT_NE(initial_coop_token, second_coop_token);
  }

  // Now open a popup in another browsing context group in the same
  // CoopRelatedGroup.
  ShellAddedObserver shell_observer;
  ASSERT_TRUE(ExecJs(current_frame_host(),
                     JsReplace("window.w = open($1);", coop_rp_page)));
  WebContentsImpl* popup_window =
      static_cast<WebContentsImpl*>(shell_observer.GetShell()->web_contents());
  ASSERT_TRUE(WaitForLoadStop(popup_window));
  RenderFrameHostImpl* popup_rfh = popup_window->GetPrimaryMainFrame();
  SiteInstanceImpl* popup_si = popup_rfh->GetSiteInstance();
  base::UnguessableToken popup_bi_token = popup_si->browsing_instance_token();
  base::UnguessableToken popup_coop_token =
      popup_si->coop_related_group_token();
  ASSERT_FALSE(popup_si->IsRelatedSiteInstance(second_si.get()));
  ASSERT_TRUE(popup_si->IsCoopRelatedSiteInstance(second_si.get()));
  EXPECT_NE(popup_bi_token, second_bi_token);
  EXPECT_EQ(popup_coop_token, second_coop_token);

  // Now go back. If the BFCache is enabled, it will be used. In any case, we
  // should be back to the original SiteInstance.
  web_contents()->GetController().GoBack();
  ASSERT_TRUE(WaitForLoadStop(web_contents()));
  if (IsBackForwardCacheEnabled()) {
    ASSERT_EQ(current_frame_host(), initial_rfh);
  }
  SiteInstanceImpl* back_si = current_frame_host()->GetSiteInstance();
  EXPECT_EQ(back_si->browsing_instance_token(), initial_bi_token);
  EXPECT_EQ(back_si->coop_related_group_token(), initial_coop_token);
  EXPECT_NE(popup_bi_token, initial_bi_token);
  EXPECT_NE(popup_coop_token, initial_coop_token);

  // Ensure any BrowsingContextGroupInfo update has been propagated. Doing a
  // same-document navigation works, because the interfaces are associated.
  ASSERT_TRUE(NavigateToURL(popup_window, coop_rp_page_with_fragment));

  // If the BrowsingContextGroupInfo was properly propagated to the renderer
  // upon the BFCache navigation, access to the popup should be unrestricted.
  EXPECT_TRUE(ExecJs(popup_rfh, "opener.blur()"));
}

// Verify that navigating to another browsing context group in the same
// CoopRelatedGroup from a crashed frame propagates the BrowsingContextGroupInfo
// properly.
IN_PROC_BROWSER_TEST_P(CoopRestrictPropertiesAccessBrowserTest,
                       PostCrashNavigation) {
  GURL regular_page(https_server()->GetURL("a.test", "/title1.html"));
  GURL regular_page_2(https_server()->GetURL("b.test", "/title1.html"));
  GURL coop_rp_page(https_server()->GetURL(
      "c.test",
      "/set-header"
      "?cross-origin-opener-policy: restrict-properties"));

  // To be able to properly test that access is preserved after a crashed
  // process navigates again, we don't want both the openee and the opener to
  // live in the same process and to both crash.
  if (!AreAllSitesIsolatedForTesting()) {
    return;
  }

  // Start from a regular page and a popup in the same browsing context group.
  ASSERT_TRUE(NavigateToURL(shell(), regular_page));
  ShellAddedObserver shell_observer;
  ASSERT_TRUE(ExecJs(current_frame_host(),
                     JsReplace("window.w = open($1);", regular_page_2)));
  WebContentsImpl* popup_window =
      static_cast<WebContentsImpl*>(shell_observer.GetShell()->web_contents());

  ASSERT_TRUE(WaitForLoadStop(popup_window));
  RenderFrameHostImpl* popup_rfh = popup_window->GetPrimaryMainFrame();
  ASSERT_TRUE(current_frame_host()->GetSiteInstance()->IsRelatedSiteInstance(
      popup_rfh->GetSiteInstance()));

  // Simulate the renderer process used for the popup crashing.
  RenderProcessHost* process = popup_rfh->GetSiteInstance()->GetProcess();
  ASSERT_TRUE(process);
  RenderProcessHostWatcher crash_observer(
      process, RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  process->Shutdown(0);
  crash_observer.Wait();

  // Navigate the popup to another browsing context group in the same
  // CoopRelatedGroup.
  ASSERT_TRUE(NavigateToURL(popup_window, coop_rp_page));
  RenderFrameHostImpl* second_popup_rfh = popup_window->GetPrimaryMainFrame();
  ASSERT_FALSE(current_frame_host()->GetSiteInstance()->IsRelatedSiteInstance(
      second_popup_rfh->GetSiteInstance()));
  ASSERT_TRUE(
      current_frame_host()->GetSiteInstance()->IsCoopRelatedSiteInstance(
          second_popup_rfh->GetSiteInstance()));

  // Because they are in different browsing context groups in the same
  // CoopRelatedGroup, access should be restricted.
  std::string result =
      EvalJs(current_frame_host(),
             "try { window.w.blur() } catch (e) { e.toString(); }")
          .ExtractString();
  EXPECT_THAT(result, ::testing::MatchesRegex(kCoopRpErrorMessageRegex));

  result = EvalJs(second_popup_rfh,
                  "try { opener.blur() } catch (e) { e.toString(); }")
               .ExtractString();
  EXPECT_THAT(result, ::testing::MatchesRegex(kCoopRpErrorMessageRegex));

  // Access to window.closed should not throw any exception.
  EXPECT_EQ(true, EvalJs(second_popup_rfh, "opener.closed == false"));
  EXPECT_EQ(true, EvalJs(current_frame_host(), "window.w.closed == false"));
}

// Verify that navigating to another browsing context group in another
// CoopRelatedGroup, in one of the rare cases that preserve openers (here to a
// WebUI), propagates the correct BrowsingContextGroupInfo.
IN_PROC_BROWSER_TEST_P(CoopRestrictPropertiesAccessBrowserTest,
                       NavigationToOtherCoopRelatedGroup) {
  GURL regular_page(https_server()->GetURL("a.test", "/title1.html"));
  GURL regular_page_2(https_server()->GetURL("b.test", "/title1.html"));
  GURL webui_page("chrome://ukm");

  // Start from a regular page and a popup in the same browsing context group.
  ASSERT_TRUE(NavigateToURL(shell(), regular_page));
  ShellAddedObserver shell_observer;
  ASSERT_TRUE(ExecJs(current_frame_host(),
                     JsReplace("window.w = open($1);", regular_page_2)));
  WebContentsImpl* popup_window =
      static_cast<WebContentsImpl*>(shell_observer.GetShell()->web_contents());

  ASSERT_TRUE(WaitForLoadStop(popup_window));
  RenderFrameHostImpl* popup_rfh = popup_window->GetPrimaryMainFrame();
  ASSERT_TRUE(current_frame_host()->GetSiteInstance()->IsRelatedSiteInstance(
      popup_rfh->GetSiteInstance()));

  // Navigate to a WebUI page. It should use another browsing context group in
  // another CoopRelatedGroup. This WebUI page will not have an opener, but will
  // NOT clear proxies, keeping the handle in the main page valid.
  // TODO(crbug.com/40239885): This is an unspec'd behavior and might
  // change in the future.
  ASSERT_TRUE(NavigateToURL(popup_window, webui_page));
  RenderFrameHostImpl* webui_popup_rfh = popup_window->GetPrimaryMainFrame();
  ASSERT_FALSE(
      current_frame_host()->GetSiteInstance()->IsCoopRelatedSiteInstance(
          webui_popup_rfh->GetSiteInstance()));
  ASSERT_EQ(true, EvalJs(webui_popup_rfh, "window.opener == null"));
  ASSERT_EQ(false, EvalJs(current_frame_host(), "window.w == null"));

  // Because they are in different browsing context groups in different
  // CoopRelatedGroups, access to cross-origin properties should conservatively
  // NOT be restricted.
  // TODO(crbug.com/40275679): This might change in the future, if we
  // decide to impose restrictions on all accesses from different browsing
  // context groups.
  EXPECT_TRUE(ExecJs(current_frame_host(), "window.w.blur()"));

  // Some actions should be blocked nonetheless, regardless of COOP:
  // restrict-properties. This is the case for sending postMessages. Set up a
  // listener in the WebUI page, and send a message from the main page. If we
  // have not received anything within a second, consider this passed. Receiving
  // the message would throw an exception.
  ASSERT_TRUE(ExecJs(webui_popup_rfh, R"(
      window.future_message = new Promise(
        (resolve, reject) => {
          onmessage = (event) => {
            if (event.data == 'test') {
              reject('Received message');
            }
          };
          setTimeout(resolve, 1000);
        }); 0;)"));  // This avoids waiting on the promise right now.
  EXPECT_TRUE(
      ExecJs(current_frame_host(), "window.w.postMessage('test', '*')"));

  // If we've received the message, this promise would be rejected and an
  // exception would be thrown.
  EXPECT_TRUE(ExecJs(webui_popup_rfh, "window.future_message;"));

  // Navigating frames in other CoopRelatedGroup should also not be permitted.
  // Try to start a navigation and verify that nothing happened.
  EXPECT_TRUE(ExecJs(current_frame_host(),
                     JsReplace("window.w.location = $1", regular_page)));
  EXPECT_TRUE(WaitForLoadStop(popup_window));
  EXPECT_EQ(popup_window->GetLastCommittedURL(), webui_page);
}

// This test verifies that two pages in different browsing context groups with
// the same origin trying to access each other does not cause a crash.
IN_PROC_BROWSER_TEST_P(CoopRestrictPropertiesAccessBrowserTest,
                       SameOriginInDifferentBrowsingContextGroupAccess) {
  GURL regular_page(https_server()->GetURL("a.test", "/title1.html"));
  GURL coop_rp_page(https_server()->GetURL(
      "a.test",
      "/set-header"
      "?cross-origin-opener-policy: restrict-properties"));

  // Start from a regular page and open a same-origin popup in another browsing
  // context group in the same CoopRelatedGroup. Although the two pages are
  // same-origin, they should only be able to reach out to each other using
  // postMessage() and closed.
  ASSERT_TRUE(NavigateToURL(shell(), regular_page));
  ShellAddedObserver shell_observer;
  EXPECT_TRUE(ExecJs(current_frame_host(),
                     JsReplace("window.w = window.open($1)", coop_rp_page)));
  WebContentsImpl* popup_window =
      static_cast<WebContentsImpl*>(shell_observer.GetShell()->web_contents());
  ASSERT_TRUE(WaitForLoadStop(popup_window));
  RenderFrameHostImpl* popup_rfh = popup_window->GetPrimaryMainFrame();
  ASSERT_FALSE(current_frame_host()->GetSiteInstance()->IsRelatedSiteInstance(
      popup_rfh->GetSiteInstance()));
  ASSERT_TRUE(
      current_frame_host()->GetSiteInstance()->IsCoopRelatedSiteInstance(
          popup_rfh->GetSiteInstance()));

  // Because they are in different browsing context groups in the same
  // CoopRelatedGroup, access to cross-origin properties should be restricted.
  std::string result =
      EvalJs(current_frame_host(),
             "try { window.w.blur() } catch (e) { e.toString(); }")
          .ExtractString();
  EXPECT_THAT(result, ::testing::MatchesRegex(kCoopRpErrorMessageRegex));

  result =
      EvalJs(popup_rfh, "try { opener.blur() } catch (e) { e.toString(); }")
          .ExtractString();
  EXPECT_THAT(result, ::testing::MatchesRegex(kCoopRpErrorMessageRegex));

  // Similarly, same-origin properties access should be blocked.
  result = EvalJs(current_frame_host(),
                  "try { window.w.name} catch (e) { e.toString(); }")
               .ExtractString();
  EXPECT_THAT(result, ::testing::MatchesRegex(kCoopRpErrorMessageRegex));

  result = EvalJs(popup_rfh, "try { opener.name} catch (e) { e.toString(); }")
               .ExtractString();
  EXPECT_THAT(result, ::testing::MatchesRegex(kCoopRpErrorMessageRegex));

  // Always-allowed properties should still be accessible.
  EXPECT_EQ(true, EvalJs(current_frame_host(), "window.w.closed == false"));
  EXPECT_EQ(true, EvalJs(popup_rfh, "opener.closed == false"));
}

// Similar to above test, but forces process reuse to have both the popup and
// the main page live in the same process.
IN_PROC_BROWSER_TEST_P(
    CoopRestrictPropertiesAccessBrowserTest,
    SameOriginInDifferentBrowsingContextGroupAccessSameProcess) {
  // Some platform force COOP pages to be isolated, making this test irrelevant.
  if (SiteIsolationPolicy::IsSiteIsolationForCOOPEnabled()) {
    return;
  }

  // Set a process limit of 1 for testing. This will force same-origin pages
  // with different COOP status to share a process.
  RenderProcessHostImpl::SetMaxRendererProcessCount(1);

  GURL regular_page(https_server()->GetURL("a.test", "/title1.html"));
  GURL coop_rp_page(https_server()->GetURL(
      "a.test",
      "/set-header"
      "?cross-origin-opener-policy: restrict-properties"));

  // Start from a regular page and open a same-origin popup in another browsing
  // context group in the same CoopRelatedGroup. Although the two pages are
  // same-origin, they should only be able to reach out to each other using
  // postMessage() and closed.
  ASSERT_TRUE(NavigateToURL(shell(), regular_page));
  ShellAddedObserver shell_observer;
  EXPECT_TRUE(ExecJs(current_frame_host(),
                     JsReplace("window.w = window.open($1)", coop_rp_page)));
  WebContentsImpl* popup_window =
      static_cast<WebContentsImpl*>(shell_observer.GetShell()->web_contents());

  ASSERT_TRUE(WaitForLoadStop(popup_window));
  RenderFrameHostImpl* popup_rfh = popup_window->GetPrimaryMainFrame();
  ASSERT_FALSE(current_frame_host()->GetSiteInstance()->IsRelatedSiteInstance(
      popup_rfh->GetSiteInstance()));
  ASSERT_TRUE(
      current_frame_host()->GetSiteInstance()->IsCoopRelatedSiteInstance(
          popup_rfh->GetSiteInstance()));
  ASSERT_EQ(current_frame_host()->GetProcess(), popup_rfh->GetProcess());

  // Because they are in different browsing context groups in the same
  // CoopRelatedGroup, access to cross-origin properties should be restricted.
  std::string result =
      EvalJs(current_frame_host(),
             "try { window.w.blur() } catch (e) { e.toString(); }")
          .ExtractString();
  EXPECT_THAT(result, ::testing::MatchesRegex(kCoopRpErrorMessageRegex));

  result =
      EvalJs(popup_rfh, "try { opener.blur() } catch (e) { e.toString(); }")
          .ExtractString();
  EXPECT_THAT(result, ::testing::MatchesRegex(kCoopRpErrorMessageRegex));

  // Similarly, same-origin properties access should also be blocked.
  result = EvalJs(current_frame_host(),
                  "try { window.w.name } catch (e) { e.toString(); }")
               .ExtractString();
  EXPECT_THAT(result, ::testing::MatchesRegex(kCoopRpErrorMessageRegex));

  result = EvalJs(popup_rfh, "try { opener.name } catch (e) { e.toString(); }")
               .ExtractString();
  EXPECT_THAT(result, ::testing::MatchesRegex(kCoopRpErrorMessageRegex));

  // Always-allowed properties should still be accessible.
  EXPECT_EQ(true, EvalJs(current_frame_host(), "window.w.closed == false"));
  EXPECT_EQ(true, EvalJs(popup_rfh, "opener.closed == false"));
}

// Regression test for https://crbug.com/1491282.  Ensure that when a
// navigation to a COOP: RP page requires a new BrowsingInstance in a new
// CoopRelatedGroup, a subsequent navigation that stays in the same
// CoopRelatedGroup does not crash.  In this case, it is essential that when a
// new non-COOP BrowsingInstance in a new CoopRelatedGroup is created at
// request start time, that BrowsingInstance isn't incorrectly reused at
// response started time, if the response came back with COOP: RP headers and
// requires a BrowsingInstance with a different common_coop_origin().
IN_PROC_BROWSER_TEST_P(CoopRestrictPropertiesAccessBrowserTest,
                       NewBrowsingInstanceFromBeginNavigationCannotBeReused) {
  // Start on a WebUI page. The repro for https://crbug.com/1491282 required
  // this, because the security swap from WebUI to normal pages requires a new
  // BrowsingInstance (with no common_coop_origin) and a new CoopRelatedGroup
  // at both request and response time. In contrast, navigating from a normal
  // page to a COOP:RP page would pick a new BrowsingInstance (with a
  // common_coop_origin) in the same CoopRelatedGroup at response time, because
  // the kRelatedCoopSwap reason is chosen after checking for security swaps
  // but before checking for proactive swaps. A new CoopRelatedGroup guarantees
  // that ConvertToSiteInstance() will attempt to reuse the speculative
  // RenderFrameHost's SiteInstance (the "candidate_instance") at response
  // time, rather than getting a SiteInstance + BrowsingInstance in the same
  // CoopRelatedGroup.
  GURL webui_page("chrome://ukm");
  ASSERT_TRUE(NavigateToURL(shell(), webui_page));
  scoped_refptr<SiteInstanceImpl> webui_instance(
      current_frame_host()->GetSiteInstance());

  // Now, navigate to a COOP: restrict-properties page.  This will create a
  // fresh BrowsingInstance at request start time, and evaluate whether it can
  // stay in that BrowsingInstance after receiving the response.  In
  // https://crbug.com/1491282, the BrowsingInstance from request start was
  // incorrectly reused, resulting in not having a common_coop_origin() at the
  // end of this navigation.  Ensure this is not the case.
  GURL coop_rp_page(https_server()->GetURL(
      "a.test",
      "/set-header"
      "?cross-origin-opener-policy: restrict-properties"));
  ASSERT_TRUE(NavigateToURL(shell(), coop_rp_page));
  scoped_refptr<SiteInstanceImpl> coop_rp_instance(
      current_frame_host()->GetSiteInstance());
  EXPECT_FALSE(
      webui_instance->IsCoopRelatedSiteInstance(coop_rp_instance.get()));
  EXPECT_TRUE(coop_rp_instance->GetCommonCoopOrigin().has_value());
  EXPECT_EQ("a.test", coop_rp_instance->GetCommonCoopOrigin()->host());

  // Ensure that we can navigate to a page without COOP: restrict-properties.
  // This should swap BrowsingInstances but stay in the same CoopRelatedGroup,
  // and this shouldn't crash.
  GURL non_coop_rp_page(https_server()->GetURL("b.test", "/title1.html"));
  ASSERT_TRUE(NavigateToURL(shell(), non_coop_rp_page));
  SiteInstanceImpl* non_coop_instance(current_frame_host()->GetSiteInstance());
  EXPECT_FALSE(non_coop_instance->GetCommonCoopOrigin().has_value());
  EXPECT_FALSE(coop_rp_instance->IsRelatedSiteInstance(non_coop_instance));
  EXPECT_TRUE(coop_rp_instance->IsCoopRelatedSiteInstance(non_coop_instance));
}

IN_PROC_BROWSER_TEST_P(CoopRestrictPropertiesAccessBrowserTest, Prerender) {
  GURL regular_page(https_server()->GetURL("a.test", "/title1.html"));
  GURL coop_rp_page(https_server()->GetURL(
      "a.test",
      "/set-header"
      "?cross-origin-opener-policy: restrict-properties"));
  GURL regular_page_2(https_server()->GetURL("b.test", "/title1.html"));
  GURL regular_page_2_with_fragment(
      https_server()->GetURL("b.test", "/title1.html#fragment"));

  // Start on a regular page.
  ASSERT_TRUE(NavigateToURL(shell(), regular_page));
  scoped_refptr<SiteInstanceImpl> initial_si =
      current_frame_host()->GetSiteInstance();
  base::UnguessableToken initial_bi_token =
      initial_si->browsing_instance_token();
  base::UnguessableToken initial_coop_token =
      initial_si->coop_related_group_token();

  // Now prerender a COOP: restrict-properties page and activate it. Prerender
  // does not support staying in the same CoopRelatedGroup, so it will use a
  // completely new CoopRelatedGroup. During activation we should get new
  // BrowsingContextGroupInfo tokens.
  // TODO(crbug.com/40917339): This is an undesired consequence of
  // always starting the prerendering in another BrowsingInstance. See if this
  // should be fixed.
  FrameTreeNodeId host_id = prerender_helper().AddPrerender(coop_rp_page);
  RenderFrameHostImpl* prerender_frame_host = static_cast<RenderFrameHostImpl*>(
      prerender_helper().GetPrerenderedMainFrameHost(host_id));
  ASSERT_TRUE(prerender_frame_host);
  ASSERT_FALSE(
      prerender_frame_host->GetSiteInstance()->IsCoopRelatedSiteInstance(
          initial_si.get()));
  prerender_helper().NavigatePrimaryPage(coop_rp_page);
  RenderFrameHostImpl* activated_rfh = current_frame_host();
  ASSERT_EQ(prerender_frame_host, current_frame_host());
  EXPECT_NE(initial_bi_token,
            activated_rfh->GetSiteInstance()->browsing_instance_token());
  EXPECT_NE(initial_coop_token,
            activated_rfh->GetSiteInstance()->coop_related_group_token());

  // Now open a popup to another regular page.
  ShellAddedObserver shell_observer;
  ASSERT_TRUE(
      ExecJs(current_frame_host(),
             JsReplace("window.w = window.open($1, '');", regular_page_2)));
  WebContentsImpl* popup_window =
      static_cast<WebContentsImpl*>(shell_observer.GetShell()->web_contents());
  ASSERT_TRUE(WaitForLoadStop(popup_window));
  RenderFrameHostImpl* popup_rfh = popup_window->GetPrimaryMainFrame();
  ASSERT_FALSE(current_frame_host()->GetSiteInstance()->IsRelatedSiteInstance(
      popup_rfh->GetSiteInstance()));
  ASSERT_TRUE(
      current_frame_host()->GetSiteInstance()->IsCoopRelatedSiteInstance(
          popup_rfh->GetSiteInstance()));

  // Verify the visible effects of the appropriate tokens being passed down the
  // renderer during the prerender activation. Restricted cross-origin
  // properties access should be blocked.
  std::string opener_to_openee_access =
      EvalJs(current_frame_host(),
             "try { window.w.blur() } catch (e) { e.toString(); }")
          .ExtractString();
  EXPECT_THAT(opener_to_openee_access,
              ::testing::MatchesRegex(kCoopRpErrorMessageRegex));

  std::string openee_to_opener_access =
      EvalJs(popup_rfh, "try { opener.blur() } catch (e) { e.toString(); }")
          .ExtractString();
  EXPECT_THAT(openee_to_opener_access,
              ::testing::MatchesRegex(kCoopRpErrorMessageRegex));

  // Always-allowed properties should still be accessible.
  EXPECT_EQ(true, EvalJs(current_frame_host(), "window.w.closed == false"));
  EXPECT_EQ(true, EvalJs(popup_rfh, "opener.closed == false"));

  // Finally go back in history. We end up in the original SiteInstance.
  web_contents()->GetController().GoBack();
  ASSERT_TRUE(WaitForLoadStop(web_contents()));
  SiteInstanceImpl* back_si = current_frame_host()->GetSiteInstance();
  ASSERT_EQ(back_si, initial_si.get());
  EXPECT_EQ(initial_bi_token, back_si->browsing_instance_token());
  EXPECT_EQ(initial_coop_token, back_si->coop_related_group_token());

  // Do a quick same-document navigation on the popup to make sure
  // BrowsingContextGroupInfo updates are propagated to the renderer. This works
  // because the interfaces are associated.
  ASSERT_TRUE(NavigateToURL(popup_window, regular_page_2_with_fragment));

  // TODO(crbug.com/40917339): The current end behavior is that we end up
  // with a page in another BrowsingInstance, with proxies still around. No
  // restriction is enforced in the renderer, because the tokens for the
  // CoopRelatedGroup do not match, but all browser mitigated APIs will be
  // blocked (postMessage, navigations).
  EXPECT_TRUE(ExecJs(popup_rfh, "opener.blur()"));
}

}  // namespace content
