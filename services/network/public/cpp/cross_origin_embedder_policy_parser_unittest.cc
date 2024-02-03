// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/cross_origin_embedder_policy_parser.h"

#include <optional>
#include <string>
#include <vector>

#include "net/http/http_response_headers.h"
#include "services/network/public/mojom/cross_origin_embedder_policy.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace network {

namespace {

constexpr auto kNone = mojom::CrossOriginEmbedderPolicyValue::kNone;
constexpr auto kCredentialless =
    mojom::CrossOriginEmbedderPolicyValue::kCredentialless;
constexpr auto kRequireCorp =
    mojom::CrossOriginEmbedderPolicyValue::kRequireCorp;
const auto kNoHeader = std::optional<std::string>();
const auto kNoEndpoint = std::optional<std::string>();

struct TestCase {
  std::optional<std::string> coep_header;
  std::optional<std::string> coep_report_only_header;

  mojom::CrossOriginEmbedderPolicyValue value;
  std::optional<std::string> reporting_endpoint;
  mojom::CrossOriginEmbedderPolicyValue report_only_value;
  std::optional<std::string> report_only_reporting_endpoint;
};

void CheckTestCase(const TestCase& test_case) {
  auto message = testing::Message()
                 << "coep: " << test_case.coep_header.value_or("(missing)")
                 << ", coep-report-only: "
                 << test_case.coep_report_only_header.value_or("(missing)");
  SCOPED_TRACE(message);
  auto headers =
      base::MakeRefCounted<net::HttpResponseHeaders>("HTTP/1.1 200 OK\n");
  if (test_case.coep_header) {
    headers->AddHeader("cross-origin-embedder-policy", *test_case.coep_header);
  }
  if (test_case.coep_report_only_header) {
    headers->AddHeader("cross-origin-embedder-policy-report-only",
                       *test_case.coep_report_only_header);
  }
  const auto coep = ParseCrossOriginEmbedderPolicy(*headers);

  ASSERT_EQ(coep.value, test_case.value);
  ASSERT_EQ(coep.reporting_endpoint, test_case.reporting_endpoint);
  ASSERT_EQ(coep.report_only_value, test_case.report_only_value);
  ASSERT_EQ(coep.report_only_reporting_endpoint,
            test_case.report_only_reporting_endpoint);
}

}  // namespace

TEST(CrossOriginEmbedderPolicyTest, Parse) {
  TestCase test_cases[] = {
      // No headers
      {kNoHeader, kNoHeader, kNone, kNoEndpoint, kNone, kNoEndpoint},

      // COEP: none
      {"none", kNoHeader, kNone, kNoEndpoint, kNone, kNoEndpoint},
      // COEP-RO: none
      {kNoHeader, "none", kNone, kNoEndpoint, kNone, kNoEndpoint},
      // COEP: none with reporting endpoint
      {"none; report-to=\"endpoint\"", kNoHeader, kNone, kNoEndpoint, kNone,
       kNoEndpoint},
      // COEP-RO: none with reporting endpoint
      {kNoHeader, "none; report-to=\"endpoint\"", kNone, kNoEndpoint, kNone,
       kNoEndpoint},

      // COEP: require-corp
      {"require-corp", kNoHeader, kRequireCorp, kNoEndpoint, kNone,
       kNoEndpoint},

      // COEP: credentialless
      {"credentialless", kNoHeader, kCredentialless, kNoEndpoint, kNone,
       kNoEndpoint},

      // COEP-RO: require-corp
      {kNoHeader, "require-corp", kNone, kNoEndpoint, kRequireCorp,
       kNoEndpoint},

      // COEP-RO: credentialless
      {kNoHeader, "credentialless", kNone, kNoEndpoint, kCredentialless,
       kNoEndpoint},

      // COEP: require-corp with reporting-endpoint
      {"require-corp; report-to=\"endpoint\"", kNoHeader, kRequireCorp,
       "endpoint", kNone, kNoEndpoint},

      // COEP: credentialless with reporting endpoint
      {"credentialless; report-to=\"endpoint\"", kNoHeader, kCredentialless,
       "endpoint", kNone, kNoEndpoint},

      // COEP-RO: require-corp with reporting-endpoint
      {kNoHeader, "require-corp; report-to=\"endpoint\"", kNone, kNoEndpoint,
       kRequireCorp, "endpoint"},

      // COEP-RO: credentialless with reporting endpoint
      {kNoHeader, "credentialless; report-to=\"endpoint\"", kNone, kNoEndpoint,
       kCredentialless, "endpoint"},

      // COEP:require-corp, with both headers
      {"require-corp; report-to=\"endpoint1\"",
       "require-corp; report-to=\"endpoint2\"", kRequireCorp, "endpoint1",
       kRequireCorp, "endpoint2"},

      // COEP:credentialless, with both headers
      {"credentialless; report-to=\"endpoint1\"",
       "credentialless; report-to=\"endpoint2\"", kCredentialless, "endpoint1",
       kCredentialless, "endpoint2"},

      // With random spaces
      {"  require-corp; report-to=\"endpoint1\"  ",
       " require-corp; report-to=\"endpoint2\"", kRequireCorp, "endpoint1",
       kRequireCorp, "endpoint2"},

      // With unrelated params
      {"require-corp; foo; report-to=\"x\"; bar=piyo", kNoHeader, kRequireCorp,
       "x", kNone, kNoEndpoint},

      // With duplicate reporting endpoints
      {"require-corp; report-to=\"x\"; report-to=\"y\"", kNoHeader,
       kRequireCorp, "y", kNone, kNoEndpoint},

      // Errors
      {"REQUIRE-CORP", kNoHeader, kNone, kNoEndpoint, kNone, kNoEndpoint},
      {"CREDENTIALLESS", kNoHeader, kNone, kNoEndpoint, kNone, kNoEndpoint},
      {" require-corp; REPORT-TO=\"endpoint\"", kNoHeader, kNone, kNoEndpoint,
       kNone, kNoEndpoint},
      {"foobar; report-to=\"endpoint\"", kNoHeader, kNone, kNoEndpoint, kNone,
       kNoEndpoint},
      {"\"require-corp\"", kNoHeader, kNone, kNoEndpoint, kNone, kNoEndpoint},
      {"require-corp; report-to=endpoint", kNoHeader, kRequireCorp, kNoEndpoint,
       kNone, kNoEndpoint},
      {"require-corp; report-to=3", kNoHeader, kRequireCorp, kNoEndpoint, kNone,
       kNoEndpoint},
      {"require-corp; report-to", kNoHeader, kRequireCorp, kNoEndpoint, kNone,
       kNoEndpoint},
      {"require-corp; report-to=\"x\"", "TOTALLY BLOKEN", kRequireCorp, "x",
       kNone, kNoEndpoint},
      {"TOTALLY BLOKEN", "require-corp; report-to=\"x\"", kNone, kNoEndpoint,
       kRequireCorp, "x"},
  };

  for (const TestCase& test_case : test_cases)
    CheckTestCase(test_case);
}

}  // namespace network
