// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/aggregatable_debug_report.h"

#include <optional>
#include <vector>

#include "base/containers/enum_set.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "base/uuid.h"
#include "components/aggregation_service/aggregation_coordinator_utils.h"
#include "components/attribution_reporting/aggregatable_debug_reporting_config.h"
#include "components/attribution_reporting/debug_types.h"
#include "components/attribution_reporting/debug_types.mojom.h"
#include "components/attribution_reporting/features.h"
#include "components/attribution_reporting/suitable_origin.h"
#include "content/browser/aggregation_service/aggregatable_report.h"
#include "content/browser/aggregation_service/aggregation_service_test_utils.h"
#include "content/browser/attribution_reporting/attribution_report.h"
#include "content/browser/attribution_reporting/attribution_test_utils.h"
#include "content/browser/attribution_reporting/attribution_trigger.h"
#include "content/browser/attribution_reporting/create_report_result.h"
#include "content/browser/attribution_reporting/storable_source.h"
#include "content/browser/attribution_reporting/store_source_result.h"
#include "content/browser/attribution_reporting/stored_source.h"
#include "net/base/schemeful_site.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {
namespace {

using AggregatableDebugData =
    ::attribution_reporting::AggregatableDebugReportingConfig::DebugData;

using ::attribution_reporting::AggregatableDebugReportingConfig;
using ::attribution_reporting::AggregatableDebugReportingContribution;
using ::attribution_reporting::SourceAggregatableDebugReportingConfig;
using ::attribution_reporting::SuitableOrigin;
using ::attribution_reporting::mojom::DebugDataType;
using ::blink::mojom::AggregatableReportHistogramContribution;

using AggregatableResult = ::content::AttributionTrigger::AggregatableResult;
using EventLevelResult = ::content::AttributionTrigger::EventLevelResult;

using ::testing::Eq;
using ::testing::Field;
using ::testing::IsEmpty;
using ::testing::Optional;
using ::testing::Property;
using ::testing::UnorderedElementsAre;
using ::testing::UnorderedElementsAreArray;

constexpr base::Time kSourceTime;
constexpr base::Time kTriggerTime;

constexpr StoredSource::Id kSourceId(1);

bool OperationAllowed() {
  return true;
}

AttributionReport DefaultEventLevelReport() {
  return ReportBuilder(AttributionInfoBuilder().Build(),
                       SourceBuilder().BuildStored())
      .Build();
}

AttributionReport DefaultAggregatableReport() {
  return ReportBuilder(AttributionInfoBuilder().Build(),
                       SourceBuilder().BuildStored())
      .BuildAggregatableAttribution();
}

AggregatableDebugData DebugDataAll(
    const attribution_reporting::DebugDataTypes& types) {
  AggregatableDebugReportingContribution contribution =
      *AggregatableDebugReportingContribution::Create(
          /*key_piece=*/1, /*value=*/2);
  AggregatableDebugReportingConfig::DebugData debug_data;
  for (DebugDataType type : types) {
    debug_data.try_emplace(type, contribution);
  }
  return debug_data;
}

class AggregatableDebugReportTest : public testing::Test {
 private:
  base::test::ScopedFeatureList scoped_feature_list{
      attribution_reporting::features::kAttributionAggregatableDebugReporting};
};

TEST_F(AggregatableDebugReportTest, SourceDebugReport_Enablement) {
  const struct {
    const char* desc;
    bool is_within_fenced_frame = false;
    bool operation_allowed = true;
    SourceAggregatableDebugReportingConfig config =
        *SourceAggregatableDebugReportingConfig::Create(
            /*budget=*/10,
            AggregatableDebugReportingConfig(
                /*key_piece=*/3,
                /*debug_data=*/
                {
                    {DebugDataType::kSourceUnknownError,
                     *AggregatableDebugReportingContribution::Create(
                         /*key_piece=*/5,
                         /*value=*/3)},
                },
                /*aggregation_coordinator_origin=*/std::nullopt));
    ::testing::Matcher<std::optional<AggregatableDebugReport>> matches;
  } kTestCases[] = {
      {
          .desc = "enabled",
          .matches = Optional(AllOf(Property(
              &AggregatableDebugReport::contributions,
              UnorderedElementsAre(AggregatableReportHistogramContribution(
                  /*bucket=*/7, /*value=*/3,
                  /*filtering_id=*/std::nullopt))))),
      },
      {
          .desc = "no_debug_data",
          .config = SourceAggregatableDebugReportingConfig(),
          .matches = Eq(std::nullopt),
      },
      {
          .desc = "within_fenced_frame",
          .is_within_fenced_frame = true,
          .matches = Eq(std::nullopt),
      },
      {
          .desc = "operation_disallowed",
          .operation_allowed = false,
          .matches = Eq(std::nullopt),
      },
      {
          .desc = "no_matching_debug_data",
          .config = *SourceAggregatableDebugReportingConfig::Create(
              /*budget=*/10,
              AggregatableDebugReportingConfig(
                  /*key_piece=*/3,
                  /*debug_data=*/
                  {
                      {DebugDataType::kSourceDestinationLimit,
                       *AggregatableDebugReportingContribution::Create(
                           /*key_piece=*/5,
                           /*value=*/3)},
                  },
                  /*aggregation_coordinator_origin=*/std::nullopt)),
          .matches = Optional(
              Property(&AggregatableDebugReport::contributions, IsEmpty())),
      },
  };

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(test_case.desc);

    EXPECT_THAT(
        AggregatableDebugReport::Create(
            [&]() { return test_case.operation_allowed; },
            StoreSourceResult(
                SourceBuilder()
                    .SetIsWithinFencedFrame(test_case.is_within_fenced_frame)
                    .SetAggregatableDebugReportingConfig(test_case.config)
                    .Build(),
                /*is_noised=*/false, kSourceTime,
                StoreSourceResult::InternalError())),
        test_case.matches);
  }
}

TEST_F(AggregatableDebugReportTest, SourceDebugReport) {
  const struct {
    DebugDataType type;
    StoreSourceResult::Result result;
    bool is_noised = false;
  } kTestCases[] = {
      {
          DebugDataType::kSourceChannelCapacityLimit,
          StoreSourceResult::ExceedsMaxChannelCapacity(3.1),
      },
      {
          DebugDataType::kSourceDestinationGlobalRateLimit,
          StoreSourceResult::DestinationGlobalLimitReached(),
      },
      {
          DebugDataType::kSourceDestinationLimit,
          StoreSourceResult::InsufficientUniqueDestinationCapacity(10),
      },
      {
          DebugDataType::kSourceDestinationRateLimit,
          StoreSourceResult::DestinationReportingLimitReached(50),
      },
      {
          DebugDataType::kSourceDestinationRateLimit,
          StoreSourceResult::DestinationBothLimitsReached(50),
      },
      {
          DebugDataType::kSourceNoised,
          StoreSourceResult::Success(/*min_fake_report_time=*/std::nullopt,
                                     kSourceId),
          true,
      },
      {
          DebugDataType::kSourceReportingOriginLimit,
          StoreSourceResult::ExcessiveReportingOrigins(),
      },
      {
          DebugDataType::kSourceReportingOriginPerSiteLimit,
          StoreSourceResult::ReportingOriginsPerSiteLimitReached(2),
      },
      {
          DebugDataType::kSourceStorageLimit,
          StoreSourceResult::InsufficientSourceCapacity(10),
      },
      {
          DebugDataType::kSourceSuccess,
          StoreSourceResult::Success(/*min_fake_report_time=*/std::nullopt,
                                     kSourceId),
      },
      {
          DebugDataType::kSourceTriggerStateCardinalityLimit,
          StoreSourceResult::ExceedsMaxTriggerStateCardinality(3),
      },
      {
          DebugDataType::kSourceUnknownError,
          StoreSourceResult::InternalError(),
      },
  };

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(test_case.type);

    EXPECT_THAT(
        AggregatableDebugReport::Create(
            &OperationAllowed,
            StoreSourceResult(
                SourceBuilder()
                    .SetAggregatableDebugReportingConfig(
                        *SourceAggregatableDebugReportingConfig::Create(
                            /*budget=*/10,
                            AggregatableDebugReportingConfig(
                                /*key_piece=*/1,
                                /*debug_data=*/
                                {
                                    {test_case.type,
                                     *AggregatableDebugReportingContribution::
                                         Create(
                                             /*key_piece=*/2, /*value=*/5)},
                                },
                                /*aggregation_coordinator_origin=*/
                                std::nullopt)))
                    .Build(),
                /*is_noised=*/test_case.is_noised, kSourceTime,
                test_case.result)),
        Optional(Property(
            &AggregatableDebugReport::contributions,
            UnorderedElementsAre(AggregatableReportHistogramContribution(
                /*bucket=*/3, /*value=*/5,
                /*filtering_id=*/std::nullopt)))));
  }
}

TEST_F(AggregatableDebugReportTest, SourceDebugReport_Unsupported) {
  const struct {
    StoreSourceResult::Result result;
  } kTestCases[] = {
      {
          StoreSourceResult::ProhibitedByBrowserPolicy(),
      },
  };

  const StorableSource source =
      SourceBuilder()
          .SetAggregatableDebugReportingConfig(
              *SourceAggregatableDebugReportingConfig::Create(
                  /*budget=*/10,
                  AggregatableDebugReportingConfig(
                      /*key_piece=*/1,
                      DebugDataAll(
                          attribution_reporting::SourceDebugDataTypes()),
                      /*aggregation_coordinator_origin=*/std::nullopt)))
          .Build();

  for (const auto& test_case : kTestCases) {
    EXPECT_THAT(
        AggregatableDebugReport::Create(
            &OperationAllowed,
            StoreSourceResult(source, /*is_noised=*/false, kSourceTime,
                              test_case.result)),
        Optional(Property(&AggregatableDebugReport::contributions, IsEmpty())));
  }
}

TEST_F(AggregatableDebugReportTest, TriggerDebugReport_Enablement) {
  const struct {
    const char* desc;
    bool is_within_fenced_frame = false;
    bool operation_allowed = true;
    AggregatableDebugReportingConfig config = AggregatableDebugReportingConfig(
        /*key_piece=*/3,
        /*debug_data=*/
        {
            {DebugDataType::kTriggerUnknownError,
             *AggregatableDebugReportingContribution::Create(
                 /*key_piece=*/5,
                 /*value=*/3)},
        },
        /*aggregation_coordinator_origin=*/std::nullopt);
    ::testing::Matcher<std::optional<AggregatableDebugReport>> matches;
  } kTestCases[] = {
      {
          .desc = "enabled",
          .matches = Optional(AllOf(Property(
              &AggregatableDebugReport::contributions,
              UnorderedElementsAre(AggregatableReportHistogramContribution(
                  /*bucket=*/7, /*value=*/3,
                  /*filtering_id=*/std::nullopt))))),
      },
      {
          .desc = "no_debug_data",
          .config = AggregatableDebugReportingConfig(),
          .matches = Eq(std::nullopt),
      },
      {
          .desc = "within_fenced_frame",
          .is_within_fenced_frame = true,
          .matches = Eq(std::nullopt),
      },
      {
          .desc = "operation_disallowed",
          .operation_allowed = false,
          .matches = Eq(std::nullopt),
      },
      {
          .desc = "no_matching_debug_data",
          .config = AggregatableDebugReportingConfig(
              /*key_piece=*/3,
              /*debug_data=*/
              {
                  {DebugDataType::kTriggerNoMatchingSource,
                   *AggregatableDebugReportingContribution::Create(
                       /*key_piece=*/5,
                       /*value=*/3)},
              },
              /*aggregation_coordinator_origin=*/std::nullopt),
          .matches = Optional(
              Property(&AggregatableDebugReport::contributions, IsEmpty())),
      },
  };

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(test_case.desc);

    EXPECT_THAT(
        AggregatableDebugReport::Create(
            [&]() { return test_case.operation_allowed; },
            CreateReportResult(
                kTriggerTime,
                TriggerBuilder()
                    .SetAggregatableDebugReportingConfig(test_case.config)
                    .SetIsWithinFencedFrame(test_case.is_within_fenced_frame)
                    .Build(),
                /*event_level_status=*/EventLevelResult::kInternalError,
                /*aggregatable_status=*/AggregatableResult::kInternalError,
                /*replaced_event_level_report=*/std::nullopt,
                /*new_event_level_report=*/std::nullopt,
                /*new_aggregatable_report=*/std::nullopt,
                /*source=*/std::nullopt)),
        test_case.matches);
  }
}

TEST_F(AggregatableDebugReportTest, TriggerDebugReport_EventLevel) {
  const struct {
    EventLevelResult result;
    DebugDataType type;
    bool has_new_report = false;
    bool has_replaced_report = false;
    bool has_dropped_report = false;
    bool has_matching_source = false;
    CreateReportResult::Limits limits;
  } kTestCases[] = {
      {
          .result = EventLevelResult::kInternalError,
          .type = DebugDataType::kTriggerUnknownError,
      },
      {
          .result = EventLevelResult::kNoCapacityForConversionDestination,
          .type = DebugDataType::kTriggerEventStorageLimit,
          .has_matching_source = true,
          .limits =
              CreateReportResult::Limits{
                  .max_event_level_reports_per_destination = 10},
      },
      {
          .result = EventLevelResult::kNoMatchingImpressions,
          .type = DebugDataType::kTriggerNoMatchingSource,
      },
      {
          .result = EventLevelResult::kDeduplicated,
          .type = DebugDataType::kTriggerEventDeduplicated,
          .has_matching_source = true,
      },
      {
          .result = EventLevelResult::kExcessiveAttributions,
          .type =
              DebugDataType::kTriggerEventAttributionsPerSourceDestinationLimit,
          .has_matching_source = true,
          .limits =
              CreateReportResult::Limits{.rate_limits_max_attributions = 10},
      },
      {
          .result = EventLevelResult::kPriorityTooLow,
          .type = DebugDataType::kTriggerEventLowPriority,
          .has_dropped_report = true,
          .has_matching_source = true,
      },
      {
          .result = EventLevelResult::kNeverAttributedSource,
          .type = DebugDataType::kTriggerEventNoise,
          .has_matching_source = true,
      },
      {
          .result = EventLevelResult::kExcessiveReportingOrigins,
          .type = DebugDataType::kTriggerReportingOriginLimit,
          .has_matching_source = true,
          .limits =
              CreateReportResult::Limits{
                  .rate_limits_max_attribution_reporting_origins = 5},
      },
      {
          .result = EventLevelResult::kNoMatchingSourceFilterData,
          .type = DebugDataType::kTriggerNoMatchingFilterData,
          .has_matching_source = true,
      },
      {
          .result = EventLevelResult::kNoMatchingConfigurations,
          .type = DebugDataType::kTriggerEventNoMatchingConfigurations,
          .has_matching_source = true,
      },
      {
          .result = EventLevelResult::kExcessiveReports,
          .type = DebugDataType::kTriggerEventExcessiveReports,
          .has_dropped_report = true,
          .has_matching_source = true,
      },
      {
          .result = EventLevelResult::kFalselyAttributedSource,
          .type = DebugDataType::kTriggerEventNoise,
          .has_matching_source = true,
      },
      {
          .result = EventLevelResult::kReportWindowPassed,
          .type = DebugDataType::kTriggerEventReportWindowPassed,
          .has_matching_source = true,
      },
      {
          .result = EventLevelResult::kReportWindowNotStarted,
          .type = DebugDataType::kTriggerEventReportWindowNotStarted,
          .has_matching_source = true,
      },
      {
          .result = EventLevelResult::kNoMatchingTriggerData,
          .type = DebugDataType::kTriggerEventNoMatchingTriggerData,
          .has_matching_source = true,
      },
  };

  const AttributionReport event_level_report = DefaultEventLevelReport();

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(test_case.result);

    EXPECT_THAT(
        AggregatableDebugReport::Create(
            OperationAllowed,
            CreateReportResult(
                kTriggerTime,
                TriggerBuilder()
                    .SetAggregatableDebugReportingConfig(
                        AggregatableDebugReportingConfig(
                            /*key_piece=*/5, /*debug_data=*/
                            {
                                {test_case.type,
                                 *AggregatableDebugReportingContribution::
                                     Create(
                                         /*key_piece=*/3,
                                         /*value=*/6)},
                            },
                            /*aggregation_coordinator_origin=*/std::nullopt))
                    .Build(),
                /*event_level_status=*/test_case.result,
                /*aggregatable_status=*/AggregatableResult::kNotRegistered,
                test_case.has_replaced_report
                    ? std::make_optional(event_level_report)
                    : std::nullopt,
                test_case.has_new_report
                    ? std::make_optional(event_level_report)
                    : std::nullopt,
                /*new_aggregatable_report=*/std::nullopt,
                test_case.has_matching_source
                    ? std::make_optional(
                          SourceBuilder()
                              .SetAggregatableDebugReportingConfig(
                                  *SourceAggregatableDebugReportingConfig::
                                      Create(
                                          /*budget=*/100,
                                          AggregatableDebugReportingConfig(
                                              /*key_piece=*/9,
                                              /*debug_data=*/{},
                                              /*aggregation_coordinator_origin=*/
                                              std::nullopt)))
                              .BuildStored())
                    : std::nullopt,
                test_case.limits,
                test_case.has_dropped_report
                    ? std::make_optional(event_level_report)
                    : std::nullopt)),
        Optional(Property(
            &AggregatableDebugReport::contributions,
            UnorderedElementsAre(AggregatableReportHistogramContribution(
                /*bucket=*/test_case.has_matching_source ? 15 : 7,
                /*value=*/6,
                /*filtering_id=*/std::nullopt)))));
  }
}

TEST_F(AggregatableDebugReportTest, TriggerDebugReport_EventLevelUnsupported) {
  const struct {
    EventLevelResult result;
    bool has_new_report = false;
    bool has_replaced_report = false;
    bool has_matching_source = false;
  } kTestCases[] = {
      {
          .result = EventLevelResult::kSuccess,
          .has_new_report = true,
          .has_matching_source = true,
      },
      {
          .result = EventLevelResult::kSuccessDroppedLowerPriority,
          .has_new_report = true,
          .has_replaced_report = true,
          .has_matching_source = true,
      },
      {
          .result = EventLevelResult::kProhibitedByBrowserPolicy,
      },
      {
          .result = EventLevelResult::kNotRegistered,
      },
  };

  const AttributionTrigger trigger =
      TriggerBuilder()
          .SetAggregatableDebugReportingConfig(AggregatableDebugReportingConfig(
              /*key_piece=*/2,
              DebugDataAll(attribution_reporting::TriggerDebugDataTypes()),
              /*aggregation_coordinator_origin=*/std::nullopt))
          .Build();

  const AttributionReport event_level_report = DefaultEventLevelReport();

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(test_case.result);

    EXPECT_THAT(
        AggregatableDebugReport::Create(
            OperationAllowed,
            CreateReportResult(
                kTriggerTime, trigger,
                /*event_level_status=*/test_case.result,
                /*aggregatable_status=*/AggregatableResult::kNotRegistered,
                test_case.has_replaced_report
                    ? std::make_optional(event_level_report)
                    : std::nullopt,
                test_case.has_new_report
                    ? std::make_optional(event_level_report)
                    : std::nullopt,
                /*new_aggregatable_report=*/std::nullopt,
                /*source=*/
                test_case.has_matching_source
                    ? std::make_optional(SourceBuilder().BuildStored())
                    : std::nullopt)),
        Optional(Property(&AggregatableDebugReport::contributions, IsEmpty())));
  }
}

TEST_F(AggregatableDebugReportTest, TriggerDebugReport_Aggregatable) {
  const struct {
    AggregatableResult result;
    DebugDataType type;
    bool has_matching_source = false;
    CreateReportResult::Limits limits;
  } kTestCases[] = {
      {
          .result = AggregatableResult::kInternalError,
          .type = DebugDataType::kTriggerUnknownError,
      },
      {
          .result = AggregatableResult::kNoCapacityForConversionDestination,
          .type = DebugDataType::kTriggerAggregateStorageLimit,
          .has_matching_source = true,
          .limits =
              CreateReportResult::Limits{
                  .max_aggregatable_reports_per_destination = 20},
      },
      {
          .result = AggregatableResult::kNoMatchingImpressions,
          .type = DebugDataType::kTriggerNoMatchingSource,
      },
      {
          .result = AggregatableResult::kExcessiveAttributions,
          .type = DebugDataType::
              kTriggerAggregateAttributionsPerSourceDestinationLimit,
          .has_matching_source = true,
          .limits =
              CreateReportResult::Limits{.rate_limits_max_attributions = 10},
      },
      {
          .result = AggregatableResult::kExcessiveReportingOrigins,
          .type = DebugDataType::kTriggerReportingOriginLimit,
          .has_matching_source = true,
          .limits =
              CreateReportResult::Limits{
                  .rate_limits_max_attribution_reporting_origins = 5},
      },
      {
          .result = AggregatableResult::kNoHistograms,
          .type = DebugDataType::kTriggerAggregateNoContributions,
          .has_matching_source = true,
      },
      {
          .result = AggregatableResult::kInsufficientBudget,
          .type = DebugDataType::kTriggerAggregateInsufficientBudget,
          .has_matching_source = true,
      },
      {
          .result = AggregatableResult::kNoMatchingSourceFilterData,
          .type = DebugDataType::kTriggerNoMatchingFilterData,
          .has_matching_source = true,
      },
      {
          .result = AggregatableResult::kDeduplicated,
          .type = DebugDataType::kTriggerAggregateDeduplicated,
          .has_matching_source = true,
      },
      {
          .result = AggregatableResult::kReportWindowPassed,
          .type = DebugDataType::kTriggerAggregateReportWindowPassed,
          .has_matching_source = true,
      },
      {
          .result = AggregatableResult::kExcessiveReports,
          .type = DebugDataType::kTriggerAggregateExcessiveReports,
          .has_matching_source = true,
          .limits =
              CreateReportResult::Limits{.max_aggregatable_reports_per_source =
                                             10},
      },
  };

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(test_case.result);

    EXPECT_THAT(
        AggregatableDebugReport::Create(
            OperationAllowed,
            CreateReportResult(
                kTriggerTime,
                TriggerBuilder()
                    .SetAggregatableDebugReportingConfig(
                        AggregatableDebugReportingConfig(
                            /*key_piece=*/5, /*debug_data=*/
                            {
                                {test_case.type,
                                 *AggregatableDebugReportingContribution::
                                     Create(
                                         /*key_piece=*/3,
                                         /*value=*/6)},
                            },
                            /*aggregation_coordinator_origin=*/std::nullopt))
                    .Build(),
                EventLevelResult::kNotRegistered,
                /*aggregatable_status=*/test_case.result,
                /*replaced_event_level_report=*/std::nullopt,
                /*new_event_level_report=*/std::nullopt,
                /*new_aggregatable_report=*/std::nullopt,
                test_case.has_matching_source
                    ? std::make_optional(
                          SourceBuilder()
                              .SetAggregatableDebugReportingConfig(
                                  *SourceAggregatableDebugReportingConfig::
                                      Create(
                                          /*budget=*/100,
                                          AggregatableDebugReportingConfig(
                                              /*key_piece=*/9,
                                              /*debug_data=*/{},
                                              /*aggregation_coordinator_origin=*/
                                              std::nullopt)))
                              .BuildStored())
                    : std::nullopt,
                test_case.limits)),
        Optional(Property(
            &AggregatableDebugReport::contributions,
            UnorderedElementsAre(AggregatableReportHistogramContribution(
                /*bucket=*/test_case.has_matching_source ? 15 : 7,
                /*value=*/6,
                /*filtering_id=*/std::nullopt)))));
  }
}

TEST_F(AggregatableDebugReportTest,
       TriggerDebugReport_AggregatableUnsupported) {
  const struct {
    AggregatableResult result;
    bool has_new_report = false;
    bool has_matching_source = false;
  } kTestCases[] = {
      {
          .result = AggregatableResult::kSuccess,
          .has_new_report = true,
          .has_matching_source = true,
      },
      {
          .result = AggregatableResult::kProhibitedByBrowserPolicy,
      },
      {
          .result = AggregatableResult::kNotRegistered,
      },
  };

  const AttributionTrigger trigger =
      TriggerBuilder()
          .SetAggregatableDebugReportingConfig(AggregatableDebugReportingConfig(
              /*key_piece=*/2,
              DebugDataAll(attribution_reporting::TriggerDebugDataTypes()),
              /*aggregation_coordinator_origin=*/std::nullopt))
          .Build();

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(test_case.result);

    EXPECT_THAT(
        AggregatableDebugReport::Create(
            OperationAllowed,
            CreateReportResult(
                kTriggerTime, trigger, EventLevelResult::kNotRegistered,
                /*aggregatable_status=*/test_case.result,
                /*replaced_event_level_report=*/std::nullopt,
                /*new_event_level_report=*/std::nullopt,
                test_case.has_new_report
                    ? std::make_optional(DefaultAggregatableReport())
                    : std::nullopt,
                /*source=*/
                test_case.has_matching_source
                    ? std::make_optional(SourceBuilder().BuildStored())
                    : std::nullopt)),
        Optional(Property(&AggregatableDebugReport::contributions, IsEmpty())));
  }
}

TEST_F(AggregatableDebugReportTest,
       TriggerDebugReport_EventLevelAndAggregatable) {
  const struct {
    const char* desc;
    EventLevelResult event_level_result;
    AggregatableResult aggregatable_result;
    bool has_matching_source = false;
    AggregatableDebugReportingConfig config;
    std::vector<AggregatableReportHistogramContribution> expected_contributions;
  } kTestCases[] = {
      {
          .desc = "duplicate",
          .event_level_result = EventLevelResult::kNoMatchingImpressions,
          .aggregatable_result = AggregatableResult::kNoMatchingImpressions,
          .config = AggregatableDebugReportingConfig(
              /*key_piece=*/1,
              /*debug_data=*/
              {
                  {DebugDataType::kTriggerNoMatchingSource,
                   *AggregatableDebugReportingContribution::Create(
                       /*key_piece=*/2,
                       /*value=*/3)},
              },
              /*aggregation_coordinator_origin=*/std::nullopt),
          .expected_contributions = {AggregatableReportHistogramContribution(
              /*bucket=*/3,
              /*value=*/3, /*filtering_id=*/std::nullopt)},
      },
      {
          .desc = "different",
          .event_level_result = EventLevelResult::kDeduplicated,
          .aggregatable_result = AggregatableResult::kDeduplicated,
          .has_matching_source = true,
          .config = AggregatableDebugReportingConfig(
              /*key_piece=*/1,
              /*debug_data=*/
              {
                  {DebugDataType::kTriggerEventDeduplicated,
                   *AggregatableDebugReportingContribution::Create(
                       /*key_piece=*/2,
                       /*value=*/3)},
                  {DebugDataType::kTriggerAggregateDeduplicated,
                   *AggregatableDebugReportingContribution::Create(
                       /*key_piece=*/4,
                       /*value=*/9)},
              },
              /*aggregation_coordinator_origin=*/std::nullopt),
          .expected_contributions = {AggregatableReportHistogramContribution(
                                         /*bucket=*/3,
                                         /*value=*/3,
                                         /*filtering_id=*/std::nullopt),
                                     AggregatableReportHistogramContribution(
                                         /*bucket=*/5,
                                         /*value=*/9,
                                         /*filtering_id=*/std::nullopt)},
      },
  };

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(test_case.desc);

    int expected_budget_required = 0;
    for (const auto& contribution : test_case.expected_contributions) {
      expected_budget_required += contribution.value;
    }

    EXPECT_THAT(
        AggregatableDebugReport::Create(
            &OperationAllowed,
            CreateReportResult(
                kTriggerTime,
                TriggerBuilder()
                    .SetAggregatableDebugReportingConfig(test_case.config)
                    .Build(),
                test_case.event_level_result, test_case.aggregatable_result,
                /*replaced_event_level_report=*/std::nullopt,
                /*new_event_level_report=*/std::nullopt,
                /*new_aggregatable_report=*/std::nullopt,
                test_case.has_matching_source
                    ? std::make_optional(SourceBuilder().BuildStored())
                    : std::nullopt)),
        Optional(AllOf(Property(&AggregatableDebugReport::contributions,
                                UnorderedElementsAreArray(
                                    test_case.expected_contributions)),
                       Property(&AggregatableDebugReport::BudgetRequired,
                                expected_budget_required))));
  }
}

}  // namespace

TEST_F(AggregatableDebugReportTest, SourceDebugReport_Data) {
  const base::Time source_time = base::Time::Now();
  const SuitableOrigin source_origin =
      *SuitableOrigin::Deserialize("https://a.test");
  const SuitableOrigin reporting_origin =
      *SuitableOrigin::Deserialize("https://r.test");
  const SuitableOrigin aggregation_coordinator_origin =
      *SuitableOrigin::Deserialize("https://c.test");

  EXPECT_THAT(
      AggregatableDebugReport::Create(
          &OperationAllowed,
          StoreSourceResult(
              SourceBuilder()
                  .SetSourceOrigin(source_origin)
                  .SetReportingOrigin(reporting_origin)
                  .SetDestinationSites(
                      {net::SchemefulSite::Deserialize("https://d2.test"),
                       net::SchemefulSite::Deserialize("https://d1.test")})
                  .SetAggregatableDebugReportingConfig(
                      *SourceAggregatableDebugReportingConfig::Create(
                          /*budget=*/10,
                          AggregatableDebugReportingConfig(
                              /*key_piece=*/3,
                              /*debug_data=*/
                              {
                                  {DebugDataType::kSourceUnknownError,
                                   *AggregatableDebugReportingContribution::
                                       Create(
                                           /*key_piece=*/6, /*value=*/5)},
                              },
                              aggregation_coordinator_origin)))
                  .Build(),
              /*is_noised=*/false, source_time,
              StoreSourceResult::InternalError())),
      Optional(AllOf(
          Property(&AggregatableDebugReport::context_site,
                   net::SchemefulSite(source_origin)),
          Property(&AggregatableDebugReport::reporting_origin,
                   reporting_origin),
          Property(&AggregatableDebugReport::ReportingSite,
                   net::SchemefulSite(reporting_origin)),
          Property(&AggregatableDebugReport::scheduled_report_time,
                   source_time),
          Property(
              &AggregatableDebugReport::contributions,
              UnorderedElementsAre(AggregatableReportHistogramContribution(
                  /*bucket=*/7, /*value=*/5, /*filtering_id=*/std::nullopt))),
          Field(&AggregatableDebugReport::aggregation_coordinator_origin_,
                Optional(aggregation_coordinator_origin)),
          Field(&AggregatableDebugReport::effective_destination_,
                net::SchemefulSite::Deserialize("https://d1.test")),
          Property(&AggregatableDebugReport::BudgetRequired, 5))));
}

TEST_F(AggregatableDebugReportTest, TriggerDebugReport_Data) {
  const base::Time trigger_time = base::Time::Now();
  const SuitableOrigin destination_origin =
      *SuitableOrigin::Deserialize("https://d.test");
  const SuitableOrigin reporting_origin =
      *SuitableOrigin::Deserialize("https://r.test");
  const SuitableOrigin aggregation_coordinator_origin =
      *SuitableOrigin::Deserialize("https://c.test");

  EXPECT_THAT(
      AggregatableDebugReport::Create(
          &OperationAllowed,
          CreateReportResult(
              trigger_time,
              TriggerBuilder()
                  .SetDestinationOrigin(destination_origin)
                  .SetReportingOrigin(reporting_origin)
                  .SetAggregatableDebugReportingConfig(
                      AggregatableDebugReportingConfig(
                          /*key_piece=*/3,
                          /*debug_data=*/
                          {
                              {DebugDataType::kTriggerUnknownError,
                               *AggregatableDebugReportingContribution::Create(
                                   /*key_piece=*/6,
                                   /*value=*/5)},
                          },
                          aggregation_coordinator_origin))
                  .Build(),
              EventLevelResult::kInternalError,
              AggregatableResult::kInternalError)),
      Optional(AllOf(
          Property(&AggregatableDebugReport::context_site,
                   net::SchemefulSite(destination_origin)),
          Property(&AggregatableDebugReport::reporting_origin,
                   reporting_origin),
          Property(&AggregatableDebugReport::ReportingSite,
                   net::SchemefulSite(reporting_origin)),
          Property(&AggregatableDebugReport::scheduled_report_time,
                   trigger_time),
          Property(
              &AggregatableDebugReport::contributions,
              UnorderedElementsAre(AggregatableReportHistogramContribution(
                  /*bucket=*/7, /*value=*/5, /*filtering_id=*/std::nullopt))),
          Field(&AggregatableDebugReport::aggregation_coordinator_origin_,
                Optional(aggregation_coordinator_origin)),
          Field(&AggregatableDebugReport::effective_destination_,
                net::SchemefulSite(destination_origin)),
          Property(&AggregatableDebugReport::BudgetRequired, 5))));
}

TEST_F(AggregatableDebugReportTest, CreateAggregatableReportRequest) {
  base::test::ScopedFeatureList scoped_feature_list_;
  ::aggregation_service::ScopedAggregationCoordinatorAllowlistForTesting
      scoped_coordinator_allowlist_{
          {url::Origin::Create(GURL("https://a.test"))}};

  base::Time scheduled_report_time =
      base::Time::FromMillisecondsSinceUnixEpoch(1652984901234);
  base::Uuid report_id = DefaultExternalReportID();

  auto report = AggregatableDebugReport::CreateForTesting(
      /*contributions=*/{AggregatableReportHistogramContribution(
          /*bucket=*/123,
          /*value=*/456, /*filtering_id=*/std::nullopt)},
      /*context_site=*/net::SchemefulSite::Deserialize("https://c.test"),
      /*reporting_origin=*/*SuitableOrigin::Deserialize("https://r.test"),
      /*effective_destination=*/
      net::SchemefulSite::Deserialize("https://d.test"),
      /*aggregation_coordinator_origin=*/
      *SuitableOrigin::Deserialize("https://a.test"), scheduled_report_time);
  report.set_report_id(report_id);

  auto request = report.CreateAggregatableReportRequest();
  ASSERT_TRUE(request.has_value());

  auto expected_request = AggregatableReportRequest::Create(
      AggregationServicePayloadContents(
          AggregationServicePayloadContents::Operation::kHistogram,
          {AggregatableReportHistogramContribution(
              /*bucket=*/123,
              /*value=*/456, /*filtering_id=*/std::nullopt)},
          blink::mojom::AggregationServiceMode::kDefault,
          /*aggregation_coordinator_origin=*/
          url::Origin::Create(GURL("https://a.test")),
          /*max_contributions_allowed=*/2,
          /*filtering_id_max_bytes=*/std::nullopt),
      AggregatableReportSharedInfo(
          scheduled_report_time, report_id,
          /*reporting_origin=*/
          url::Origin::Create(GURL("https://r.test")),
          AggregatableReportSharedInfo::DebugMode::kDisabled,
          /*additional_fields=*/
          base::Value::Dict().Set("attribution_destination", "https://d.test"),
          /*api_version=*/"0.1",
          /*api_identifier=*/"attribution-reporting-debug"));
  ASSERT_TRUE(expected_request.has_value());

  EXPECT_TRUE(
      aggregation_service::ReportRequestsEqual(*request, *expected_request));
}

}  // namespace content
