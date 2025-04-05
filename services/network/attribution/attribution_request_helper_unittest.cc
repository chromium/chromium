// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/attribution/attribution_request_helper.h"

#include <memory>
#include <string>
#include <vector>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "net/http/http_request_headers.h"
#include "net/http/structured_headers.h"
#include "net/url_request/url_request.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/attribution.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace network {
namespace {

using ::network::mojom::AttributionReportingEligibility;
using ::testing::IsEmpty;

constexpr char kAttributionReportingEligible[] =
    "Attribution-Reporting-Eligible";

constexpr char kAdAuctionRegistrationEligible[] =
    "Sec-Ad-Auction-Event-Recording-Eligible";

TEST(AttributionRequestHelperTest, SetAttributionReportingHeaders) {
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

TEST(AttributionRequestHelperTest, SetAttributionReportingSupportHeaders) {
  const struct {
    mojom::AttributionSupport support;
  } kTestCases[] = {
      {mojom::AttributionSupport::kWeb},
      {mojom::AttributionSupport::kWebAndOs},
      {mojom::AttributionSupport::kOs},
  };

  for (const auto& test_case : kTestCases) {
    base::HistogramTester histograms;
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

    histograms.ExpectBucketCount("Conversions.RequestSupportHeader",
                                 test_case.support,
                                 /*expected_count=*/1);
  }
}

TEST(AttributionRequestHelperTest, SetAdAuctionRegistrationEligibleHeaders) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kAdAuctionEventRegistration);
  {
    ResourceRequest resource_request;
    resource_request.attribution_reporting_eligibility =
        AttributionReportingEligibility::kUnset;
    net::HttpRequestHeaders headers =
        ComputeAttributionReportingHeaders(resource_request);

    EXPECT_FALSE(headers.HasHeader(kAdAuctionRegistrationEligible));
  }

  const struct {
    AttributionReportingEligibility eligibility;
    std::vector<std::string> required_keys;
    std::vector<std::string> prohibited_keys;
  } kTestCases[] = {
      {AttributionReportingEligibility::kEmpty, {}, {"view", "click"}},
      {AttributionReportingEligibility::kEventSource, {"view"}, {"click"}},
      {AttributionReportingEligibility::kNavigationSource, {"click"}, {"view"}},
      {AttributionReportingEligibility::kTrigger, {}, {"view", "click"}},
      {AttributionReportingEligibility::kEventSourceOrTrigger,
       {"view"},
       {"click"}},
  };

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(test_case.eligibility);

    ResourceRequest resource_request;
    resource_request.attribution_reporting_eligibility = test_case.eligibility;
    net::HttpRequestHeaders headers =
        ComputeAttributionReportingHeaders(resource_request);

    std::string actual = headers.GetHeader(kAdAuctionRegistrationEligible)
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

}  // namespace
}  // namespace network
