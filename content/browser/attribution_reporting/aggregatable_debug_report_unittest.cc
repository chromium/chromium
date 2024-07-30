// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/aggregatable_debug_report.h"

#include <stddef.h>

#include <optional>
#include <vector>

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
#include "content/browser/aggregation_service/aggregation_service_features.h"
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
#include "third_party/blink/public/mojom/aggregation_service/aggregatable_report.mojom.h"
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

class AggregatableDebugReportTest : public testing::Test,
                                    public testing::WithParamInterface<bool> {
 public:
  AggregatableDebugReportTest() {
    std::vector<base::test::FeatureRef> enabled_features = {
        attribution_reporting::features::
            kAttributionAggregatableDebugReporting};
    std::vector<base::test::FeatureRef> disabled_features;

    const bool filtering_ids_enabled = GetParam();
    if (filtering_ids_enabled) {
      enabled_features.emplace_back(
          attribution_reporting::features::
              kAttributionReportingAggregatableFilteringIds);
      enabled_features.emplace_back(
          kPrivacySandboxAggregationServiceFilteringIds);
    } else {
      disabled_features.emplace_back(
          attribution_reporting::features::
              kAttributionReportingAggregatableFilteringIds);
    }

    scoped_feature_list_.InitWithFeatures(enabled_features, disabled_features);
  }

  bool filtering_ids_enabled() const { return GetParam(); }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(All, AggregatableDebugReportTest, ::testing::Bool());

TEST_P(AggregatableDebugReportTest, SourceDebugReport_Enablement) {
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
                /*destination_limit=*/std::nullopt,
                StoreSourceResult::InternalError())),
        test_case.matches);
  }
}

TEST_P(AggregatableDebugReportTest, SourceDebugReport) {
  const struct {
    DebugDataType type;
    StoreSourceResult::Result result;
    bool is_noised = false;
    std::optional<int> destination_limit;
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
          DebugDataType::kSourceDestinationGlobalRateLimit,
          StoreSourceResult::DestinationGlobalLimitReached(),
          false,
          10,
      },
      {
          DebugDataType::kSourceDestinationLimit,
          StoreSourceResult::InsufficientUniqueDestinationCapacity(10),
      },
      {
          DebugDataType::kSourceDestinationPerDayRateLimit,
          StoreSourceResult::DestinationPerDayReportingLimitReached(20),
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
          DebugDataType::kSourceNoised,
          StoreSourceResult::Success(/*min_fake_report_time=*/std::nullopt,
                                     kSourceId),
          true,
          10,
      },
      {
          DebugDataType::kSourceReportingOriginLimit,
          StoreSourceResult::ExcessiveReportingOrigins(),
      },
      {
          DebugDataType::kSourceReportingOriginLimit,
          StoreSourceResult::ExcessiveReportingOrigins(),
          false,
          10,
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
          DebugDataType::kSourceSuccess,
          StoreSourceResult::Success(/*min_fake_report_time=*/std::nullopt,
                                     kSourceId),
          false,
          10,
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

    std::vector<AggregatableReportHistogramContribution> expected_contributions(
        {AggregatableReportHistogramContribution(
            /*bucket=*/3, /*value=*/5,
            /*filtering_id=*/std::nullopt)});
    if (test_case.destination_limit.has_value()) {
      expected_contributions.emplace_back(/*bucket=*/5, /*value=*/6,
                                          /*filtering_id=*/std::nullopt);
    }

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
                                    {DebugDataType::
                                         kSourceDestinationLimitReplaced,
                                     *AggregatableDebugReportingContribution::
                                         Create(
                                             /*key+piece=*/4, /*value=*/6)},
                                },
                                /*aggregation_coordinator_origin=*/
                                std::nullopt)))
                    .Build(),
                /*is_noised=*/test_case.is_noised, kSourceTime,
                test_case.destination_limit, test_case.result)),
        Optional(Property(&AggregatableDebugReport::contributions,
                          UnorderedElementsAreArray(expected_contributions))));
  }
}

TEST_P(AggregatableDebugReportTest, SourceDebugReport_Unsupported) {
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
                              /*destination_limit=*/std::nullopt,
                              test_case.result)),
        Optional(Property(&AggregatableDebugReport::contributions, IsEmpty())));
  }
}

TEST_P(AggregatableDebugReportTest, TriggerDebugReport_Enablement) {
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
                /*event_level_result=*/CreateReportResult::InternalError(),
                /*aggregatable_result=*/CreateReportResult::InternalError(),
                /*source=*/std::nullopt,
                /*min_null_aggregatble_report_time=*/std::nullopt)),
        test_case.matches);
  }
}

TEST_P(AggregatableDebugReportTest, TriggerDebugReport_EventLevel) {
  const struct {
    CreateReportResult::EventLevel result;
    DebugDataType type;
    bool has_matching_source = false;
  } kTestCases[] = {
      {
          .result = CreateReportResult::InternalError(),
          .type = DebugDataType::kTriggerUnknownError,
      },
      {
          .result = CreateReportResult::NoCapacityForConversionDestination(
              /*max=*/10),
          .type = DebugDataType::kTriggerEventStorageLimit,
          .has_matching_source = true,
      },
      {
          .result = CreateReportResult::NoMatchingImpressions(),
          .type = DebugDataType::kTriggerNoMatchingSource,
      },
      {
          .result = CreateReportResult::Deduplicated(),
          .type = DebugDataType::kTriggerEventDeduplicated,
          .has_matching_source = true,
      },
      {
          .result = CreateReportResult::ExcessiveAttributions(/*max=*/10),
          .type =
              DebugDataType::kTriggerEventAttributionsPerSourceDestinationLimit,
          .has_matching_source = true,
      },
      {
          .result =
              CreateReportResult::PriorityTooLow(DefaultEventLevelReport()),
          .type = DebugDataType::kTriggerEventLowPriority,
          .has_matching_source = true,
      },
      {
          .result = CreateReportResult::NeverAttributedSource(),
          .type = DebugDataType::kTriggerEventNoise,
          .has_matching_source = true,
      },
      {
          .result = CreateReportResult::ExcessiveReportingOrigins(/*max=*/5),
          .type = DebugDataType::kTriggerReportingOriginLimit,
          .has_matching_source = true,
      },
      {
          .result = CreateReportResult::NoMatchingSourceFilterData(),
          .type = DebugDataType::kTriggerNoMatchingFilterData,
          .has_matching_source = true,
      },
      {
          .result = CreateReportResult::NoMatchingConfigurations(),
          .type = DebugDataType::kTriggerEventNoMatchingConfigurations,
          .has_matching_source = true,
      },
      {
          .result = CreateReportResult::ExcessiveEventLevelReports(
              DefaultEventLevelReport()),
          .type = DebugDataType::kTriggerEventExcessiveReports,
          .has_matching_source = true,
      },
      {
          .result = CreateReportResult::FalselyAttributedSource(),
          .type = DebugDataType::kTriggerEventNoise,
          .has_matching_source = true,
      },
      {
          .result = CreateReportResult::ReportWindowPassed(),
          .type = DebugDataType::kTriggerEventReportWindowPassed,
          .has_matching_source = true,
      },
      {
          .result = CreateReportResult::ReportWindowNotStarted(),
          .type = DebugDataType::kTriggerEventReportWindowNotStarted,
          .has_matching_source = true,
      },
      {
          .result = CreateReportResult::NoMatchingTriggerData(),
          .type = DebugDataType::kTriggerEventNoMatchingTriggerData,
          .has_matching_source = true,
      },
  };

  for (const auto& test_case : kTestCases) {
    const CreateReportResult result(
        kTriggerTime,
        TriggerBuilder()
            .SetAggregatableDebugReportingConfig(
                AggregatableDebugReportingConfig(
                    /*key_piece=*/5, /*debug_data=*/
                    {
                        {test_case.type,
                         *AggregatableDebugReportingContribution::Create(
                             /*key_piece=*/3,
                             /*value=*/6)},
                    },
                    /*aggregation_coordinator_origin=*/std::nullopt))
            .Build(),
        /*event_level_result=*/test_case.result,
        /*aggregatable_result=*/CreateReportResult::NotRegistered(),
        test_case.has_matching_source
            ? std::make_optional(
                  SourceBuilder()
                      .SetAggregatableDebugReportingConfig(
                          *SourceAggregatableDebugReportingConfig::Create(
                              /*budget=*/100,
                              AggregatableDebugReportingConfig(
                                  /*key_piece=*/9,
                                  /*debug_data=*/{},
                                  /*aggregation_coordinator_origin=*/
                                  std::nullopt)))
                      .BuildStored())
            : std::nullopt,
        /*min_null_aggregatable_report_time=*/std::nullopt);

    SCOPED_TRACE(result.event_level_status());

    EXPECT_THAT(
        AggregatableDebugReport::Create(OperationAllowed, result),
        Optional(Property(
            &AggregatableDebugReport::contributions,
            UnorderedElementsAre(AggregatableReportHistogramContribution(
                /*bucket=*/test_case.has_matching_source ? 15 : 7,
                /*value=*/6,
                /*filtering_id=*/std::nullopt)))));
  }
}

TEST_P(AggregatableDebugReportTest, TriggerDebugReport_EventLevelUnsupported) {
  const struct {
    CreateReportResult::EventLevel result;
    bool has_matching_source = false;
    bool expected;
  } kTestCases[] = {
      {
          .result = CreateReportResult::EventLevelSuccess(
              DefaultEventLevelReport(), /*replaced_report=*/std::nullopt),
          .has_matching_source = true,
          .expected = true,
      },
      {
          .result = CreateReportResult::EventLevelSuccess(
              DefaultEventLevelReport(),
              /*replaced_report=*/DefaultEventLevelReport()),
          .has_matching_source = true,
          .expected = true,
      },
      {
          .result = CreateReportResult::ProhibitedByBrowserPolicy(),
          .expected = true,
      },
      {
          .result = CreateReportResult::NotRegistered(),
          .expected = false,
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
    const CreateReportResult result(
        kTriggerTime, trigger,
        /*event_level_result=*/test_case.result,
        /*aggregatable_result=*/CreateReportResult::NotRegistered(),
        /*source=*/
        test_case.has_matching_source
            ? std::make_optional(SourceBuilder().BuildStored())
            : std::nullopt,
        /*min_null_aggregatable_report_time=*/std::nullopt);

    SCOPED_TRACE(result.event_level_status());

    auto report = AggregatableDebugReport::Create(OperationAllowed, result);

    if (test_case.expected) {
      EXPECT_THAT(report,
                  Optional(Property(&AggregatableDebugReport::contributions,
                                    IsEmpty())));
    } else {
      EXPECT_FALSE(report.has_value());
    }
  }
}

TEST_P(AggregatableDebugReportTest, TriggerDebugReport_Aggregatable) {
  const struct {
    CreateReportResult::Aggregatable result;
    DebugDataType type;
    bool has_matching_source = false;
  } kTestCases[] = {
      {
          .result = CreateReportResult::InternalError(),
          .type = DebugDataType::kTriggerUnknownError,
      },
      {
          .result = CreateReportResult::NoCapacityForConversionDestination(
              /*max=*/20),
          .type = DebugDataType::kTriggerAggregateStorageLimit,
          .has_matching_source = true,
      },
      {
          .result = CreateReportResult::NoMatchingImpressions(),
          .type = DebugDataType::kTriggerNoMatchingSource,
      },
      {
          .result = CreateReportResult::ExcessiveAttributions(/*max=*/10),
          .type = DebugDataType::
              kTriggerAggregateAttributionsPerSourceDestinationLimit,
          .has_matching_source = true,
      },
      {
          .result = CreateReportResult::ExcessiveReportingOrigins(/*max=*/5),
          .type = DebugDataType::kTriggerReportingOriginLimit,
          .has_matching_source = true,
      },
      {
          .result = CreateReportResult::NoHistograms(),
          .type = DebugDataType::kTriggerAggregateNoContributions,
          .has_matching_source = true,
      },
      {
          .result = CreateReportResult::InsufficientBudget(),
          .type = DebugDataType::kTriggerAggregateInsufficientBudget,
          .has_matching_source = true,
      },
      {
          .result = CreateReportResult::NoMatchingSourceFilterData(),
          .type = DebugDataType::kTriggerNoMatchingFilterData,
          .has_matching_source = true,
      },
      {
          .result = CreateReportResult::Deduplicated(),
          .type = DebugDataType::kTriggerAggregateDeduplicated,
          .has_matching_source = true,
      },
      {
          .result = CreateReportResult::ReportWindowPassed(),
          .type = DebugDataType::kTriggerAggregateReportWindowPassed,
          .has_matching_source = true,
      },
      {
          .result =
              CreateReportResult::ExcessiveAggregatableReports(/*max=*/10),
          .type = DebugDataType::kTriggerAggregateExcessiveReports,
          .has_matching_source = true,
      },
  };

  for (const auto& test_case : kTestCases) {
    const CreateReportResult result(
        kTriggerTime,
        TriggerBuilder()
            .SetAggregatableDebugReportingConfig(
                AggregatableDebugReportingConfig(
                    /*key_piece=*/5, /*debug_data=*/
                    {
                        {test_case.type,
                         *AggregatableDebugReportingContribution::Create(
                             /*key_piece=*/3,
                             /*value=*/6)},
                    },
                    /*aggregation_coordinator_origin=*/std::nullopt))
            .Build(),
        /*event_level_result=*/CreateReportResult::NotRegistered(),
        /*aggregatable_result=*/test_case.result,
        test_case.has_matching_source
            ? std::make_optional(
                  SourceBuilder()
                      .SetAggregatableDebugReportingConfig(
                          *SourceAggregatableDebugReportingConfig::Create(
                              /*budget=*/100,
                              AggregatableDebugReportingConfig(
                                  /*key_piece=*/9,
                                  /*debug_data=*/{},
                                  /*aggregation_coordinator_origin=*/
                                  std::nullopt)))
                      .BuildStored())
            : std::nullopt,
        /*min_null_aggregatable_report_time=*/std::nullopt);

    SCOPED_TRACE(result.aggregatable_status());

    EXPECT_THAT(
        AggregatableDebugReport::Create(OperationAllowed, result),
        Optional(Property(
            &AggregatableDebugReport::contributions,
            UnorderedElementsAre(AggregatableReportHistogramContribution(
                /*bucket=*/test_case.has_matching_source ? 15 : 7,
                /*value=*/6,
                /*filtering_id=*/std::nullopt)))));
  }
}

TEST_P(AggregatableDebugReportTest,
       TriggerDebugReport_AggregatableUnsupported) {
  const struct {
    CreateReportResult::Aggregatable result;
    bool has_matching_source = false;
    bool expected;
  } kTestCases[] = {
      {
          .result = CreateReportResult::AggregatableSuccess(
              DefaultAggregatableReport()),
          .has_matching_source = true,
          .expected = true,
      },
      {
          .result = CreateReportResult::ProhibitedByBrowserPolicy(),
          .expected = true,
      },
      {
          .result = CreateReportResult::NotRegistered(),
          .expected = false,
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
    const CreateReportResult result(
        kTriggerTime, trigger,
        /*event_level_result=*/CreateReportResult::NotRegistered(),
        /*aggregatable_result=*/test_case.result,
        /*source=*/
        test_case.has_matching_source
            ? std::make_optional(SourceBuilder().BuildStored())
            : std::nullopt,
        /*min_null_aggregatable_report_time=*/std::nullopt);

    SCOPED_TRACE(result.aggregatable_status());

    auto report = AggregatableDebugReport::Create(OperationAllowed, result);

    if (test_case.expected) {
      EXPECT_THAT(report,
                  Optional(Property(&AggregatableDebugReport::contributions,
                                    IsEmpty())));
    } else {
      EXPECT_FALSE(report.has_value());
    }
  }
}

TEST_P(AggregatableDebugReportTest,
       TriggerDebugReport_EventLevelAndAggregatable) {
  const struct {
    const char* desc;
    CreateReportResult::EventLevel event_level_result;
    CreateReportResult::Aggregatable aggregatable_result;
    bool has_matching_source = false;
    AggregatableDebugReportingConfig config;
    std::vector<AggregatableReportHistogramContribution> expected_contributions;
  } kTestCases[] = {
      {
          .desc = "duplicate",
          .event_level_result = CreateReportResult::NoMatchingImpressions(),
          .aggregatable_result = CreateReportResult::NoMatchingImpressions(),
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
          .event_level_result = CreateReportResult::Deduplicated(),
          .aggregatable_result = CreateReportResult::Deduplicated(),
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
                test_case.has_matching_source
                    ? std::make_optional(SourceBuilder().BuildStored())
                    : std::nullopt,
                /*min_null_aggregatable_report_time=*/std::nullopt)),
        Optional(AllOf(Property(&AggregatableDebugReport::contributions,
                                UnorderedElementsAreArray(
                                    test_case.expected_contributions)),
                       Property(&AggregatableDebugReport::BudgetRequired,
                                expected_budget_required))));
  }
}

}  // namespace

TEST_P(AggregatableDebugReportTest, SourceDebugReport_Data) {
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
              /*destination_limit=*/std::nullopt,
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

TEST_P(AggregatableDebugReportTest, TriggerDebugReport_Data) {
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
              CreateReportResult::InternalError(),
              CreateReportResult::InternalError(),
              /*source=*/std::nullopt,
              /*min_null_aggregatable_report_time=*/std::nullopt)),
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

TEST_P(AggregatableDebugReportTest, CreateAggregatableReportRequest) {
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

  std::optional<size_t> expected_filtering_id_max_bytes;
  if (filtering_ids_enabled()) {
    expected_filtering_id_max_bytes = 1u;
  }
  auto expected_request = AggregatableReportRequest::Create(
      AggregationServicePayloadContents(
          AggregationServicePayloadContents::Operation::kHistogram,
          {AggregatableReportHistogramContribution(
              /*bucket=*/123,
              /*value=*/456, /*filtering_id=*/std::nullopt)},
          blink::mojom::AggregationServiceMode::kDefault,
          /*aggregation_coordinator_origin=*/
          url::Origin::Create(GURL("https://a.test")),
          /*max_contributions_allowed=*/2u, expected_filtering_id_max_bytes),
      AggregatableReportSharedInfo(
          scheduled_report_time, report_id,
          /*reporting_origin=*/
          url::Origin::Create(GURL("https://r.test")),
          AggregatableReportSharedInfo::DebugMode::kDisabled,
          /*additional_fields=*/
          base::Value::Dict().Set("attribution_destination", "https://d.test"),
          /*api_version=*/filtering_ids_enabled() ? "1.0" : "0.1",
          /*api_identifier=*/"attribution-reporting-debug"));
  ASSERT_TRUE(expected_request.has_value());

  EXPECT_TRUE(
      aggregation_service::ReportRequestsEqual(*request, *expected_request));
}

}  // namespace content
