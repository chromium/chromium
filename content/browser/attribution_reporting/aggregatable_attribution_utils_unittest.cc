// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/aggregatable_attribution_utils.h"

#include <string>
#include <utility>
#include <vector>

#include "content/browser/attribution_reporting/aggregatable_histogram_contribution.h"
#include "content/browser/attribution_reporting/attribution_aggregatable_source.h"
#include "content/browser/attribution_reporting/attribution_aggregatable_trigger.h"
#include "content/browser/attribution_reporting/attribution_filter_data.h"
#include "content/browser/attribution_reporting/attribution_reporting.pb.h"
#include "content/browser/attribution_reporting/attribution_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/mojom/conversions/attribution_data_host.mojom.h"

namespace content {

namespace {

using ::testing::ElementsAre;

using FilterValues = base::flat_map<std::string, std::vector<std::string>>;

}  // namespace

TEST(AggregatableAttributionUtilsTest, CreateAggregatableHistogram) {
  proto::AttributionAggregatableSource source_proto =
      AggregatableSourceProtoBuilder()
          .AddKey("key1", AggregatableKeyProtoBuilder()
                              .SetHighBits(0)
                              .SetLowBits(345)
                              .Build())
          .AddKey("key2", AggregatableKeyProtoBuilder()
                              .SetHighBits(0)
                              .SetLowBits(5)
                              .Build())
          .AddKey("key3", AggregatableKeyProtoBuilder()
                              .SetHighBits(0)
                              .SetLowBits(123)
                              .Build())
          .Build();

  auto trigger_mojo = blink::mojom::AttributionAggregatableTrigger::New();

  // The first trigger data applies to "key1", "key3".
  trigger_mojo->trigger_data.push_back(
      blink::mojom::AttributionAggregatableTriggerData::New(
          blink::mojom::AttributionAggregatableKey::New(/*high_bits=*/0,
                                                        /*low_bits=*/1024),
          /*source_keys=*/std::vector<std::string>{"key1", "key3"},
          /*filters=*/
          blink::mojom::AttributionFilterData::New(
              FilterValues{{"filter", {"value"}}}),
          /*not_filters=*/blink::mojom::AttributionFilterData::New()));

  // The second trigger data applies to "key2", "key4" is ignored.
  trigger_mojo->trigger_data.push_back(
      blink::mojom::AttributionAggregatableTriggerData::New(
          blink::mojom::AttributionAggregatableKey::New(/*high_bits=*/0,
                                                        /*low_bits=*/2688),
          /*source_keys=*/std::vector<std::string>{"key2", "key4"},
          /*filters=*/
          blink::mojom::AttributionFilterData::New(
              FilterValues{{"a", {"b", "c"}}}),
          /*not_filters=*/blink::mojom::AttributionFilterData::New()));

  // The third trigger will be ignored due to mismatched filters.
  trigger_mojo->trigger_data.push_back(
      blink::mojom::AttributionAggregatableTriggerData::New(
          blink::mojom::AttributionAggregatableKey::New(/*high_bits=*/0,
                                                        /*low_bits=*/4096),
          /*source_keys=*/std::vector<std::string>{"key1", "key2"},
          /*filters=*/
          blink::mojom::AttributionFilterData::New(
              FilterValues{{"filter", {}}}),
          /*not_filters=*/blink::mojom::AttributionFilterData::New()));

  // The fourth trigger will be ignored due to matched not_filters.
  trigger_mojo->trigger_data.push_back(
      blink::mojom::AttributionAggregatableTriggerData::New(
          blink::mojom::AttributionAggregatableKey::New(/*high_bits=*/0,
                                                        /*low_bits=*/4096),
          /*source_keys=*/std::vector<std::string>{"key1", "key2"},
          /*filters=*/blink::mojom::AttributionFilterData::New(),
          /*not_filters=*/
          blink::mojom::AttributionFilterData::New(
              FilterValues{{"filter", {"value"}}})));

  trigger_mojo->values = {{"key1", 32768}, {"key2", 1664}};

  absl::optional<AttributionFilterData> source_filter_data =
      AttributionFilterData::FromSourceFilterValues({{"filter", {"value"}}});
  ASSERT_TRUE(source_filter_data.has_value());

  absl::optional<AttributionAggregatableSource> source =
      AttributionAggregatableSource::Create(std::move(source_proto));
  ASSERT_TRUE(source.has_value());

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

}  // namespace content
