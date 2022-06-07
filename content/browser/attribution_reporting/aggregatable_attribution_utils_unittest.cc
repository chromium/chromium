// Copyright 2022 The Chromium Authors. All rights reserved.
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
#include "content/browser/attribution_reporting/attribution_aggregatable_source.h"
#include "content/browser/attribution_reporting/attribution_aggregatable_trigger.h"
#include "content/browser/attribution_reporting/attribution_filter_data.h"
#include "content/browser/attribution_reporting/attribution_report.h"
#include "content/browser/attribution_reporting/attribution_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/numeric/int128.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/mojom/conversions/attribution_data_host.mojom.h"

namespace content {

namespace {

using ::testing::ElementsAre;

using FilterValues = base::flat_map<std::string, std::vector<std::string>>;

}  // namespace

TEST(AggregatableAttributionUtilsTest, CreateAggregatableHistogram) {
  base::HistogramTester histograms;

  auto source = AttributionAggregatableSource::FromKeys(
      {{"key1", 345}, {"key2", 5}, {"key3", 123}});
  ASSERT_TRUE(source.has_value());

  auto trigger_mojo = blink::mojom::AttributionAggregatableTrigger::New();

  // The first trigger data applies to "key1", "key3".
  trigger_mojo->trigger_data.push_back(
      blink::mojom::AttributionAggregatableTriggerData::New(
          absl::MakeUint128(/*high=*/0, /*low=*/1024),
          /*source_keys=*/std::vector<std::string>{"key1", "key3"},
          /*filters=*/
          blink::mojom::AttributionFilterData::New(
              FilterValues{{"filter", {"value"}}}),
          /*not_filters=*/blink::mojom::AttributionFilterData::New()));

  // The second trigger data applies to "key2", "key4" is ignored.
  trigger_mojo->trigger_data.push_back(
      blink::mojom::AttributionAggregatableTriggerData::New(
          absl::MakeUint128(/*high=*/0, /*low=*/2688),
          /*source_keys=*/std::vector<std::string>{"key2", "key4"},
          /*filters=*/
          blink::mojom::AttributionFilterData::New(
              FilterValues{{"a", {"b", "c"}}}),
          /*not_filters=*/blink::mojom::AttributionFilterData::New()));

  // The third trigger will be ignored due to mismatched filters.
  trigger_mojo->trigger_data.push_back(
      blink::mojom::AttributionAggregatableTriggerData::New(
          absl::MakeUint128(/*high=*/0, /*low=*/4096),
          /*source_keys=*/std::vector<std::string>{"key1", "key2"},
          /*filters=*/
          blink::mojom::AttributionFilterData::New(
              FilterValues{{"filter", {}}}),
          /*not_filters=*/blink::mojom::AttributionFilterData::New()));

  // The fourth trigger will be ignored due to matched not_filters.
  trigger_mojo->trigger_data.push_back(
      blink::mojom::AttributionAggregatableTriggerData::New(
          absl::MakeUint128(/*high=*/0, /*low=*/4096),
          /*source_keys=*/std::vector<std::string>{"key1", "key2"},
          /*filters=*/blink::mojom::AttributionFilterData::New(),
          /*not_filters=*/
          blink::mojom::AttributionFilterData::New(
              FilterValues{{"filter", {"value"}}})));

  trigger_mojo->values = {{"key1", 32768}, {"key2", 1664}};

  absl::optional<AttributionFilterData> source_filter_data =
      AttributionFilterData::FromSourceFilterValues({{"filter", {"value"}}});
  ASSERT_TRUE(source_filter_data.has_value());

  absl::optional<AttributionAggregatableTrigger> trigger =
      AttributionAggregatableTrigger::FromMojo(std::move(trigger_mojo));
  ASSERT_TRUE(trigger.has_value());

  std::vector<AggregatableHistogramContribution> contributions =
      CreateAggregatableHistogram(*source_filter_data, *source, *trigger);

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

TEST(AggregatableAttributionUtilsTest, HexEncodeAggregatableKey) {
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
    EXPECT_EQ(HexEncodeAggregatableKey(test_case.input), test_case.output)
        << test_case.input;
  }
}

TEST(AggregatableAttributionUtilsTest,
     NoTriggerData_FilteredPercentageNotRecorded) {
  base::HistogramTester histograms;

  auto source = AttributionAggregatableSource::FromKeys({{"key1", 345}});
  ASSERT_TRUE(source.has_value());

  auto trigger_mojo = blink::mojom::AttributionAggregatableTrigger::New();
  trigger_mojo->values = {{"key2", 32768}};

  absl::optional<AttributionAggregatableTrigger> trigger =
      AttributionAggregatableTrigger::FromMojo(std::move(trigger_mojo));
  ASSERT_TRUE(trigger.has_value());

  std::vector<AggregatableHistogramContribution> contributions =
      CreateAggregatableHistogram(AttributionFilterData(), *source, *trigger);

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
