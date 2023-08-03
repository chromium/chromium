// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_debug_report.h"

#include <stdint.h>

#include "base/test/values_test_util.h"
#include "base/time/time.h"
#include "content/browser/attribution_reporting/attribution_input_event.h"
#include "content/browser/attribution_reporting/attribution_test_utils.h"
#include "content/browser/attribution_reporting/attribution_trigger.h"
#include "content/browser/attribution_reporting/create_report_result.h"
#include "content/browser/attribution_reporting/os_registration.h"
#include "content/browser/attribution_reporting/storable_source.h"
#include "content/browser/attribution_reporting/store_source_result.h"
#include "net/base/schemeful_site.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace content {
namespace {

using EventLevelResult = ::content::AttributionTrigger::EventLevelResult;
using AggregatableResult = ::content::AttributionTrigger::AggregatableResult;

AttributionReport DefaultEventLevelReport(
    base::Time source_time = base::Time::Now()) {
  return ReportBuilder(AttributionInfoBuilder().Build(),
                       SourceBuilder(source_time).BuildStored())
      .SetReportTime(base::Time::UnixEpoch() + base::Hours(1))
      .Build();
}

AttributionReport DefaultAggregatableReport() {
  return ReportBuilder(AttributionInfoBuilder().Build(),
                       SourceBuilder().BuildStored())
      .SetAggregatableHistogramContributions(
          {AggregatableHistogramContribution(1, 2)})
      .BuildAggregatableAttribution();
}

TEST(AttributionDebugReportTest, NoDebugReporting_NoReportReturned) {
  EXPECT_FALSE(AttributionDebugReport::Create(
      SourceBuilder().Build(),
      /*is_debug_cookie_set=*/false,
      StoreSourceResult(
          StorableSource::Result::kInsufficientUniqueDestinationCapacity,
          /*min_fake_report_time=*/absl::nullopt,
          /*max_destinations_per_source_site_reporting_site=*/3)));

  EXPECT_FALSE(AttributionDebugReport::Create(
      TriggerBuilder().Build(),
      /*is_debug_cookie_set=*/true,
      CreateReportResult(/*trigger_time=*/base::Time::Now(),
                         EventLevelResult::kNoMatchingImpressions,
                         AggregatableResult::kNoMatchingImpressions)));
}

TEST(AttributionDebugReportTest,
     SourceDestinationLimitError_ValidReportReturned) {
  absl::optional<AttributionDebugReport> report =
      AttributionDebugReport::Create(
          SourceBuilder().SetDebugReporting(true).Build(),
          /*is_debug_cookie_set=*/false,
          StoreSourceResult(
              StorableSource::Result::kInsufficientUniqueDestinationCapacity,
              /*min_fake_report_time=*/absl::nullopt,
              /*max_destinations_per_source_site_reporting_site=*/3));
  ASSERT_TRUE(report);

  static constexpr char kExpectedJsonString[] = R"([{
    "body": {
      "attribution_destination": "https://conversion.test",
      "limit": "3",
      "source_event_id": "123",
      "source_site": "https://impression.test"
    },
    "type": "source-destination-limit"
  }])";
  EXPECT_EQ(report->ReportBody(), base::test::ParseJson(kExpectedJsonString));

  EXPECT_EQ(report->ReportUrl(), GURL("https://report.test/.well-known/"
                                      "attribution-reporting/debug/verbose"));
}

TEST(AttributionDebugReportTest, WithinFencedFrame_NoDebugReport) {
  AttributionConfig config;
  config.max_destinations_per_source_site_reporting_site = 3;

  EXPECT_FALSE(AttributionDebugReport::Create(
      SourceBuilder()
          .SetDebugReporting(true)
          .SetIsWithinFencedFrame(true)
          .Build(),
      /*is_debug_cookie_set=*/false,
      StoreSourceResult(
          StorableSource::Result::kInsufficientUniqueDestinationCapacity,
          /*min_fake_report_time=*/absl::nullopt,
          /*max_destinations_per_source_site_reporting_site=*/3)));

  EXPECT_FALSE(AttributionDebugReport::Create(
      TriggerBuilder()
          .SetDebugReporting(true)
          .SetIsWithinFencedFrame(true)
          .Build(),
      /*is_debug_cookie_set=*/true,
      CreateReportResult(/*trigger_time=*/base::Time::Now(),
                         EventLevelResult::kNoMatchingImpressions,
                         AggregatableResult::kNoMatchingImpressions)));
}

TEST(AttributionDebugReportTest, SourceDebugging) {
  const struct {
    StorableSource::Result result;
    absl::optional<int> max_destinations_per_source_site_reporting_site;
    absl::optional<int> max_sources_per_origin;
    absl::optional<int> max_destinations_per_rate_limit_window_reporting_origin;
    absl::optional<uint64_t> debug_key;
    const char* expected_report_body_without_cookie;
    const char* expected_report_body_with_cookie;
  } kTestCases[] = {
      {StorableSource::Result::kSuccess,
       /*max_destinations_per_source_site_reporting_site=*/absl::nullopt,
       /*max_sources_per_origin=*/absl::nullopt,
       /*max_sources_per_reporting_origin=*/absl::nullopt,
       /*debug_key=*/absl::nullopt,
       /*expected_report_body_without_cookie=*/nullptr,
       R"json([{
         "body": {
           "attribution_destination": "https://conversion.test",
           "source_event_id": "123",
           "source_site": "https://impression.test"
         },
         "type": "source-success"
       }])json"},
      {StorableSource::Result::kInternalError,
       /*max_destinations_per_source_site_reporting_site=*/absl::nullopt,
       /*max_sources_per_origin=*/absl::nullopt,
       /*max_sources_per_reporting_origin=*/absl::nullopt,
       /*debug_key=*/456,
       /*expected_report_body_without_cookie=*/nullptr,
       /*expected_report_body_with_cookie=*/
       R"json([{
         "body": {
           "attribution_destination": "https://conversion.test",
           "source_debug_key": "456",
           "source_event_id": "123",
           "source_site": "https://impression.test"
         },
         "type": "source-unknown-error"
       }])json"},
      {StorableSource::Result::kInsufficientSourceCapacity,
       /*max_destinations_per_source_site_reporting_site=*/absl::nullopt,
       /*max_sources_per_origin=*/10,
       /*max_sources_per_reporting_origin=*/absl::nullopt,
       /*debug_key=*/absl::nullopt,
       /*expected_report_body_without_cookie=*/nullptr,
       /*expected_report_body_with_cookie=*/
       R"json([{
         "body": {
           "attribution_destination": "https://conversion.test",
           "limit": "10",
           "source_event_id": "123",
           "source_site": "https://impression.test"
         },
         "type": "source-storage-limit"
       }])json"},
      {StorableSource::Result::kProhibitedByBrowserPolicy,
       /*max_destinations_per_source_site_reporting_site=*/absl::nullopt,
       /*max_sources_per_origin=*/absl::nullopt,
       /*max_sources_per_reporting_origin=*/absl::nullopt,
       /*debug_key=*/absl::nullopt,
       /*expected_report_body_without_cookie=*/nullptr,
       /*expected_report_body_with_cookie=*/nullptr},
      {StorableSource::Result::kInsufficientUniqueDestinationCapacity,
       /*max_destinations_per_source_site_reporting_site=*/3,
       /*max_sources_per_origin=*/absl::nullopt,
       /*max_sources_per_reporting_origin=*/absl::nullopt,
       /*debug_key=*/absl::nullopt,
       /*expected_report_body_without_cookie=*/
       R"json([{
         "body": {
           "attribution_destination": "https://conversion.test",
           "limit": "3",
           "source_event_id": "123",
           "source_site": "https://impression.test"
         },
         "type": "source-destination-limit"
       }])json",
       /*expected_report_body_with_cookie=*/
       R"json([{
         "body": {
           "attribution_destination": "https://conversion.test",
           "limit": "3",
           "source_event_id": "123",
           "source_site": "https://impression.test"
         },
         "type": "source-destination-limit"
       }])json"},
      {StorableSource::Result::kSuccessNoised,
       /*max_destinations_per_source_site_reporting_site=*/absl::nullopt,
       /*max_sources_per_origin=*/absl::nullopt,
       /*max_sources_per_reporting_origin=*/absl::nullopt,
       /*debug_key=*/absl::nullopt,
       /*expected_report_body_without_cookie=*/nullptr,
       /*expected_report_body_with_cookie=*/
       R"json([{
         "body": {
           "attribution_destination": "https://conversion.test",
           "source_event_id": "123",
           "source_site": "https://impression.test"
         },
         "type": "source-noised"
       }])json"},
      {StorableSource::Result::kExcessiveReportingOrigins,
       /*max_destinations_per_source_site_reporting_site=*/absl::nullopt,
       /*max_sources_per_origin=*/absl::nullopt,
       /*max_sources_per_reporting_origin=*/absl::nullopt,
       /*debug_key=*/789,
       /*expected_report_body_without_cookie=*/nullptr,
       /*expected_report_body_with_cookie=*/
       R"json([{
         "body": {
           "attribution_destination": "https://conversion.test",
           "source_debug_key": "789",
           "source_event_id": "123",
           "source_site": "https://impression.test"
         },
         "type": "source-success"
       }])json"},
      {StorableSource::Result::kDestinationGlobalLimitReached,
       /*max_destinations_per_source_site_reporting_origin=*/absl::nullopt,
       /*max_sources_per_origin=*/absl::nullopt,
       /*max_sources_per_reporting_origin=*/absl::nullopt,
       /*debug_key=*/789,
       /*expected_report_body_without_cookie=*/nullptr,
       /*expected_report_body_with_cookie=*/
       R"json([{
         "body": {
           "attribution_destination": "https://conversion.test",
           "source_debug_key": "789",
           "source_event_id": "123",
           "source_site": "https://impression.test"
         },
         "type": "source-success"
       }])json"},
      {StorableSource::Result::kDestinationReportingLimitReached,
       /*max_destinations_per_source_site_reporting_origin=*/absl::nullopt,
       /*max_sources_per_origin=*/absl::nullopt,
       /*max_sources_per_reporting_origin=*/50,
       /*debug_key=*/absl::nullopt,
       /*expected_report_body_without_cookie=*/
       R"json([{
         "body": {
           "attribution_destination": "https://conversion.test",
           "limit": "50",
           "source_event_id": "123",
           "source_site": "https://impression.test"
         },
         "type": "source-destination-rate-limit"
       }])json",
       /*expected_report_body_with_cookie=*/
       R"json([{
         "body": {
           "attribution_destination": "https://conversion.test",
           "limit": "50",
           "source_event_id": "123",
           "source_site": "https://impression.test"
         },
         "type": "source-destination-rate-limit"
       }])json"},
      {StorableSource::Result::kDestinationBothLimitsReached,
       /*max_destinations_per_source_site_reporting_origin=*/absl::nullopt,
       /*max_sources_per_origin=*/absl::nullopt,
       /*max_sources_per_reporting_origin=*/50,
       /*debug_key=*/absl::nullopt,
       /*expected_report_body_without_cookie=*/
       R"json([{
         "body": {
           "attribution_destination": "https://conversion.test",
           "limit": "50",
           "source_event_id": "123",
           "source_site": "https://impression.test"
         },
         "type": "source-destination-rate-limit"
       }])json",
       /*expected_report_body_with_cookie=*/
       R"json([{
         "body": {
           "attribution_destination": "https://conversion.test",
           "limit": "50",
           "source_event_id": "123",
           "source_site": "https://impression.test"
         },
         "type": "source-destination-rate-limit"
       }])json"},
  };

  for (bool is_debug_cookie_set : {false, true}) {
    for (const auto& test_case : kTestCases) {
      absl::optional<AttributionDebugReport> report =
          AttributionDebugReport::Create(
              SourceBuilder()
                  .SetDebugReporting(true)
                  .SetDebugKey(test_case.debug_key)
                  .Build(),
              is_debug_cookie_set,
              StoreSourceResult(
                  test_case.result,
                  /*min_fake_report_time=*/absl::nullopt,
                  test_case.max_destinations_per_source_site_reporting_site,
                  test_case.max_sources_per_origin,
                  test_case
                      .max_destinations_per_rate_limit_window_reporting_origin));
      const char* expected_report_body =
          is_debug_cookie_set ? test_case.expected_report_body_with_cookie
                              : test_case.expected_report_body_without_cookie;
      EXPECT_EQ(report.has_value(), expected_report_body != nullptr)
          << test_case.result << ", " << is_debug_cookie_set;
      if (expected_report_body) {
        EXPECT_EQ(report->ReportBody(),
                  base::test::ParseJson(expected_report_body))
            << test_case.result << ", " << is_debug_cookie_set;
      }
    }
  }

  // Multiple destinations
  {
    absl::optional<AttributionDebugReport> report =
        AttributionDebugReport::Create(
            SourceBuilder()
                .SetDebugReporting(true)
                .SetDestinationSites({
                    net::SchemefulSite::Deserialize("https://c.test"),
                    net::SchemefulSite::Deserialize("https://d.test"),
                })
                .Build(),
            /*is_debug_cookie_set=*/true,
            StoreSourceResult(
                StorableSource::Result::kSuccessNoised,
                /*min_fake_report_time=*/absl::nullopt,
                /*max_destinations_per_source_site_reporting_site=*/
                absl::nullopt,
                /*max_sources_per_origin=*/absl::nullopt));

    EXPECT_EQ(report->ReportBody(), base::test::ParseJson(R"json([{
         "body": {
           "attribution_destination": [
             "https://c.test",
             "https://d.test"
           ],
           "source_event_id": "123",
           "source_site": "https://impression.test"
         },
         "type": "source-noised"
      }])json"));
  }
}

TEST(AttributionDebugReportTest, TriggerDebugging) {
  const struct {
    EventLevelResult event_level_result;
    AggregatableResult aggregatable_result;
    absl::optional<StoredSource> source;
    CreateReportResult::Limits limits;
    const char* expected_report_body;
  } kTestCases[] = {
      {EventLevelResult::kNoMatchingImpressions,
       AggregatableResult::kNoMatchingImpressions,
       /*source=*/absl::nullopt, CreateReportResult::Limits(),
       R"json([{
         "body": {
           "attribution_destination": "https://conversion.test"
         },
         "type": "trigger-no-matching-source"
       }])json"},
      {EventLevelResult::kProhibitedByBrowserPolicy,
       AggregatableResult::kProhibitedByBrowserPolicy,
       /*source=*/absl::nullopt, CreateReportResult::Limits(),
       /*expected_report_body=*/nullptr},
      {EventLevelResult::kNoMatchingConfigurations,
       AggregatableResult::kExcessiveAttributions,
       /*source=*/SourceBuilder().BuildStored(),
       CreateReportResult::Limits{.rate_limits_max_attributions = 10},
       R"json([
         {
           "body": {
             "attribution_destination": "https://conversion.test",
             "source_event_id": "123",
             "source_site": "https://impression.test"
           },
           "type": "trigger-event-no-matching-configurations"
         },
         {
           "body": {
             "attribution_destination": "https://conversion.test",
             "limit": "10",
             "source_event_id": "123",
             "source_site": "https://impression.test"
           },
           "type": "trigger-attributions-per-source-destination-limit"
         }
       ])json"},
      {EventLevelResult::kNoMatchingConfigurations,
       AggregatableResult::kInsufficientBudget,
       /*source=*/SourceBuilder().BuildStored(),
       CreateReportResult::Limits{.aggregatable_budget_per_source = 100},
       R"json([
         {
           "body": {
             "attribution_destination": "https://conversion.test",
             "source_event_id": "123",
             "source_site": "https://impression.test"
           },
           "type": "trigger-event-no-matching-configurations"
         },
         {
           "body": {
             "attribution_destination": "https://conversion.test",
             "limit": "100",
             "source_event_id": "123",
             "source_site": "https://impression.test"
           },
           "type": "trigger-aggregate-insufficient-budget"
         }
       ])json"},
  };

  for (bool is_debug_cookie_set : {false, true}) {
    for (const auto& test_case : kTestCases) {
      absl::optional<AttributionDebugReport> report =
          AttributionDebugReport::Create(
              TriggerBuilder().SetDebugReporting(true).Build(),
              is_debug_cookie_set,
              CreateReportResult(
                  /*trigger_time=*/base::Time::Now(),
                  test_case.event_level_result, test_case.aggregatable_result,
                  /*replaced_event_level_report=*/absl::nullopt,
                  /*new_event_level_report=*/absl::nullopt,
                  /*new_aggregatable_report=*/absl::nullopt, test_case.source,
                  test_case.limits));
      if (is_debug_cookie_set) {
        EXPECT_EQ(report.has_value(), test_case.expected_report_body != nullptr)
            << test_case.event_level_result << ", "
            << test_case.aggregatable_result << ", " << is_debug_cookie_set;
        if (report) {
          EXPECT_EQ(report->ReportBody(),
                    base::test::ParseJson(test_case.expected_report_body))
              << test_case.event_level_result << ", "
              << test_case.aggregatable_result << ", " << is_debug_cookie_set;
        }
      } else {
        EXPECT_FALSE(report)
            << test_case.event_level_result << ", "
            << test_case.aggregatable_result << ", " << is_debug_cookie_set;
      }
    }
  }
}

TEST(AttributionDebugReportTest, EventLevelAttributionDebugging) {
  const struct {
    EventLevelResult result;
    absl::optional<AttributionReport> replaced_event_level_report;
    absl::optional<AttributionReport> new_event_level_report;
    absl::optional<StoredSource> source;
    CreateReportResult::Limits limits;
    absl::optional<AttributionReport> dropped_event_level_report;
    absl::optional<uint64_t> trigger_debug_key;
    const char* expected_report_body;
  } kTestCases[] = {
      {EventLevelResult::kSuccess,
       /*replaced_event_level_report=*/absl::nullopt,
       /*new_event_level_report=*/DefaultEventLevelReport(),
       /*source=*/SourceBuilder().BuildStored(), CreateReportResult::Limits(),
       /*dropped_event_level_report=*/absl::nullopt,
       /*trigger_debug_key=*/absl::nullopt,
       /*expected_report_body=*/nullptr},
      {EventLevelResult::kSuccessDroppedLowerPriority,
       /*replaced_event_level_report=*/DefaultEventLevelReport(),
       /*new_event_level_report=*/DefaultEventLevelReport(),
       /*source=*/SourceBuilder().BuildStored(), CreateReportResult::Limits(),
       /*dropped_event_level_report=*/absl::nullopt,
       /*trigger_debug_key=*/absl::nullopt,
       /*expected_report_body=*/nullptr},
      {EventLevelResult::kInternalError,
       /*replaced_event_level_report=*/absl::nullopt,
       /*new_event_level_report=*/absl::nullopt,
       /*source=*/absl::nullopt, CreateReportResult::Limits(),
       /*dropped_event_level_report=*/absl::nullopt,
       /*trigger_debug_key=*/123,
       R"json([{
         "body": {
           "attribution_destination": "https://conversion.test",
           "trigger_debug_key": "123"
         },
         "type": "trigger-unknown-error"
       }])json"},
      {EventLevelResult::kNoCapacityForConversionDestination,
       /*replaced_event_level_report=*/absl::nullopt,
       /*new_event_level_report=*/absl::nullopt,
       /*source=*/SourceBuilder().SetDebugKey(456).BuildStored(),
       CreateReportResult::Limits{.max_event_level_reports_per_destination =
                                      10},
       /*dropped_event_level_report=*/absl::nullopt,
       /*trigger_debug_key=*/absl::nullopt,
       R"json([{
         "body": {
           "attribution_destination": "https://conversion.test",
           "limit": "10",
           "source_debug_key": "456",
           "source_event_id": "123",
           "source_site": "https://impression.test"
         },
         "type": "trigger-event-storage-limit"
       }])json"},
      {EventLevelResult::kNoMatchingImpressions,
       /*replaced_event_level_report=*/absl::nullopt,
       /*new_event_level_report=*/absl::nullopt,
       /*source=*/absl::nullopt, CreateReportResult::Limits(),
       /*dropped_event_level_report=*/absl::nullopt,
       /*trigger_debug_key=*/absl::nullopt,
       R"json([{
         "body": {
           "attribution_destination": "https://conversion.test"
         },
         "type": "trigger-no-matching-source"
       }])json"},
      {EventLevelResult::kDeduplicated,
       /*replaced_event_level_report=*/absl::nullopt,
       /*new_event_level_report=*/absl::nullopt,
       /*source=*/SourceBuilder().SetDebugKey(789).BuildStored(),
       CreateReportResult::Limits(),
       /*dropped_event_level_report=*/absl::nullopt,
       /*trigger_debug_key=*/456,
       R"json([{
         "body": {
           "attribution_destination": "https://conversion.test",
           "source_debug_key": "789",
           "source_event_id": "123",
           "source_site": "https://impression.test",
           "trigger_debug_key": "456"
         },
         "type": "trigger-event-deduplicated"
       }])json"},
      {EventLevelResult::kExcessiveAttributions,
       /*replaced_event_level_report=*/absl::nullopt,
       /*new_event_level_report=*/absl::nullopt,
       /*source=*/SourceBuilder().BuildStored(),
       CreateReportResult::Limits{.rate_limits_max_attributions = 10},
       /*dropped_event_level_report=*/absl::nullopt,
       /*trigger_debug_key=*/absl::nullopt,
       R"json([{
         "body": {
           "attribution_destination": "https://conversion.test",
           "limit": "10",
           "source_event_id": "123",
           "source_site": "https://impression.test"
         },
         "type": "trigger-attributions-per-source-destination-limit"
       }])json"},
      {EventLevelResult::kPriorityTooLow,
       /*replaced_event_level_report=*/absl::nullopt,
       /*new_event_level_report=*/absl::nullopt,
       /*source=*/SourceBuilder(base::Time::UnixEpoch()).BuildStored(),
       CreateReportResult::Limits(),
       /*dropped_event_level_report=*/
       DefaultEventLevelReport(base::Time::UnixEpoch()),
       /*trigger_debug_key=*/absl::nullopt,
       R"json([{
         "body": {
           "attribution_destination": "https://conversion.test",
           "randomized_trigger_rate": 0.0,
           "report_id": "21abd97f-73e8-4b88-9389-a9fee6abda5e",
           "scheduled_report_time": "3600",
           "source_event_id": "123",
           "source_type": "navigation",
           "trigger_data": "0"
         },
         "type": "trigger-event-low-priority"
       }])json"},
      {EventLevelResult::kDroppedForNoise,
       /*replaced_event_level_report=*/absl::nullopt,
       /*new_event_level_report=*/absl::nullopt,
       /*source=*/SourceBuilder().BuildStored(), CreateReportResult::Limits(),
       /*dropped_event_level_report=*/absl::nullopt,
       /*trigger_debug_key=*/absl::nullopt,
       R"json([{
         "body": {
           "attribution_destination": "https://conversion.test",
           "source_event_id": "123",
           "source_site": "https://impression.test"
         },
         "type": "trigger-event-noise"
       }])json"},
      {EventLevelResult::kExcessiveReportingOrigins,
       /*replaced_event_level_report=*/absl::nullopt,
       /*new_event_level_report=*/absl::nullopt,
       /*source=*/SourceBuilder().BuildStored(),
       CreateReportResult::Limits{
           .rate_limits_max_attribution_reporting_origins = 10},
       /*dropped_event_level_report=*/absl::nullopt,
       /*trigger_debug_key=*/absl::nullopt,
       R"json([{
         "body": {
           "attribution_destination": "https://conversion.test",
           "limit": "10",
           "source_event_id": "123",
           "source_site": "https://impression.test"
         },
         "type": "trigger-reporting-origin-limit"
       }])json"},
      {EventLevelResult::kNoMatchingSourceFilterData,
       /*replaced_event_level_report=*/absl::nullopt,
       /*new_event_level_report=*/absl::nullopt,
       /*source=*/SourceBuilder().BuildStored(), CreateReportResult::Limits(),
       /*dropped_event_level_report=*/absl::nullopt,
       /*trigger_debug_key=*/absl::nullopt,
       R"json([{
         "body": {
           "attribution_destination": "https://conversion.test",
           "source_event_id": "123",
           "source_site": "https://impression.test"
         },
         "type": "trigger-no-matching-filter-data"
       }])json"},
      {EventLevelResult::kProhibitedByBrowserPolicy,
       /*replaced_event_level_report=*/absl::nullopt,
       /*new_event_level_report=*/absl::nullopt,
       /*source=*/absl::nullopt, CreateReportResult::Limits(),
       /*dropped_event_level_report=*/absl::nullopt,
       /*trigger_debug_key=*/absl::nullopt,
       /*expected_report_body=*/nullptr},
      {EventLevelResult::kNoMatchingConfigurations,
       /*replaced_event_level_report=*/absl::nullopt,
       /*new_event_level_report=*/absl::nullopt,
       /*source=*/SourceBuilder().BuildStored(), CreateReportResult::Limits(),
       /*dropped_event_level_report=*/absl::nullopt,
       /*trigger_debug_key=*/absl::nullopt,
       R"json([{
         "body": {
           "attribution_destination": "https://conversion.test",
           "source_event_id": "123",
           "source_site": "https://impression.test"
         },
         "type": "trigger-event-no-matching-configurations"
       }])json"},
      {EventLevelResult::kExcessiveReports,
       /*replaced_event_level_report=*/absl::nullopt,
       /*new_event_level_report=*/absl::nullopt,
       /*source=*/SourceBuilder(base::Time::UnixEpoch()).BuildStored(),
       CreateReportResult::Limits(),
       /*dropped_event_level_report=*/
       DefaultEventLevelReport(base::Time::UnixEpoch()),
       /*trigger_debug_key=*/absl::nullopt,
       R"json([{
         "body": {
           "attribution_destination": "https://conversion.test",
           "randomized_trigger_rate": 0.0,
           "report_id": "21abd97f-73e8-4b88-9389-a9fee6abda5e",
           "scheduled_report_time": "3600",
           "source_event_id": "123",
           "source_type": "navigation",
           "trigger_data": "0"
         },
         "type": "trigger-event-excessive-reports"
       }])json"},
      {EventLevelResult::kFalselyAttributedSource,
       /*replaced_event_level_report=*/absl::nullopt,
       /*new_event_level_report=*/absl::nullopt,
       /*source=*/SourceBuilder().BuildStored(), CreateReportResult::Limits(),
       /*dropped_event_level_report=*/absl::nullopt,
       /*trigger_debug_key=*/absl::nullopt,
       R"json([{
         "body": {
           "attribution_destination": "https://conversion.test",
           "source_event_id": "123",
           "source_site": "https://impression.test"
         },
         "type": "trigger-event-noise"
       }])json"},
      {EventLevelResult::kReportWindowNotStarted,
       /*replaced_event_level_report=*/absl::nullopt,
       /*new_event_level_report=*/absl::nullopt,
       /*source=*/SourceBuilder().BuildStored(), CreateReportResult::Limits(),
       /*dropped_event_level_report=*/absl::nullopt,
       /*trigger_debug_key=*/absl::nullopt,
       R"json([{
         "body": {
           "attribution_destination": "https://conversion.test",
           "source_event_id": "123",
           "source_site": "https://impression.test"
         },
         "type": "trigger-event-report-window-not-started"
       }])json"},
      {EventLevelResult::kReportWindowPassed,
       /*replaced_event_level_report=*/absl::nullopt,
       /*new_event_level_report=*/absl::nullopt,
       /*source=*/SourceBuilder().BuildStored(), CreateReportResult::Limits(),
       /*dropped_event_level_report=*/absl::nullopt,
       /*trigger_debug_key=*/absl::nullopt,
       R"json([{
         "body": {
           "attribution_destination": "https://conversion.test",
           "source_event_id": "123",
           "source_site": "https://impression.test"
         },
         "type": "trigger-event-report-window-passed"
       }])json"},
      {EventLevelResult::kNotRegistered,
       /*replaced_event_level_report=*/absl::nullopt,
       /*new_event_level_report=*/absl::nullopt,
       /*source=*/absl::nullopt, CreateReportResult::Limits(),
       /*dropped_event_level_report=*/absl::nullopt,
       /*trigger_debug_key=*/absl::nullopt,
       /*expected_report_body=*/nullptr},
  };

  for (bool is_debug_cookie_set : {false, true}) {
    for (const auto& test_case : kTestCases) {
      absl::optional<AttributionDebugReport> report =
          AttributionDebugReport::Create(
              TriggerBuilder()
                  .SetDebugReporting(true)
                  .SetDebugKey(test_case.trigger_debug_key)
                  .Build(),
              is_debug_cookie_set,
              CreateReportResult(
                  /*trigger_time=*/base::Time::Now(), test_case.result,
                  AggregatableResult::kNotRegistered,
                  test_case.replaced_event_level_report,
                  test_case.new_event_level_report,
                  /*new_aggregatable_report=*/absl::nullopt, test_case.source,
                  test_case.limits, test_case.dropped_event_level_report));
      if (is_debug_cookie_set) {
        EXPECT_EQ(report.has_value(), test_case.expected_report_body != nullptr)
            << test_case.result << ", " << is_debug_cookie_set;
        if (report) {
          EXPECT_EQ(report->ReportBody(),
                    base::test::ParseJson(test_case.expected_report_body))
              << test_case.result << ", " << is_debug_cookie_set;
        }
      } else {
        EXPECT_FALSE(report) << test_case.result << ", " << is_debug_cookie_set;
      }
    }
  }
}

TEST(AttributionDebugReportTest, AggregatableAttributionDebugging) {
  const struct {
    AggregatableResult result;
    absl::optional<AttributionReport> new_aggregatable_report;
    CreateReportResult::Limits limits;
    absl::optional<uint64_t> source_debug_key;
    absl::optional<uint64_t> trigger_debug_key;
    const char* expected_report_body;
  } kTestCases[] = {
      {AggregatableResult::kSuccess, DefaultAggregatableReport(),
       CreateReportResult::Limits(),
       /*source_debug_key=*/absl::nullopt,
       /*trigger_debug_key=*/absl::nullopt,
       /*expected_report_body=*/nullptr},
      {AggregatableResult::kInternalError,
       /*new_aggregatable_report=*/absl::nullopt, CreateReportResult::Limits(),
       /*source_debug_key=*/456,
       /*trigger_debug_key=*/absl::nullopt,
       R"json([{
         "body": {
           "attribution_destination": "https://conversion.test",
           "source_debug_key": "456",
           "source_event_id": "123",
           "source_site": "https://impression.test"
         },
         "type": "trigger-unknown-error"
       }])json"},
      {AggregatableResult::kNoCapacityForConversionDestination,
       /*new_aggregatable_report=*/absl::nullopt,
       CreateReportResult::Limits{.max_aggregatable_reports_per_destination =
                                      20},
       /*source_debug_key=*/absl::nullopt,
       /*trigger_debug_key=*/789,
       R"json([{
         "body": {
           "attribution_destination": "https://conversion.test",
           "limit": "20",
           "source_event_id": "123",
           "source_site": "https://impression.test",
           "trigger_debug_key": "789"
         },
         "type": "trigger-aggregate-storage-limit"
       }])json"},
      {AggregatableResult::kExcessiveAttributions,
       /*new_aggregatable_report=*/absl::nullopt,
       CreateReportResult::Limits{.rate_limits_max_attributions = 10},
       /*source_debug_key=*/789,
       /*trigger_debug_key=*/456,
       R"json([{
         "body": {
           "attribution_destination": "https://conversion.test",
           "limit": "10",
           "source_debug_key": "789",
           "source_event_id": "123",
           "source_site": "https://impression.test",
           "trigger_debug_key": "456"
         },
         "type": "trigger-attributions-per-source-destination-limit"
       }])json"},
      {AggregatableResult::kExcessiveReportingOrigins,
       /*new_aggregatable_report=*/absl::nullopt,
       CreateReportResult::Limits{
           .rate_limits_max_attribution_reporting_origins = 5},
       /*source_debug_key=*/absl::nullopt,
       /*trigger_debug_key=*/absl::nullopt,
       R"json([{
         "body": {
           "attribution_destination": "https://conversion.test",
           "limit": "5",
           "source_event_id": "123",
           "source_site": "https://impression.test"
         },
         "type": "trigger-reporting-origin-limit"
       }])json"},
      {AggregatableResult::kNoHistograms,
       /*new_aggregatable_report=*/absl::nullopt, CreateReportResult::Limits(),
       /*source_debug_key=*/absl::nullopt,
       /*trigger_debug_key=*/absl::nullopt,
       R"json([{
         "body": {
           "attribution_destination": "https://conversion.test",
           "source_event_id": "123",
           "source_site": "https://impression.test"
         },
         "type": "trigger-aggregate-no-contributions"
       }])json"},
      {AggregatableResult::kInsufficientBudget,
       /*new_aggregatable_report=*/absl::nullopt,
       CreateReportResult::Limits{.aggregatable_budget_per_source = 10},
       /*source_debug_key=*/absl::nullopt,
       /*trigger_debug_key=*/absl::nullopt,
       R"json([{
         "body": {
           "attribution_destination": "https://conversion.test",
           "limit": "10",
           "source_event_id": "123",
           "source_site": "https://impression.test"
         },
         "type": "trigger-aggregate-insufficient-budget"
       }])json"},
      {AggregatableResult::kExcessiveReports,
       /*new_aggregatable_report=*/absl::nullopt,
       CreateReportResult::Limits{.max_aggregatable_reports_per_source = 10},
       /*source_debug_key=*/absl::nullopt,
       /*trigger_debug_key=*/absl::nullopt,
       R"json([{
         "body": {
           "attribution_destination": "https://conversion.test",
           "limit": "10",
           "source_event_id": "123",
           "source_site": "https://impression.test"
         },
         "type": "trigger-aggregate-excessive-reports"
       }])json"},
      {AggregatableResult::kNoMatchingSourceFilterData,
       /*new_aggregatable_report=*/absl::nullopt, CreateReportResult::Limits(),
       /*source_debug_key=*/absl::nullopt,
       /*trigger_debug_key=*/absl::nullopt,
       R"json([{
         "body": {
           "attribution_destination": "https://conversion.test",
           "source_event_id": "123",
           "source_site": "https://impression.test"
         },
         "type": "trigger-no-matching-filter-data"
       }])json"},
      {AggregatableResult::kNotRegistered,
       /*new_aggregatable_report=*/absl::nullopt, CreateReportResult::Limits(),
       /*source_debug_key=*/absl::nullopt,
       /*trigger_debug_key=*/absl::nullopt,
       /*expected_report_body=*/nullptr},
      {AggregatableResult::kDeduplicated,
       /*new_aggregatable_report=*/absl::nullopt, CreateReportResult::Limits(),
       /*source_debug_key=*/absl::nullopt,
       /*trigger_debug_key=*/absl::nullopt,
       R"json([{
         "body": {
           "attribution_destination": "https://conversion.test",
           "source_event_id": "123",
           "source_site": "https://impression.test"
         },
         "type": "trigger-aggregate-deduplicated"
       }])json"},
      {AggregatableResult::kReportWindowPassed,
       /*new_aggregatable_report=*/absl::nullopt, CreateReportResult::Limits(),
       /*source_debug_key=*/absl::nullopt,
       /*trigger_debug_key=*/absl::nullopt,
       R"json([{
         "body": {
           "attribution_destination": "https://conversion.test",
           "source_event_id": "123",
           "source_site": "https://impression.test"
         },
         "type": "trigger-aggregate-report-window-passed"
       }])json"},
  };

  for (bool is_debug_cookie_set : {false, true}) {
    for (const auto& test_case : kTestCases) {
      absl::optional<AttributionDebugReport> report =
          AttributionDebugReport::Create(
              TriggerBuilder()
                  .SetDebugReporting(true)
                  .SetDebugKey(test_case.trigger_debug_key)
                  .Build(),
              is_debug_cookie_set,
              CreateReportResult(
                  /*trigger_time=*/base::Time::Now(),
                  EventLevelResult::kSuccess, test_case.result,
                  /*replaced_event_level_report=*/absl::nullopt,
                  /*new_event_level_report=*/DefaultEventLevelReport(),
                  test_case.new_aggregatable_report,
                  SourceBuilder()
                      .SetDebugKey(test_case.source_debug_key)
                      .BuildStored(),
                  test_case.limits));
      if (is_debug_cookie_set) {
        EXPECT_EQ(report.has_value(), test_case.expected_report_body != nullptr)
            << test_case.result << ", " << is_debug_cookie_set;
        if (report) {
          EXPECT_EQ(report->ReportBody(),
                    base::test::ParseJson(test_case.expected_report_body))
              << test_case.result << ", " << is_debug_cookie_set;
        }
      } else {
        EXPECT_FALSE(report) << test_case.result << ", " << is_debug_cookie_set;
      }
    }
  }
}

TEST(AttributionDebugReportTest, OsRegistrationDebugging) {
  const struct {
    const char* description;
    OsRegistration registration;
    const char* expected_body;
  } kTestCases[] = {
      {
          "os_source",
          OsRegistration(
              /*registration_url=*/GURL("https://a.test/x"),
              /*debug_reporting=*/true,
              /*top_level_origin=*/url::Origin::Create(GURL("https://b.test")),
              AttributionInputEvent(), /*is_within_fenced_frame=*/false),
          R"json([{
            "body": {
              "context_site": "https://b.test",
              "registration_url": "https://a.test/x"
            },
            "type": "os-source-delegated"
          }])json",
      },
      {
          "os_trigger",
          OsRegistration(
              /*registration_url=*/GURL("https://a.test/x"),
              /*debug_reporting=*/true,
              /*top_level_origin=*/url::Origin::Create(GURL("https://b.test")),
              /*input_event=*/absl::nullopt, /*is_within_fenced_frame=*/false),
          R"json([{
            "body": {
              "context_site": "https://b.test",
              "registration_url": "https://a.test/x"
            },
            "type": "os-trigger-delegated"
          }])json",
      },
      {
          "debug_reporting_disabled",
          OsRegistration(
              /*registration_url=*/GURL("https://a.test/x"),
              /*debug_reporting=*/false,
              /*top_level_origin=*/url::Origin::Create(GURL("https://b.test")),
              /*input_event=*/absl::nullopt, /*is_within_fenced_frame=*/false),
          nullptr,
      },
      {
          "within_fenced_frame",
          OsRegistration(
              /*registration_url=*/GURL("https://a.test/x"),
              /*debug_reporting=*/true,
              /*top_level_origin=*/url::Origin::Create(GURL("https://b.test")),
              /*input_event=*/absl::nullopt, /*is_within_fenced_frame=*/true),
          nullptr,
      },
      {
          "non_suitable_registration_origin",
          OsRegistration(
              /*registration_url=*/GURL("http://a.test/x"),
              /*debug_reporting=*/true,
              /*top_level_origin=*/url::Origin::Create(GURL("https://b.test")),
              /*input_event=*/absl::nullopt, /*is_within_fenced_frame=*/false),
          nullptr,
      },
  };

  for (const auto& test_case : kTestCases) {
    absl::optional<AttributionDebugReport> report =
        AttributionDebugReport::Create(test_case.registration);
    EXPECT_EQ(report.has_value(), test_case.expected_body != nullptr)
        << test_case.description;
    if (test_case.expected_body) {
      EXPECT_EQ(report->ReportBody(),
                base::test::ParseJson(test_case.expected_body))
          << test_case.description;
    }
  }
}

}  // namespace
}  // namespace content
