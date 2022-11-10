// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_debug_report.h"

#include "base/test/values_test_util.h"
#include "content/browser/attribution_reporting/attribution_observer_types.h"
#include "content/browser/attribution_reporting/attribution_storage.h"
#include "content/browser/attribution_reporting/attribution_test_utils.h"
#include "content/browser/attribution_reporting/attribution_trigger.h"
#include "content/browser/attribution_reporting/storable_source.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace content {
namespace {

using EventLevelResult = ::content::AttributionTrigger::EventLevelResult;
using AggregatableResult = ::content::AttributionTrigger::AggregatableResult;

AttributionReport DefaultEventLevelReport() {
  return ReportBuilder(
             AttributionInfoBuilder(SourceBuilder().BuildStored()).Build())
      .Build();
}

AttributionReport DefaultAggregatableReport() {
  return ReportBuilder(
             AttributionInfoBuilder(SourceBuilder().BuildStored()).Build())
      .SetAggregatableHistogramContributions(
          {AggregatableHistogramContribution(1, 2)})
      .BuildAggregatableAttribution();
}

TEST(AttributionDebugReportTest, NoDebugReporting_NoReportReturned) {
  EXPECT_FALSE(AttributionDebugReport::Create(
      SourceBuilder().Build(),
      /*is_debug_cookie_set=*/false,
      AttributionStorage::StoreSourceResult(
          StorableSource::Result::kInsufficientUniqueDestinationCapacity,
          /*min_fake_report_time=*/absl::nullopt,
          /*max_destinations_per_source_site_reporting_origin=*/3)));

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
          AttributionStorage::StoreSourceResult(
              StorableSource::Result::kInsufficientUniqueDestinationCapacity,
              /*min_fake_report_time=*/absl::nullopt,
              /*max_destinations_per_source_site_reporting_origin=*/3));
  ASSERT_TRUE(report);

  static constexpr char kExpectedJsonString[] = R"([{
    "body": {
      "attribution_destination": "https://conversion.test",
      "limit": 3,
      "source_event_id": "123",
      "source_site": "https://impression.test"
    },
    "type": "source-destination-limit"
  }])";
  EXPECT_EQ(report->ReportBody(), base::test::ParseJson(kExpectedJsonString));

  EXPECT_EQ(report->ReportURL(), GURL("https://report.test/.well-known/"
                                      "attribution-reporting/debug/verbose"));
}

TEST(AttributionDebugReportTest, WithinFencedFrame_NoDebugReport) {
  AttributionConfig config;
  config.max_destinations_per_source_site_reporting_origin = 3;

  EXPECT_FALSE(AttributionDebugReport::Create(
      SourceBuilder()
          .SetDebugReporting(true)
          .SetIsWithinFencedFrame(true)
          .Build(),
      /*is_debug_cookie_set=*/false,
      AttributionStorage::StoreSourceResult(
          StorableSource::Result::kInsufficientUniqueDestinationCapacity,
          /*min_fake_report_time=*/absl::nullopt,
          /*max_destinations_per_source_site_reporting_origin=*/3)));

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
    absl::optional<int> max_destinations_per_source_site_reporting_origin;
    absl::optional<int> max_sources_per_origin;
    const char* expected_report_body_without_cookie;
    const char* expected_report_body_with_cookie;
  } kTestCases[] = {
      {StorableSource::Result::kSuccess,
       /*max_destinations_per_source_site_reporting_origin=*/absl::nullopt,
       /*max_sources_per_origin=*/absl::nullopt,
       /*expected_report_body_without_cookie=*/nullptr,
       /*expected_report_body_with_cookie=*/nullptr},
      {StorableSource::Result::kInternalError,
       /*max_destinations_per_source_site_reporting_origin=*/absl::nullopt,
       /*max_sources_per_origin=*/absl::nullopt,
       /*expected_report_body_without_cookie=*/nullptr,
       /*expected_report_body_with_cookie=*/
       R"json([{
         "body": {
           "attribution_destination": "https://conversion.test",
           "source_event_id": "123",
           "source_site": "https://impression.test"
         },
         "type": "source-unknown-error"
       }])json"},
      {StorableSource::Result::kInsufficientSourceCapacity,
       /*max_destinations_per_source_site_reporting_origin=*/absl::nullopt,
       /*max_sources_per_origin=*/10,
       /*expected_report_body_without_cookie=*/nullptr,
       /*expected_report_body_with_cookie=*/
       R"json([{
         "body": {
           "attribution_destination": "https://conversion.test",
           "limit": 10,
           "source_event_id": "123",
           "source_site": "https://impression.test"
         },
         "type": "source-storage-limit"
       }])json"},
      {StorableSource::Result::kProhibitedByBrowserPolicy,
       /*max_destinations_per_source_site_reporting_origin=*/absl::nullopt,
       /*max_sources_per_origin=*/absl::nullopt,
       /*expected_report_body_without_cookie=*/nullptr,
       /*expected_report_body_with_cookie=*/nullptr},
      {StorableSource::Result::kInsufficientUniqueDestinationCapacity,
       /*max_destinations_per_source_site_reporting_origin=*/3,
       /*max_sources_per_origin=*/absl::nullopt,
       /*expected_report_body_without_cookie=*/
       R"json([{
         "body": {
           "attribution_destination": "https://conversion.test",
           "limit": 3,
           "source_event_id": "123",
           "source_site": "https://impression.test"
         },
         "type": "source-destination-limit"
       }])json",
       /*expected_report_body_with_cookie=*/
       R"json([{
         "body": {
           "attribution_destination": "https://conversion.test",
           "limit": 3,
           "source_event_id": "123",
           "source_site": "https://impression.test"
         },
         "type": "source-destination-limit"
       }])json"},
      {StorableSource::Result::kSuccessNoised,
       /*max_destinations_per_source_site_reporting_origin=*/absl::nullopt,
       /*max_sources_per_origin=*/absl::nullopt,
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
  };

  for (bool is_debug_cookie_set : {false, true}) {
    for (const auto& test_case : kTestCases) {
      absl::optional<AttributionDebugReport> report =
          AttributionDebugReport::Create(
              SourceBuilder().SetDebugReporting(true).Build(),
              is_debug_cookie_set,
              AttributionStorage::StoreSourceResult(
                  test_case.result,
                  /*min_fake_report_time=*/absl::nullopt,
                  test_case.max_destinations_per_source_site_reporting_origin,
                  test_case.max_sources_per_origin));
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
}

TEST(AttributionDebugReportTest, TriggerDebugging) {
  const struct {
    EventLevelResult event_level_result;
    AggregatableResult aggregatable_result;
    const char* expected_report_body;
  } kTestCases[] = {
      {EventLevelResult::kNoMatchingImpressions,
       AggregatableResult::kNoMatchingImpressions,
       /*expected_report_body=*/
       R"json([{
         "body": {
           "attribution_destination": "https://conversion.test"
         },
         "type": "trigger-no-matching-source"
       }])json"},
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
                  /*new_aggregatable_report=*/absl::nullopt));
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
    const char* expected_report_body;
  } kTestCases[] = {
      {EventLevelResult::kSuccess,
       /*replaced_event_level_report=*/absl::nullopt,
       /*new_event_level_report=*/DefaultEventLevelReport(),
       /*expected_report_body=*/nullptr},
      {EventLevelResult::kSuccessDroppedLowerPriority,
       /*replaced_event_level_report=*/DefaultEventLevelReport(),
       /*new_event_level_report=*/DefaultEventLevelReport(),
       /*expected_report_body=*/nullptr},
      {EventLevelResult::kInternalError,
       /*replaced_event_level_report=*/absl::nullopt,
       /*new_event_level_report=*/absl::nullopt,
       /*expected_report_body=*/nullptr},
      {EventLevelResult::kNoCapacityForConversionDestination,
       /*replaced_event_level_report=*/absl::nullopt,
       /*new_event_level_report=*/absl::nullopt,
       /*expected_report_body=*/nullptr},
      {EventLevelResult::kNoMatchingImpressions,
       /*replaced_event_level_report=*/absl::nullopt,
       /*new_event_level_report=*/absl::nullopt,
       R"json([{
         "body": {
           "attribution_destination": "https://conversion.test"
         },
         "type": "trigger-no-matching-source"
       }])json"},
      {EventLevelResult::kDeduplicated,
       /*replaced_event_level_report=*/absl::nullopt,
       /*new_event_level_report=*/absl::nullopt,
       /*expected_report_body=*/nullptr},
      {EventLevelResult::kExcessiveAttributions,
       /*replaced_event_level_report=*/absl::nullopt,
       /*new_event_level_report=*/absl::nullopt,
       /*expected_report_body=*/nullptr},
      {EventLevelResult::kPriorityTooLow,
       /*replaced_event_level_report=*/absl::nullopt,
       /*new_event_level_report=*/absl::nullopt,
       /*expected_report_body=*/nullptr},
      {EventLevelResult::kDroppedForNoise,
       /*replaced_event_level_report=*/absl::nullopt,
       /*new_event_level_report=*/absl::nullopt,
       /*expected_report_body=*/nullptr},
      {EventLevelResult::kExcessiveReportingOrigins,
       /*replaced_event_level_report=*/absl::nullopt,
       /*new_event_level_report=*/absl::nullopt,
       /*expected_report_body=*/nullptr},
      {EventLevelResult::kNoMatchingSourceFilterData,
       /*replaced_event_level_report=*/absl::nullopt,
       /*new_event_level_report=*/absl::nullopt,
       /*expected_report_body=*/nullptr},
      {EventLevelResult::kProhibitedByBrowserPolicy,
       /*replaced_event_level_report=*/absl::nullopt,
       /*new_event_level_report=*/absl::nullopt,
       /*expected_report_body=*/nullptr},
      {EventLevelResult::kNoMatchingConfigurations,
       /*replaced_event_level_report=*/absl::nullopt,
       /*new_event_level_report=*/absl::nullopt,
       /*expected_report_body=*/nullptr},
      {EventLevelResult::kExcessiveReports,
       /*replaced_event_level_report=*/absl::nullopt,
       /*new_event_level_report=*/absl::nullopt,
       /*expected_report_body=*/nullptr},
  };

  for (bool is_debug_cookie_set : {false, true}) {
    for (const auto& test_case : kTestCases) {
      absl::optional<AttributionDebugReport> report =
          AttributionDebugReport::Create(
              TriggerBuilder().SetDebugReporting(true).Build(),
              is_debug_cookie_set,
              CreateReportResult(
                  /*trigger_time=*/base::Time::Now(), test_case.result,
                  AggregatableResult::kNotRegistered,
                  test_case.replaced_event_level_report,
                  test_case.new_event_level_report,
                  /*new_aggregatable_report=*/absl::nullopt));
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
    const char* expected_report_body;
  } kTestCases[] = {
      {AggregatableResult::kSuccess, DefaultAggregatableReport(),
       /*expected_report_body=*/nullptr},
      {AggregatableResult::kInternalError,
       /*new_aggregatable_report=*/absl::nullopt,
       /*expected_report_body=*/nullptr},
      {AggregatableResult::kNoCapacityForConversionDestination,
       /*new_aggregatable_report=*/absl::nullopt,
       /*expected_report_body=*/nullptr},
      {AggregatableResult::kNoMatchingImpressions,
       /*new_aggregatable_report=*/absl::nullopt,
       R"json([{
         "body": {
           "attribution_destination": "https://conversion.test"
         },
         "type": "trigger-no-matching-source"
       }])json"},
      {AggregatableResult::kExcessiveAttributions,
       /*new_aggregatable_report=*/absl::nullopt,
       /*expected_report_body=*/nullptr},
      {AggregatableResult::kExcessiveReportingOrigins,
       /*new_aggregatable_report=*/absl::nullopt,
       /*expected_report_body=*/nullptr},
      {AggregatableResult::kNoHistograms,
       /*new_aggregatable_report=*/absl::nullopt,
       /*expected_report_body=*/nullptr},
      {AggregatableResult::kInsufficientBudget,
       /*new_aggregatable_report=*/absl::nullopt,
       /*expected_report_body=*/nullptr},
      {AggregatableResult::kNoMatchingSourceFilterData,
       /*new_aggregatable_report=*/absl::nullopt,
       /*expected_report_body=*/nullptr},
      {AggregatableResult::kNotRegistered,
       /*new_aggregatable_report=*/absl::nullopt,
       /*expected_report_body=*/nullptr},
      {AggregatableResult::kProhibitedByBrowserPolicy,
       /*new_aggregatable_report=*/absl::nullopt,
       /*expected_report_body=*/nullptr},
      {AggregatableResult::kDeduplicated,
       /*new_aggregatable_report=*/absl::nullopt,
       /*expected_report_body=*/nullptr},
  };

  for (bool is_debug_cookie_set : {false, true}) {
    for (const auto& test_case : kTestCases) {
      absl::optional<AttributionDebugReport> report =
          AttributionDebugReport::Create(
              TriggerBuilder().SetDebugReporting(true).Build(),
              is_debug_cookie_set,
              CreateReportResult(
                  /*trigger_time=*/base::Time::Now(),
                  EventLevelResult::kSuccess, test_case.result,
                  /*replaced_event_level_report=*/absl::nullopt,
                  /*new_event_level_report=*/DefaultEventLevelReport(),
                  test_case.new_aggregatable_report));
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

}  // namespace
}  // namespace content
