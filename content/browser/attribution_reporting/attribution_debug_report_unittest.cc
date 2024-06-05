// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_debug_report.h"

#include <stdint.h>

#include <optional>

#include "base/test/scoped_feature_list.h"
#include "base/test/values_test_util.h"
#include "base/time/time.h"
#include "components/attribution_reporting/os_registration.h"
#include "components/attribution_reporting/os_registration_error.mojom.h"
#include "components/attribution_reporting/registrar.h"
#include "components/attribution_reporting/registration_header_error.h"
#include "components/attribution_reporting/source_registration_error.mojom.h"
#include "components/attribution_reporting/suitable_origin.h"
#include "components/attribution_reporting/trigger_registration_error.mojom.h"
#include "content/browser/attribution_reporting/attribution_config.h"
#include "content/browser/attribution_reporting/attribution_features.h"
#include "content/browser/attribution_reporting/attribution_input_event.h"
#include "content/browser/attribution_reporting/attribution_test_utils.h"
#include "content/browser/attribution_reporting/attribution_trigger.h"
#include "content/browser/attribution_reporting/create_report_result.h"
#include "content/browser/attribution_reporting/os_registration.h"
#include "content/browser/attribution_reporting/storable_source.h"
#include "content/browser/attribution_reporting/store_source_result.h"
#include "content/browser/attribution_reporting/stored_source.h"
#include "content/public/browser/global_routing_id.h"
#include "net/base/schemeful_site.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/aggregation_service/aggregatable_report.mojom.h"
#include "url/gurl.h"

namespace content {
namespace {

using ::testing::Message;

using AggregatableResult = ::content::AttributionTrigger::AggregatableResult;
using EventLevelResult = ::content::AttributionTrigger::EventLevelResult;
using ::attribution_reporting::OsRegistrationItem;
using ::attribution_reporting::RegistrationHeaderError;
using ::attribution_reporting::SuitableOrigin;

constexpr attribution_reporting::Registrar kRegistrar =
    attribution_reporting::Registrar::kWeb;

constexpr base::Time kSourceTime;

constexpr StoredSource::Id kSourceId(1);

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
          {blink::mojom::AggregatableReportHistogramContribution(
              1, 2, /*filtering_id=*/std::nullopt)})
      .BuildAggregatableAttribution();
}

bool OperationAllowed() {
  return true;
}

bool OperationProhibited() {
  return false;
}

TEST(AttributionDebugReportTest, NoDebugReporting_NoReportReturned) {
  EXPECT_FALSE(AttributionDebugReport::Create(
      &OperationAllowed,
      StoreSourceResult(
          SourceBuilder().Build(),
          /*is_noised=*/false, kSourceTime,
          StoreSourceResult::InsufficientUniqueDestinationCapacity(3))));

  EXPECT_FALSE(AttributionDebugReport::Create(
      &OperationAllowed,
      /*is_debug_cookie_set=*/true,
      CreateReportResult(/*trigger_time=*/base::Time::Now(),
                         TriggerBuilder().Build(),
                         EventLevelResult::kNoMatchingImpressions,
                         AggregatableResult::kNoMatchingImpressions)));
}

TEST(AttributionDebugReportTest, OperationProhibited_NoReportReturned) {
  EXPECT_FALSE(AttributionDebugReport::Create(
      &OperationProhibited,
      StoreSourceResult(
          SourceBuilder().SetDebugReporting(true).Build(),
          /*is_noised=*/false, kSourceTime,
          StoreSourceResult::InsufficientUniqueDestinationCapacity(3))));

  EXPECT_FALSE(AttributionDebugReport::Create(
      &OperationProhibited,
      /*is_debug_cookie_set=*/true,
      CreateReportResult(/*trigger_time=*/base::Time::Now(),
                         TriggerBuilder().SetDebugReporting(true).Build(),
                         EventLevelResult::kNoMatchingImpressions,
                         AggregatableResult::kNoMatchingImpressions)));
}

TEST(AttributionDebugReportTest,
     SourceDestinationLimitError_ValidReportReturned) {
  std::optional<AttributionDebugReport> report = AttributionDebugReport::Create(
      &OperationAllowed,
      StoreSourceResult(
          SourceBuilder()
              .SetDebugReporting(true)
              .SetDebugCookieSet(true)
              .Build(),
          /*is_noised=*/false, kSourceTime,
          StoreSourceResult::InsufficientUniqueDestinationCapacity(3)));
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
      &OperationAllowed,
      StoreSourceResult(
          SourceBuilder()
              .SetDebugReporting(true)
              .SetIsWithinFencedFrame(true)
              .Build(),
          /*is_noised=*/false, kSourceTime,
          StoreSourceResult::InsufficientUniqueDestinationCapacity(3))));

  EXPECT_FALSE(AttributionDebugReport::Create(
      &OperationAllowed,
      /*is_debug_cookie_set=*/true,
      CreateReportResult(/*trigger_time=*/base::Time::Now(),
                         TriggerBuilder()
                             .SetDebugReporting(true)
                             .SetIsWithinFencedFrame(true)
                             .Build(),
                         EventLevelResult::kNoMatchingImpressions,
                         AggregatableResult::kNoMatchingImpressions)));
}

TEST(AttributionDebugReportTest, SourceDebugging) {
  const struct {
    StoreSourceResult::Result result;
    std::optional<uint64_t> debug_key;
    bool is_noised = false;
    const char* expected_report_body = nullptr;
  } kTestCases[] = {
      {
          .result = StoreSourceResult::Success(
              /*min_fake_report_time=*/std::nullopt, kSourceId),
          .debug_key = std::nullopt,
          .expected_report_body = R"json([{
            "body": {
              "attribution_destination": "https://conversion.test",
              "source_event_id": "123",
              "source_site": "https://impression.test"
            },
            "type": "source-success"
          }])json",
      },
      {
          .result = StoreSourceResult::InternalError(),
          .debug_key = 456,
          .expected_report_body = R"json([{
            "body": {
              "attribution_destination": "https://conversion.test",
              "source_debug_key": "456",
              "source_event_id": "123",
              "source_site": "https://impression.test"
            },
            "type": "source-unknown-error"
          }])json",
      },
      {
          .result = StoreSourceResult::InsufficientSourceCapacity(10),
          .debug_key = std::nullopt,
          .expected_report_body = R"json([{
            "body": {
              "attribution_destination": "https://conversion.test",
              "limit": "10",
              "source_event_id": "123",
              "source_site": "https://impression.test"
            },
            "type": "source-storage-limit"
          }])json",
      },
      {
          .result = StoreSourceResult::ProhibitedByBrowserPolicy(),
      },
      {
          .result = StoreSourceResult::InsufficientUniqueDestinationCapacity(3),
          .expected_report_body = R"json([{
            "body": {
              "attribution_destination": "https://conversion.test",
              "limit": "3",
              "source_event_id": "123",
              "source_site": "https://impression.test"
            },
            "type": "source-destination-limit"
          }])json",
      },
      {
          .result = StoreSourceResult::Success(
              /*min_fake_report_time=*/std::nullopt, kSourceId),
          .is_noised = true,
          .expected_report_body = R"json([{
            "body": {
              "attribution_destination": "https://conversion.test",
              "source_event_id": "123",
              "source_site": "https://impression.test"
            },
            "type": "source-noised"
          }])json",
      },
      {
          .result = StoreSourceResult::ExcessiveReportingOrigins(),
          .debug_key = 789,
          .expected_report_body = R"json([{
            "body": {
              "attribution_destination": "https://conversion.test",
              "source_debug_key": "789",
              "source_event_id": "123",
              "source_site": "https://impression.test"
            },
            "type": "source-success"
          }])json",
      },
      {
          .result = StoreSourceResult::ExcessiveReportingOrigins(),
          .debug_key = 789,
          .is_noised = true,
          .expected_report_body = R"json([{
            "body": {
              "attribution_destination": "https://conversion.test",
              "source_debug_key": "789",
              "source_event_id": "123",
              "source_site": "https://impression.test"
            },
            "type": "source-noised"
          }])json",
      },
      {
          .result = StoreSourceResult::DestinationGlobalLimitReached(),
          .debug_key = 789,
          .expected_report_body = R"json([{
            "body": {
              "attribution_destination": "https://conversion.test",
              "source_debug_key": "789",
              "source_event_id": "123",
              "source_site": "https://impression.test"
            },
            "type": "source-success"
          }])json",
      },
      {
          .result = StoreSourceResult::DestinationGlobalLimitReached(),
          .debug_key = 789,
          .is_noised = true,
          .expected_report_body = R"json([{
            "body": {
              "attribution_destination": "https://conversion.test",
              "source_debug_key": "789",
              "source_event_id": "123",
              "source_site": "https://impression.test"
            },
            "type": "source-noised"
          }])json",
      },
      {
          .result = StoreSourceResult::DestinationReportingLimitReached(50),
          .debug_key = std::nullopt,
          .expected_report_body = R"json([{
            "body": {
              "attribution_destination": "https://conversion.test",
              "limit": "50",
              "source_event_id": "123",
              "source_site": "https://impression.test"
            },
            "type": "source-destination-rate-limit"
          }])json",
      },
      {
          .result = StoreSourceResult::DestinationBothLimitsReached(50),
          .expected_report_body = R"json([{
            "body": {
              "attribution_destination": "https://conversion.test",
              "limit": "50",
              "source_event_id": "123",
              "source_site": "https://impression.test"
            },
            "type": "source-destination-rate-limit"
          }])json",
      },
      {
          .result = StoreSourceResult::ReportingOriginsPerSiteLimitReached(2),
          .expected_report_body = R"json([{
            "body": {
              "attribution_destination": "https://conversion.test",
              "limit": "2",
              "source_event_id": "123",
              "source_site": "https://impression.test"
            },
            "type": "source-reporting-origin-per-site-limit"
          }])json",
      },
      {
          .result = StoreSourceResult::ExceedsMaxChannelCapacity(3.1),
          .debug_key = std::nullopt,
          .expected_report_body = R"json([{
              "body": {
                "attribution_destination": "https://conversion.test",
                "limit": 3.1,
                "source_event_id": "123",
                "source_site": "https://impression.test"
              },
              "type": "source-channel-capacity-limit"
            }])json",
      },
      {
          .result = StoreSourceResult::ExceedsMaxTriggerStateCardinality(3),
          .debug_key = std::nullopt,
          .expected_report_body = R"json([{
              "body": {
                "attribution_destination": "https://conversion.test",
                "limit": "3",
                "source_event_id": "123",
                "source_site": "https://impression.test"
              },
              "type": "source-trigger-state-cardinality-limit"
            }])json",
      },
  };

  for (bool is_debug_cookie_set : {false, true}) {
    for (const auto& test_case : kTestCases) {
      StoreSourceResult result(SourceBuilder()
                                   .SetDebugReporting(true)
                                   .SetDebugKey(test_case.debug_key)
                                   .SetDebugCookieSet(is_debug_cookie_set)
                                   .Build(),
                               test_case.is_noised, kSourceTime,
                               test_case.result);

      SCOPED_TRACE(Message() << "is_debug_cookie_set: " << is_debug_cookie_set
                             << ", result: " << result.status());

      std::optional<AttributionDebugReport> report =
          AttributionDebugReport::Create(&OperationAllowed, std::move(result));
      if (is_debug_cookie_set) {
        EXPECT_EQ(report.has_value(),
                  test_case.expected_report_body != nullptr);
        if (report) {
          EXPECT_EQ(report->ReportBody(),
                    base::test::ParseJson(test_case.expected_report_body));
        }
      } else {
        EXPECT_FALSE(report);
      }
    }
  }

  // Multiple destinations
  {
    std::optional<AttributionDebugReport> report =
        AttributionDebugReport::Create(
            &OperationAllowed,
            StoreSourceResult(
                SourceBuilder()
                    .SetDebugReporting(true)
                    .SetDebugCookieSet(true)
                    .SetDestinationSites({
                        net::SchemefulSite::Deserialize("https://c.test"),
                        net::SchemefulSite::Deserialize("https://d.test"),
                    })
                    .Build(),
                /*is_noised=*/true, kSourceTime,
                StoreSourceResult::Success(
                    /*min_fake_report_time=*/std::nullopt, kSourceId)));

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
    bool has_matching_source = false;
    CreateReportResult::Limits limits;
    const char* expected_report_body;
  } kTestCases[] = {
      {EventLevelResult::kNoMatchingImpressions,
       AggregatableResult::kNoMatchingImpressions,
       /*has_matching_source=*/false, CreateReportResult::Limits(),
       R"json([{
         "body": {
           "attribution_destination": "https://conversion.test"
         },
         "type": "trigger-no-matching-source"
       }])json"},
      {EventLevelResult::kProhibitedByBrowserPolicy,
       AggregatableResult::kProhibitedByBrowserPolicy,
       /*has_matching_source=*/false, CreateReportResult::Limits(),
       /*expected_report_body=*/nullptr},
      {EventLevelResult::kNoMatchingConfigurations,
       AggregatableResult::kExcessiveAttributions,
       /*has_matching_source=*/true,
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
           "type": "trigger-aggregate-attributions-per-source-destination-limit"
         }
       ])json"},
      {EventLevelResult::kNoMatchingConfigurations,
       AggregatableResult::kInsufficientBudget,
       /*has_matching_source=*/true, CreateReportResult::Limits(),
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
             "limit": "65536",
             "source_event_id": "123",
             "source_site": "https://impression.test"
           },
           "type": "trigger-aggregate-insufficient-budget"
         }
       ])json"},
  };

  for (bool is_source_debug_cookie_set : {false, true}) {
    for (bool is_trigger_debug_cookie_set : {false, true}) {
      for (const auto& test_case : kTestCases) {
        SCOPED_TRACE(
            Message()
            << "is_source_debug_cookie_set: " << is_source_debug_cookie_set
            << ", is_trigger_debug_cookie_set: " << is_trigger_debug_cookie_set
            << ", event_level_result: " << test_case.event_level_result
            << ", aggregatable_result: " << test_case.aggregatable_result);

        std::optional<AttributionDebugReport> report =
            AttributionDebugReport::Create(
                &OperationAllowed, is_trigger_debug_cookie_set,
                CreateReportResult(
                    /*trigger_time=*/base::Time::Now(),
                    TriggerBuilder().SetDebugReporting(true).Build(),
                    test_case.event_level_result, test_case.aggregatable_result,
                    /*replaced_event_level_report=*/std::nullopt,
                    /*new_event_level_report=*/std::nullopt,
                    /*new_aggregatable_report=*/std::nullopt,
                    test_case.has_matching_source
                        ? std::make_optional(
                              SourceBuilder()
                                  .SetDebugCookieSet(is_source_debug_cookie_set)
                                  .BuildStored())
                        : std::nullopt,
                    test_case.limits));
        if (is_trigger_debug_cookie_set &&
            (!test_case.has_matching_source || is_source_debug_cookie_set)) {
          EXPECT_EQ(report.has_value(),
                    test_case.expected_report_body != nullptr);
          if (report) {
            EXPECT_EQ(report->ReportBody(),
                      base::test::ParseJson(test_case.expected_report_body));
          }
        } else {
          EXPECT_FALSE(report);
        }
      }
    }
  }
}

TEST(AttributionDebugReportTest, EventLevelAttributionDebugging) {
  const struct {
    EventLevelResult result;
    std::optional<AttributionReport> replaced_event_level_report;
    std::optional<AttributionReport> new_event_level_report;
    bool has_matching_source = false;
    CreateReportResult::Limits limits;
    std::optional<AttributionReport> dropped_event_level_report;
    std::optional<uint64_t> trigger_debug_key;
    const char* expected_report_body;
    std::optional<uint64_t> source_debug_key;
  } kTestCases[] = {
      {EventLevelResult::kSuccess,
       /*replaced_event_level_report=*/std::nullopt,
       /*new_event_level_report=*/DefaultEventLevelReport(),
       /*has_matching_source=*/true, CreateReportResult::Limits(),
       /*dropped_event_level_report=*/std::nullopt,
       /*trigger_debug_key=*/std::nullopt,
       /*expected_report_body=*/nullptr},
      {EventLevelResult::kSuccessDroppedLowerPriority,
       /*replaced_event_level_report=*/DefaultEventLevelReport(),
       /*new_event_level_report=*/DefaultEventLevelReport(),
       /*has_matching_source=*/true, CreateReportResult::Limits(),
       /*dropped_event_level_report=*/std::nullopt,
       /*trigger_debug_key=*/std::nullopt,
       /*expected_report_body=*/nullptr},
      {EventLevelResult::kInternalError,
       /*replaced_event_level_report=*/std::nullopt,
       /*new_event_level_report=*/std::nullopt,
       /*has_matching_source=*/false, CreateReportResult::Limits(),
       /*dropped_event_level_report=*/std::nullopt,
       /*trigger_debug_key=*/123,
       R"json([{
         "body": {
           "attribution_destination": "https://conversion.test",
           "trigger_debug_key": "123"
         },
         "type": "trigger-unknown-error"
       }])json"},
      {EventLevelResult::kNoCapacityForConversionDestination,
       /*replaced_event_level_report=*/std::nullopt,
       /*new_event_level_report=*/std::nullopt,
       /*has_matching_source=*/true,
       CreateReportResult::Limits{.max_event_level_reports_per_destination =
                                      10},
       /*dropped_event_level_report=*/std::nullopt,
       /*trigger_debug_key=*/std::nullopt,
       R"json([{
         "body": {
           "attribution_destination": "https://conversion.test",
           "limit": "10",
           "source_debug_key": "456",
           "source_event_id": "123",
           "source_site": "https://impression.test"
         },
         "type": "trigger-event-storage-limit"
       }])json",
       /*source_debug_key=*/456},
      {EventLevelResult::kNoMatchingImpressions,
       /*replaced_event_level_report=*/std::nullopt,
       /*new_event_level_report=*/std::nullopt,
       /*has_matching_source=*/false, CreateReportResult::Limits(),
       /*dropped_event_level_report=*/std::nullopt,
       /*trigger_debug_key=*/std::nullopt,
       R"json([{
         "body": {
           "attribution_destination": "https://conversion.test"
         },
         "type": "trigger-no-matching-source"
       }])json"},
      {EventLevelResult::kDeduplicated,
       /*replaced_event_level_report=*/std::nullopt,
       /*new_event_level_report=*/std::nullopt,
       /*has_matching_source=*/true, CreateReportResult::Limits(),
       /*dropped_event_level_report=*/std::nullopt,
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
       }])json",
       /*source_debug_key=*/789},
      {EventLevelResult::kExcessiveAttributions,
       /*replaced_event_level_report=*/std::nullopt,
       /*new_event_level_report=*/std::nullopt,
       /*has_matching_source=*/true,
       CreateReportResult::Limits{.rate_limits_max_attributions = 10},
       /*dropped_event_level_report=*/std::nullopt,
       /*trigger_debug_key=*/std::nullopt,
       R"json([{
         "body": {
           "attribution_destination": "https://conversion.test",
           "limit": "10",
           "source_event_id": "123",
           "source_site": "https://impression.test"
         },
         "type": "trigger-event-attributions-per-source-destination-limit"
       }])json"},
      {EventLevelResult::kPriorityTooLow,
       /*replaced_event_level_report=*/std::nullopt,
       /*new_event_level_report=*/std::nullopt,
       /*has_matching_source=*/true, CreateReportResult::Limits(),
       /*dropped_event_level_report=*/
       DefaultEventLevelReport(base::Time::UnixEpoch()),
       /*trigger_debug_key=*/std::nullopt,
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
      {EventLevelResult::kNeverAttributedSource,
       /*replaced_event_level_report=*/std::nullopt,
       /*new_event_level_report=*/std::nullopt,
       /*has_matching_source=*/true, CreateReportResult::Limits(),
       /*dropped_event_level_report=*/std::nullopt,
       /*trigger_debug_key=*/std::nullopt,
       R"json([{
         "body": {
           "attribution_destination": "https://conversion.test",
           "source_event_id": "123",
           "source_site": "https://impression.test"
         },
         "type": "trigger-event-noise"
       }])json"},
      {EventLevelResult::kExcessiveReportingOrigins,
       /*replaced_event_level_report=*/std::nullopt,
       /*new_event_level_report=*/std::nullopt,
       /*has_matching_source=*/true,
       CreateReportResult::Limits{
           .rate_limits_max_attribution_reporting_origins = 10},
       /*dropped_event_level_report=*/std::nullopt,
       /*trigger_debug_key=*/std::nullopt,
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
       /*replaced_event_level_report=*/std::nullopt,
       /*new_event_level_report=*/std::nullopt,
       /*has_matching_source=*/true, CreateReportResult::Limits(),
       /*dropped_event_level_report=*/std::nullopt,
       /*trigger_debug_key=*/std::nullopt,
       R"json([{
         "body": {
           "attribution_destination": "https://conversion.test",
           "source_event_id": "123",
           "source_site": "https://impression.test"
         },
         "type": "trigger-no-matching-filter-data"
       }])json"},
      {EventLevelResult::kProhibitedByBrowserPolicy,
       /*replaced_event_level_report=*/std::nullopt,
       /*new_event_level_report=*/std::nullopt,
       /*has_matching_source=*/false, CreateReportResult::Limits(),
       /*dropped_event_level_report=*/std::nullopt,
       /*trigger_debug_key=*/std::nullopt,
       /*expected_report_body=*/nullptr},
      {EventLevelResult::kNoMatchingConfigurations,
       /*replaced_event_level_report=*/std::nullopt,
       /*new_event_level_report=*/std::nullopt,
       /*has_matching_source=*/true, CreateReportResult::Limits(),
       /*dropped_event_level_report=*/std::nullopt,
       /*trigger_debug_key=*/std::nullopt,
       R"json([{
         "body": {
           "attribution_destination": "https://conversion.test",
           "source_event_id": "123",
           "source_site": "https://impression.test"
         },
         "type": "trigger-event-no-matching-configurations"
       }])json"},
      {EventLevelResult::kExcessiveReports,
       /*replaced_event_level_report=*/std::nullopt,
       /*new_event_level_report=*/std::nullopt,
       /*has_matching_source=*/true, CreateReportResult::Limits(),
       /*dropped_event_level_report=*/
       DefaultEventLevelReport(base::Time::UnixEpoch()),
       /*trigger_debug_key=*/std::nullopt,
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
       /*replaced_event_level_report=*/std::nullopt,
       /*new_event_level_report=*/std::nullopt,
       /*has_matching_source=*/true, CreateReportResult::Limits(),
       /*dropped_event_level_report=*/std::nullopt,
       /*trigger_debug_key=*/std::nullopt,
       R"json([{
         "body": {
           "attribution_destination": "https://conversion.test",
           "source_event_id": "123",
           "source_site": "https://impression.test"
         },
         "type": "trigger-event-noise"
       }])json"},
      {EventLevelResult::kReportWindowNotStarted,
       /*replaced_event_level_report=*/std::nullopt,
       /*new_event_level_report=*/std::nullopt,
       /*has_matching_source=*/true, CreateReportResult::Limits(),
       /*dropped_event_level_report=*/std::nullopt,
       /*trigger_debug_key=*/std::nullopt,
       R"json([{
         "body": {
           "attribution_destination": "https://conversion.test",
           "source_event_id": "123",
           "source_site": "https://impression.test"
         },
         "type": "trigger-event-report-window-not-started"
       }])json"},
      {EventLevelResult::kReportWindowPassed,
       /*replaced_event_level_report=*/std::nullopt,
       /*new_event_level_report=*/std::nullopt,
       /*has_matching_source=*/true, CreateReportResult::Limits(),
       /*dropped_event_level_report=*/std::nullopt,
       /*trigger_debug_key=*/std::nullopt,
       R"json([{
         "body": {
           "attribution_destination": "https://conversion.test",
           "source_event_id": "123",
           "source_site": "https://impression.test"
         },
         "type": "trigger-event-report-window-passed"
       }])json"},
      {EventLevelResult::kNotRegistered,
       /*replaced_event_level_report=*/std::nullopt,
       /*new_event_level_report=*/std::nullopt,
       /*has_matching_source=*/false, CreateReportResult::Limits(),
       /*dropped_event_level_report=*/std::nullopt,
       /*trigger_debug_key=*/std::nullopt,
       /*expected_report_body=*/nullptr},
  };

  for (bool is_source_debug_cookie_set : {false, true}) {
    for (bool is_trigger_debug_cookie_set : {false, true}) {
      for (const auto& test_case : kTestCases) {
        if (!is_source_debug_cookie_set && test_case.source_debug_key) {
          continue;
        }

        SCOPED_TRACE(Message() << "is_source_debug_cookie_set: "
                               << is_source_debug_cookie_set
                               << ", is_trigger_debug_cookie_set: "
                               << is_trigger_debug_cookie_set
                               << ", result: " << test_case.result);

        std::optional<AttributionDebugReport> report =
            AttributionDebugReport::Create(
                &OperationAllowed, is_trigger_debug_cookie_set,
                CreateReportResult(
                    /*trigger_time=*/base::Time::Now(),
                    TriggerBuilder()
                        .SetDebugReporting(true)
                        .SetDebugKey(test_case.trigger_debug_key)
                        .Build(),
                    test_case.result, AggregatableResult::kNotRegistered,
                    test_case.replaced_event_level_report,
                    test_case.new_event_level_report,
                    /*new_aggregatable_report=*/std::nullopt,
                    test_case.has_matching_source
                        ? std::make_optional(
                              SourceBuilder(base::Time::UnixEpoch())
                                  .SetDebugCookieSet(is_source_debug_cookie_set)
                                  .SetDebugKey(test_case.source_debug_key)
                                  .BuildStored())
                        : std::nullopt,
                    test_case.limits, test_case.dropped_event_level_report));
        if (is_trigger_debug_cookie_set &&
            (!test_case.has_matching_source || is_source_debug_cookie_set)) {
          EXPECT_EQ(report.has_value(),
                    test_case.expected_report_body != nullptr);
          if (report) {
            EXPECT_EQ(report->ReportBody(),
                      base::test::ParseJson(test_case.expected_report_body));
          }
        } else {
          EXPECT_FALSE(report);
        }
      }
    }
  }
}

TEST(AttributionDebugReportTest, AggregatableAttributionDebugging) {
  const struct {
    AggregatableResult result;
    std::optional<AttributionReport> new_aggregatable_report;
    CreateReportResult::Limits limits;
    std::optional<uint64_t> source_debug_key;
    std::optional<uint64_t> trigger_debug_key;
    const char* expected_report_body;
  } kTestCases[] = {
      {AggregatableResult::kSuccess, DefaultAggregatableReport(),
       CreateReportResult::Limits(),
       /*source_debug_key=*/std::nullopt,
       /*trigger_debug_key=*/std::nullopt,
       /*expected_report_body=*/nullptr},
      {AggregatableResult::kInternalError,
       /*new_aggregatable_report=*/std::nullopt, CreateReportResult::Limits(),
       /*source_debug_key=*/456,
       /*trigger_debug_key=*/std::nullopt,
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
       /*new_aggregatable_report=*/std::nullopt,
       CreateReportResult::Limits{.max_aggregatable_reports_per_destination =
                                      20},
       /*source_debug_key=*/std::nullopt,
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
       /*new_aggregatable_report=*/std::nullopt,
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
         "type": "trigger-aggregate-attributions-per-source-destination-limit"
       }])json"},
      {AggregatableResult::kExcessiveReportingOrigins,
       /*new_aggregatable_report=*/std::nullopt,
       CreateReportResult::Limits{
           .rate_limits_max_attribution_reporting_origins = 5},
       /*source_debug_key=*/std::nullopt,
       /*trigger_debug_key=*/std::nullopt,
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
       /*new_aggregatable_report=*/std::nullopt, CreateReportResult::Limits(),
       /*source_debug_key=*/std::nullopt,
       /*trigger_debug_key=*/std::nullopt,
       R"json([{
         "body": {
           "attribution_destination": "https://conversion.test",
           "source_event_id": "123",
           "source_site": "https://impression.test"
         },
         "type": "trigger-aggregate-no-contributions"
       }])json"},
      {AggregatableResult::kInsufficientBudget,
       /*new_aggregatable_report=*/std::nullopt, CreateReportResult::Limits(),
       /*source_debug_key=*/std::nullopt,
       /*trigger_debug_key=*/std::nullopt,
       R"json([{
         "body": {
           "attribution_destination": "https://conversion.test",
           "limit": "65536",
           "source_event_id": "123",
           "source_site": "https://impression.test"
         },
         "type": "trigger-aggregate-insufficient-budget"
       }])json"},
      {AggregatableResult::kExcessiveReports,
       /*new_aggregatable_report=*/std::nullopt,
       CreateReportResult::Limits{.max_aggregatable_reports_per_source = 10},
       /*source_debug_key=*/std::nullopt,
       /*trigger_debug_key=*/std::nullopt,
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
       /*new_aggregatable_report=*/std::nullopt, CreateReportResult::Limits(),
       /*source_debug_key=*/std::nullopt,
       /*trigger_debug_key=*/std::nullopt,
       R"json([{
         "body": {
           "attribution_destination": "https://conversion.test",
           "source_event_id": "123",
           "source_site": "https://impression.test"
         },
         "type": "trigger-no-matching-filter-data"
       }])json"},
      {AggregatableResult::kNotRegistered,
       /*new_aggregatable_report=*/std::nullopt, CreateReportResult::Limits(),
       /*source_debug_key=*/std::nullopt,
       /*trigger_debug_key=*/std::nullopt,
       /*expected_report_body=*/nullptr},
      {AggregatableResult::kDeduplicated,
       /*new_aggregatable_report=*/std::nullopt, CreateReportResult::Limits(),
       /*source_debug_key=*/std::nullopt,
       /*trigger_debug_key=*/std::nullopt,
       R"json([{
         "body": {
           "attribution_destination": "https://conversion.test",
           "source_event_id": "123",
           "source_site": "https://impression.test"
         },
         "type": "trigger-aggregate-deduplicated"
       }])json"},
      {AggregatableResult::kReportWindowPassed,
       /*new_aggregatable_report=*/std::nullopt, CreateReportResult::Limits(),
       /*source_debug_key=*/std::nullopt,
       /*trigger_debug_key=*/std::nullopt,
       R"json([{
         "body": {
           "attribution_destination": "https://conversion.test",
           "source_event_id": "123",
           "source_site": "https://impression.test"
         },
         "type": "trigger-aggregate-report-window-passed"
       }])json"},
  };

  for (bool is_source_debug_cookie_set : {false, true}) {
    for (bool is_trigger_debug_cookie_set : {false, true}) {
      for (const auto& test_case : kTestCases) {
        if (!is_source_debug_cookie_set && test_case.source_debug_key) {
          continue;
        }

        SCOPED_TRACE(Message() << "is_source_debug_cookie_set: "
                               << is_source_debug_cookie_set
                               << ", is_trigger_debug_cookie_set: "
                               << is_trigger_debug_cookie_set
                               << ", result: " << test_case.result);

        std::optional<AttributionDebugReport> report =
            AttributionDebugReport::Create(
                &OperationAllowed, is_trigger_debug_cookie_set,
                CreateReportResult(
                    /*trigger_time=*/base::Time::Now(),
                    TriggerBuilder()
                        .SetDebugReporting(true)
                        .SetDebugKey(test_case.trigger_debug_key)
                        .Build(),
                    EventLevelResult::kSuccess, test_case.result,
                    /*replaced_event_level_report=*/std::nullopt,
                    /*new_event_level_report=*/DefaultEventLevelReport(),
                    test_case.new_aggregatable_report,
                    SourceBuilder()
                        .SetDebugKey(test_case.source_debug_key)
                        .SetDebugCookieSet(is_source_debug_cookie_set)
                        .BuildStored(),
                    test_case.limits));
        if (is_trigger_debug_cookie_set && is_source_debug_cookie_set) {
          EXPECT_EQ(report.has_value(),
                    test_case.expected_report_body != nullptr);
          if (report) {
            EXPECT_EQ(report->ReportBody(),
                      base::test::ParseJson(test_case.expected_report_body));
          }
        } else {
          EXPECT_FALSE(report);
        }
      }
    }
  }
}

TEST(AttributionDebugReportTest, OsRegistrationDebugging) {
  const auto operation_allowed = [](const url::Origin&) { return true; };
  const auto operation_allowed_if_not_registration_origin =
      [](const url::Origin& origin) {
        return origin != url::Origin::Create(GURL("https://c.test"));
      };

  const struct {
    const char* description;
    OsRegistration registration;
    base::FunctionRef<bool(const url::Origin&)> is_operation_allowed;
    const char* expected_body;
  } kTestCases[] = {
      {
          "os_source",
          OsRegistration(
              {OsRegistrationItem(GURL("https://a.test/x"),
                                  /*debug_reporting=*/true)},
              /*top_level_origin=*/url::Origin::Create(GURL("https://b.test")),
              AttributionInputEvent(),
              /*is_within_fenced_frame=*/false,
              /*render_frame_id=*/GlobalRenderFrameHostId(), kRegistrar),
          operation_allowed,
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
              {OsRegistrationItem(GURL("https://a.test/x"),
                                  /*debug_reporting=*/true)},
              /*top_level_origin=*/url::Origin::Create(GURL("https://b.test")),
              /*input_event=*/std::nullopt, /*is_within_fenced_frame=*/false,
              /*render_frame_id=*/GlobalRenderFrameHostId(), kRegistrar),
          operation_allowed,
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
              {OsRegistrationItem(GURL("https://a.test/x"),
                                  /*debug_reporting=*/false)},
              /*top_level_origin=*/url::Origin::Create(GURL("https://b.test")),
              /*input_event=*/std::nullopt, /*is_within_fenced_frame=*/false,
              /*render_frame_id=*/GlobalRenderFrameHostId(), kRegistrar),
          operation_allowed,
          nullptr,
      },
      {
          "within_fenced_frame",
          OsRegistration(
              {OsRegistrationItem(GURL("https://a.test/x"),
                                  /*debug_reporting=*/true)},
              /*top_level_origin=*/url::Origin::Create(GURL("https://b.test")),
              /*input_event=*/std::nullopt, /*is_within_fenced_frame=*/true,
              /*render_frame_id=*/GlobalRenderFrameHostId(), kRegistrar),
          operation_allowed,
          nullptr,
      },
      {
          "non_suitable_registration_origin",
          OsRegistration(
              {OsRegistrationItem(GURL("http://a.test/x"),
                                  /*debug_reporting=*/true)},
              /*top_level_origin=*/url::Origin::Create(GURL("https://b.test")),
              /*input_event=*/std::nullopt, /*is_within_fenced_frame=*/false,
              /*render_frame_id=*/GlobalRenderFrameHostId(), kRegistrar),
          operation_allowed,
          nullptr,
      },
      {
          "operation_prohibited",
          OsRegistration(
              {OsRegistrationItem(GURL("https://c.test/x"),
                                  /*debug_reporting=*/true)},
              /*top_level_origin=*/url::Origin::Create(GURL("https://b.test")),
              /*input_event=*/std::nullopt, /*is_within_fenced_frame=*/false,
              /*render_frame_id=*/GlobalRenderFrameHostId(), kRegistrar),
          operation_allowed_if_not_registration_origin,
          nullptr,
      },
  };

  for (const auto& test_case : kTestCases) {
    std::optional<AttributionDebugReport> report =
        AttributionDebugReport::Create(test_case.registration,
                                       /*item_index=*/0,
                                       test_case.is_operation_allowed);
    EXPECT_EQ(report.has_value(), test_case.expected_body != nullptr)
        << test_case.description;
    if (test_case.expected_body) {
      EXPECT_EQ(report->ReportBody(),
                base::test::ParseJson(test_case.expected_body))
          << test_case.description;
    }
  }
}

TEST(AttributionDebugReportTest, RegistrationHeaderErrorDebugReports) {
  const auto reporting_origin = *SuitableOrigin::Deserialize("https://r.test");
  const auto context_origin = *SuitableOrigin::Deserialize("https://c.test");

  const auto operation_allowed = [](const url::Origin&) { return true; };

  const auto operation_allowed_if_not_reporting_origin =
      [&](const url::Origin& origin) { return origin != reporting_origin; };

  const struct {
    const char* name;
    attribution_reporting::RegistrationHeaderErrorDetails details;
    bool is_within_fenced_frame = false;
    base::FunctionRef<bool(const url::Origin&)> is_operation_allowed =
        operation_allowed;
    const char* expected_body;
    const char* expected_body_with_details;
  } kTestCases[] = {
      {
          .name = "source",
          .details = attribution_reporting::mojom::SourceRegistrationError::
              kInvalidJson,
          .expected_body = R"json([{
            "body": {
              "context_site": "https://c.test",
              "header": "Attribution-Reporting-Register-Source",
              "value": "!!!"
            },
            "type": "header-parsing-error"
          }])json",
          .expected_body_with_details = R"json([{
            "body": {
              "context_site": "https://c.test",
              "header": "Attribution-Reporting-Register-Source",
              "value": "!!!",
              "error": {
                "msg": "invalid JSON"
              }
            },
            "type": "header-parsing-error"
          }])json",
      },
      {
          .name = "trigger",
          .details = attribution_reporting::mojom::TriggerRegistrationError::
              kInvalidJson,
          .expected_body = R"json([{
            "body": {
              "context_site": "https://c.test",
              "header": "Attribution-Reporting-Register-Trigger",
              "value": "!!!"
            },
            "type": "header-parsing-error"
          }])json",
          .expected_body_with_details = R"json([{
            "body": {
              "context_site": "https://c.test",
              "header": "Attribution-Reporting-Register-Trigger",
              "value": "!!!",
              "error": {
                "msg": "invalid JSON"
              }
            },
            "type": "header-parsing-error"
          }])json",
      },
      {
          .name = "os_source",
          .details = attribution_reporting::OsSourceRegistrationError(
              attribution_reporting::mojom::OsRegistrationError::kInvalidList),
          .expected_body = R"json([{
            "body": {
              "context_site": "https://c.test",
              "header": "Attribution-Reporting-Register-OS-Source",
              "value": "!!!"
            },
            "type": "header-parsing-error"
          }])json",
          .expected_body_with_details = R"json([{
            "body": {
              "context_site": "https://c.test",
              "header": "Attribution-Reporting-Register-OS-Source",
              "value": "!!!",
              "error": {
                "msg": "must be a list of URLs"
              }
            },
            "type": "header-parsing-error"
          }])json",
      },
      {
          .name = "os_trigger",
          .details = attribution_reporting::OsTriggerRegistrationError(
              attribution_reporting::mojom::OsRegistrationError::kInvalidList),
          .expected_body = R"json([{
            "body": {
              "context_site": "https://c.test",
              "header": "Attribution-Reporting-Register-OS-Trigger",
              "value": "!!!"
            },
            "type": "header-parsing-error"
          }])json",
          .expected_body_with_details = R"json([{
            "body": {
              "context_site": "https://c.test",
              "header": "Attribution-Reporting-Register-OS-Trigger",
              "value": "!!!",
              "error": {
                "msg": "must be a list of URLs"
              }
            },
            "type": "header-parsing-error"
          }])json",
      },
      {
          .name = "within_fenced_frame",
          .details = attribution_reporting::mojom::SourceRegistrationError::
              kInvalidJson,
          .is_within_fenced_frame = true,
          .expected_body = nullptr,
          .expected_body_with_details = nullptr,
      },
      {
          .name = "operation_prohibited",
          .details = attribution_reporting::mojom::SourceRegistrationError::
              kInvalidJson,
          .is_operation_allowed = operation_allowed_if_not_reporting_origin,
          .expected_body = nullptr,
          .expected_body_with_details = nullptr,
      },
  };

  for (const bool feature_enabled : {false, true}) {
    SCOPED_TRACE(feature_enabled);
    base::test::ScopedFeatureList scoped_feature_list;
    if (feature_enabled) {
      scoped_feature_list.InitAndEnableFeature(kAttributionHeaderErrorDetails);
    } else {
      scoped_feature_list.InitAndDisableFeature(kAttributionHeaderErrorDetails);
    }

    for (const auto& test_case : kTestCases) {
      SCOPED_TRACE(test_case.name);
      std::optional<AttributionDebugReport> report =
          AttributionDebugReport::Create(
              reporting_origin,
              RegistrationHeaderError(/*header_value=*/"!!!",
                                      test_case.details),
              context_origin, test_case.is_within_fenced_frame,
              test_case.is_operation_allowed);
      const char* expected_body = feature_enabled
                                      ? test_case.expected_body_with_details
                                      : test_case.expected_body;
      EXPECT_EQ(report.has_value(), expected_body != nullptr);
      if (expected_body) {
        EXPECT_EQ(report->ReportBody(), base::test::ParseJson(expected_body));
      }
    }
  }
}

}  // namespace
}  // namespace content
