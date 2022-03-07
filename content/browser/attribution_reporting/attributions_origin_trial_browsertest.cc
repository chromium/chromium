// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/strings/strcat.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "content/browser/attribution_reporting/attribution_manager_impl.h"
#include "content/browser/attribution_reporting/stored_source.h"
#include "content/browser/storage_partition_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
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
#include "third_party/blink/public/common/features.h"
#include "url/gurl.h"

namespace content {

namespace {
constexpr char kBaseDataDir[] = "content/test/data/attribution_reporting/";
}

class AttributionsOriginTrialBrowserTest : public ContentBrowserTest {
 public:
  AttributionsOriginTrialBrowserTest() = default;

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

IN_PROC_BROWSER_TEST_F(AttributionsOriginTrialBrowserTest,
                       OriginTrialEnabled_FeatureDetected) {
  EXPECT_TRUE(NavigateToURL(
      shell(), GURL("https://example.test/impression_with_origin_trial.html")));

  EXPECT_EQ(true, EvalJs(shell(),
                         "document.featurePolicy.features().includes('"
                         "attribution-reporting')"));
  EXPECT_EQ(true, EvalJs(shell(), "window.attributionReporting !== undefined"));
}

IN_PROC_BROWSER_TEST_F(AttributionsOriginTrialBrowserTest,
                       OriginTrialDisabled_FeatureNotDetected) {
  // Navigate to a page without an OT token.
  EXPECT_TRUE(NavigateToURL(
      shell(), GURL("https://example.test/page_with_impression_creator.html")));

  EXPECT_EQ(false, EvalJs(shell(),
                          "document.featurePolicy.features().includes('"
                          "conversion-measurement')"));
  EXPECT_EQ(true, EvalJs(shell(), "window.attributionReporting === undefined"));
}

#if BUILDFLAG(IS_LINUX)
// TODO(https://crbug.com/1121464): Flaky on linux.
#define MAYBE_OriginTrialEnabled_ImpressionRegistered \
  DISABLED_OriginTrialEnabled_ImpressionRegistered
#else
#define MAYBE_OriginTrialEnabled_ImpressionRegistered \
  OriginTrialEnabled_ImpressionRegistered
#endif

IN_PROC_BROWSER_TEST_F(AttributionsOriginTrialBrowserTest,
                       MAYBE_OriginTrialEnabled_ImpressionRegistered) {
  EXPECT_TRUE(NavigateToURL(
      shell(), GURL("https://example.test/impression_with_origin_trial.html")));

  EXPECT_TRUE(ExecJs(shell(), R"(
    createImpressionTag({id: 'link',
                        url: 'https://example.test/page_with_conversion_redirect.html',
                        data: '1',
                        destination: 'https://example.test/'});)"));

  TestNavigationObserver observer(web_contents());
  EXPECT_TRUE(ExecJs(shell(), "simulateClick('link');"));
  observer.Wait();

  AttributionManagerImpl* attribution_manager =
      static_cast<StoragePartitionImpl*>(
          web_contents()->GetBrowserContext()->GetDefaultStoragePartition())
          ->GetAttributionManager();

  base::RunLoop run_loop;

  // Verify we have received and logged an impression for the origin trial.
  attribution_manager->GetActiveSourcesForWebUI(base::BindLambdaForTesting(
      [&](std::vector<StoredSource> impressions) -> void {
        EXPECT_EQ(1u, impressions.size());
        run_loop.Quit();
      }));
  run_loop.Run();
}

// TODO(johnidel): Add tests that exercise the conversion side logic as well.
// This requires also using an embedded test server because the
// UrlLoadInterceptor cannot properly redirect the conversion pings.

class AttributionsOriginTrialNoBrowserFeatureBrowserTest
    : public AttributionsOriginTrialBrowserTest {
 public:
  AttributionsOriginTrialNoBrowserFeatureBrowserTest() {
    feature_list_.InitAndDisableFeature(
        blink::features::kConversionMeasurement);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(AttributionsOriginTrialNoBrowserFeatureBrowserTest,
                       BrowserSideLogicNotEnabled_FeatureNotDetected) {
  EXPECT_TRUE(NavigateToURL(
      shell(), GURL("https://example.test/impression_with_origin_trial.html")));

  EXPECT_EQ(false, EvalJs(shell(),
                          "document.featurePolicy.features().includes('"
                          "attribution-reporting')"));
  EXPECT_EQ(true, EvalJs(shell(), "window.attributionReporting === undefined"));
}

}  // namespace content
