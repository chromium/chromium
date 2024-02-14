// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/strings/strcat.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "components/attribution_reporting/features.h"
#include "content/browser/fenced_frame/fenced_document_data.h"
#include "content/common/features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/url_loader_interceptor.h"
#include "content/shell/browser/shell.h"
#include "services/network/public/cpp/attribution_reporting_runtime_features.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/mojom/attribution.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace content {

namespace {
constexpr char kBaseDataDir[] = "content/test/data/attribution_reporting/";

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

  // Helper function that returns whether the ARA cross app web feature is set
  // on the browser-side.
  bool CheckBrowserSideCrossAppWebRuntimeFeature() {
    RenderFrameHost* rfh = shell()->web_contents()->GetPrimaryMainFrame();
    return FencedDocumentData::GetForCurrentDocument(rfh)->features().Has(
        network::AttributionReportingRuntimeFeature::kCrossAppWeb);
  }

 protected:
  network::mojom::AttributionReportingEligibility
      last_request_attribution_reporting_eligibility_;

 private:
  std::unique_ptr<URLLoaderInterceptor> url_loader_interceptor_;
};

class CrossAppWebAttributionEnabledOriginTrialBrowserTest
    : public CrossAppWebAttributionBrowserTestBase {
 public:
  CrossAppWebAttributionEnabledOriginTrialBrowserTest() {
    feature_list_.InitWithFeatures(
        /*enabled_features=*/{network::features::
                                  kAttributionReportingCrossAppWeb},
        /*disabled_features=*/{
            features::kAttributionReportingCrossAppWebOverride});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(CrossAppWebAttributionEnabledOriginTrialBrowserTest,
                       OriginTrialEnabled_FeatureDetected) {
  EXPECT_TRUE(NavigateToURL(
      shell(), GURL("https://example.test/page_with_cross_app_web_ot.html")));

  EXPECT_EQ(true, EvalJs(shell(),
                         "document.featurePolicy.features().includes('"
                         "attribution-reporting')"));

  EXPECT_TRUE(CheckBrowserSideCrossAppWebRuntimeFeature());
}

IN_PROC_BROWSER_TEST_F(CrossAppWebAttributionEnabledOriginTrialBrowserTest,
                       OriginTrialDisabled_FeatureNotDetected) {
  // Navigate to a page without an OT token.
  EXPECT_TRUE(NavigateToURL(
      shell(),
      GURL("https://example.test/page_without_cross_app_web_ot.html")));

  EXPECT_EQ(false, EvalJs(shell(),
                          "document.featurePolicy.features().includes('"
                          "attribution-reporting')"));

  EXPECT_FALSE(CheckBrowserSideCrossAppWebRuntimeFeature());
}

IN_PROC_BROWSER_TEST_F(CrossAppWebAttributionEnabledOriginTrialBrowserTest,
                       OriginTrialEnabledDynamically) {
  // Navigate to a page without an OT token.
  EXPECT_TRUE(NavigateToURL(
      shell(),
      GURL("https://example.test/page_without_cross_app_web_ot.html")));

  EXPECT_EQ(false, EvalJs(shell(),
                          "document.featurePolicy.features().includes('"
                          "attribution-reporting')"));

  EXPECT_FALSE(CheckBrowserSideCrossAppWebRuntimeFeature());

  // The document appends a new OT token into the DOM:
  // The token was generated with this command:
  // ```
  // generate_token.py \
  //   https://example.test \
  //   AttributionReportingCrossAppWeb \
  //   --expire-timestamp=2000000000
  // ```
  ASSERT_TRUE(ExecJs(shell(), R"(
      const otMeta = document.createElement('meta');
      otMeta.httpEquiv = 'origin-trial';
      otMeta.content = 'A9BaOy042ycDxXs05VDyRk0Watjk61gX/oPt1FBpibFw01QErvfz9' +
          'HFyeFoWmUgo9TTs4zX24sJUlIfMtK3xiwwAAABqeyJvcmlnaW4iOiAiaHR0cHM6Ly9' +
          'leGFtcGxlLnRlc3Q6NDQzIiwgImZlYXR1cmUiOiAiQXR0cmlidXRpb25SZXBvcnRpb' +
          'mdDcm9zc0FwcFdlYiIsICJleHBpcnkiOiAyMDAwMDAwMDAwfQ==';
      document.head.append(otMeta);
    )"));
  EXPECT_EQ(true, EvalJs(shell(),
                         "document.featurePolicy.features().includes('"
                         "attribution-reporting')"));
  EXPECT_TRUE(CheckBrowserSideCrossAppWebRuntimeFeature());
}

IN_PROC_BROWSER_TEST_F(CrossAppWebAttributionEnabledOriginTrialBrowserTest,
                       OriginTrialEnabledByResponseHeader) {
  // Navigate to a page with an OT token in the response header.
  EXPECT_TRUE(NavigateToURL(
      shell(), GURL("https://example.test/"
                    "page_with_cross_app_web_ot_in_resp_header.html")));

  EXPECT_EQ(true, EvalJs(shell(),
                         "document.featurePolicy.features().includes('"
                         "attribution-reporting')"));
  EXPECT_TRUE(CheckBrowserSideCrossAppWebRuntimeFeature());
}

IN_PROC_BROWSER_TEST_F(CrossAppWebAttributionEnabledOriginTrialBrowserTest,
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

IN_PROC_BROWSER_TEST_F(CrossAppWebAttributionEnabledOriginTrialBrowserTest,
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

IN_PROC_BROWSER_TEST_F(CrossAppWebAttributionEnabledOriginTrialBrowserTest,
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

  EXPECT_TRUE(CheckBrowserSideCrossAppWebRuntimeFeature());
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

struct OverrideTestCase {
  bool conversion_measurement_enabled;
  bool attribution_reporting_cross_app_web_enabled;
  bool expected;
};

class CrossAppWebAttributionOverrideBrowserTest
    : public CrossAppWebAttributionBrowserTestBase,
      public ::testing::WithParamInterface<OverrideTestCase> {
 public:
  CrossAppWebAttributionOverrideBrowserTest() {
    std::vector<base::test::FeatureRef> enabled_features;
    std::vector<base::test::FeatureRef> disabled_features;

    enabled_features.emplace_back(
        features::kAttributionReportingCrossAppWebOverride);

    if (GetParam().conversion_measurement_enabled) {
      enabled_features.emplace_back(
          attribution_reporting::features::kConversionMeasurement);
    } else {
      disabled_features.emplace_back(
          attribution_reporting::features::kConversionMeasurement);
    }

    if (GetParam().attribution_reporting_cross_app_web_enabled) {
      enabled_features.emplace_back(
          network::features::kAttributionReportingCrossAppWeb);
    } else {
      disabled_features.emplace_back(
          network::features::kAttributionReportingCrossAppWeb);
    }

    feature_list_.InitWithFeatures(enabled_features, disabled_features);
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
};

INSTANTIATE_TEST_SUITE_P(
    All,
    CrossAppWebAttributionOverrideBrowserTest,
    ::testing::Values(
        OverrideTestCase{
            .conversion_measurement_enabled = true,
            .attribution_reporting_cross_app_web_enabled = true,
            .expected = true,
        },
        OverrideTestCase{
            .conversion_measurement_enabled = false,
            .attribution_reporting_cross_app_web_enabled = true,
            .expected = false,
        },
        OverrideTestCase{
            .conversion_measurement_enabled = true,
            .attribution_reporting_cross_app_web_enabled = false,
            .expected = false,
        },
        OverrideTestCase{
            .conversion_measurement_enabled = false,
            .attribution_reporting_cross_app_web_enabled = false,
            .expected = false,
        }));

IN_PROC_BROWSER_TEST_P(CrossAppWebAttributionOverrideBrowserTest, NoOT) {
  // Navigate to a page without an OT token.
  ASSERT_TRUE(NavigateToURL(
      shell(),
      GURL("https://example.test/page_without_cross_app_web_ot.html")));

  EXPECT_EQ(GetParam().expected,
            EvalJs(shell(),
                   "document.featurePolicy.features().includes('"
                   "attribution-reporting')"));

  EXPECT_EQ(GetParam().expected, CheckBrowserSideCrossAppWebRuntimeFeature());
}

}  // namespace
}  // namespace content
