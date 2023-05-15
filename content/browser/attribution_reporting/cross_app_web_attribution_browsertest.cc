// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/strcat.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/url_loader_interceptor.h"
#include "content/shell/browser/shell.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/mojom/attribution.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace content {

namespace {
constexpr char kBaseDataDir[] = "content/test/data/attribution_reporting/";
}  // namespace

class CrossAppWebAttributionBrowserTestBase : public ContentBrowserTest {
 public:
  CrossAppWebAttributionBrowserTestBase() = default;

  void SetUpOnMainThread() override {
    ContentBrowserTest::SetUpOnMainThread();

    // We use a URLLoaderInterceptor, rather than the EmbeddedTestServer, since
    // the origin trial token in the response is associated with a fixed
    // origin, whereas EmbeddedTestServer serves content on a random port.
    url_loader_interceptor_ =
        std::make_unique<URLLoaderInterceptor>(base::BindLambdaForTesting(
            [&](URLLoaderInterceptor::RequestParams* params) -> bool {
              last_request_attribution_reporting_eligibility_ =
                  params->url_request.attribution_reporting_eligibility;

              URLLoaderInterceptor::WriteResponse(
                  base::StrCat(
                      {kBaseDataDir, params->url_request.url.path_piece()}),
                  params->client.get());

              return true;
            }));
  }

  void TearDownOnMainThread() override { url_loader_interceptor_.reset(); }

 protected:
  network::mojom::AttributionReportingEligibility
      last_request_attribution_reporting_eligibility_;

 private:
  std::unique_ptr<URLLoaderInterceptor> url_loader_interceptor_;
};

class CrossAppWebAttributionEnabledBrowserTest
    : public CrossAppWebAttributionBrowserTestBase {
 public:
  CrossAppWebAttributionEnabledBrowserTest() {
    feature_list_.InitAndEnableFeature(
        network::features::kAttributionReportingCrossAppWeb);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(CrossAppWebAttributionEnabledBrowserTest,
                       OriginTrialEnabled_FeatureDetected) {
  EXPECT_TRUE(NavigateToURL(
      shell(), GURL("https://example.test/page_with_cross_app_web_ot.html")));

  EXPECT_EQ(true, EvalJs(shell(),
                         "document.featurePolicy.features().includes('"
                         "attribution-reporting')"));
}

IN_PROC_BROWSER_TEST_F(CrossAppWebAttributionEnabledBrowserTest,
                       OriginTrialDisabled_FeatureNotDetected) {
  // Navigate to a page without an OT token.
  EXPECT_TRUE(NavigateToURL(
      shell(),
      GURL("https://example.test/page_without_cross_app_web_ot.html")));

  EXPECT_EQ(false, EvalJs(shell(),
                          "document.featurePolicy.features().includes('"
                          "attribution-reporting')"));
}

IN_PROC_BROWSER_TEST_F(CrossAppWebAttributionEnabledBrowserTest,
                       OriginTrialEnabled_EligibilitySet) {
  EXPECT_TRUE(NavigateToURL(
      shell(), GURL("https://example.test/page_with_cross_app_web_ot.html")));

  ASSERT_TRUE(
      ExecJs(shell()->web_contents(),
             content::JsReplace(
                 R"(const img = document.createElement('img');
                    img.attributionSrc = $1;)",
                 GURL("https://example.test/register_source_headers.html"))));

  EXPECT_EQ(
      last_request_attribution_reporting_eligibility_,
      network::mojom::AttributionReportingEligibility::kEventSourceOrTrigger);
}

IN_PROC_BROWSER_TEST_F(CrossAppWebAttributionEnabledBrowserTest,
                       OriginTrialDisabled_EligibilityNotSet) {
  // Navigate to a page without an OT token.
  EXPECT_TRUE(NavigateToURL(
      shell(),
      GURL("https://example.test/page_without_cross_app_web_ot.html")));

  ASSERT_TRUE(
      ExecJs(shell()->web_contents(),
             content::JsReplace(
                 R"(const img = document.createElement('img');
                    img.src = $1;
                    img.attributionSrc = '';)",
                 GURL("https://example.test/register_source_headers.html"))));

  EXPECT_EQ(last_request_attribution_reporting_eligibility_,
            network::mojom::AttributionReportingEligibility::kUnset);
}

IN_PROC_BROWSER_TEST_F(CrossAppWebAttributionEnabledBrowserTest,
                       OriginTrialEnabledByThirdPartyToken_EligibilitySet) {
  EXPECT_TRUE(NavigateToURL(
      shell(),
      GURL("https://a.test/page_with_cross_app_web_third_party_ot.html")));

  EXPECT_EQ(true, EvalJs(shell(),
                         "document.featurePolicy.features().includes('"
                         "attribution-reporting')"));

  ASSERT_TRUE(ExecJs(shell()->web_contents(),
                     content::JsReplace(
                         R"(const img = document.createElement('img');
                            img.attributionSrc = $1;)",
                         GURL("https://a.test/register_source_headers.html"))));

  EXPECT_EQ(
      last_request_attribution_reporting_eligibility_,
      network::mojom::AttributionReportingEligibility::kEventSourceOrTrigger);
}

class CrossAppWebAttributionDisabledBrowserTest
    : public CrossAppWebAttributionBrowserTestBase {
 public:
  CrossAppWebAttributionDisabledBrowserTest() {
    feature_list_.InitAndDisableFeature(
        network::features::kAttributionReportingCrossAppWeb);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(CrossAppWebAttributionDisabledBrowserTest,
                       BaseFeatureDisabled_FeatureNotDetected) {
  EXPECT_TRUE(NavigateToURL(
      shell(), GURL("https://example.test/page_with_cross_app_web_ot.html")));

  EXPECT_EQ(false, EvalJs(shell(),
                          "document.featurePolicy.features().includes('"
                          "attribution-reporting')"));
}

}  // namespace content
