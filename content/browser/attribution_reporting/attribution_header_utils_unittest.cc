// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_header_utils.h"

#include <limits>
#include <memory>
#include <string>
#include <utility>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/values_test_util.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "base/types/optional_util.h"
#include "base/values.h"
#include "content/browser/attribution_reporting/attribution_aggregation_keys.h"
#include "content/browser/attribution_reporting/attribution_filter_data.h"
#include "content/browser/attribution_reporting/attribution_reporting.mojom.h"
#include "content/browser/attribution_reporting/attribution_source_type.h"
#include "content/browser/attribution_reporting/attribution_test_utils.h"
#include "content/browser/attribution_reporting/common_source_info.h"
#include "content/browser/attribution_reporting/storable_source.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/numeric/int128.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/attribution_reporting/constants.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {
namespace {

using ::attribution_reporting::mojom::SourceRegistrationError;

TEST(AttributionRegistrationParsingTest, ParseAggregationKeys) {
  const struct {
    const char* description;
    absl::optional<base::Value> json;
    base::expected<AttributionAggregationKeys, SourceRegistrationError>
        expected;
  } kTestCases[] = {
      {"Null", absl::nullopt, AttributionAggregationKeys()},
      {"Not a dictionary", base::Value(base::Value::List()),
       base::unexpected(SourceRegistrationError::kAggregationKeysWrongType)},
      {"key not a string", base::test::ParseJson(R"({"key":123})"),
       base::unexpected(
           SourceRegistrationError::kAggregationKeysValueWrongType)},
      {"key doesn't start with 0x", base::test::ParseJson(R"({"key":"159"})"),
       base::unexpected(
           SourceRegistrationError::kAggregationKeysValueWrongFormat)},
      {"Invalid key", base::test::ParseJson(R"({"key":"0xG59"})"),
       base::unexpected(
           SourceRegistrationError::kAggregationKeysValueWrongFormat)},
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
       base::test::ParseJson(R"({"key1":"0x159","key2":""})"),
       base::unexpected(
           SourceRegistrationError::kAggregationKeysValueWrongFormat)},
  };

  for (const auto& test_case : kTestCases) {
    EXPECT_EQ(AttributionAggregationKeys::FromJSON(
                  base::OptionalToPtr(test_case.json)),
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
    EXPECT_EQ(AttributionAggregationKeys::FromJSON(&value).has_value(),
              test_case.Expected().has_value())
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
    base::expected<AttributionFilterData, SourceRegistrationError> expected;
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
          base::unexpected(
              SourceRegistrationError::kFilterDataHasSourceTypeKey),
      },
      {
          "not_dictionary",
          base::Value(base::Value::List()),
          base::unexpected(SourceRegistrationError::kFilterDataWrongType),
      },
      {
          "value_not_array",
          base::test::ParseJson(R"json({"a": true})json"),
          base::unexpected(SourceRegistrationError::kFilterDataListWrongType),
      },
      {
          "array_element_not_string",
          base::test::ParseJson(R"json({"a": [true]})json"),
          base::unexpected(SourceRegistrationError::kFilterDataValueWrongType),
      },
      {
          "too_many_keys",
          make_filter_data_with_keys(51),
          base::unexpected(SourceRegistrationError::kFilterDataTooManyKeys),
      },
      {
          "key_too_long",
          make_filter_data_with_key_length(26),
          base::unexpected(SourceRegistrationError::kFilterDataKeyTooLong),
      },
      {
          "too_many_values",
          make_filter_data_with_values(51),
          base::unexpected(SourceRegistrationError::kFilterDataListTooLong),
      },
      {
          "value_too_long",
          make_filter_data_with_value_length(26),
          base::unexpected(SourceRegistrationError::kFilterDataValueTooLong),
      },
  };

  for (auto& test_case : kTestCases) {
    EXPECT_EQ(AttributionFilterData::FromSourceJSON(
                  base::OptionalToPtr(test_case.json)),
              test_case.expected)
        << test_case.description;
  }

  {
    base::Value json = make_filter_data_with_keys(50);
    EXPECT_TRUE(AttributionFilterData::FromSourceJSON(&json).has_value());
  }

  {
    base::Value json = make_filter_data_with_key_length(25);
    EXPECT_TRUE(AttributionFilterData::FromSourceJSON(&json).has_value());
  }

  {
    base::Value json = make_filter_data_with_values(50);
    EXPECT_TRUE(AttributionFilterData::FromSourceJSON(&json).has_value());
  }

  {
    base::Value json = make_filter_data_with_value_length(25);
    EXPECT_TRUE(AttributionFilterData::FromSourceJSON(&json).has_value());
  }
}

TEST(AttributionRegistrationParsingTest, ParseSourceRegistration) {
  const base::Time source_time = base::Time::Now();
  const auto reporting_origin = url::Origin::Create(GURL("https://r.example"));
  const auto source_origin = url::Origin::Create(GURL("https://s.example"));
  const auto source_type = AttributionSourceType::kNavigation;

  const auto destination_origin =
      url::Origin::Create(GURL("https://d.example"));

  const base::Time default_expiry_time = source_time + base::Days(30);

  const struct {
    const char* desc;
    const char* json;
    base::expected<StorableSource, SourceRegistrationError> expected;
  } kTestCases[] = {
      {
          "required_fields_only",
          R"json({"destination":"https://d.example"})json",
          StorableSource(CommonSourceInfo(
              /*source_event_id=*/0, source_origin, destination_origin,
              reporting_origin, source_time, default_expiry_time, source_type,
              /*priority=*/0, AttributionFilterData(),
              /*debug_key=*/absl::nullopt, AttributionAggregationKeys())),
      },
      {
          "source_event_id_valid",
          R"json({"source_event_id":"1","destination":"https://d.example"})json",
          StorableSource(CommonSourceInfo(
              /*source_event_id=*/1, source_origin, destination_origin,
              reporting_origin, source_time, default_expiry_time, source_type,
              /*priority=*/0, AttributionFilterData(),
              /*debug_key=*/absl::nullopt, AttributionAggregationKeys())),
      },
      {
          "source_event_id_wrong_type",
          R"json({"source_event_id":1,"destination":"https://d.example"})json",
          StorableSource(CommonSourceInfo(
              /*source_event_id=*/0, source_origin, destination_origin,
              reporting_origin, source_time, default_expiry_time, source_type,
              /*priority=*/0, AttributionFilterData(),
              /*debug_key=*/absl::nullopt, AttributionAggregationKeys())),
      },
      {
          "source_event_id_invalid_defaults_to_0",
          R"json({"source_event_id":"-1","destination":"https://d.example"})json",
          StorableSource(CommonSourceInfo(
              /*source_event_id=*/0, source_origin, destination_origin,
              reporting_origin, source_time, default_expiry_time, source_type,
              /*priority=*/0, AttributionFilterData(),
              /*debug_key=*/absl::nullopt, AttributionAggregationKeys())),
      },
      {
          "destination_missing",
          R"json({})json",
          base::unexpected(SourceRegistrationError::kDestinationMissing),
      },
      {
          "destination_wrong_type",
          R"json({"destination":0})json",
          base::unexpected(SourceRegistrationError::kDestinationWrongType),
      },
      {
          "destination_untrustworthy",
          R"json({"destination":"http://d.example"})json",
          base::unexpected(SourceRegistrationError::kDestinationUntrustworthy),
      },
      {
          "priority_valid",
          R"json({"priority":"-5","destination":"https://d.example"})json",
          StorableSource(CommonSourceInfo(
              /*source_event_id=*/0, source_origin, destination_origin,
              reporting_origin, source_time, default_expiry_time, source_type,
              /*priority=*/-5, AttributionFilterData(),
              /*debug_key=*/absl::nullopt, AttributionAggregationKeys())),
      },
      {
          "priority_wrong_type_defaults_to_0",
          R"json({"priority":-5,"destination":"https://d.example"})json",
          StorableSource(CommonSourceInfo(
              /*source_event_id=*/0, source_origin, destination_origin,
              reporting_origin, source_time, default_expiry_time, source_type,
              /*priority=*/0, AttributionFilterData(),
              /*debug_key=*/absl::nullopt, AttributionAggregationKeys())),
      },
      {
          "priority_invalid_defaults_to_0",
          R"json({"priority":"abc","destination":"https://d.example"})json",
          StorableSource(CommonSourceInfo(
              /*source_event_id=*/0, source_origin, destination_origin,
              reporting_origin, source_time, default_expiry_time, source_type,
              /*priority=*/0, AttributionFilterData(),
              /*debug_key=*/absl::nullopt, AttributionAggregationKeys())),
      },
      {
          "expiry_valid",
          R"json({"expiry":"172801","destination":"https://d.example"})json",
          StorableSource(CommonSourceInfo(
              /*source_event_id=*/0, source_origin, destination_origin,
              reporting_origin, source_time,
              /*expiry_time=*/source_time + base::Seconds(172801), source_type,
              /*priority=*/0, AttributionFilterData(),
              /*debug_key=*/absl::nullopt, AttributionAggregationKeys())),
      },
      {
          "expiry_wrong_type",
          R"json({"expiry":172800,"destination":"https://d.example"})json",
          StorableSource(CommonSourceInfo(
              /*source_event_id=*/0, source_origin, destination_origin,
              reporting_origin, source_time, default_expiry_time, source_type,
              /*priority=*/0, AttributionFilterData(),
              /*debug_key=*/absl::nullopt, AttributionAggregationKeys())),
      },
      {
          "expiry_invalid",
          R"json({"expiry":"abc","destination":"https://d.example"})json",
          StorableSource(CommonSourceInfo(
              /*source_event_id=*/0, source_origin, destination_origin,
              reporting_origin, source_time, default_expiry_time, source_type,
              /*priority=*/0, AttributionFilterData(),
              /*debug_key=*/absl::nullopt, AttributionAggregationKeys())),
      },
      {
          "expiry_below_min",
          R"json({"expiry":"86399","destination":"https://d.example"})json",
          StorableSource(CommonSourceInfo(
              /*source_event_id=*/0, source_origin, destination_origin,
              reporting_origin, source_time,
              /*expiry_time=*/source_time + base::Days(1), source_type,
              /*priority=*/0, AttributionFilterData(),
              /*debug_key=*/absl::nullopt, AttributionAggregationKeys())),
      },
      {
          "debug_key_valid",
          R"json({"debug_key":"5","destination":"https://d.example"})json",
          StorableSource(CommonSourceInfo(
              /*source_event_id=*/0, source_origin, destination_origin,
              reporting_origin, source_time, default_expiry_time, source_type,
              /*priority=*/0, AttributionFilterData(),
              /*debug_key=*/5, AttributionAggregationKeys())),
      },
      {
          "debug_key_invalid",
          R"json({"debug_key":"-5","destination":"https://d.example"})json",
          StorableSource(CommonSourceInfo(
              /*source_event_id=*/0, source_origin, destination_origin,
              reporting_origin, source_time, default_expiry_time, source_type,
              /*priority=*/0, AttributionFilterData(),
              /*debug_key=*/absl::nullopt, AttributionAggregationKeys())),
      },
      {
          "debug_key_wrong_type",
          R"json({"debug_key":5,"destination":"https://d.example"})json",
          StorableSource(CommonSourceInfo(
              /*source_event_id=*/0, source_origin, destination_origin,
              reporting_origin, source_time, default_expiry_time, source_type,
              /*priority=*/0, AttributionFilterData(),
              /*debug_key=*/absl::nullopt, AttributionAggregationKeys())),
      },
      {
          "filter_data_valid",
          R"json({"filter_data":{"a":["b"]},"destination":"https://d.example"})json",
          StorableSource(CommonSourceInfo(
              /*source_event_id=*/0, source_origin, destination_origin,
              reporting_origin, source_time, default_expiry_time, source_type,
              /*priority=*/0,
              AttributionFilterData::CreateForTesting({{"a", {"b"}}}),
              /*debug_key=*/absl::nullopt, AttributionAggregationKeys())),
      },
      {
          "filter_data_wrong_type",
          R"json({"filter_data":5,"destination":"https://d.example"})json",
          base::unexpected(SourceRegistrationError::kFilterDataWrongType),
      },
      {
          "aggregation_keys_valid",
          R"json({"aggregation_keys":{"a":"0x1"},"destination":"https://d.example"})json",
          StorableSource(CommonSourceInfo(
              /*source_event_id=*/0, source_origin, destination_origin,
              reporting_origin, source_time, default_expiry_time, source_type,
              /*priority=*/0, AttributionFilterData(),
              /*debug_key=*/absl::nullopt,
              *AttributionAggregationKeys::FromKeys(
                  {{"a", absl::MakeUint128(0, 1)}}))),
      },
      {
          "aggregation_keys_wrong_type",
          R"json({"aggregation_keys":5,"destination":"https://d.example"})json",
          base::unexpected(SourceRegistrationError::kAggregationKeysWrongType),
      },
  };

  for (const auto& test_case : kTestCases) {
    base::Value value = base::test::ParseJson(test_case.json);
    ASSERT_TRUE(value.is_dict()) << test_case.desc;

    EXPECT_EQ(
        test_case.expected,
        ParseSourceRegistration(std::move(*value.GetIfDict()), source_time,
                                reporting_origin, source_origin, source_type))
        << test_case.desc;
  }
}

}  // namespace
}  // namespace content
