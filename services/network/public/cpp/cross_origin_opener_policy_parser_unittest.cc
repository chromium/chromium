// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/cross_origin_opener_policy_parser.h"

#include <string>
#include <vector>

#include "base/test/scoped_feature_list.h"
#include "net/http/http_response_headers.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/mojom/cross_origin_embedder_policy.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace network {

TEST(CrossOriginOpenerPolicyTest, Parse) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kCrossOriginOpenerPolicy);

  using mojom::CrossOriginOpenerPolicyValue;
  constexpr auto kSameOrigin = CrossOriginOpenerPolicyValue::kSameOrigin;
  constexpr auto kSameOriginAllowPopups =
      CrossOriginOpenerPolicyValue::kSameOriginAllowPopups;
  constexpr auto kUnsafeNone = CrossOriginOpenerPolicyValue::kUnsafeNone;
  constexpr auto kSameOriginPlusCoep =
      CrossOriginOpenerPolicyValue::kSameOriginPlusCoep;
  constexpr auto kCoepNone = mojom::CrossOriginEmbedderPolicyValue::kNone;
  constexpr auto kCoepCorp =
      mojom::CrossOriginEmbedderPolicyValue::kRequireCorp;

  const auto kNoHeader = base::Optional<std::string>();
  const auto kNoEndpoint = kNoHeader;

  const struct {
    base::Optional<std::string> raw_coop_string;
    mojom::CrossOriginEmbedderPolicyValue coep_value;
    base::Optional<std::string> raw_coop_report_only_string;
    mojom::CrossOriginEmbedderPolicyValue coep_report_only_value;
    base::Optional<std::string> expected_endpoint;
    CrossOriginOpenerPolicyValue expected_policy;
    base::Optional<std::string> expected_endpoit_report_only;
    CrossOriginOpenerPolicyValue expected_policy_report_only;
  } kTestCases[] = {
      {"same-origin", kCoepNone, kNoHeader, kCoepNone, kNoEndpoint, kSameOrigin,
       kNoEndpoint, kUnsafeNone},
      {"same-origin-allow-popups", kCoepNone, kNoHeader, kCoepNone, kNoEndpoint,
       kSameOriginAllowPopups, kNoEndpoint, kUnsafeNone},
      {"unsafe-none", kCoepNone, kNoHeader, kCoepNone, kNoEndpoint, kUnsafeNone,
       kNoEndpoint, kUnsafeNone},
      // Leading whitespaces.
      {"   same-origin", kCoepNone, kNoHeader, kCoepNone, kNoEndpoint,
       kSameOrigin, kNoEndpoint, kUnsafeNone},
      // Leading character tabulation.
      {"\tsame-origin", kCoepNone, kNoHeader, kCoepNone, kNoEndpoint,
       kSameOrigin, kNoEndpoint, kUnsafeNone},
      // Trailing whitespaces.
      {"same-origin-allow-popups   ", kCoepNone, kNoHeader, kCoepNone,
       kNoEndpoint, kSameOriginAllowPopups, kNoEndpoint, kUnsafeNone},
      // Empty string.
      {"", kCoepNone, kNoHeader, kCoepNone, kNoEndpoint, kUnsafeNone,
       kNoEndpoint, kUnsafeNone},
      // Only whitespaces.
      {"   ", kCoepNone, kNoHeader, kCoepNone, kNoEndpoint, kUnsafeNone,
       kNoEndpoint, kUnsafeNone},
      // Invalid same-site value
      {"same-site", kCoepNone, kNoHeader, kCoepNone, kNoEndpoint, kUnsafeNone,
       kNoEndpoint, kUnsafeNone},
      // Misspelling.
      {"some-origin", kCoepNone, kNoHeader, kCoepNone, kNoEndpoint, kUnsafeNone,
       kNoEndpoint, kUnsafeNone},
      // Trailing line-tab.
      {"same-origin\x0B", kCoepNone, kNoHeader, kCoepNone, kNoEndpoint,
       kUnsafeNone, kNoEndpoint, kUnsafeNone},
      // Adding report endpoint.
      {"same-origin; report-to=\"endpoint\"", kCoepNone, kNoHeader, kCoepNone,
       "endpoint", kSameOrigin, kNoEndpoint, kUnsafeNone},
      // Extraneous parameter, ignored.
      {"same-origin; report-to=\"endpoint\"; foo=bar", kCoepNone, kNoHeader,
       kCoepNone, "endpoint", kSameOrigin, kNoEndpoint, kUnsafeNone},
      // Multiple endpoints
      {"same-origin; report-to=\"endpoint\"; report-to=\"foo\"", kCoepNone,
       kNoHeader, kCoepNone, "foo", kSameOrigin, kNoEndpoint, kUnsafeNone},
      // Leading spaces in the reporting endpoint.
      {"same-origin-allow-popups;    report-to=\"endpoint\"", kCoepNone,
       kNoHeader, kCoepNone, "endpoint", kSameOriginAllowPopups, kNoEndpoint,
       kUnsafeNone},
      // Unsafe-none with endpoint.
      {"unsafe-none; report-to=\"endpoint\"", kCoepNone, kNoHeader, kCoepNone,
       "endpoint", kUnsafeNone, kNoEndpoint, kUnsafeNone},
      // Unknown parameters should just be ignored.
      {"same-origin; invalidparameter=\"unknown\"", kCoepNone, kNoHeader,
       kCoepNone, kNoEndpoint, kSameOrigin, kNoEndpoint, kUnsafeNone},
      // Non-string report-to value.
      {"same-origin; report-to=other-endpoint", kCoepNone, kNoHeader, kCoepNone,
       kNoEndpoint, kSameOrigin, kNoEndpoint, kUnsafeNone},
      // Malformated parameter value.
      {"same-origin-allow-popups;   foo", kCoepNone, kNoHeader, kCoepNone,
       kNoEndpoint, kSameOriginAllowPopups, kNoEndpoint, kUnsafeNone},
      // Report to empty string endpoint.
      {"same-origin; report-to=\"\"", kCoepNone, kNoHeader, kCoepNone, "",
       kSameOrigin, kNoEndpoint, kUnsafeNone},
      // Empty parameter value, parsing fails.
      {"same-origin; report-to=", kCoepNone, kNoHeader, kCoepNone, kNoEndpoint,
       kUnsafeNone, kNoEndpoint, kUnsafeNone},
      // Empty parameter key, parsing fails.
      {"same-origin; =\"\"", kCoepNone, kNoHeader, kCoepNone, kNoEndpoint,
       kUnsafeNone, kNoEndpoint, kUnsafeNone},
      // Report only same origin header.
      {kNoHeader, kCoepNone, "same-origin", kCoepNone, kNoEndpoint, kUnsafeNone,
       kNoEndpoint, kSameOrigin},
      // Report only header.
      {kNoHeader, kCoepNone, "same-origin-allow-popups", kCoepNone, kNoEndpoint,
       kUnsafeNone, kNoEndpoint, kSameOriginAllowPopups},
      // Report only header.
      {kNoHeader, kCoepNone, "unsafe-none", kCoepNone, kNoEndpoint, kUnsafeNone,
       kNoEndpoint, kUnsafeNone},
      // Report only same origin header with endpoint.
      {kNoHeader, kCoepNone, "same-origin; report-to=\"endpoint\"", kCoepNone,
       kNoEndpoint, kUnsafeNone, "endpoint", kSameOrigin},
      // Report only allow popups with endpoint.
      {kNoHeader, kCoepNone, "same-origin-allow-popups; report-to=\"endpoint\"",
       kCoepNone, kNoEndpoint, kUnsafeNone, "endpoint", kSameOriginAllowPopups},
      // Report only unsafe none with endpoint.
      {kNoHeader, kCoepNone, "unsafe-none; report-to=\"endpoint\" ", kCoepNone,
       kNoEndpoint, kUnsafeNone, "endpoint", kUnsafeNone},
      // Invalid COOP header with valid COOP report only.
      {"INVALID HEADER", kCoepNone, "same-origin; report-to=\"endpoint\"",
       kCoepNone, kNoEndpoint, kUnsafeNone, "endpoint", kSameOrigin},
      // Unsafe none COOP and allow popups COOP report only.
      {"unsafe-none", kCoepNone,
       "same-origin-allow-popups; report-to=\"endpoint\"", kCoepNone,
       kNoEndpoint, kUnsafeNone, "endpoint", kSameOriginAllowPopups},
      // Same-origin-allow-popups coop + same-origin report-only.
      {"same-origin-allow-popups", kCoepNone,
       "same-origin; report-to=\"endpoint\" ", kCoepNone, kNoEndpoint,
       kSameOriginAllowPopups, "endpoint", kSameOrigin},
      // Same-origin-allow-popups coop + same-origin report-only, with reporting
      // on both.
      {"same-origin-allow-popups; report-to=\"endpointA\"", kCoepNone,
       "same-origin; report-to=\"endpointB\" ", kCoepNone, "endpointA",
       kSameOriginAllowPopups, "endpointB", kSameOrigin},
      // Same-origin-allow-popups coop + same-origin report-only, with reporting
      // on both, same endpoint.
      {"same-origin-allow-popups; report-to=\"endpoint\"", kCoepNone,
       "same-origin; report-to=\"endpoint\" ", kCoepNone, "endpoint",
       kSameOriginAllowPopups, "endpoint", kSameOrigin},
      // Unsafe coop + same-origin report-only, with reporting on both.
      {"unsafe-none; report-to=\"endpoint\"", kCoepNone,
       "same-origin; report-to=\"endpoint\" ", kCoepNone, "endpoint",
       kUnsafeNone, "endpoint", kSameOrigin},
      // Same-origin with reporting COOP, invalid COOP report-only.
      {"same-origin; report-to=\"endpoint\"", kCoepNone, "INVALID HEADER",
       kCoepNone, "endpoint", kSameOrigin, kNoEndpoint, kUnsafeNone},
      // Same-origin with COEP
      {"same-origin", kCoepCorp, kNoHeader, kCoepNone, kNoEndpoint,
       kSameOriginPlusCoep, kNoEndpoint, kUnsafeNone},
      // Same-origin-allow-popups with COEP
      {"same-origin-allow-popups", kCoepCorp, kNoHeader, kCoepNone, kNoEndpoint,
       kSameOriginAllowPopups, kNoEndpoint, kUnsafeNone},
      // Same-origin with report only COEP
      {"same-origin", kCoepNone, kNoHeader, kCoepCorp, kNoEndpoint, kSameOrigin,
       kNoEndpoint, kUnsafeNone},
      // reporting Same-origin with COEP
      {kNoHeader, kCoepCorp, "same-origin", kCoepNone, kNoEndpoint, kUnsafeNone,
       kNoEndpoint, kSameOriginPlusCoep},
      // reporting Same-origin with reporting COEP
      {kNoHeader, kCoepNone, "same-origin", kCoepCorp, kNoEndpoint, kUnsafeNone,
       kNoEndpoint, kSameOriginPlusCoep},
  };

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(testing::Message()
                 << std::endl
                 << "raw_coop_string = "
                 << (test_case.raw_coop_string ? *test_case.raw_coop_string
                                               : "No header")
                 << std::endl
                 << "raw_coop_report_only_string = "
                 << (test_case.raw_coop_report_only_string
                         ? *test_case.raw_coop_report_only_string
                         : "No header")
                 << std::endl);
    scoped_refptr<net::HttpResponseHeaders> headers(
        new net::HttpResponseHeaders("HTTP/1.1 200 OK"));
    if (test_case.raw_coop_string) {
      headers->AddHeader("cross-origin-Opener-policy",
                         *test_case.raw_coop_string);
    }
    if (test_case.raw_coop_report_only_string) {
      headers->AddHeader("cross-origin-opener-policy-report-only",
                         *test_case.raw_coop_report_only_string);
    }
    network::CrossOriginEmbedderPolicy coep;
    coep.value = test_case.coep_value;
    coep.report_only_value = test_case.coep_report_only_value;

    auto parsed_policy = ParseCrossOriginOpenerPolicy(*headers, coep);
    EXPECT_EQ(test_case.expected_endpoint, parsed_policy.reporting_endpoint);
    EXPECT_EQ(test_case.expected_policy, parsed_policy.value);
    EXPECT_EQ(test_case.expected_endpoit_report_only,
              parsed_policy.report_only_reporting_endpoint);
    EXPECT_EQ(test_case.expected_policy_report_only,
              parsed_policy.report_only_value);
  }
}

TEST(CrossOriginOpenerPolicyTest, Default) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kCrossOriginOpenerPolicy);
  network::CrossOriginEmbedderPolicy coep;

  // If no COOP header is specified:
  scoped_refptr<net::HttpResponseHeaders> headers(
      new net::HttpResponseHeaders("HTTP/1.1 200 OK"));

  // Then we have no policy enforced by default:
  network::CrossOriginOpenerPolicy parsed_policy =
      ParseCrossOriginOpenerPolicy(*headers, coep);
  EXPECT_EQ(base::nullopt, parsed_policy.reporting_endpoint);
  EXPECT_EQ(mojom::CrossOriginOpenerPolicyValue::kUnsafeNone,
            parsed_policy.value);
  EXPECT_EQ(base::nullopt, parsed_policy.report_only_reporting_endpoint);
  EXPECT_EQ(mojom::CrossOriginOpenerPolicyValue::kUnsafeNone,
            parsed_policy.report_only_value);
}

TEST(CrossOriginOpenerPolicyTest, DefaultWithCOOPByDefault) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {features::kCrossOriginOpenerPolicy,
       features::kCrossOriginOpenerPolicyByDefault},
      {});
  network::CrossOriginEmbedderPolicy coep;

  // If no COOP header is specified:
  scoped_refptr<net::HttpResponseHeaders> headers(
      new net::HttpResponseHeaders("HTTP/1.1 200 OK"));

  // Then we have `same-origin-allow-popups` as enforced by default, but no
  // policy reported on by default:
  network::CrossOriginOpenerPolicy parsed_policy =
      ParseCrossOriginOpenerPolicy(*headers, coep);
  EXPECT_EQ(base::nullopt, parsed_policy.reporting_endpoint);
  EXPECT_EQ(mojom::CrossOriginOpenerPolicyValue::kSameOriginAllowPopups,
            parsed_policy.value);
  EXPECT_EQ(base::nullopt, parsed_policy.report_only_reporting_endpoint);
  EXPECT_EQ(mojom::CrossOriginOpenerPolicyValue::kUnsafeNone,
            parsed_policy.report_only_value);
}

}  // namespace network
