// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>
#include <vector>

#include "base/location.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_timeouts.h"
#include "components/attribution_reporting/features.h"
#include "content/common/features.h"
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
constexpr char kBaseDataDir[] = "content/test/data/";

struct TestCase {
  bool conversion_measurement_enabled;
  bool attribution_reporting_cross_app_web_enabled;
  bool expected;
};

class CrossAppWebAttributionBrowserTest
    : public ContentBrowserTest,
      public ::testing::WithParamInterface<TestCase> {
 public:
  CrossAppWebAttributionBrowserTest() {
    std::vector<base::test::FeatureRef> enabled_features;
    std::vector<base::test::FeatureRef> disabled_features(
        {features::kPrivacySandboxAdsAPIsM1Override});

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
  std::optional<network::mojom::AttributionReportingEligibility>
      last_request_attribution_reporting_eligibility_;

 private:
  std::unique_ptr<URLLoaderInterceptor> url_loader_interceptor_;
  base::test::ScopedFeatureList feature_list_;
};

INSTANTIATE_TEST_SUITE_P(
    All,
    CrossAppWebAttributionBrowserTest,
    ::testing::Values(
        TestCase{
            .conversion_measurement_enabled = true,
            .attribution_reporting_cross_app_web_enabled = true,
            .expected = true,
        },
        TestCase{
            .conversion_measurement_enabled = false,
            .attribution_reporting_cross_app_web_enabled = true,
            .expected = false,
        },
        TestCase{
            .conversion_measurement_enabled = true,
            .attribution_reporting_cross_app_web_enabled = false,
            .expected = false,
        },
        TestCase{
            .conversion_measurement_enabled = false,
            .attribution_reporting_cross_app_web_enabled = false,
            .expected = false,
        }));

IN_PROC_BROWSER_TEST_P(CrossAppWebAttributionBrowserTest, NoOT) {
  // Navigate to a page without an OT token.
  ASSERT_TRUE(NavigateToURL(shell(), GURL("https://example.test/title1.html")));

  const bool expected_enabled = GetParam().expected;

  EXPECT_EQ(expected_enabled,
            EvalJs(shell(),
                   "document.featurePolicy.features().includes('"
                   "attribution-reporting')"));

  last_request_attribution_reporting_eligibility_.reset();

  ASSERT_TRUE(ExecJs(shell()->web_contents(),
                     content::JsReplace(
                         R"(const img = document.createElement('img');
                    img.src = $1;
                    img.attributionSrc = '';)",
                         GURL("https://example.test/title1.html"))));

  while (!last_request_attribution_reporting_eligibility_.has_value()) {
    base::RunLoop run_loop;
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(), TestTimeouts::tiny_timeout());
    run_loop.Run();
  }

  EXPECT_EQ(expected_enabled,
            last_request_attribution_reporting_eligibility_.value() !=
                network::mojom::AttributionReportingEligibility::kUnset);
}

}  // namespace
}  // namespace content
