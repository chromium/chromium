// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_report.h"

#include <stdint.h>

#include <string>

#include "base/containers/flat_set.h"
#include "base/test/values_test_util.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/attribution_reporting/source_type.mojom.h"
#include "content/browser/aggregation_service/aggregatable_report.h"
#include "content/browser/aggregation_service/aggregation_service_test_utils.h"
#include "content/browser/attribution_reporting/attribution_info.h"
#include "content/browser/attribution_reporting/attribution_report.h"
#include "content/browser/attribution_reporting/attribution_test_utils.h"
#include "content/browser/attribution_reporting/stored_source.h"
#include "net/base/schemeful_site.h"
#include "net/http/http_request_headers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {
namespace {

using ::attribution_reporting::mojom::SourceType;
using ::base::test::IsJson;

TEST(AttributionReportTest, ReportURL) {
  ReportBuilder builder(AttributionInfoBuilder().Build(),
                        SourceBuilder().BuildStored());

  EXPECT_EQ(
      "https://report.test/.well-known/attribution-reporting/"
      "report-event-attribution",
      builder.Build().ReportURL());

  EXPECT_EQ(
      "https://report.test/.well-known/attribution-reporting/debug/"
      "report-event-attribution",
      builder.Build().ReportURL(/*debug=*/true));

  EXPECT_EQ(
      "https://report.test/.well-known/attribution-reporting/"
      "report-aggregate-attribution",
      builder.BuildAggregatableAttribution().ReportURL());

  EXPECT_EQ(
      "https://report.test/.well-known/attribution-reporting/debug/"
      "report-aggregate-attribution",
      builder.BuildAggregatableAttribution().ReportURL(/*debug=*/true));
}

TEST(AttributionReportTest, ReportBody) {
  const struct {
    SourceType source_type;
    base::Value::Dict expected;
  } kTestCases[] = {
      {SourceType::kNavigation, base::test::ParseJsonDict(R"json({
        "attribution_destination":"https://conversion.test",
        "randomized_trigger_rate":0.2,
        "report_id":"21abd97f-73e8-4b88-9389-a9fee6abda5e",
        "scheduled_report_time":"3600",
        "source_event_id":"100",
        "source_type":"navigation",
        "trigger_data":"5"
      })json")},
      {SourceType::kEvent, base::test::ParseJsonDict(R"json({
        "attribution_destination":"https://conversion.test",
        "randomized_trigger_rate":0.2,
        "report_id":"21abd97f-73e8-4b88-9389-a9fee6abda5e",
        "scheduled_report_time":"3600",
        "source_event_id":"100",
        "source_type":"event",
        "trigger_data":"5"
      })json")},
  };

  for (const auto& test_case : kTestCases) {
    AttributionReport report =
        ReportBuilder(AttributionInfoBuilder()
                          .SetTime(base::Time::UnixEpoch() + base::Seconds(1))
                          .Build(),
                      SourceBuilder(base::Time::UnixEpoch())
                          .SetSourceEventId(100)
                          .SetSourceType(test_case.source_type)
                          .BuildStored())
            .SetTriggerData(5)
            .SetRandomizedTriggerRate(0.2)
            .SetReportTime(base::Time::UnixEpoch() + base::Hours(1))
            .Build();

    EXPECT_THAT(report.ReportBody(), IsJson(test_case.expected));
  }
}

TEST(AttributionReportTest, ReportBody_MultiDestination) {
  const struct {
    base::flat_set<net::SchemefulSite> destination_sites;
    base::Value::Dict expected;
  } kTestCases[] = {
      {
          {
              net::SchemefulSite::Deserialize("https://b.test"),
              net::SchemefulSite::Deserialize("https://d.test"),
          },
          base::test::ParseJsonDict(R"json({
            "attribution_destination":["https://b.test","https://d.test"],
            "randomized_trigger_rate":0.0,
            "report_id":"21abd97f-73e8-4b88-9389-a9fee6abda5e",
            "scheduled_report_time":"3600",
            "source_event_id":"123",
            "source_type":"navigation",
            "trigger_data":"0"
          })json"),
      },
      {
          {
              net::SchemefulSite::Deserialize("https://d.test"),
          },
          base::test::ParseJsonDict(R"json({
            "attribution_destination":"https://d.test",
            "randomized_trigger_rate":0.0,
            "report_id":"21abd97f-73e8-4b88-9389-a9fee6abda5e",
            "scheduled_report_time":"3600",
            "source_event_id":"123",
            "source_type":"navigation",
            "trigger_data":"0"
          })json"),
      },
  };

  for (const auto& test_case : kTestCases) {
    AttributionReport report =
        ReportBuilder(AttributionInfoBuilder()
                          .SetTime(base::Time::UnixEpoch() + base::Seconds(1))
                          .Build(),
                      SourceBuilder(base::Time::UnixEpoch())
                          .SetDestinationSites(test_case.destination_sites)
                          .BuildStored())
            .SetReportTime(base::Time::UnixEpoch() + base::Hours(1))
            .Build();

    EXPECT_THAT(report.ReportBody(), IsJson(test_case.expected));
  }
}

TEST(AttributionReportTest, ReportBody_DebugKeys) {
  const struct {
    absl::optional<uint64_t> source_debug_key;
    absl::optional<uint64_t> trigger_debug_key;
    base::Value::Dict expected;
  } kTestCases[] = {
      {absl::nullopt, absl::nullopt, base::test::ParseJsonDict(R"json({
        "attribution_destination":"https://conversion.test",
        "randomized_trigger_rate":0.2,
        "report_id":"21abd97f-73e8-4b88-9389-a9fee6abda5e",
        "scheduled_report_time":"3600",
        "source_event_id":"100",
        "source_type":"navigation",
        "trigger_data":"5"
      })json")},
      {7, absl::nullopt, base::test::ParseJsonDict(R"json({
        "attribution_destination":"https://conversion.test",
        "randomized_trigger_rate":0.2,
        "report_id":"21abd97f-73e8-4b88-9389-a9fee6abda5e",
        "scheduled_report_time":"3600",
        "source_debug_key":"7",
        "source_event_id":"100",
        "source_type":"navigation",
        "trigger_data":"5"
      })json")},
      {absl::nullopt, 7, base::test::ParseJsonDict(R"json({
        "attribution_destination":"https://conversion.test",
        "randomized_trigger_rate":0.2,
        "report_id":"21abd97f-73e8-4b88-9389-a9fee6abda5e",
        "scheduled_report_time":"3600",
        "source_event_id":"100",
        "source_type":"navigation",
        "trigger_data":"5",
        "trigger_debug_key":"7"
      })json")},
      {7, 8, base::test::ParseJsonDict(R"json({
        "attribution_destination":"https://conversion.test",
        "randomized_trigger_rate":0.2,
        "report_id":"21abd97f-73e8-4b88-9389-a9fee6abda5e",
        "scheduled_report_time":"3600",
        "source_debug_key":"7",
        "source_event_id":"100",
        "source_type":"navigation",
        "trigger_data":"5",
        "trigger_debug_key":"8"
      })json")},
  };

  for (const auto& test_case : kTestCases) {
    AttributionReport report =
        ReportBuilder(AttributionInfoBuilder()
                          .SetTime(base::Time::UnixEpoch() + base::Seconds(1))
                          .SetDebugKey(test_case.trigger_debug_key)
                          .Build(),
                      SourceBuilder(base::Time::UnixEpoch())
                          .SetSourceEventId(100)
                          .SetDebugKey(test_case.source_debug_key)
                          .BuildStored())
            .SetTriggerData(5)
            .SetRandomizedTriggerRate(0.2)
            .SetReportTime(base::Time::UnixEpoch() + base::Hours(1))
            .Build();

    EXPECT_THAT(report.ReportBody(), IsJson(test_case.expected));
  }
}

TEST(AttributionReportTest, ReportBody_Aggregatable) {
  base::Value::Dict expected = base::test::ParseJsonDict(R"json({
    "aggregation_service_payloads":"not generated prior to send",
    "shared_info":"not generated prior to send"
  })json");

  AttributionReport report =
      ReportBuilder(
          AttributionInfoBuilder().Build(),
          SourceBuilder(base::Time::FromJavaTime(1234483200000)).BuildStored())
          .SetAggregatableHistogramContributions(
              {AggregatableHistogramContribution(/*key=*/1, /*value=*/2)})
          .BuildAggregatableAttribution();

  EXPECT_THAT(report.ReportBody(), IsJson(expected));
}

TEST(AttributionReportTest, PopulateAdditionalHeaders) {
  const absl::optional<std::string> kTestCases[] = {
      absl::nullopt,
      "foo",
  };

  for (const auto& attestation_token : kTestCases) {
    AttributionReport report = ReportBuilder(AttributionInfoBuilder().Build(),
                                             SourceBuilder().BuildStored())
                                   .SetAttestationToken(attestation_token)
                                   .BuildAggregatableAttribution();

    net::HttpRequestHeaders headers;
    report.PopulateAdditionalHeaders(headers);

    if (attestation_token.has_value()) {
      std::string header;
      headers.GetHeader("Sec-Attribution-Reporting-Private-State-Token",
                        &header);
      EXPECT_EQ(header, *attestation_token);
    } else {
      EXPECT_TRUE(headers.IsEmpty());
    }
  }
}

TEST(AttributionReportTest, NullAggregatableReport) {
  base::Value::Dict expected = base::test::ParseJsonDict(R"json({
    "aggregation_coordinator_identifier": "aws-cloud",
    "aggregation_service_payloads": [{
      "key_id": "key",
      "payload": "ABCD1234"
    }],
    "shared_info":"example_shared_info"
  })json");

  AttributionReport report = ReportBuilder(AttributionInfoBuilder().Build(),
                                           SourceBuilder().BuildStored())
                                 .BuildNullAggregatable();
  EXPECT_EQ(report.ReportURL(),
            GURL("https://report.test/.well-known/attribution-reporting/"
                 "report-aggregate-attribution"));

  auto& data =
      absl::get<AttributionReport::NullAggregatableData>(report.data());
  data.common_data.assembled_report = AggregatableReport(
      {AggregatableReport::AggregationServicePayload(
          /*payload=*/kABCD1234AsBytes,
          /*key_id=*/"key",
          /*debug_cleartext_payload=*/absl::nullopt)},
      "example_shared_info",
      /*debug_key=*/absl::nullopt,
      /*additional_fields=*/{},
      ::aggregation_service::mojom::AggregationCoordinator::kDefault);

  EXPECT_THAT(report.ReportBody(), IsJson(expected));
}

}  // namespace
}  // namespace content
