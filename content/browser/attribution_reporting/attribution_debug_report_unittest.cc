// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_debug_report.h"

#include <stdint.h>

#include <optional>

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
          /*destination_limit=*/std::nullopt,
          StoreSourceResult::InsufficientUniqueDestinationCapacity(3))));

  EXPECT_FALSE(AttributionDebugReport::Create(
      &OperationAllowed,
      /*is_debug_cookie_set=*/true,
      CreateReportResult(/*trigger_time=*/base::Time::Now(),
                         TriggerBuilder().Build(),
                         CreateReportResult::NoMatchingImpressions(),
                         CreateReportResult::NoMatchingImpressions(),
                         /*source=*/std::nullopt,
                         /*min_null_aggregatable_report_time=*/std::nullopt)));
}

TEST(AttributionDebugReportTest, OperationProhibited_NoReportReturned) {
  EXPECT_FALSE(AttributionDebugReport::Create(
      &OperationProhibited,
      StoreSourceResult(
          SourceBuilder().SetDebugReporting(true).Build(),
          /*is_noised=*/false, kSourceTime,
          /*destination_limit=*/std::nullopt,
          StoreSourceResult::InsufficientUniqueDestinationCapacity(3))));

  EXPECT_FALSE(AttributionDebugReport::Create(
      &OperationProhibited,
      /*is_debug_cookie_set=*/true,
      CreateReportResult(/*trigger_time=*/base::Time::Now(),
                         TriggerBuilder().SetDebugReporting(true).Build(),
                         CreateReportResult::NoMatchingImpressions(),
                         CreateReportResult::NoMatchingImpressions(),
                         /*source=*/std::nullopt,
                         /*min_null_aggregatable_report_time=*/std::nullopt)));
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
          /*destination_limit=*/std::nullopt,
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
          /*destination_limit=*/std::nullopt,
          StoreSourceResult::InsufficientUniqueDestinationCapacity(3))));

  EXPECT_FALSE(AttributionDebugReport::Create(
      &OperationAllowed,
      /*is_debug_cookie_set=*/true,
      CreateReportResult(/*trigger_time=*/base::Time::Now(),
                         TriggerBuilder()
                             .SetDebugReporting(true)
                             .SetIsWithinFencedFrame(true)
                             .Build(),
                         CreateReportResult::NoMatchingImpressions(),
                         CreateReportResult::NoMatchingImpressions(),
                         /*source=*/std::nullopt,
                         /*min_null_aggregatable_report_time=*/std::nullopt)));
}

TEST(AttributionDebugReportTest, SourceDebugging) {
  const struct {
    StoreSourceResult::Result result;
    std::optional<uint64_t> debug_key;
    bool is_noised = false;
    std::optional<int> destination_limit;
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
          .destination_limit = 3,
          .expected_report_body = R"json([{
            "body": {
              "attribution_destination": "https://conversion.test",
              "source_destination_limit": "3",
              "source_event_id": "123",
              "source_site": "https://impression.test"
            },
            "type": "source-success"
          }])json",
      },
      {
          .result = StoreSourceResult::Success(
              /*min_fake_report_time=*/std::nullopt, kSourceId),
          .is_noised = true,
          .destination_limit = 3,
          .expected_report_body = R"json([{
            "body": {
              "attribution_destination": "https://conversion.test",
              "source_destination_limit": "3",
              "source_event_id": "123",
              "source_site": "https://impression.test"
            },
            "type": "source-noised"
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
          .destination_limit = 5,
          .expected_report_body = R"json([{
            "body": {
              "attribution_destination": "https://conversion.test",
              "source_debug_key": "789",
              "source_destination_limit": "5",
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
          .destination_limit = 3,
          .expected_report_body = R"json([{
            "body": {
              "attribution_destination": "https://conversion.test",
              "source_debug_key": "789",
              "source_destination_limit": "3",
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
      {
          .result = StoreSourceResult::ExceedsMaxEventStatesLimit(3),
          .debug_key = std::nullopt,
          .expected_report_body = R"json([{
              "body": {
                "attribution_destination": "https://conversion.test",
                "limit": "3",
                "source_event_id": "123",
                "source_site": "https://impression.test"
              },
              "type": "source-max-event-states-limit"
            }])json",
      },
      {
          .result =
              StoreSourceResult::DestinationPerDayReportingLimitReached(100),
          .expected_report_body = R"json([{
            "body": {
              "attribution_destination": "https://conversion.test",
              "limit": "100",
              "source_event_id": "123",
              "source_site": "https://impression.test"
            },
            "type": "source-destination-per-day-rate-limit"
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
                               test_case.destination_limit, test_case.result);

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
                /*destination_limit=*/std::nullopt,
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
    CreateReportResult::EventLevel event_level_result;
    CreateReportResult::Aggregatable aggregatable_result;
    bool has_matching_source = false;
    const char* expected_report_body;
  } kTestCases[] = {
      {CreateReportResult::NoMatchingImpressions(),
       CreateReportResult::NoMatchingImpressions(),
       /*has_matching_source=*/false,
       R"json([{
         "body": {
           "attribution_destination": "https://conversion.test"
         },
         "type": "trigger-no-matching-source"
       }])json"},
      {CreateReportResult::ProhibitedByBrowserPolicy(),
       CreateReportResult::ProhibitedByBrowserPolicy(),
       /*has_matching_source=*/false,
       /*expected_report_body=*/nullptr},
      {CreateReportResult::NoMatchingConfigurations(),
       CreateReportResult::ExcessiveAttributions(/*max=*/10),
       /*has_matching_source=*/true,
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
      {CreateReportResult::NoMatchingConfigurations(),
       CreateReportResult::InsufficientBudget(),
       /*has_matching_source=*/true,
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
        const CreateReportResult result(
            /*trigger_time=*/base::Time::Now(),
            TriggerBuilder().SetDebugReporting(true).Build(),
            test_case.event_level_result, test_case.aggregatable_result,
            test_case.has_matching_source
                ? std::make_optional(
                      SourceBuilder()
                          .SetDebugCookieSet(is_source_debug_cookie_set)
                          .BuildStored())
                : std::nullopt,
            /*min_null_aggregatable_report_time=*/std::nullopt);

        SCOPED_TRACE(Message()
                     << "is_source_debug_cookie_set: "
                     << is_source_debug_cookie_set
                     << ", is_trigger_debug_cookie_set: "
                     << is_trigger_debug_cookie_set << ", event_level_result: "
                     << result.event_level_status() << ", aggregatable_result: "
                     << result.aggregatable_status());

        std::optional<AttributionDebugReport> report =
            AttributionDebugReport::Create(&OperationAllowed,
                                           is_trigger_debug_cookie_set, result);
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
    CreateReportResult::EventLevel result;
    bool has_matching_source = false;
    std::optional<uint64_t> trigger_debug_key;
    const char* expected_report_body;
    std::optional<uint64_t> source_debug_key;
  } kTestCases[] = {
      {CreateReportResult::EventLevelSuccess(
           DefaultEventLevelReport(),
           /*replaced_event_level_report=*/std::nullopt),
       /*has_matching_source=*/true,
       /*trigger_debug_key=*/std::nullopt,
       /*expected_report_body=*/nullptr},
      {CreateReportResult::EventLevelSuccess(
           DefaultEventLevelReport(),
           /*replaced_event_level_report=*/DefaultEventLevelReport()),
       /*has_matching_source=*/true,
       /*trigger_debug_key=*/std::nullopt,
       /*expected_report_body=*/nullptr},
      {CreateReportResult::InternalError(),
       /*has_matching_source=*/false,
       /*trigger_debug_key=*/123,
       R"json([{
         "body": {
           "attribution_destination": "https://conversion.test",
           "trigger_debug_key": "123"
         },
         "type": "trigger-unknown-error"
       }])json"},
      {CreateReportResult::NoCapacityForConversionDestination(/*max=*/10),
       /*has_matching_source=*/true,
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
      {CreateReportResult::NoMatchingImpressions(),
       /*has_matching_source=*/false,
       /*trigger_debug_key=*/std::nullopt,
       R"json([{
         "body": {
           "attribution_destination": "https://conversion.test"
         },
         "type": "trigger-no-matching-source"
       }])json"},
      {CreateReportResult::Deduplicated(),
       /*has_matching_source=*/true,
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
      {CreateReportResult::ExcessiveAttributions(/*max=*/10),
       /*has_matching_source=*/true,
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
      {CreateReportResult::PriorityTooLow(
           DefaultEventLevelReport(base::Time::UnixEpoch())),
       /*has_matching_source=*/true,
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
      {CreateReportResult::NeverAttributedSource(),
       /*has_matching_source=*/true,
       /*trigger_debug_key=*/std::nullopt,
       R"json([{
         "body": {
           "attribution_destination": "https://conversion.test",
           "source_event_id": "123",
           "source_site": "https://impression.test"
         },
         "type": "trigger-event-noise"
       }])json"},
      {CreateReportResult::ExcessiveReportingOrigins(/*max=*/10),
       /*has_matching_source=*/true,
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
      {CreateReportResult::NoMatchingSourceFilterData(),
       /*has_matching_source=*/true,
       /*trigger_debug_key=*/std::nullopt,
       R"json([{
         "body": {
           "attribution_destination": "https://conversion.test",
           "source_event_id": "123",
           "source_site": "https://impression.test"
         },
         "type": "trigger-no-matching-filter-data"
       }])json"},
      {CreateReportResult::ProhibitedByBrowserPolicy(),
       /*has_matching_source=*/false,
       /*trigger_debug_key=*/std::nullopt,
       /*expected_report_body=*/nullptr},
      {CreateReportResult::NoMatchingConfigurations(),
       /*has_matching_source=*/true,
       /*trigger_debug_key=*/std::nullopt,
       R"json([{
         "body": {
           "attribution_destination": "https://conversion.test",
           "source_event_id": "123",
           "source_site": "https://impression.test"
         },
         "type": "trigger-event-no-matching-configurations"
       }])json"},
      {CreateReportResult::ExcessiveEventLevelReports(
           DefaultEventLevelReport(base::Time::UnixEpoch())),
       /*has_matching_source=*/true,
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
      {CreateReportResult::FalselyAttributedSource(),
       /*has_matching_source=*/true,
       /*trigger_debug_key=*/std::nullopt,
       R"json([{
         "body": {
           "attribution_destination": "https://conversion.test",
           "source_event_id": "123",
           "source_site": "https://impression.test"
         },
         "type": "trigger-event-noise"
       }])json"},
      {CreateReportResult::ReportWindowNotStarted(),
       /*has_matching_source=*/true,
       /*trigger_debug_key=*/std::nullopt,
       R"json([{
         "body": {
           "attribution_destination": "https://conversion.test",
           "source_event_id": "123",
           "source_site": "https://impression.test"
         },
         "type": "trigger-event-report-window-not-started"
       }])json"},
      {CreateReportResult::ReportWindowPassed(),
       /*has_matching_source=*/true,
       /*trigger_debug_key=*/std::nullopt,
       R"json([{
         "body": {
           "attribution_destination": "https://conversion.test",
           "source_event_id": "123",
           "source_site": "https://impression.test"
         },
         "type": "trigger-event-report-window-passed"
       }])json"},
      {CreateReportResult::NotRegistered(),
       /*has_matching_source=*/false,
       /*trigger_debug_key=*/std::nullopt,
       /*expected_report_body=*/nullptr},
  };

  for (bool is_source_debug_cookie_set : {false, true}) {
    for (bool is_trigger_debug_cookie_set : {false, true}) {
      for (const auto& test_case : kTestCases) {
        if (!is_source_debug_cookie_set && test_case.source_debug_key) {
          continue;
        }

        const CreateReportResult result(
            /*trigger_time=*/base::Time::Now(),
            TriggerBuilder()
                .SetDebugReporting(true)
                .SetDebugKey(test_case.trigger_debug_key)
                .Build(),
            /*event_level_result=*/test_case.result,
            /*aggregatable_result=*/CreateReportResult::NotRegistered(),
            test_case.has_matching_source
                ? std::make_optional(
                      SourceBuilder(base::Time::UnixEpoch())
                          .SetDebugCookieSet(is_source_debug_cookie_set)
                          .SetDebugKey(test_case.source_debug_key)
                          .BuildStored())
                : std::nullopt,
            /*min_null_aggregatable_report_time=*/std::nullopt);

        SCOPED_TRACE(Message() << "is_source_debug_cookie_set: "
                               << is_source_debug_cookie_set
                               << ", is_trigger_debug_cookie_set: "
                               << is_trigger_debug_cookie_set
                               << ", result: " << result.event_level_status());

        std::optional<AttributionDebugReport> report =
            AttributionDebugReport::Create(&OperationAllowed,
                                           is_trigger_debug_cookie_set, result);
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
    CreateReportResult::Aggregatable result;
    std::optional<uint64_t> source_debug_key;
    std::optional<uint64_t> trigger_debug_key;
    const char* expected_report_body;
  } kTestCases[] = {
      {CreateReportResult::AggregatableSuccess(DefaultAggregatableReport()),
       /*source_debug_key=*/std::nullopt,
       /*trigger_debug_key=*/std::nullopt,
       /*expected_report_body=*/nullptr},
      {CreateReportResult::InternalError(),
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
      {CreateReportResult::NoCapacityForConversionDestination(/*max=*/20),
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
      {CreateReportResult::ExcessiveAttributions(/*max=*/10),
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
      {CreateReportResult::ExcessiveReportingOrigins(/*max=*/5),
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
      {CreateReportResult::NoHistograms(),
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
      {CreateReportResult::InsufficientBudget(),
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
      {CreateReportResult::ExcessiveAggregatableReports(/*max=*/10),
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
      {CreateReportResult::NoMatchingSourceFilterData(),
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
      {CreateReportResult::NotRegistered(),
       /*source_debug_key=*/std::nullopt,
       /*trigger_debug_key=*/std::nullopt,
       /*expected_report_body=*/nullptr},
      {CreateReportResult::Deduplicated(),
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
      {CreateReportResult::ReportWindowPassed(),
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

        const CreateReportResult result(
            /*trigger_time=*/base::Time::Now(),
            TriggerBuilder()
                .SetDebugReporting(true)
                .SetDebugKey(test_case.trigger_debug_key)
                .Build(),
            CreateReportResult::EventLevelSuccess(
                DefaultEventLevelReport(),
                /*replaced_report=*/std::nullopt),
            /*aggregatable_result=*/test_case.result,
            SourceBuilder()
                .SetDebugKey(test_case.source_debug_key)
                .SetDebugCookieSet(is_source_debug_cookie_set)
                .BuildStored(),
            /*min_null_aggregatable_report_time=*/std::nullopt);

        SCOPED_TRACE(Message() << "is_source_debug_cookie_set: "
                               << is_source_debug_cookie_set
                               << ", is_trigger_debug_cookie_set: "
                               << is_trigger_debug_cookie_set
                               << ", result: " << result.aggregatable_status());

        std::optional<AttributionDebugReport> report =
            AttributionDebugReport::Create(&OperationAllowed,
                                           is_trigger_debug_cookie_set, result);
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
      },
      {
          .name = "within_fenced_frame",
          .details = attribution_reporting::mojom::SourceRegistrationError::
              kInvalidJson,
          .is_within_fenced_frame = true,
          .expected_body = nullptr,
      },
      {
          .name = "operation_prohibited",
          .details = attribution_reporting::mojom::SourceRegistrationError::
              kInvalidJson,
          .is_operation_allowed = operation_allowed_if_not_reporting_origin,
          .expected_body = nullptr,
      },
  };

    for (const auto& test_case : kTestCases) {
      SCOPED_TRACE(test_case.name);
      std::optional<AttributionDebugReport> report =
          AttributionDebugReport::Create(
              reporting_origin,
              RegistrationHeaderError(/*header_value=*/"!!!",
                                      test_case.details),
              context_origin, test_case.is_within_fenced_frame,
              test_case.is_operation_allowed);
      EXPECT_EQ(report.has_value(), test_case.expected_body != nullptr);
      if (test_case.expected_body) {
        EXPECT_EQ(report->ReportBody(),
                  base::test::ParseJson(test_case.expected_body));
      }
    }
}

}  // namespace
}  // namespace content
