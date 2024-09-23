// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/document_isolation_policy_parser.h"

#include <optional>
#include <string>
#include <vector>

#include "net/http/http_response_headers.h"
#include "services/network/public/mojom/document_isolation_policy.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace network {

namespace {

constexpr auto kNone = mojom::DocumentIsolationPolicyValue::kNone;
constexpr auto kIsolateAndCredentialless =
    mojom::DocumentIsolationPolicyValue::kIsolateAndCredentialless;
constexpr auto kIsolateAndRequireCorp =
    mojom::DocumentIsolationPolicyValue::kIsolateAndRequireCorp;
const auto kNoHeader = std::optional<std::string>();
const auto kNoEndpoint = std::optional<std::string>();

struct TestCase {
  std::optional<std::string> dip_header;
  std::optional<std::string> dip_report_only_header;

  mojom::DocumentIsolationPolicyValue value;
  std::optional<std::string> reporting_endpoint;
  mojom::DocumentIsolationPolicyValue report_only_value;
  std::optional<std::string> report_only_reporting_endpoint;
};

void CheckTestCase(const TestCase& test_case) {
  auto message = testing::Message()
                 << "dip: " << test_case.dip_header.value_or("(missing)")
                 << ", dip-report-only: "
                 << test_case.dip_report_only_header.value_or("(missing)");
  SCOPED_TRACE(message);
  auto headers =
      base::MakeRefCounted<net::HttpResponseHeaders>("HTTP/1.1 200 OK\n");
  if (test_case.dip_header) {
    headers->AddHeader("document-isolation-policy", *test_case.dip_header);
  }
  if (test_case.dip_report_only_header) {
    headers->AddHeader("document-isolation-policy-report-only",
                       *test_case.dip_report_only_header);
  }
  const auto dip = ParseDocumentIsolationPolicy(*headers);

  ASSERT_EQ(dip.value, test_case.value);
  ASSERT_EQ(dip.reporting_endpoint, test_case.reporting_endpoint);
  ASSERT_EQ(dip.report_only_value, test_case.report_only_value);
  ASSERT_EQ(dip.report_only_reporting_endpoint,
            test_case.report_only_reporting_endpoint);
}

}  // namespace

TEST(DocumentIsolationPolicyTest, Parse) {
  TestCase test_cases[] = {
      // No headers
      {kNoHeader, kNoHeader, kNone, kNoEndpoint, kNone, kNoEndpoint},

      // DIP: none
      {"none", kNoHeader, kNone, kNoEndpoint, kNone, kNoEndpoint},
      // DIP-RO: none
      {kNoHeader, "none", kNone, kNoEndpoint, kNone, kNoEndpoint},
      // DIP: none with reporting endpoint
      {"none; report-to=\"endpoint\"", kNoHeader, kNone, kNoEndpoint, kNone,
       kNoEndpoint},
      // DIP-RO: none with reporting endpoint
      {kNoHeader, "none; report-to=\"endpoint\"", kNone, kNoEndpoint, kNone,
       kNoEndpoint},

      // DIP: isolate-and-require-corp
      {"isolate-and-require-corp", kNoHeader, kIsolateAndRequireCorp,
       kNoEndpoint, kNone, kNoEndpoint},

      // DIP: isolate-and-credentialless
      {"isolate-and-credentialless", kNoHeader, kIsolateAndCredentialless,
       kNoEndpoint, kNone, kNoEndpoint},

      // DIP-RO: isolate-and-require-corp
      {kNoHeader, "isolate-and-require-corp", kNone, kNoEndpoint,
       kIsolateAndRequireCorp, kNoEndpoint},

      // DIP-RO: isolate-and-credentialless
      {kNoHeader, "isolate-and-credentialless", kNone, kNoEndpoint,
       kIsolateAndCredentialless, kNoEndpoint},

      // DIP: isolate-and-require-corp with reporting-endpoint
      {"isolate-and-require-corp; report-to=\"endpoint\"", kNoHeader,
       kIsolateAndRequireCorp, "endpoint", kNone, kNoEndpoint},

      // DIP: isolate-and-credentialless with reporting endpoint
      {"isolate-and-credentialless; report-to=\"endpoint\"", kNoHeader,
       kIsolateAndCredentialless, "endpoint", kNone, kNoEndpoint},

      // DIP-RO: isolate-and-require-corp with reporting-endpoint
      {kNoHeader, "isolate-and-require-corp; report-to=\"endpoint\"", kNone,
       kNoEndpoint, kIsolateAndRequireCorp, "endpoint"},

      // DIP-RO: isolate-and-credentialless with reporting endpoint
      {kNoHeader, "isolate-and-credentialless; report-to=\"endpoint\"", kNone,
       kNoEndpoint, kIsolateAndCredentialless, "endpoint"},

      // DIP:isolate-and-require-corp, with both headers
      {"isolate-and-require-corp; report-to=\"endpoint1\"",
       "isolate-and-require-corp; report-to=\"endpoint2\"",
       kIsolateAndRequireCorp, "endpoint1", kIsolateAndRequireCorp,
       "endpoint2"},

      // DIP:isolate-and-credentialless, with both headers
      {"isolate-and-credentialless; report-to=\"endpoint1\"",
       "isolate-and-credentialless; report-to=\"endpoint2\"",
       kIsolateAndCredentialless, "endpoint1", kIsolateAndCredentialless,
       "endpoint2"},

      // With random spaces
      {"  isolate-and-require-corp; report-to=\"endpoint1\"  ",
       " isolate-and-require-corp; report-to=\"endpoint2\"",
       kIsolateAndRequireCorp, "endpoint1", kIsolateAndRequireCorp,
       "endpoint2"},

      // With unrelated params
      {"isolate-and-require-corp; foo; report-to=\"x\"; bar=piyo", kNoHeader,
       kIsolateAndRequireCorp, "x", kNone, kNoEndpoint},

      // With duplicate reporting endpoints
      {"isolate-and-require-corp; report-to=\"x\"; report-to=\"y\"", kNoHeader,
       kIsolateAndRequireCorp, "y", kNone, kNoEndpoint},

      // Errors
      {"REQUIRE-CORP", kNoHeader, kNone, kNoEndpoint, kNone, kNoEndpoint},
      {"CREDENTIALLESS", kNoHeader, kNone, kNoEndpoint, kNone, kNoEndpoint},
      {" isolate-and-require-corp; REPORT-TO=\"endpoint\"", kNoHeader, kNone,
       kNoEndpoint, kNone, kNoEndpoint},
      {"foobar; report-to=\"endpoint\"", kNoHeader, kNone, kNoEndpoint, kNone,
       kNoEndpoint},
      {"\"isolate-and-require-corp\"", kNoHeader, kNone, kNoEndpoint, kNone,
       kNoEndpoint},
      {"isolate-and-require-corp; report-to=endpoint", kNoHeader,
       kIsolateAndRequireCorp, kNoEndpoint, kNone, kNoEndpoint},
      {"isolate-and-require-corp; report-to=3", kNoHeader,
       kIsolateAndRequireCorp, kNoEndpoint, kNone, kNoEndpoint},
      {"isolate-and-require-corp; report-to", kNoHeader, kIsolateAndRequireCorp,
       kNoEndpoint, kNone, kNoEndpoint},
      {"isolate-and-require-corp; report-to=\"x\"", "TOTALLY BROKEN",
       kIsolateAndRequireCorp, "x", kNone, kNoEndpoint},
      {"TOTALLY BROKEN", "isolate-and-require-corp; report-to=\"x\"", kNone,
       kNoEndpoint, kIsolateAndRequireCorp, "x"},
  };

  for (const TestCase& test_case : test_cases) {
    CheckTestCase(test_case);
  }
}

}  // namespace network
