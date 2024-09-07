// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_report.h"

#include <stdint.h>

#include <optional>
#include <string>

#include "base/containers/flat_set.h"
#include "base/test/values_test_util.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/aggregation_service/aggregation_coordinator_utils.h"
#include "components/attribution_reporting/source_type.mojom.h"
#include "content/browser/aggregation_service/aggregatable_report.h"
#include "content/browser/aggregation_service/aggregation_service_test_utils.h"
#include "content/browser/attribution_reporting/attribution_info.h"
#include "content/browser/attribution_reporting/attribution_report.h"
#include "content/browser/attribution_reporting/attribution_test_utils.h"
#include "content/browser/attribution_reporting/stored_source.h"
#include "net/base/schemeful_site.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/aggregation_service/aggregatable_report.mojom.h"
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
                          .SetRandomizedResponseRate(0.2)
                          .BuildStored())
            .SetTriggerData(5)
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
    std::optional<uint64_t> source_debug_key;
    std::optional<uint64_t> trigger_debug_key;
    base::Value::Dict expected;
  } kTestCases[] = {
      {std::nullopt, std::nullopt, base::test::ParseJsonDict(R"json({
        "attribution_destination":"https://conversion.test",
        "randomized_trigger_rate":0.2,
        "report_id":"21abd97f-73e8-4b88-9389-a9fee6abda5e",
        "scheduled_report_time":"3600",
        "source_event_id":"100",
        "source_type":"navigation",
        "trigger_data":"5"
      })json")},
      {7, std::nullopt, base::test::ParseJsonDict(R"json({
        "attribution_destination":"https://conversion.test",
        "randomized_trigger_rate":0.2,
        "report_id":"21abd97f-73e8-4b88-9389-a9fee6abda5e",
        "scheduled_report_time":"3600",
        "source_event_id":"100",
        "source_type":"navigation",
        "trigger_data":"5"
      })json")},
      {std::nullopt, 7, base::test::ParseJsonDict(R"json({
        "attribution_destination":"https://conversion.test",
        "randomized_trigger_rate":0.2,
        "report_id":"21abd97f-73e8-4b88-9389-a9fee6abda5e",
        "scheduled_report_time":"3600",
        "source_event_id":"100",
        "source_type":"navigation",
        "trigger_data":"5",
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
                          .SetDebugCookieSet(true)
                          .SetRandomizedResponseRate(0.2)
                          .BuildStored())
            .SetTriggerData(5)
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
      ReportBuilder(AttributionInfoBuilder().Build(),
                    SourceBuilder().BuildStored())
          .SetAggregatableHistogramContributions(
              {blink::mojom::AggregatableReportHistogramContribution(
                  /*bucket=*/1, /*value=*/2, /*filtering_id=*/std::nullopt)})
          .BuildAggregatableAttribution();

  EXPECT_THAT(report.ReportBody(), IsJson(expected));
}

TEST(AttributionReportTest, NullAggregatableReport) {
  ::aggregation_service::ScopedAggregationCoordinatorAllowlistForTesting
      scoped_coordinator_allowlist(
          {url::Origin::Create(GURL("https://a.test"))});

  base::Value::Dict expected = base::test::ParseJsonDict(R"json({
    "aggregation_coordinator_origin":"https://a.test",
    "aggregation_service_payloads": [{
      "key_id": "key",
      "payload": "ABCD1234"
    }],
    "shared_info":"example_shared_info",
    "trigger_context_id":"123"
  })json");

  AttributionReport report = ReportBuilder(AttributionInfoBuilder().Build(),
                                           SourceBuilder().BuildStored())
                                 .SetSourceRegistrationTimeConfig(
                                     attribution_reporting::mojom::
                                         SourceRegistrationTimeConfig::kExclude)
                                 .SetTriggerContextId("123")
                                 .BuildNullAggregatable();
  EXPECT_EQ(report.ReportURL(),
            GURL("https://report.test/.well-known/attribution-reporting/"
                 "report-aggregate-attribution"));

  auto& data =
      absl::get<AttributionReport::NullAggregatableData>(report.data());
  data.common_data.assembled_report =
      AggregatableReport({AggregatableReport::AggregationServicePayload(
                             /*payload=*/kABCD1234AsBytes,
                             /*key_id=*/"key",
                             /*debug_cleartext_payload=*/std::nullopt)},
                         "example_shared_info",
                         /*debug_key=*/std::nullopt,
                         /*additional_fields=*/{},
                         /*aggregation_coordinator_origin=*/std::nullopt);

  EXPECT_THAT(report.ReportBody(), IsJson(expected));
}

TEST(AttributionReportTest, ReportBody_AggregatableAttributionReport) {
  ::aggregation_service::ScopedAggregationCoordinatorAllowlistForTesting
      scoped_coordinator_allowlist(
          {url::Origin::Create(GURL("https://a.test"))});

  base::Value::Dict expected = base::test::ParseJsonDict(R"json({
    "aggregation_coordinator_origin": "https://a.test",
    "aggregation_service_payloads": [{
      "key_id": "key",
      "payload": "ABCD1234"
    }],
    "shared_info": "example_shared_info",
    "trigger_context_id": "123"
  })json");

  AttributionReport report =
      ReportBuilder(AttributionInfoBuilder().Build(),
                    SourceBuilder().BuildStored())
          .SetSourceRegistrationTimeConfig(
              attribution_reporting::mojom::SourceRegistrationTimeConfig::
                  kExclude)
          .SetTriggerContextId("123")
          .SetAggregatableHistogramContributions(
              {blink::mojom::AggregatableReportHistogramContribution(
                  /*bucket=*/1, /*value=*/2, /*filtering_id=*/std::nullopt)})
          .BuildAggregatableAttribution();

  auto& data =
      absl::get<AttributionReport::AggregatableAttributionData>(report.data());
  data.common_data.assembled_report =
      AggregatableReport({AggregatableReport::AggregationServicePayload(
                             /*payload=*/kABCD1234AsBytes,
                             /*key_id=*/"key",
                             /*debug_cleartext_payload=*/std::nullopt)},
                         "example_shared_info",
                         /*debug_key=*/std::nullopt,
                         /*additional_fields=*/{},
                         /*aggregation_coordinator_origin=*/std::nullopt);

  EXPECT_THAT(report.ReportBody(), IsJson(expected));
}

}  // namespace
}  // namespace content
