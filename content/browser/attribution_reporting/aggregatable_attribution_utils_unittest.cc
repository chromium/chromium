// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/aggregatable_attribution_utils.h"

#include <stdint.h>

#include <limits>
#include <string>
#include <utility>
#include <vector>

#include "base/test/metrics/histogram_tester.h"
#include "base/time/time.h"
#include "base/values.h"
#include "content/browser/attribution_reporting/aggregatable_histogram_contribution.h"
#include "content/browser/attribution_reporting/attribution_aggregatable_trigger_data.h"
#include "content/browser/attribution_reporting/attribution_aggregatable_values.h"
#include "content/browser/attribution_reporting/attribution_aggregation_keys.h"
#include "content/browser/attribution_reporting/attribution_filter_data.h"
#include "content/browser/attribution_reporting/attribution_report.h"
#include "content/browser/attribution_reporting/attribution_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/numeric/int128.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace content {

namespace {

using ::testing::ElementsAre;

using FilterValues = base::flat_map<std::string, std::vector<std::string>>;

}  // namespace

TEST(AggregatableAttributionUtilsTest, CreateAggregatableHistogram) {
  base::HistogramTester histograms;

  auto source = AttributionAggregationKeys::FromKeys(
      {{"key1", 345}, {"key2", 5}, {"key3", 123}});
  ASSERT_TRUE(source.has_value());

  std::vector<AttributionAggregatableTriggerData> aggregatable_trigger_data{
      // The first trigger data applies to "key1", "key3".
      AttributionAggregatableTriggerData::CreateForTesting(
          absl::MakeUint128(/*high=*/0, /*low=*/1024),
          /*source_keys=*/{"key1", "key3"},
          /*filters=*/
          AttributionFilterData::CreateForTesting({{"filter", {"value"}}}),
          /*not_filters=*/AttributionFilterData()),

      // The second trigger data applies to "key2", "key4" is ignored.
      AttributionAggregatableTriggerData::CreateForTesting(
          absl::MakeUint128(/*high=*/0, /*low=*/2688),
          /*source_keys=*/{"key2", "key4"},
          /*filters=*/
          AttributionFilterData::CreateForTesting({{"a", {"b", "c"}}}),
          /*not_filters=*/AttributionFilterData()),

      // The third trigger will be ignored due to mismatched filters.
      AttributionAggregatableTriggerData::CreateForTesting(
          absl::MakeUint128(/*high=*/0, /*low=*/4096),
          /*source_keys=*/{"key1", "key2"},
          /*filters=*/
          AttributionFilterData::CreateForTesting({{"filter", {}}}),
          /*not_filters=*/AttributionFilterData()),

      // The fourth trigger will be ignored due to matched not_filters.
      AttributionAggregatableTriggerData::CreateForTesting(
          absl::MakeUint128(/*high=*/0, /*low=*/4096),
          /*source_keys=*/{"key1", "key2"},
          /*filters=*/AttributionFilterData(),
          /*not_filters=*/
          AttributionFilterData::CreateForTesting({{"filter", {"value"}}}))};

  absl::optional<AttributionFilterData> source_filter_data =
      AttributionFilterData::FromSourceFilterValues({{"filter", {"value"}}});
  ASSERT_TRUE(source_filter_data.has_value());

  auto aggregatable_values = AttributionAggregatableValues::CreateForTesting(
      {{"key1", 32768}, {"key2", 1664}});

  std::vector<AggregatableHistogramContribution> contributions =
      CreateAggregatableHistogram(*source_filter_data, *source,
                                  aggregatable_trigger_data,
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

TEST(AggregatableAttributionUtilsTest, HexEncodeAggregationKey) {
  const struct {
    absl::uint128 input;
    std::string output;
  } kTestCases[] = {
      {0, "0x0"},
      {absl::MakeUint128(/*high=*/0,
                         /*low=*/std::numeric_limits<uint64_t>::max()),
       "0xffffffffffffffff"},
      {absl::MakeUint128(/*high=*/1,
                         /*low=*/std::numeric_limits<uint64_t>::max()),
       "0x1ffffffffffffffff"},
      {std::numeric_limits<absl::uint128>::max(),
       "0xffffffffffffffffffffffffffffffff"},
  };

  for (const auto& test_case : kTestCases) {
    EXPECT_EQ(HexEncodeAggregationKey(test_case.input), test_case.output)
        << test_case.input;
  }
}

TEST(AggregatableAttributionUtilsTest,
     NoTriggerData_FilteredPercentageNotRecorded) {
  base::HistogramTester histograms;

  auto source = AttributionAggregationKeys::FromKeys({{"key1", 345}});
  ASSERT_TRUE(source.has_value());

  std::vector<AggregatableHistogramContribution> contributions =
      CreateAggregatableHistogram(
          AttributionFilterData(), *source,
          /*aggregatable_trigger_data=*/{},
          /*aggregatable_values=*/
          AttributionAggregatableValues::CreateForTesting({{"key2", 32768}}));

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

}  // namespace content
