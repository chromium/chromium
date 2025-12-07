// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/integrity_policy_parser.h"

#include <optional>
#include <string>
#include <vector>

#include "base/test/scoped_feature_list.h"
#include "net/http/http_response_headers.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/integrity_policy.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace network {

namespace {

struct TestCase {
  std::optional<std::string> integrity_policy_header;

  std::vector<mojom::IntegrityPolicy_Destination> blocked_destinations;
  std::vector<mojom::IntegrityPolicy_Source> sources;
  std::vector<std::string> endpoints;
  std::vector<std::string> parsing_errors;
  IntegrityPolicyHeaderType type = IntegrityPolicyHeaderType::kEnforce;
};

}  // namespace

TEST(IntegrityPolicyParserTest, Parse) {
  base::test::ScopedFeatureList scoped_feature;
  scoped_feature.InitAndEnableFeature(features::kIntegrityPolicyScript);
  const auto kNoHeader = std::optional<std::string>();
  const auto kEmptyHeader = std::string();
  const auto kEmptyDestination =
      std::vector<mojom::IntegrityPolicy_Destination>();
  const auto kScriptDestination =
      std::vector<mojom::IntegrityPolicy_Destination>(
          {mojom::IntegrityPolicy_Destination::kScript});
  const auto kEmptySource = std::vector<mojom::IntegrityPolicy_Source>();
  const auto kInlineSource = std::vector<mojom::IntegrityPolicy_Source>(
      {mojom::IntegrityPolicy_Source::kInline});
  const auto kEmptyVectorString = std::vector<std::string>();

  TestCase test_cases[] = {
      {kNoHeader, kEmptyDestination, kEmptySource, kEmptyVectorString,
       kEmptyVectorString},
      {kEmptyHeader, kEmptyDestination, kEmptySource, kEmptyVectorString,
       kEmptyVectorString},
      {"not a dictionary",
       kEmptyDestination,
       kEmptySource,
       kEmptyVectorString,
       {"The Integrity-Policy value \"not a dictionary\" is not a "
        "dictionary."}},
      {"not a dictionary",
       kEmptyDestination,
       kEmptySource,
       kEmptyVectorString,
       {"The Integrity-Policy-Report-Only value \"not a dictionary\" is not a "
        "dictionary."},
       IntegrityPolicyHeaderType::kReportOnly},
      {"wrongkey=(something)",
       kEmptyDestination,
       kInlineSource,
       kEmptyVectorString,
       {"Unrecognized wrongkey in Integrity-Policy header."}},
      {"wrongkey=(something), blocked-destinations=(script)",
       kScriptDestination,
       kInlineSource,
       kEmptyVectorString,
       {"Unrecognized wrongkey in Integrity-Policy header."}},
      {"wrongkey=(something), blocked-destinations=(wrongdestination script)",
       kScriptDestination,
       kInlineSource,
       kEmptyVectorString,
       {"Unrecognized wrongkey in Integrity-Policy header.",
        "The Integrity-Policy destination 'wrongdestination' is not "
        "supported."}},
      {"blocked-destinations=(unsupported)",
       kEmptyDestination,
       kInlineSource,
       kEmptyVectorString,
       {"The Integrity-Policy destination 'unsupported' is not supported."}},
      {"sources=(inline)", kEmptyDestination, kInlineSource, kEmptyVectorString,
       kEmptyVectorString},
      {"blocked-destinations=(script)", kScriptDestination, kInlineSource,
       kEmptyVectorString, kEmptyVectorString},
      {"blocked-destinations=(script), sources=(inline)", kScriptDestination,
       kInlineSource, kEmptyVectorString, kEmptyVectorString},
      {"blocked-destinations=(script), sources=(inline other)",
       kScriptDestination,
       kInlineSource,
       kEmptyVectorString,
       {"The Integrity-Policy source 'other' is not supported."}},
      {"blocked-destinations=(script), sources=(other)",
       kScriptDestination,
       kEmptySource,
       kEmptyVectorString,
       {"The Integrity-Policy source 'other' is not supported."}},
      {"endpoints=(https://example.com/api)",
       kEmptyDestination,
       kInlineSource,
       {"https://example.com/api"},
       kEmptyVectorString},
      {"sources=(inline), blocked-destinations=(script), "
       "endpoints=(https://example.com/api)",
       kScriptDestination,
       kInlineSource,
       {"https://example.com/api"},
       kEmptyVectorString},
      {"blocked-destinations=(script), endpoints=(endpoint1 endpoint2)",
       kScriptDestination,
       kInlineSource,
       {"endpoint1", "endpoint2"},
       kEmptyVectorString},
      {"blocked-destinations=(script), endpoints=(invalid1, invalid2)",
       kEmptyDestination,
       kEmptySource,
       kEmptyVectorString,
       {"The Integrity-Policy value \"blocked-destinations=(script), "
        "endpoints=(invalid1, invalid2)\" is not a dictionary."}},
      {"sources=(other), blocked-destinations=(script)",
       kScriptDestination,
       kEmptySource,
       kEmptyVectorString,
       {"The Integrity-Policy source 'other' is not supported."}},
      {"sources=(inline), blocked-destinations=[], endpoints=[]",
       kEmptyDestination,
       kEmptySource,
       kEmptyVectorString,
       {"The Integrity-Policy value \"sources=(inline), "
        "blocked-destinations=[], endpoints=[]\" is not a dictionary."}},

  };

  for (const TestCase& test_case : test_cases) {
    auto headers =
        base::MakeRefCounted<net::HttpResponseHeaders>("HTTP/1.1 200 OK\n");
    std::string header_value;
    if (test_case.integrity_policy_header) {
      if (test_case.type == IntegrityPolicyHeaderType::kEnforce) {
        headers->AddHeader("Integrity-Policy",
                           *test_case.integrity_policy_header);
      } else {
        headers->AddHeader("Integrity-Policy-Report-Only",
                           *test_case.integrity_policy_header);
      }
      header_value += *test_case.integrity_policy_header;
    }
    auto message = testing::Message() << "header: " << header_value;

    SCOPED_TRACE(message);
    const auto policy =
        ParseIntegrityPolicyFromHeaders(*headers, test_case.type);

    EXPECT_EQ(policy.blocked_destinations, test_case.blocked_destinations);
    EXPECT_EQ(policy.sources, test_case.sources);
    EXPECT_EQ(policy.endpoints, test_case.endpoints);
    EXPECT_EQ(policy.parsing_errors, test_case.parsing_errors);
  }
}

}  // namespace network
