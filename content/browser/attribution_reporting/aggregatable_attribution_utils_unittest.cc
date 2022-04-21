// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/aggregatable_attribution_utils.h"

#include <stdint.h>

#include <limits>
#include <string>
#include <utility>
#include <vector>

#include "content/browser/attribution_reporting/aggregatable_histogram_contribution.h"
#include "content/browser/attribution_reporting/attribution_aggregatable_source.h"
#include "content/browser/attribution_reporting/attribution_aggregatable_trigger.h"
#include "content/browser/attribution_reporting/attribution_filter_data.h"
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

}  // namespace content
