// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/aggregatable_attribution_utils.h"

#include <stdint.h>

#include <optional>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/aggregation_service/aggregation_coordinator_utils.h"
#include "components/attribution_reporting/aggregatable_filtering_id_max_bytes.h"
#include "components/attribution_reporting/aggregatable_trigger_data.h"
#include "components/attribution_reporting/aggregatable_values.h"
#include "components/attribution_reporting/aggregation_keys.h"
#include "components/attribution_reporting/constants.h"
#include "components/attribution_reporting/features.h"
#include "components/attribution_reporting/filters.h"
#include "components/attribution_reporting/source_registration_error.mojom.h"
#include "components/attribution_reporting/source_type.mojom.h"
#include "components/attribution_reporting/suitable_origin.h"
#include "content/browser/aggregation_service/aggregatable_report.h"
#include "content/browser/aggregation_service/aggregation_service_features.h"
#include "content/browser/attribution_reporting/attribution_report.h"
#include "content/browser/attribution_reporting/attribution_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/numeric/int128.h"
#include "third_party/blink/public/mojom/aggregation_service/aggregatable_report.mojom.h"

namespace content {

namespace {

using ::attribution_reporting::AggregatableFilteringIdsMaxBytes;
using ::attribution_reporting::AggregatableValues;
using ::attribution_reporting::AggregatableValuesValue;
using ::attribution_reporting::FilterConfig;
using ::attribution_reporting::FilterPair;
using ::attribution_reporting::kDefaultFilteringId;
using ::attribution_reporting::mojom::SourceType;
using ::blink::mojom::AggregatableReportHistogramContribution;
using ::testing::ElementsAre;

}  // namespace

TEST(AggregatableAttributionUtilsTest, CreateAggregatableHistogram) {
  base::HistogramTester histograms;

  auto source = attribution_reporting::AggregationKeys::FromKeys(
      {{"key1", 345}, {"key2", 5}, {"key3", 123}});
  ASSERT_TRUE(source.has_value());

  base::Time source_time = base::Time::Now();
  base::Time trigger_time = source_time + base::Seconds(5);

  std::vector<attribution_reporting::AggregatableTriggerData>
      aggregatable_trigger_data{
          // The first trigger data applies to "key1", "key3".
          attribution_reporting::AggregatableTriggerData(
              absl::MakeUint128(/*high=*/0, /*low=*/1024),
              /*source_keys=*/{"key1", "key3"},
              FilterPair(
                  /*positive=*/{*FilterConfig::Create({{"filter", {"value"}}})},
                  /*negative=*/{})),

          // The second trigger data applies to "key2", "key4" is ignored.
          attribution_reporting::AggregatableTriggerData(
              absl::MakeUint128(/*high=*/0, /*low=*/2688),
              /*source_keys=*/{"key2", "key4"},
              FilterPair(
                  /*positive=*/{*FilterConfig::Create({{"a", {"b", "c"}}})},
                  /*negative=*/{})),

          // The third trigger will be ignored due to mismatched filters.
          attribution_reporting::AggregatableTriggerData(
              absl::MakeUint128(/*high=*/0, /*low=*/4096),
              /*source_keys=*/{"key1", "key2"},
              FilterPair(/*positive=*/{*FilterConfig::Create({{"filter", {}}})},
                         /*negative=*/{})),

          // The fourth trigger will be ignored due to matched not_filters.
          attribution_reporting::AggregatableTriggerData(
              absl::MakeUint128(/*high=*/0, /*low=*/4096),
              /*source_keys=*/{"key1", "key2"},
              FilterPair(
                  /*positive=*/{},
                  /*negative=*/{*FilterConfig::Create(
                      {{"filter", {"value"}}})})),

          // The fifth trigger will be ignored due to mismatched
          // lookback_window.
          attribution_reporting::AggregatableTriggerData(
              absl::MakeUint128(/*high=*/0, /*low=*/4096),
              /*source_keys=*/{"key1", "key3"},
              FilterPair(
                  /*positive=*/{*FilterConfig::Create(
                      {{"filter", {"value"}}},
                      /*lookback_window=*/base::Seconds(5) -
                          base::Microseconds(1))},
                  /*negative=*/{})),
      };

  std::optional<attribution_reporting::FilterData> source_filter_data =
      attribution_reporting::FilterData::Create({{"filter", {"value"}}});
  ASSERT_TRUE(source_filter_data.has_value());

  auto aggregatable_values = *attribution_reporting::AggregatableValues::Create(
      {{"key1", *AggregatableValuesValue::Create(32768, kDefaultFilteringId)},
       {"key2", *AggregatableValuesValue::Create(1664, kDefaultFilteringId)}},
      FilterPair());

  std::vector<AggregatableReportHistogramContribution> contributions =
      CreateAggregatableHistogram(
          *source_filter_data, SourceType::kEvent, source_time, trigger_time,
          *source, std::move(aggregatable_trigger_data), {aggregatable_values});

  // "key3" is not present as no value is found.
  EXPECT_THAT(
      contributions,
      ElementsAre(AggregatableReportHistogramContribution(
                      /*bucket=*/1369, /*value=*/32768, kDefaultFilteringId),
                  AggregatableReportHistogramContribution(
                      /*bucket=*/2693, /*value=*/1664, kDefaultFilteringId)));

  histograms.ExpectUniqueSample(
      "Conversions.AggregatableReport.FilteredTriggerDataPercentage", 60, 1);
  histograms.ExpectUniqueSample(
      "Conversions.AggregatableReport.DroppedKeysPercentage", 33, 1);
  histograms.ExpectUniqueSample(
      "Conversions.AggregatableReport.NumContributionsPerReport2", 2, 1);
  histograms.ExpectUniqueSample(
      "Conversions.AggregatableReport.TotalBudgetPerReport", 34432, 1);
}

TEST(AggregatableAttributionUtilsTest,
     CreateAggregatableHistogram_ValuesFiltered) {
  auto source = attribution_reporting::AggregationKeys::FromKeys(
      {{"key1", 345}, {"key2", 5}});
  ASSERT_TRUE(source.has_value());

  base::Time source_time = base::Time::Now();
  base::Time trigger_time = source_time + base::Seconds(5);

  std::vector<attribution_reporting::AggregatableTriggerData>
      aggregatable_trigger_data{
          attribution_reporting::AggregatableTriggerData(
              absl::MakeUint128(/*high=*/0, /*low=*/1024),
              /*source_keys=*/{"key1", "key2"}, FilterPair()),
      };

  attribution_reporting::FilterData source_filter_data =
      *attribution_reporting::FilterData::Create({{"product", {"1"}}});

  const struct {
    const char* description;
    std::vector<AggregatableValues> aggregatable_values;
    std::vector<AggregatableReportHistogramContribution> expected;
  } kTestCases[] =
      {{
           .description = "filter_not_matching",
           .aggregatable_values =
               {*attribution_reporting::AggregatableValues::Create(
                   {{"key1", *AggregatableValuesValue::Create(
                                 32768, kDefaultFilteringId)}},
                   FilterPair(
                       /*positive=*/{*FilterConfig::Create(
                           {{"product", {"2"}}})},
                       /*negative=*/{}))},
           .expected = {},
       },
       {
           .description = "first_entry_skipped",
           .aggregatable_values =
               {*attribution_reporting::AggregatableValues::Create(
                    {{"key1", *AggregatableValuesValue::Create(
                                  32768, kDefaultFilteringId)}},
                    FilterPair(
                        /*positive=*/{*FilterConfig::Create(
                            {{"product", {"2"}}})},
                        /*negative=*/{})),
                *attribution_reporting::AggregatableValues::Create(
                    {{"key2", *AggregatableValuesValue::Create(
                                  1664, kDefaultFilteringId)}},
                    FilterPair(
                        /*positive=*/{*FilterConfig::Create(
                            {{"product", {"1"}}})},
                        /*negative=*/{}))},
           .expected = {AggregatableReportHistogramContribution(
               1029, 1664, kDefaultFilteringId)},
       },
       {
           .description = "second_entry_ignored",
           .aggregatable_values =
               {*attribution_reporting::AggregatableValues::Create(
                    {{"key1", *AggregatableValuesValue::Create(
                                  32768, kDefaultFilteringId)}},
                    FilterPair(
                        /*positive=*/{*FilterConfig::Create(
                            {{"product", {"1"}}})},
                        /*negative=*/{})),
                *attribution_reporting::AggregatableValues::Create(
                    {{"key2", *AggregatableValuesValue::Create(
                                  1664, kDefaultFilteringId)}},
                    FilterPair(
                        /*positive=*/{*FilterConfig::Create(
                            {{"product", {"1"}}})},
                        /*negative=*/{}))},
           .expected = {AggregatableReportHistogramContribution(
               1369, 32768,
               kDefaultFilteringId)},
       },
       {
           .description = "filters_matched_keys_mismatched_no_contributions",
           .aggregatable_values =
               {*attribution_reporting::AggregatableValues::Create(
                    {{"key3", *AggregatableValuesValue::Create(
                                  32768, kDefaultFilteringId)}},
                    FilterPair(
                        /*positive=*/{*FilterConfig::Create(
                            {{"product", {"1"}}})},
                        /*negative=*/{})),
                // Shouldn't contribute as only the first aggregatable values
                // entry with matching filters is considered.
                *attribution_reporting::AggregatableValues::Create(
                    {{"key2", *AggregatableValuesValue::Create(
                                  1664, kDefaultFilteringId)}},
                    FilterPair(
                        /*positive=*/{*FilterConfig::Create(
                            {{"product", {"1"}}})},
                        /*negative=*/{}))},
           .expected = {},
       },
       {
           .description = "not_filter_matching_first_entry_skipped",
           .aggregatable_values =
               {*attribution_reporting::AggregatableValues::Create(
                    {{"key1", *AggregatableValuesValue::Create(
                                  32768, kDefaultFilteringId)}},
                    FilterPair(/*positive=*/{},
                               /*negative=*/{*FilterConfig::Create(
                                   {{"product", {"1"}}})})),
                *attribution_reporting::AggregatableValues::Create(
                    {{"key2", *AggregatableValuesValue::Create(
                                  1664, kDefaultFilteringId)}},
                    FilterPair(
                        /*positive=*/{*FilterConfig::Create(
                            {{"product", {"1"}}})},
                        /*negative=*/{}))},
           .expected = {AggregatableReportHistogramContribution(
               1029, 1664, kDefaultFilteringId)},
       }};
  for (auto& test_case : kTestCases) {
    std::vector<AggregatableReportHistogramContribution> contributions =
        CreateAggregatableHistogram(
            source_filter_data, SourceType::kEvent, source_time, trigger_time,
            *source, aggregatable_trigger_data, test_case.aggregatable_values);

    EXPECT_THAT(contributions, test_case.expected) << test_case.description;
  }
}

TEST(AggregatableAttributionUtilsTest,
     NoTriggerData_FilteredPercentageNotRecorded) {
  base::HistogramTester histograms;

  auto source =
      attribution_reporting::AggregationKeys::FromKeys({{"key1", 345}});
  ASSERT_TRUE(source.has_value());

  std::vector<AggregatableReportHistogramContribution> contributions =
      CreateAggregatableHistogram(
          attribution_reporting::FilterData(), SourceType::kNavigation,
          /*source_time=*/base::Time::Now(), /*trigger_time=*/base::Time::Now(),
          *source,
          /*aggregatable_trigger_data=*/{},
          /*aggregatable_values=*/
          {*attribution_reporting::AggregatableValues::Create(
              {{"key2",
                *AggregatableValuesValue::Create(32768, kDefaultFilteringId)}},
              FilterPair())});

  histograms.ExpectTotalCount(
      "Conversions.AggregatableReport.FilteredTriggerDataPercentage", 0);
  histograms.ExpectUniqueSample(
      "Conversions.AggregatableReport.DroppedKeysPercentage", 100, 1);
  histograms.ExpectUniqueSample(
      "Conversions.AggregatableReport.NumContributionsPerReport2", 0, 1);
  histograms.ExpectUniqueSample(
      "Conversions.AggregatableReport.TotalBudgetPerReport", 0, 1);
}

TEST(AggregatableAttributionUtilsTest, RoundsSourceRegistrationTime) {
  const struct {
    std::string description;
    int64_t source_time;
    std::string expected_serialized_time;
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
    base::Time source_time =
        base::Time::FromMillisecondsSinceUnixEpoch(test_case.source_time);
    AttributionReport report =
        ReportBuilder(AttributionInfoBuilder().Build(),
                      SourceBuilder(source_time).BuildStored())
            .SetAggregatableHistogramContributions(
                {AggregatableReportHistogramContribution(
                    /*bucket=*/1,
                    /*value=*/2, /*filtering_id=*/std::nullopt)})
            .BuildAggregatableAttribution();

    std::optional<AggregatableReportRequest> request =
        CreateAggregatableReportRequest(report);
    ASSERT_TRUE(request.has_value());
    const base::Value::Dict& additional_fields =
        request->shared_info().additional_fields;
    const std::string* actual_serialized_time =
        additional_fields.FindString("source_registration_time");
    ASSERT_TRUE(actual_serialized_time);
    EXPECT_EQ(*actual_serialized_time, test_case.expected_serialized_time)
        << test_case.description;
  }
}

TEST(AggregatableAttributionUtilsTest, AggregationCoordinatorSet) {
  auto coordinator_origin = attribution_reporting::SuitableOrigin::Create(
      ::aggregation_service::GetDefaultAggregationCoordinatorOrigin());
  AttributionReport report =
      ReportBuilder(AttributionInfoBuilder().Build(),
                    SourceBuilder().BuildStored())
          .SetAggregatableHistogramContributions(
              {AggregatableReportHistogramContribution(
                  /*bucket=*/1,
                  /*value=*/2, /*filtering_id=*/std::nullopt)})
          .SetAggregationCoordinatorOrigin(*coordinator_origin)
          .BuildAggregatableAttribution();

  std::optional<AggregatableReportRequest> request =
      CreateAggregatableReportRequest(report);
  ASSERT_TRUE(request.has_value());
  EXPECT_EQ(request->payload_contents().aggregation_coordinator_origin,
            coordinator_origin);
}

TEST(AggregatableAttributionUtilsTest, AggregatableReportRequestForNullReport) {
  std::optional<AggregatableReportRequest> request =
      CreateAggregatableReportRequest(
          ReportBuilder(
              AttributionInfoBuilder().Build(),
              SourceBuilder(
                  base::Time::FromMillisecondsSinceUnixEpoch(1234567890123))
                  .BuildStored())
              .BuildNullAggregatable());
  ASSERT_TRUE(request.has_value());
  EXPECT_TRUE(request->payload_contents().contributions.empty());
  EXPECT_FALSE(
      request->payload_contents().aggregation_coordinator_origin.has_value());
  const std::string* source_registration_time =
      request->shared_info().additional_fields.FindString(
          "source_registration_time");
  ASSERT_TRUE(source_registration_time);
  EXPECT_EQ(*source_registration_time, "1234483200");
}

TEST(AggregatableAttributionUtilsTest,
     AggregatableReportRequestExcludingSourceRegistrationTime) {
  std::optional<AggregatableReportRequest> request =
      CreateAggregatableReportRequest(
          ReportBuilder(
              AttributionInfoBuilder().Build(),
              SourceBuilder(
                  base::Time::FromMillisecondsSinceUnixEpoch(1234567890123))
                  .BuildStored())
              .SetAggregatableHistogramContributions(
                  {AggregatableReportHistogramContribution(
                      /*bucket=*/1,
                      /*value=*/2, /*filtering_id=*/std::nullopt)})
              .SetSourceRegistrationTimeConfig(
                  attribution_reporting::mojom::SourceRegistrationTimeConfig::
                      kExclude)
              .BuildAggregatableAttribution());
  ASSERT_TRUE(request.has_value());
  EXPECT_FALSE(request->shared_info().additional_fields.Find(
      "source_registration_time"));
}

TEST(AggregatableAttributionUtilsTest, TotalBudgetMetrics) {
  const struct {
    const char* desc;
    attribution_reporting::AggregationKeys::Keys keys;
    AggregatableValues::Values values;
    int expected;
  } kTestCases[] = {
      {
          .desc = "within-max",
          .keys = {{"a", 1}, {"b", 2}},
          .values = {{"a",
                      *AggregatableValuesValue::Create(1, kDefaultFilteringId)},
                     {"b", *AggregatableValuesValue::Create(
                               65535, kDefaultFilteringId)}},
          .expected = 65536,
      },
      {
          .desc = "exceed-max",
          .keys = {{"a", 1}, {"b", 2}},
          .values = {{"a", *AggregatableValuesValue::Create(
                               10, kDefaultFilteringId)},
                     {"b", *AggregatableValuesValue::Create(
                               65536, kDefaultFilteringId)}},
          .expected = 100000,
      },
  };

  for (auto& test_case : kTestCases) {
    SCOPED_TRACE(test_case.desc);

    base::Time now = base::Time::Now();

    base::HistogramTester histograms;
    std::ignore = CreateAggregatableHistogram(
        attribution_reporting::FilterData(), SourceType::kEvent,
        /*source_time=*/now,
        /*trigger_time=*/now,
        *attribution_reporting::AggregationKeys::FromKeys(test_case.keys),
        {attribution_reporting::AggregatableTriggerData()},
        {*AggregatableValues::Create(test_case.values, FilterPair())});
    histograms.ExpectUniqueSample(
        "Conversions.AggregatableReport.TotalBudgetPerReport",
        test_case.expected, 1);
  }
}

TEST(AggregatableAttributionUtilsTest,
     AggregatableReportRequestWithFilteringIds) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      /*enabled_features=*/{kPrivacySandboxAggregationServiceFilteringIds,
                            attribution_reporting::features::
                                kAttributionReportingAggregatableFilteringIds},
      /*disabled_features=*/{});
  std::optional<AggregatableReportRequest> request =
      CreateAggregatableReportRequest(
          ReportBuilder(AttributionInfoBuilder().Build(),
                        SourceBuilder().BuildStored())
              .SetAggregatableFilteringIdsMaxBytes(
                  *AggregatableFilteringIdsMaxBytes::Create(2))
              .SetAggregatableHistogramContributions(
                  {AggregatableReportHistogramContribution(
                      /*bucket=*/1,
                      /*value=*/2,
                      /*filtering_id=*/3)})
              .SetSourceRegistrationTimeConfig(
                  attribution_reporting::mojom::SourceRegistrationTimeConfig::
                      kExclude)
              .BuildAggregatableAttribution());
  ASSERT_TRUE(request.has_value());
  std::optional<uint64_t> filtering_id =
      request->payload_contents().contributions.front().filtering_id;
  ASSERT_TRUE(filtering_id.has_value());
  EXPECT_EQ(filtering_id, 3u);

  EXPECT_EQ(request->shared_info().api_version, "1.0");

  auto max_bytes = request->payload_contents().filtering_id_max_bytes;
  ASSERT_TRUE(max_bytes.has_value());
  EXPECT_EQ(max_bytes.value(), 2u);
}

TEST(AggregatableAttributionUtilsTest,
     AggregatableReportRequestWithFilteringIdsFeatureDisabled_UnsetInRequest) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      attribution_reporting::features::
          kAttributionReportingAggregatableFilteringIds);
  std::optional<AggregatableReportRequest> request =
      CreateAggregatableReportRequest(
          ReportBuilder(AttributionInfoBuilder().Build(),
                        SourceBuilder().BuildStored())
              .SetAggregatableFilteringIdsMaxBytes(
                  *AggregatableFilteringIdsMaxBytes::Create(2))
              .SetAggregatableHistogramContributions(
                  {AggregatableReportHistogramContribution(
                      /*bucket=*/1,
                      /*value=*/2,
                      /*filtering_id=*/120)})
              .SetSourceRegistrationTimeConfig(
                  attribution_reporting::mojom::SourceRegistrationTimeConfig::
                      kExclude)
              .BuildAggregatableAttribution());
  ASSERT_TRUE(request.has_value());
  std::optional<uint64_t> filtering_id =
      request->payload_contents().contributions.front().filtering_id;
  ASSERT_FALSE(filtering_id.has_value());

  EXPECT_EQ(request->shared_info().api_version, "0.1");

  auto max_bytes = request->payload_contents().filtering_id_max_bytes;
  ASSERT_FALSE(max_bytes.has_value());
}

}  // namespace content
