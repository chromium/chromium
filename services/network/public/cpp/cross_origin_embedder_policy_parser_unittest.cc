// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/cross_origin_embedder_policy_parser.h"

#include <string>
#include <vector>

#include "base/optional.h"
#include "base/test/scoped_feature_list.h"
#include "net/http/http_response_headers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace network {

TEST(CrossOriginEmbedderPolicyTest, Parse) {
  constexpr auto kNone = mojom::CrossOriginEmbedderPolicyValue::kNone;
  constexpr auto kRequireCorp =
      mojom::CrossOriginEmbedderPolicyValue::kRequireCorp;
  const auto kNoHeader = base::Optional<std::string>();
  const auto kNoEndpoint = base::Optional<std::string>();

  struct TestCase {
    base::Optional<std::string> coep_header;
    base::Optional<std::string> coep_report_only_header;

    mojom::CrossOriginEmbedderPolicyValue value;
    base::Optional<std::string> reporting_endpoint;
    mojom::CrossOriginEmbedderPolicyValue report_only_value;
    base::Optional<std::string> report_only_reporting_endpoint;
  } cases[] = {
      // No headers
      {kNoHeader, kNoHeader, kNone, kNoEndpoint, kNone, kNoEndpoint},
      // COEP: require-corp
      {"require-corp", kNoHeader, kRequireCorp, kNoEndpoint, kNone,
       kNoEndpoint},
      // COEP: require-corp with reporting endpoint
      {"require-corp; report-to=\"endpoint\"", kNoHeader, kRequireCorp,
       "endpoint", kNone, kNoEndpoint},
      // COEP-RO: require-corp
      {kNoHeader, "require-corp", kNone, kNoEndpoint, kRequireCorp,
       kNoEndpoint},
      // COEP-RO: require-corp with reporting endpoint
      {kNoHeader, "require-corp; report-to=\"endpoint\"", kNone, kNoEndpoint,
       kRequireCorp, "endpoint"},

      // With both headers
      {"require-corp; report-to=\"endpoint1\"",
       "require-corp; report-to=\"endpoint2\"", kRequireCorp, "endpoint1",
       kRequireCorp, "endpoint2"},
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

  for (const auto& testcase : cases) {
    auto message = testing::Message()
                   << "coep: " << testcase.coep_header.value_or("(missing)")
                   << ", coep-report-only: "
                   << testcase.coep_report_only_header.value_or("(missing)");
    SCOPED_TRACE(message);
    auto headers =
        base::MakeRefCounted<net::HttpResponseHeaders>("HTTP/1.1 200 OK\n");
    if (testcase.coep_header) {
      headers->AddHeader("cross-origin-embedder-policy", *testcase.coep_header);
    }
    if (testcase.coep_report_only_header) {
      headers->AddHeader("cross-origin-embedder-policy-report-only",
                         *testcase.coep_report_only_header);
    }
    const auto coep = ParseCrossOriginEmbedderPolicy(*headers);

    EXPECT_EQ(coep.value, testcase.value);
    EXPECT_EQ(coep.reporting_endpoint, testcase.reporting_endpoint);
    EXPECT_EQ(coep.report_only_value, testcase.report_only_value);
    EXPECT_EQ(coep.report_only_reporting_endpoint,
              testcase.report_only_reporting_endpoint);
  }
}

}  // namespace network
