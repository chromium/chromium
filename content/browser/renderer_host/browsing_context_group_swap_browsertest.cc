// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/browsing_context_group_swap.h"

#include <optional>

#include "base/test/scoped_feature_list.h"
#include "content/browser/renderer_host/navigation_request.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/content_features.h"
#include "content/public/test/back_forward_cache_util.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/content_mock_cert_verifier.h"
#include "content/shell/browser/shell.h"
#include "content/test/render_document_feature.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/default_handlers.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

namespace content {

namespace {

class BrowsingContextGroupSwapBrowserTest
    : public ContentBrowserTest,
      public ::testing::WithParamInterface<std::tuple<std::string, bool>> {
 public:
  BrowsingContextGroupSwapBrowserTest()
      : https_server_(net::EmbeddedTestServer::TYPE_HTTPS) {
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
    auto [render_document_level, enable_back_forward_cache] = info.param;
    return base::StringPrintf(
        "%s_%s",
        GetRenderDocumentLevelNameForTestParams(render_document_level).c_str(),
        enable_back_forward_cache ? "BFCacheEnabled" : "BFCacheDisabled");
  }

  bool IsBackForwardCacheEnabled() { return std::get<1>(GetParam()); }

  net::EmbeddedTestServer* https_server() { return &https_server_; }

 protected:
  void SetUpOnMainThread() override {
    ContentBrowserTest::SetUpOnMainThread();
    mock_cert_verifier_.mock_cert_verifier()->set_default_result(net::OK);
    host_resolver()->AddRule("*", "127.0.0.1");
    https_server()->ServeFilesFromSourceDirectory(GetTestDataFilePath());
    SetupCrossSiteRedirector(https_server());
    net::test_server::RegisterDefaultHandlers(&https_server_);
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

  content::ContentMockCertVerifier mock_cert_verifier_;
  base::test::ScopedFeatureList feature_list_for_render_document_;
  base::test::ScopedFeatureList feature_list_for_back_forward_cache_;
  net::EmbeddedTestServer https_server_;
};

class BrowsingContextGroupSwapObserver : public WebContentsObserver {
 public:
  explicit BrowsingContextGroupSwapObserver(WebContents* web_contents)
      : WebContentsObserver(web_contents), latest_swap_(std::nullopt) {}

  void DidFinishNavigation(NavigationHandle* navigation_handle) override {
    latest_swap_ = NavigationRequest::From(navigation_handle)
                       ->browsing_context_group_swap();
  }

  BrowsingContextGroupSwap GetLatestBrowsingContextGroupSwap() {
    return latest_swap_.value();
  }

 private:
  std::optional<BrowsingContextGroupSwap> latest_swap_;
};

IN_PROC_BROWSER_TEST_P(BrowsingContextGroupSwapBrowserTest, Basic_Navigation) {
  GURL regular_page_1(https_server()->GetURL("a.test", "/title1.html"));
  GURL regular_page_2(https_server()->GetURL("a.test", "/title2.html"));

  BrowsingContextGroupSwapObserver swap_observer(shell()->web_contents());
  ASSERT_TRUE(NavigateToURL(shell(), regular_page_1));
  ASSERT_TRUE(NavigateToURL(shell(), regular_page_2));

  BrowsingContextGroupSwap observed_swap =
      swap_observer.GetLatestBrowsingContextGroupSwap();
  EXPECT_EQ(observed_swap.type(),
            IsBackForwardCacheEnabled()
                ? BrowsingContextGroupSwapType::kProactiveSwap
                : BrowsingContextGroupSwapType::kNoSwap);
}

IN_PROC_BROWSER_TEST_P(BrowsingContextGroupSwapBrowserTest, Coop_Navigation) {
  GURL regular_page(https_server()->GetURL("a.test", "/title1.html"));
  GURL coop_page(https_server()->GetURL(
      "a.test", "/set-header?Cross-Origin-Opener-Policy: same-origin"));

  BrowsingContextGroupSwapObserver swap_observer(shell()->web_contents());
  ASSERT_TRUE(NavigateToURL(shell(), regular_page));
  ASSERT_TRUE(NavigateToURL(shell(), coop_page));

  BrowsingContextGroupSwap observed_swap =
      swap_observer.GetLatestBrowsingContextGroupSwap();
  EXPECT_EQ(observed_swap.type(), BrowsingContextGroupSwapType::kCoopSwap);
}

IN_PROC_BROWSER_TEST_P(BrowsingContextGroupSwapBrowserTest,
                       Security_Navigation) {
  GURL regular_page(https_server()->GetURL("a.test", "/title1.html"));
  GURL webui_page("chrome://ukm");

  BrowsingContextGroupSwapObserver swap_observer(shell()->web_contents());
  ASSERT_TRUE(NavigateToURL(shell(), regular_page));
  ASSERT_TRUE(NavigateToURL(shell(), webui_page));

  BrowsingContextGroupSwap observed_swap =
      swap_observer.GetLatestBrowsingContextGroupSwap();
  EXPECT_EQ(observed_swap.type(), BrowsingContextGroupSwapType::kSecuritySwap);
}

static auto kTestParams =
    testing::Combine(testing::ValuesIn(RenderDocumentFeatureLevelValues()),
                     testing::Bool());
INSTANTIATE_TEST_SUITE_P(All,
                         BrowsingContextGroupSwapBrowserTest,
                         kTestParams,
                         BrowsingContextGroupSwapBrowserTest::DescribeParams);

}  // namespace

}  // namespace content
