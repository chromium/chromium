// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_report.h"

#include <stdint.h>

#include <string>
#include <utility>
#include <vector>

#include "base/time/time.h"
#include "content/browser/aggregation_service/aggregatable_report.h"
#include "content/browser/aggregation_service/aggregation_service_test_utils.h"
#include "content/browser/attribution_reporting/attribution_report.h"
#include "content/browser/attribution_reporting/attribution_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

TEST(AttributionReportTest, AggregatableReportBody_SourceRegistrationTime) {
  const struct {
    std::string description;
    int64_t source_time;
    std::string source_time_rounded;
  } kTestCases[] = {
      {"14288 * 86400000", 1234483200000, "1234483200"},
      {"14288 * 86400000 + 1", 1234483200001, "1234483200"},
      {"14288.5 * 86400000 - 1", 1234526399999, "1234483200"},
      {"14288.5 * 86400000", 1234526400000, "1234569600"},
      {"14288.5 * 86400000 + 1", 1234526400001, "1234569600"},
      {"14289 * 86400000 -1", 1234569599999, "1234569600"},
  };

  for (const auto& test_case : kTestCases) {
    AttributionReport report =
        ReportBuilder(
            AttributionInfoBuilder(
                SourceBuilder(base::Time::FromJavaTime(test_case.source_time))
                    .BuildStored())
                .Build())
            .SetAggregatableHistogramContributions(
                {AggregatableHistogramContribution(/*key=*/1, /*value=*/2)})
            .BuildAggregatableAttribution();

    std::vector<AggregatableReport::AggregationServicePayload> payloads;
    payloads.emplace_back(/*payload=*/kABCD1234AsBytes,
                          /*key_id=*/"key_1",
                          /*debug_cleartext_payload=*/absl::nullopt);
    payloads.emplace_back(/*payload=*/kEFGH5678AsBytes,
                          /*key_id=*/"key_2",
                          /*debug_cleartext_payload=*/absl::nullopt);

    AggregatableReportSharedInfo shared_info(
        base::Time::FromJavaTime(1234567890123),
        /*privacy_budget_key=*/"example_pbk", report.external_report_id(),
        /*reporting_origin=*/
        url::Origin::Create(GURL("https://example.reporting")),
        AggregatableReportSharedInfo::DebugMode::kDisabled);

    auto& aggregatable_data =
        absl::get<AttributionReport::AggregatableAttributionData>(
            report.data());
    aggregatable_data.assembled_report =
        AggregatableReport(std::move(payloads), shared_info.SerializeAsJson());

    base::Value report_body = report.ReportBody();
    ASSERT_TRUE(report_body.is_dict()) << test_case.description;
    const base::Value::Dict& dict = report_body.GetDict();
    const std::string* source_registration_time =
        dict.FindString("source_registration_time");
    ASSERT_TRUE(source_registration_time) << test_case.description;
    EXPECT_EQ(*source_registration_time, test_case.source_time_rounded)
        << test_case.description;
  }
}

TEST(AttributionReportTest, PrivacyBudgetKey) {
  // Pre-hashed CBOR bytes
  // { 0xA4, 0x67, 0x76, 0x65, 0x72, 0x73, 0x69, 0x6F, 0x6E, 0x60, 0x6B, 0x64,
  //   0x65, 0x73, 0x74, 0x69, 0x6E, 0x61, 0x74, 0x69, 0x6F, 0x6E, 0x77, 0x68,
  //   0x74, 0x74, 0x70, 0x73, 0x3A, 0x2F, 0x2F, 0x63, 0x6F, 0x6E, 0x76, 0x65,
  //   0x72, 0x73, 0x69, 0x6F, 0x6E, 0x2E, 0x74, 0x65, 0x73, 0x74, 0x6B, 0x73,
  //   0x6F, 0x75, 0x72, 0x63, 0x65, 0x5F, 0x73, 0x69, 0x74, 0x65, 0x77, 0x68,
  //   0x74, 0x74, 0x70, 0x73, 0x3A, 0x2F, 0x2F, 0x69, 0x6D, 0x70, 0x72, 0x65,
  //   0x73, 0x73, 0x69, 0x6F, 0x6E, 0x2E, 0x74, 0x65, 0x73, 0x74, 0x70, 0x72,
  //   0x65, 0x70, 0x6F, 0x72, 0x74, 0x69, 0x6E, 0x67, 0x5F, 0x6F, 0x72, 0x69,
  //   0x67, 0x69, 0x6E, 0x73, 0x68, 0x74, 0x74, 0x70, 0x73, 0x3A, 0x2F, 0x2F,
  //   0x72, 0x65, 0x70, 0x6F, 0x72, 0x74, 0x2E, 0x74, 0x65, 0x73, 0x74 }

  // base64 encoded SHA256 hash string of the bytes above.
  const std::string kExpectedPrivacyBudgetKey(
      "NOM7HGJIb2ReR2jRlz1E0WIywBdUB/qLC6nCFyDqmRQ=");

  AttributionReport report =
      ReportBuilder(AttributionInfoBuilder(
                        SourceBuilder(base::Time::FromJavaTime(1234567890123))
                            .BuildStored())
                        .Build())
          .SetAggregatableHistogramContributions(
              {AggregatableHistogramContribution(/*key=*/1, /*value=*/2)})
          .BuildAggregatableAttribution();
  EXPECT_EQ(report.PrivacyBudgetKey(), kExpectedPrivacyBudgetKey);
}

}  // namespace content
