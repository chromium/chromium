// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_header_utils.h"

#include <limits>
#include <memory>
#include <string>
#include <utility>

#include "base/stl_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/values_test_util.h"
#include "base/values.h"
#include "content/browser/attribution_reporting/attribution_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/numeric/int128.h"
#include "third_party/blink/public/common/attribution_reporting/constants.h"

namespace content {
namespace {

TEST(AttributionRegistrationParsingTest, ParseAggregationKeys) {
  const struct {
    const char* description;
    absl::optional<base::Value> json;
    absl::optional<AttributionAggregationKeys> expected;
  } kTestCases[] = {
      {"Null", absl::nullopt, AttributionAggregationKeys()},
      {"Not a dictionary", base::Value(base::Value::List()), absl::nullopt},
      {"key not a string", base::test::ParseJson(R"({"key":123})"),
       absl::nullopt},
      {"Invalid key", base::test::ParseJson(R"({"key":"0xG59"})"),
       absl::nullopt},
      {"One valid key", base::test::ParseJson(R"({"key":"0x159"})"),
       *AttributionAggregationKeys::FromKeys(
           {{"key", absl::MakeUint128(/*high=*/0, /*low=*/345)}})},
      {"Two valid keys",
       base::test::ParseJson(
           R"({"key1":"0x159","key2":"0x50000000000000159"})"),
       *AttributionAggregationKeys::FromKeys({
           {"key1", absl::MakeUint128(/*high=*/0, /*low=*/345)},
           {"key2", absl::MakeUint128(/*high=*/5, /*low=*/345)},
       })},
      {"Second key invalid",
       base::test::ParseJson(R"({"key1":"0x159","key2":""})"), absl::nullopt},
  };

  for (const auto& test_case : kTestCases) {
    EXPECT_EQ(AttributionAggregationKeys::FromJSON(
                  base::OptionalOrNullptr(test_case.json)),
              test_case.expected)
        << test_case.description;
  }
}

TEST(AttributionRegistrationParsingTest, ParseAggregationKeys_CheckSize) {
  struct AttributionAggregatableSourceSizeTestCase {
    const char* description;
    bool valid;
    size_t key_count;
    size_t key_size;

    base::Value::Dict GetHeader() const {
      base::Value::Dict dict;
      for (size_t i = 0u; i < key_count; ++i) {
        dict.Set(GetKey(i), "0x1");
      }
      return dict;
    }

    absl::optional<AttributionAggregationKeys> Expected() const {
      if (!valid)
        return absl::nullopt;

      AttributionAggregationKeys::Keys keys;
      for (size_t i = 0u; i < key_count; ++i) {
        keys.emplace(GetKey(i), absl::MakeUint128(/*high=*/0, /*low=*/1));
      }

      return *AttributionAggregationKeys::FromKeys(std::move(keys));
    }

   private:
    std::string GetKey(size_t index) const {
      // Note that this might not be robust as
      // `blink::kMaxAttributionAggregationKeysPerSourceOrTrigger` varies which
      // might generate invalid JSON.
      return std::string(key_size, 'A' + index % 26 + 32 * (index / 26));
    }
  };

  const AttributionAggregatableSourceSizeTestCase kTestCases[] = {
      {"empty", true, 0, 0},
      {"max_keys", true,
       blink::kMaxAttributionAggregationKeysPerSourceOrTrigger, 1},
      {"too_many_keys", false,
       blink::kMaxAttributionAggregationKeysPerSourceOrTrigger + 1, 1},
      {"max_key_size", true, 1, blink::kMaxBytesPerAttributionAggregationKeyId},
      {"excessive_key_size", false, 1,
       blink::kMaxBytesPerAttributionAggregationKeyId + 1},
  };

  for (const auto& test_case : kTestCases) {
    base::Value value(test_case.GetHeader());
    EXPECT_EQ(AttributionAggregationKeys::FromJSON(&value),
              test_case.Expected())
        << test_case.description;
  }
}

TEST(AttributionRegistrationParsingTest, ParseFilterData) {
  const auto make_filter_data_with_keys = [](size_t n) {
    base::Value::Dict dict;
    for (size_t i = 0; i < n; ++i) {
      dict.Set(base::NumberToString(i), base::Value::List());
    }
    return base::Value(std::move(dict));
  };

  const auto make_filter_data_with_key_length = [](size_t n) {
    base::Value::Dict dict;
    dict.Set(std::string(n, 'a'), base::Value::List());
    return base::Value(std::move(dict));
  };

  const auto make_filter_data_with_values = [](size_t n) {
    base::Value::List list;
    for (size_t i = 0; i < n; ++i) {
      list.Append("x");
    }

    base::Value::Dict dict;
    dict.Set("a", std::move(list));
    return base::Value(std::move(dict));
  };

  const auto make_filter_data_with_value_length = [](size_t n) {
    base::Value::List list;
    list.Append(std::string(n, 'a'));

    base::Value::Dict dict;
    dict.Set("a", std::move(list));
    return base::Value(std::move(dict));
  };

  struct {
    const char* description;
    absl::optional<base::Value> json;
    absl::optional<AttributionFilterData> expected;
  } kTestCases[] = {
      {
          "Null",
          absl::nullopt,
          AttributionFilterData(),
      },
      {
          "empty",
          base::Value(base::Value::Dict()),
          AttributionFilterData(),
      },
      {
          "multiple",
          base::test::ParseJson(R"json({
            "a": ["b"],
            "c": ["e", "d"],
            "f": []
          })json"),
          AttributionFilterData::CreateForTesting({
              {"a", {"b"}},
              {"c", {"e", "d"}},
              {"f", {}},
          }),
      },
      {
          "forbidden_key",
          base::test::ParseJson(R"json({
          "source_type": ["a"]
        })json"),
          absl::nullopt,
      },
      {
          "not_dictionary",
          base::Value(base::Value::List()),
          absl::nullopt,
      },
      {
          "value_not_array",
          base::test::ParseJson(R"json({"a": true})json"),
          absl::nullopt,
      },
      {
          "array_element_not_string",
          base::test::ParseJson(R"json({"a": [true]})json"),
          absl::nullopt,
      },
      {
          "too_many_keys",
          make_filter_data_with_keys(51),
          absl::nullopt,
      },
      {
          "key_too_long",
          make_filter_data_with_key_length(26),
          absl::nullopt,
      },
      {
          "too_many_values",
          make_filter_data_with_values(51),
          absl::nullopt,
      },
      {
          "value_too_long",
          make_filter_data_with_value_length(26),
          absl::nullopt,
      },
  };

  for (auto& test_case : kTestCases) {
    EXPECT_EQ(AttributionFilterData::FromSourceJSON(
                  base::OptionalOrNullptr(test_case.json)),
              test_case.expected)
        << test_case.description;
  }

  {
    base::Value json = make_filter_data_with_keys(50);
    EXPECT_TRUE(AttributionFilterData::FromSourceJSON(&json));
  }

  {
    base::Value json = make_filter_data_with_key_length(25);
    EXPECT_TRUE(AttributionFilterData::FromSourceJSON(&json));
  }

  {
    base::Value json = make_filter_data_with_values(50);
    EXPECT_TRUE(AttributionFilterData::FromSourceJSON(&json));
  }

  {
    base::Value json = make_filter_data_with_value_length(25);
    EXPECT_TRUE(AttributionFilterData::FromSourceJSON(&json));
  }
}

}  // namespace
}  // namespace content
