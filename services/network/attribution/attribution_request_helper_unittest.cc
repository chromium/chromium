// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/attribution/attribution_request_helper.h"

#include <memory>
#include <string>
#include <vector>

#include "base/functional/bind.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "net/http/http_request_headers.h"
#include "net/http/structured_headers.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/redirect_info.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_builder.h"
#include "net/url_request/url_request_test_util.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/attribution.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace network {
namespace {

using ::network::mojom::AttributionReportingEligibility;
using ::testing::IsEmpty;

constexpr char kAttributionReportingEligible[] =
    "Attribution-Reporting-Eligible";

class AttributionRequestHelperTest : public testing::Test {
 protected:
  void SetUp() override {
    helper_ = AttributionRequestHelper::CreateForTesting();

    context_ = net::CreateTestURLRequestContextBuilder()->Build();
  }

  std::unique_ptr<net::URLRequest> CreateTestUrlRequestFrom(
      const GURL& to_url,
      const GURL& from_url) {
    auto request = CreateTestUrlRequest(to_url);
    request->set_isolation_info(net::IsolationInfo::CreateForInternalRequest(
        url::Origin::Create(from_url)));

    return request;
  }

  std::unique_ptr<net::URLRequest> CreateTestUrlRequest(const GURL& to_url) {
    auto request =
        context_->CreateRequest(to_url, net::RequestPriority::DEFAULT_PRIORITY,
                                &delegate_, TRAFFIC_ANNOTATION_FOR_TESTS);

    return request;
  }

  net::RedirectInfo CreateRedirectInfo(const net::URLRequest& request,
                                       const GURL& to_url) {
    // We only care about the `new_location`, other properties are set to look
    // valid but are of no importance to the tests using the helper method.
    return net::RedirectInfo::ComputeRedirectInfo(
        /*original_method=*/request.method(), /*original_url=*/request.url(),
        /*original_site_for_cookies=*/request.site_for_cookies(),
        /*original_first_party_url_policy=*/
        net::RedirectInfo::FirstPartyURLPolicy::UPDATE_URL_ON_REDIRECT,
        /*original_referrer_policy=*/request.referrer_policy(),
        /*original_referrer=*/request.referrer(),
        /*http_status_code=*/net::HTTP_FOUND,
        /*new_location=*/to_url, /*referrer_policy_header=*/std::nullopt,
        /*insecure_scheme_was_upgraded=*/false, /*copy_fragment=*/false,
        /*is_signed_exchange_fallback_redirect=*/false);
  }

  mojom::URLResponseHeadPtr CreateResponse() {
    auto response = mojom::URLResponseHead::New();
    response->response_time = base::Time::Now();

    return response;
  }

  void RunBeginWith(net::URLRequest& request) {
    base::RunLoop run_loop;
    helper_->Begin(request, run_loop.QuitClosure());
    run_loop.Run();
  }

  void RunRedirectWith(net::URLRequest& request,
                       mojom::URLResponseHeadPtr response,
                       const ::net::RedirectInfo& redirect_info) {
    base::RunLoop run_loop;
    auto expected_response_time = response->response_time;
    helper_->OnReceiveRedirect(
        request, std::move(response), redirect_info,
        base::BindLambdaForTesting([&](mojom::URLResponseHeadPtr response) {
          EXPECT_EQ(expected_response_time, response->response_time);
          run_loop.Quit();
        }));
    run_loop.Run();
  }

  void RunFinalizeWith(mojom::URLResponseHead& response) {
    base::RunLoop run_loop;
    helper_->Finalize(response, run_loop.QuitClosure());
    run_loop.Run();
  }

  base::test::TaskEnvironment task_environment_;
  base::HistogramTester histograms_;
  std::unique_ptr<AttributionRequestHelper> helper_;
  GURL example_valid_request_url_ =
      GURL("https://reporting-origin.example/test/path/#123");

 private:
  std::unique_ptr<net::URLRequestContext> context_;
  net::TestDelegate delegate_;
};

TEST_F(AttributionRequestHelperTest, BeginRedirectAndFinalize_NoOp) {
  std::unique_ptr<net::URLRequest> request = CreateTestUrlRequestFrom(
      /*to_url=*/example_valid_request_url_,
      /*from_url=*/GURL("https://origin.example/path/123#foo"));

  RunBeginWith(*request);
  RunRedirectWith(
      *request, CreateResponse(),
      CreateRedirectInfo(*request, /*to_url=*/example_valid_request_url_));
  RunFinalizeWith(*CreateResponse());

  // This test checks that callbacks are properly called.
}

TEST_F(AttributionRequestHelperTest, CreateIfNeeded) {
  const struct {
    AttributionReportingEligibility eligibility;
    bool expect_instance_to_be_created;
  } kTestCases[] = {
      {AttributionReportingEligibility::kUnset, false},
      {AttributionReportingEligibility::kEmpty, false},
      {AttributionReportingEligibility::kEventSource, false},
      {AttributionReportingEligibility::kNavigationSource, false},
      {AttributionReportingEligibility::kTrigger, false},
      {AttributionReportingEligibility::kEventSourceOrTrigger, false},
  };

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(test_case.eligibility);

    auto instance =
        AttributionRequestHelper::CreateIfNeeded(test_case.eligibility);
    bool instance_created = !!instance;
    EXPECT_EQ(instance_created, test_case.expect_instance_to_be_created);
  }
}

TEST_F(AttributionRequestHelperTest, SetAttributionReportingHeaders) {
  {
    ResourceRequest resource_request;
    resource_request.attribution_reporting_eligibility =
        AttributionReportingEligibility::kUnset;
    net::HttpRequestHeaders headers =
        ComputeAttributionReportingHeaders(resource_request);

    EXPECT_FALSE(headers.HasHeader(kAttributionReportingEligible));
  }

  const struct {
    AttributionReportingEligibility eligibility;
    std::vector<std::string> required_keys;
    std::vector<std::string> prohibited_keys;
  } kTestCases[] = {
      {AttributionReportingEligibility::kEmpty,
       {},
       {"event-source", "navigation-source", "trigger"}},
      {AttributionReportingEligibility::kEventSource,
       {"event-source"},
       {"navigation-source", "trigger"}},
      {AttributionReportingEligibility::kNavigationSource,
       {"navigation-source"},
       {"event-source", "trigger"}},
      {AttributionReportingEligibility::kTrigger,
       {"trigger"},
       {"event-source", "navigation-source"}},
      {AttributionReportingEligibility::kEventSourceOrTrigger,
       {"event-source", "trigger"},
       {"navigation-source"}},
  };

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(test_case.eligibility);

    ResourceRequest resource_request;
    resource_request.attribution_reporting_eligibility = test_case.eligibility;
    net::HttpRequestHeaders headers =
        ComputeAttributionReportingHeaders(resource_request);

    std::string actual = headers.GetHeader(kAttributionReportingEligible)
                             .value_or(std::string());

    auto dict = net::structured_headers::ParseDictionary(actual);
    EXPECT_TRUE(dict.has_value());

    for (const auto& key : test_case.required_keys) {
      EXPECT_TRUE(dict->contains(key)) << key;
    }

    for (const auto& key : test_case.prohibited_keys) {
      EXPECT_FALSE(dict->contains(key)) << key;
    }
  }
}

class AttributionCrossAppWebRequestHelperTest
    : public AttributionRequestHelperTest {
 public:
  AttributionCrossAppWebRequestHelperTest() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{network::features::
                                  kAttributionReportingCrossAppWeb},
        /*disabled_features=*/{});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(AttributionCrossAppWebRequestHelperTest,
       SetAttributionReportingSupportHeaders) {
  const struct {
    mojom::AttributionSupport support;
  } kTestCases[] = {
      {mojom::AttributionSupport::kWeb},
      {mojom::AttributionSupport::kWebAndOs},
      {mojom::AttributionSupport::kOs},
  };

  for (const auto& test_case : kTestCases) {
    ResourceRequest resource_request;
    resource_request.attribution_reporting_eligibility =
        AttributionReportingEligibility::kEventSource;
    resource_request.attribution_reporting_support = test_case.support;
    net::HttpRequestHeaders headers =
        ComputeAttributionReportingHeaders(resource_request);

    std::string actual = headers.GetHeader(kAttributionReportingEligible)
                             .value_or(std::string());

    auto dict = net::structured_headers::ParseDictionary(actual);
    EXPECT_TRUE(dict.has_value());

    histograms_.ExpectBucketCount("Conversions.RequestSupportHeader",
                                  test_case.support,
                                  /*expected_count=*/1);
  }
}

}  // namespace
}  // namespace network
