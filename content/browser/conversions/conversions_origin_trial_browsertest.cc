// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/strings/strcat.h"
#include "base/test/bind_test_util.h"
#include "base/test/scoped_feature_list.h"
#include "content/browser/conversions/conversion_manager_impl.h"
#include "content/browser/conversions/storable_impression.h"
#include "content/browser/storage_partition_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/url_loader_interceptor.h"
#include "content/shell/browser/shell.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace content {

namespace {
constexpr char kBaseDataDir[] = "content/test/data/conversions/";
}

class ConversionsOriginTrialBrowserTestBase : public ContentBrowserTest {
 public:
  ConversionsOriginTrialBrowserTestBase() = default;

  void SetUpOnMainThread() override {
    ContentBrowserTest::SetUpOnMainThread();

    // We use a URLLoaderInterceptor, rather than the EmbeddedTestServer, since
    // the origin trial token in the response is associated with a fixed
    // origin, whereas EmbeddedTestServer serves content on a random port.
    url_loader_interceptor_ =
        std::make_unique<URLLoaderInterceptor>(base::BindLambdaForTesting(
            [&](URLLoaderInterceptor::RequestParams* params) -> bool {
              URLLoaderInterceptor::WriteResponse(
                  base::StrCat(
                      {kBaseDataDir, params->url_request.url.path_piece()}),
                  params->client.get());
              return true;
            }));
  }

  void TearDownOnMainThread() override { url_loader_interceptor_.reset(); }

  WebContents* web_contents() { return shell()->web_contents(); }

 private:
  std::unique_ptr<URLLoaderInterceptor> url_loader_interceptor_;
};

class ConversionsOriginTrialBrowserTest
    : public ConversionsOriginTrialBrowserTestBase {
 public:
  ConversionsOriginTrialBrowserTest() {
    feature_list_.InitAndEnableFeature(features::kConversionMeasurement);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(ConversionsOriginTrialBrowserTest,
                       OriginTrialEnabled_FeatureDetected) {
  EXPECT_TRUE(NavigateToURL(
      shell(), GURL("https://example.test/impression_with_origin_trial.html")));

  EXPECT_EQ(true, EvalJs(shell(),
                         "document.featurePolicy.features().includes('"
                         "conversion-measurement')"));
}

IN_PROC_BROWSER_TEST_F(ConversionsOriginTrialBrowserTest,
                       OriginTrialEnabled_ImpressionRegistered) {
  EXPECT_TRUE(NavigateToURL(
      shell(), GURL("https://example.test/impression_with_origin_trial.html")));

  EXPECT_TRUE(ExecJs(shell(), R"(
    createImpressionTag("link" /* id */,
                        "https://example.test/page_with_conversion_redirect.html" /* url */,
                        "1" /* impression data */,
                        "https://example.test/" /* conversion_destination */);)"));

  TestNavigationObserver observer(web_contents());
  EXPECT_TRUE(ExecJs(shell(), "simulateClick('link');"));
  observer.Wait();

  ConversionManagerImpl* conversion_manager =
      static_cast<StoragePartitionImpl*>(
          BrowserContext::GetDefaultStoragePartition(
              web_contents()->GetBrowserContext()))
          ->GetConversionManager();

  base::RunLoop run_loop;

  // Verify we have received and logged an impression for the origin trial.
  conversion_manager->GetActiveImpressionsForWebUI(base::BindLambdaForTesting(
      [&](std::vector<StorableImpression> impressions) -> void {
        EXPECT_EQ(1u, impressions.size());
        run_loop.Quit();
      }));
  run_loop.Run();
}

// TODO(johnidel): Add tests that exercise the conversion side logic as well.
// This requires also using an embedded test server because the
// UrlLoadInterceptor cannot properly redirect the conversion pings.

class ConversionsOriginTrialNoBrowserFeatureBrowserTest
    : public ConversionsOriginTrialBrowserTestBase {
 public:
  ConversionsOriginTrialNoBrowserFeatureBrowserTest() {
    feature_list_.InitAndDisableFeature(features::kConversionMeasurement);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(ConversionsOriginTrialNoBrowserFeatureBrowserTest,
                       BrowserSideLogicNotEnabled_FeatureNotDetected) {
  EXPECT_TRUE(NavigateToURL(
      shell(), GURL("https://example.test/impression_with_origin_trial.html")));

  EXPECT_EQ(false, EvalJs(shell(),
                          "document.featurePolicy.features().includes('"
                          "conversion-measurement')"));
}

}  // namespace content
