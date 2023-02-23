// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/aggregatable_attribution_utils.h"

#include <stdint.h>

#include <string>
#include <utility>
#include <vector>

#include "base/test/metrics/histogram_tester.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/aggregation_service/aggregation_service.mojom.h"
#include "components/attribution_reporting/aggregatable_trigger_data.h"
#include "components/attribution_reporting/aggregatable_values.h"
#include "components/attribution_reporting/aggregation_keys.h"
#include "components/attribution_reporting/filters.h"
#include "components/attribution_reporting/source_type.mojom.h"
#include "content/browser/attribution_reporting/aggregatable_histogram_contribution.h"
#include "content/browser/attribution_reporting/attribution_report.h"
#include "content/browser/attribution_reporting/attribution_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/numeric/int128.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace content {

namespace {

using ::attribution_reporting::FilterPair;
using ::attribution_reporting::mojom::SourceType;
using ::testing::ElementsAre;

using AttributionFilters = ::attribution_reporting::Filters;

}  // namespace

TEST(AggregatableAttributionUtilsTest, CreateAggregatableHistogram) {
  base::HistogramTester histograms;

  auto source = attribution_reporting::AggregationKeys::FromKeys(
      {{"key1", 345}, {"key2", 5}, {"key3", 123}});
  ASSERT_TRUE(source.has_value());

  std::vector<attribution_reporting::AggregatableTriggerData>
      aggregatable_trigger_data{
          // The first trigger data applies to "key1", "key3".
          *attribution_reporting::AggregatableTriggerData::Create(
              absl::MakeUint128(/*high=*/0, /*low=*/1024),
              /*source_keys=*/{"key1", "key3"},
              FilterPair{.positive = *AttributionFilters::Create(
                             {{"filter", {"value"}}})}),

          // The second trigger data applies to "key2", "key4" is ignored.
          *attribution_reporting::AggregatableTriggerData::Create(
              absl::MakeUint128(/*high=*/0, /*low=*/2688),
              /*source_keys=*/{"key2", "key4"},
              FilterPair{.positive =
                             *AttributionFilters::Create({{"a", {"b", "c"}}})}),

          // The third trigger will be ignored due to mismatched filters.
          *attribution_reporting::AggregatableTriggerData::Create(
              absl::MakeUint128(/*high=*/0, /*low=*/4096),
              /*source_keys=*/{"key1", "key2"},
              FilterPair{.positive =
                             *AttributionFilters::Create({{"filter", {}}})}),

          // The fourth trigger will be ignored due to matched not_filters.
          *attribution_reporting::AggregatableTriggerData::Create(
              absl::MakeUint128(/*high=*/0, /*low=*/4096),
              /*source_keys=*/{"key1", "key2"},
              FilterPair{.negative = *AttributionFilters::Create(
                             {{"filter", {"value"}}})})};

  absl::optional<attribution_reporting::FilterData> source_filter_data =
      attribution_reporting::FilterData::Create({{"filter", {"value"}}});
  ASSERT_TRUE(source_filter_data.has_value());

  auto aggregatable_values = *attribution_reporting::AggregatableValues::Create(
      {{"key1", 32768}, {"key2", 1664}});

  std::vector<AggregatableHistogramContribution> contributions =
      CreateAggregatableHistogram(
          *source_filter_data, SourceType::kEvent, *source,
          *attribution_reporting::AggregatableTriggerDataList::Create(
              std::move(aggregatable_trigger_data)),
          aggregatable_values);

  // "key3" is not present as no value is found.
  EXPECT_THAT(
      contributions,
      ElementsAre(
          AggregatableHistogramContribution(/*key=*/1369, /*value=*/32768u),
          AggregatableHistogramContribution(/*key=*/2693, /*value=*/1664u)));

  histograms.ExpectUniqueSample(
      "Conversions.AggregatableReport.FilteredTriggerDataPercentage", 50, 1);
  histograms.ExpectUniqueSample(
      "Conversions.AggregatableReport.DroppedKeysPercentage", 33, 1);
  histograms.ExpectUniqueSample(
      "Conversions.AggregatableReport.NumContributionsPerReport", 2, 1);
}

TEST(AggregatableAttributionUtilsTest,
     NoTriggerData_FilteredPercentageNotRecorded) {
  base::HistogramTester histograms;

  auto source =
      attribution_reporting::AggregationKeys::FromKeys({{"key1", 345}});
  ASSERT_TRUE(source.has_value());

  std::vector<AggregatableHistogramContribution> contributions =
      CreateAggregatableHistogram(
          attribution_reporting::FilterData(), SourceType::kNavigation, *source,
          /*aggregatable_trigger_data=*/{},
          /*aggregatable_values=*/
          *attribution_reporting::AggregatableValues::Create(
              {{"key2", 32768}}));

  histograms.ExpectTotalCount(
      "Conversions.AggregatableReport.FilteredTriggerDataPercentage", 0);
  histograms.ExpectUniqueSample(
      "Conversions.AggregatableReport.DroppedKeysPercentage", 100, 1);
  histograms.ExpectUniqueSample(
      "Conversions.AggregatableReport.NumContributionsPerReport", 0, 1);
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
    base::Time source_time = base::Time::FromJavaTime(test_case.source_time);
    AttributionReport report =
        ReportBuilder(
            AttributionInfoBuilder(SourceBuilder(source_time).BuildStored())
                .Build())
            .SetAggregatableHistogramContributions(
                {AggregatableHistogramContribution(/*key=*/1, /*value=*/2)})
            .BuildAggregatableAttribution();

    absl::optional<AggregatableReportRequest> request =
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
  for (auto aggregation_coordinator :
       {::aggregation_service::mojom::AggregationCoordinator::kAwsCloud}) {
    AttributionReport report =
        ReportBuilder(
            AttributionInfoBuilder(SourceBuilder().BuildStored()).Build())
            .SetAggregatableHistogramContributions(
                {AggregatableHistogramContribution(/*key=*/1, /*value=*/2)})
            .SetAggregationCoordinator(aggregation_coordinator)
            .BuildAggregatableAttribution();

    absl::optional<AggregatableReportRequest> request =
        CreateAggregatableReportRequest(report);
    ASSERT_TRUE(request.has_value()) << aggregation_coordinator;
    EXPECT_EQ(request->payload_contents().aggregation_coordinator,
              aggregation_coordinator)
        << aggregation_coordinator;
  }
}

}  // namespace content
