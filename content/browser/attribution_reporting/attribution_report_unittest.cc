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
      {"14288.5 * 86400000", 1234526400000, "1234483200"},
      {"14288.5 * 86400000 + 1", 1234526400001, "1234483200"},
      {"14289 * 86400000 -1", 1234569599999, "1234483200"},
      {"14289 * 86400000", 1234569600000, "1234569600"},
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

    base::Value::Dict dict = report.ReportBody();
    const std::string* source_registration_time =
        dict.FindString("source_registration_time");
    ASSERT_TRUE(source_registration_time) << test_case.description;
    EXPECT_EQ(*source_registration_time, test_case.source_time_rounded)
        << test_case.description;
  }
}

TEST(AttributionReportTest, PrivacyBudgetKey) {
  // Pre-hashed CBOR bytes, base64-encoded for brevity:
  // "pWd2ZXJzaW9uYGtkZXN0aW5hdGlvbndodHRwczovL2NvbnZlcnNpb24udGVzdGtzb3VyY2Vf"
  // "c2l0ZXdodHRwczovL2ltcHJlc3Npb24udGVzdHByZXBvcnRpbmdfb3JpZ2luc2h0dHBzOi8v"
  // "cmVwb3J0LnRlc3R4GHNvdXJjZV9yZWdpc3RyYXRpb25fdGltZRpJlLgA"

  // base64-encoded SHA256 hash string of the bytes above.
  const std::string kExpectedPrivacyBudgetKey(
      "aOEbtVxG8dYzAR2K/xuW/OppNaQikp5RdjAXshOQ9w8=");

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
