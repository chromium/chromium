// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/attribution_response_parsing.h"

#include <limits>
#include <memory>
#include <string>
#include <utility>

#include "base/test/metrics/histogram_tester.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/numeric/int128.h"
#include "third_party/blink/public/common/attribution_reporting/constants.h"
#include "third_party/blink/public/mojom/conversions/attribution_data_host.mojom-blink.h"
#include "third_party/blink/renderer/platform/json/json_parser.h"
#include "third_party/blink/renderer/platform/json/json_values.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "third_party/blink/renderer/platform/wtf/wtf_size_t.h"

namespace blink::attribution_response_parsing {

namespace {

class AttributionFilterDataBuilder {
 public:
  AttributionFilterDataBuilder() = default;
  ~AttributionFilterDataBuilder() = default;

  AttributionFilterDataBuilder& AddFilter(String filter_name,
                                          Vector<String> filter_values) {
    filters_.filter_values.insert(std::move(filter_name),
                                  std::move(filter_values));
    return *this;
  }

  mojom::blink::AttributionFilterDataPtr Build() const {
    return filters_.Clone();
  }

 private:
  mojom::blink::AttributionFilterData filters_;
};

template <typename T>
class VectorBuilder {
 public:
  VectorBuilder() = default;
  ~VectorBuilder() = default;

  VectorBuilder<T>&& Add(T value) && {
    vector_.push_back(std::move(value));
    return std::move(*this);
  }

  Vector<T> Build() && { return std::move(vector_); }

 private:
  Vector<T> vector_;
};

}  // namespace

TEST(AttributionResponseParsingTest, ParseAggregationKeys) {
  const struct {
    String description;
    std::unique_ptr<JSONValue> json;
    bool valid;
    WTF::HashMap<String, absl::uint128> expected;
  } kTestCases[] = {
      {"Null", nullptr, true, {}},
      {"Not a dictionary", std::make_unique<JSONArray>(), false, {}},
      {"key not a string", ParseJSON(R"({"key":123})"), false, {}},
      {"Invalid key", ParseJSON(R"({"key":"0xG59"})"), false, {}},
      {"One valid key",
       ParseJSON(R"({"key":"0x159"})"),
       true,
       {{"key", absl::MakeUint128(/*high=*/0, /*low=*/345)}}},
      {"Two valid keys",
       ParseJSON(R"({"key1":"0x159","key2":"0x50000000000000159"})"),
       true,
       {
           {"key1", absl::MakeUint128(/*high=*/0, /*low=*/345)},
           {"key2", absl::MakeUint128(/*high=*/5, /*low=*/345)},
       }},
      {"Second key invalid",
       ParseJSON(R"({"key1":"0x159","key2":""})"),
       false,
       {}},
  };

  for (const auto& test_case : kTestCases) {
    WTF::HashMap<String, absl::uint128> actual;
    bool valid = ParseAggregationKeys(test_case.json.get(), actual);
    EXPECT_EQ(test_case.valid, valid) << test_case.description;
    if (test_case.valid)
      EXPECT_EQ(test_case.expected, actual) << test_case.description;
  }
}

TEST(AttributionResponseParsingTest, ParseAggregationKeys_CheckSize) {
  struct AttributionAggregatableSourceSizeTestCase {
    String description;
    bool valid;
    wtf_size_t key_count;
    wtf_size_t key_size;

    std::unique_ptr<JSONValue> GetHeader() const {
      auto object = std::make_unique<JSONObject>();
      for (wtf_size_t i = 0u; i < key_count; ++i) {
        object->SetString(GetKey(i), "0x1");
      }
      return object;
    }

    WTF::HashMap<String, absl::uint128> GetAggregationKeys() const {
      WTF::HashMap<String, absl::uint128> aggregation_keys;
      if (!valid)
        return aggregation_keys;

      for (wtf_size_t i = 0u; i < key_count; ++i) {
        aggregation_keys.insert(GetKey(i),
                                absl::MakeUint128(/*high=*/0, /*low=*/1));
      }

      return aggregation_keys;
    }

   private:
    String GetKey(wtf_size_t index) const {
      // Note that this might not be robust as
      // `blink::kMaxAttributionAggregationKeysPerSourceOrTrigger` varies which
      // might generate invalid JSON.
      return String(
          std::string(key_size, 'A' + index % 26 + 32 * (index / 26)));
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
    std::unique_ptr<JSONValue> json = test_case.GetHeader();
    WTF::HashMap<String, absl::uint128> actual;
    bool valid = ParseAggregationKeys(json.get(), actual);
    EXPECT_EQ(test_case.valid, valid) << test_case.description;
    if (test_case.valid) {
      EXPECT_EQ(test_case.GetAggregationKeys(), actual)
          << test_case.description;
    }
  }
}

TEST(AttributionResponseParsingTest, ParseAttributionAggregatableTrigger) {
  const struct {
    String description;
    std::unique_ptr<JSONValue> json;
    bool valid;
    Vector<mojom::blink::AttributionAggregatableTriggerDataPtr> expected;
  } kTestCases[] = {
      {"Null", nullptr, true, {}},
      {"Not an array", ParseJSON(R"({})"), false, {}},
      {"Element not a dictionary", ParseJSON(R"([123])"), false, {}},
      {"Missing source_keys field",
       ParseJSON(R"([{"key_piece":"0x400"}])"),
       false,
       {}},
      {"source_keys not an array",
       ParseJSON(R"([{"key_piece":"0x400","source_keys":"key"}])"),
       false,
       {}},
      {"source_keys element not a string",
       ParseJSON(R"([{"key_piece":"0x400","source_keys":[123]}])")},
      {"Missing key_piece field",
       ParseJSON(R"([{"source_keys":["key"]}])"),
       false,
       {}},
      {"Invalid key",
       ParseJSON(R"([{"key_piece":"0xG00","source_keys":["key"]}])"),
       false,
       {}},
      {"Valid trigger",
       ParseJSON(R"([{"key_piece":"0x400","source_keys":["key"]}])"), true,
       VectorBuilder<mojom::blink::AttributionAggregatableTriggerDataPtr>()
           .Add(mojom::blink::AttributionAggregatableTriggerData::New(
               absl::MakeUint128(/*high=*/0, /*low=*/1024),
               /*source_keys=*/Vector<String>{"key"},
               /*filters=*/mojom::blink::AttributionFilterData::New(),
               /*not_filters=*/mojom::blink::AttributionFilterData::New()))
           .Build()},
      {"Valid trigger with filters", ParseJSON(R"([{
         "key_piece": "0x400",
         "source_keys": ["key"],
         "filters": {"filter": ["value1"]},
         "not_filters": {"filter": ["value2"]}
       }])"),
       true,
       VectorBuilder<mojom::blink::AttributionAggregatableTriggerDataPtr>()
           .Add(mojom::blink::AttributionAggregatableTriggerData::New(
               absl::MakeUint128(/*high=*/0, /*low=*/1024),
               /*source_keys=*/Vector<String>{"key"},
               /*filters=*/
               AttributionFilterDataBuilder()
                   .AddFilter("filter", Vector<String>{"value1"})
                   .Build(),
               /*not_filters=*/
               AttributionFilterDataBuilder()
                   .AddFilter("filter", Vector<String>{"value2"})
                   .Build()))
           .Build()},
      {"Two valid trigger data",
       ParseJSON(R"([{"key_piece":"0x400","source_keys":["key1"]},
           {"key_piece":"0xA80","source_keys":["key2"]}])"),
       true,
       VectorBuilder<mojom::blink::AttributionAggregatableTriggerDataPtr>()
           .Add(mojom::blink::AttributionAggregatableTriggerData::New(
               absl::MakeUint128(/*high=*/0, /*low=*/1024),
               /*source_keys=*/Vector<String>{"key1"},
               /*filters=*/mojom::blink::AttributionFilterData::New(),
               /*not_filters=*/mojom::blink::AttributionFilterData::New()))
           .Add(mojom::blink::AttributionAggregatableTriggerData::New(
               absl::MakeUint128(/*high=*/0, /*low=*/2688),
               /*source_keys=*/Vector<String>{"key2"},
               /*filters=*/mojom::blink::AttributionFilterData::New(),
               /*not_filters=*/mojom::blink::AttributionFilterData::New()))
           .Build()},
  };

  for (const auto& test_case : kTestCases) {
    WTF::Vector<mojom::blink::AttributionAggregatableTriggerDataPtr>
        trigger_data;
    bool valid = ParseAttributionAggregatableTriggerData(test_case.json.get(),
                                                         trigger_data);
    EXPECT_EQ(test_case.valid, valid) << test_case.description;
    if (test_case.valid) {
      EXPECT_EQ(test_case.expected, trigger_data) << test_case.description;
    }
  }
}

TEST(AttributionResponseParsingTest,
     ParseAttributionAggregatableTrigger_CheckSize) {
  struct AttributionAggregatableTriggerSizeTestCase {
    String description;
    bool valid;
    wtf_size_t trigger_data_count;
    wtf_size_t key_count;
    wtf_size_t key_size;

    std::unique_ptr<JSONArray> GetHeader() const {
      String key = GetKey();

      auto array = std::make_unique<JSONArray>();
      for (wtf_size_t i = 0u; i < trigger_data_count; ++i) {
        auto object = std::make_unique<JSONObject>();
        object->SetString("key_piece", "0x1");

        auto keys = std::make_unique<JSONArray>();
        for (wtf_size_t j = 0u; j < key_count; ++j) {
          keys->PushString(key);
        }
        object->SetArray("source_keys", std::move(keys));

        array->PushObject(std::move(object));
      }

      return array;
    }

    WTF::Vector<mojom::blink::AttributionAggregatableTriggerDataPtr>
    GetTriggerData() const {
      if (!valid)
        return {};

      WTF::Vector<mojom::blink::AttributionAggregatableTriggerDataPtr> data;
      for (wtf_size_t i = 0u; i < trigger_data_count; ++i) {
        data.push_back(mojom::blink::AttributionAggregatableTriggerData::New(
            absl::MakeUint128(/*high=*/0, /*low=*/1),
            /*source_keys=*/Vector<String>(key_count, GetKey()),
            /*filters=*/mojom::blink::AttributionFilterData::New(),
            /*not_filters=*/mojom::blink::AttributionFilterData::New()));
      }
      return data;
    }

   private:
    String GetKey() const { return String(std::string(key_size, 'A')); }
  };

  const AttributionAggregatableTriggerSizeTestCase kTestCases[] = {
      {"empty", true, 0, 0, 0},
      {"max_trigger_data", true,
       blink::kMaxAttributionAggregatableTriggerDataPerTrigger, 0, 0},
      {"too_many_trigger_data", false,
       blink::kMaxAttributionAggregatableTriggerDataPerTrigger + 1, 0, 0},
      {"max_key_count", true, 1,
       blink::kMaxAttributionAggregationKeysPerSourceOrTrigger, 0},
      {"too many keys", false, 1,
       blink::kMaxAttributionAggregationKeysPerSourceOrTrigger + 1, 0},
      {"max_key_size", true, 1, 1, kMaxBytesPerAttributionAggregationKeyId},
      {"excessive_key_size", false, 1, 1,
       kMaxBytesPerAttributionAggregationKeyId + 1},
  };

  for (const auto& test_case : kTestCases) {
    std::unique_ptr<JSONArray> json = test_case.GetHeader();
    WTF::Vector<mojom::blink::AttributionAggregatableTriggerDataPtr>
        trigger_data;
    bool valid =
        ParseAttributionAggregatableTriggerData(json.get(), trigger_data);

    EXPECT_EQ(test_case.valid, valid) << test_case.description;
    if (test_case.valid) {
      EXPECT_EQ(test_case.GetTriggerData(), trigger_data)
          << test_case.description;
    }
  }
}

TEST(AttributionResponseParsingTest, ParseAttributionAggregatableValues) {
  const struct {
    String description;
    std::unique_ptr<JSONValue> json;
    bool valid;
    WTF::HashMap<String, uint32_t> values;
  } kTestCases[] = {
      {"Null", nullptr, true, {}},
      {"Value not an integer", ParseJSON(R"({"key":"1"})"), false, {}},
      {"Invalid value", ParseJSON(R"({"key":-1})"), false, {}},
      {"Valid value", ParseJSON(R"({"key":123})"), true, {{"key", 123}}},
      {"Two valid values",
       ParseJSON(R"({"key1":123,"key2":456})"),
       true,
       {{"key1", 123}, {"key2", 456}}},
      {"Max valid value",
       ParseJSON(R"({"key":65536})"),
       true,
       {{"key", 65536}}},
      {"Value out of range", ParseJSON(R"({"key":65537})"), false, {}},
  };

  for (const auto& test_case : kTestCases) {
    WTF::HashMap<String, uint32_t> values;
    bool valid =
        ParseAttributionAggregatableValues(test_case.json.get(), values);
    EXPECT_EQ(test_case.valid, valid) << test_case.description;
    if (test_case.valid)
      EXPECT_EQ(test_case.values, values) << test_case.description;
  }
}

TEST(AttributionResponseParsingTest,
     ParseAttributionAggregatableValues_CheckSize) {
  struct AttributionAggregatableValuesSizeTestCase {
    String description;
    bool valid;
    wtf_size_t key_count;
    wtf_size_t key_size;

    std::unique_ptr<JSONValue> GetHeader() const {
      auto object = std::make_unique<JSONObject>();
      for (wtf_size_t i = 0u; i < key_count; ++i) {
        object->SetInteger(GetKey(i), i + 1);
      }
      return object;
    }

    WTF::HashMap<String, uint32_t> GetValues() const {
      if (!valid)
        return {};

      WTF::HashMap<String, uint32_t> values;
      for (wtf_size_t i = 0u; i < key_count; ++i) {
        values.insert(GetKey(i), i + 1);
      }
      return values;
    }

   private:
    String GetKey(wtf_size_t index) const {
      // Note that this might not be robust as
      // `blink::kMaxAttributionAggregationKeysPerSourceOrTrigger` varies which
      // might generate invalid JSON characters.
      return String(
          std::string(key_size, 'A' + index % 26 + 32 * (index / 26)));
    }
  };

  const AttributionAggregatableValuesSizeTestCase kTestCases[] = {
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
    std::unique_ptr<JSONValue> json = test_case.GetHeader();
    WTF::HashMap<String, uint32_t> values;
    bool valid = ParseAttributionAggregatableValues(json.get(), values);

    EXPECT_EQ(test_case.valid, valid) << test_case.description;
    if (test_case.valid)
      EXPECT_EQ(test_case.GetValues(), values) << test_case.description;
  }
}

TEST(AttributionResponseParsingTest, ParseFilterData) {
  const auto make_filter_data_with_keys = [](wtf_size_t n) {
    auto root = std::make_unique<JSONObject>();
    for (wtf_size_t i = 0; i < n; ++i) {
      root->SetArray(String::Number(i), std::make_unique<JSONArray>());
    }
    return root;
  };

  const auto make_filter_data_with_key_length = [](wtf_size_t n) {
    auto root = std::make_unique<JSONObject>();
    root->SetArray(String(std::string(n, 'a')), std::make_unique<JSONArray>());
    return root;
  };

  const auto make_filter_data_with_values = [](wtf_size_t n) {
    auto array = std::make_unique<JSONArray>();
    for (wtf_size_t i = 0; i < n; ++i) {
      array->PushString("x");
    }

    auto root = std::make_unique<JSONObject>();
    root->SetArray("a", std::move(array));
    return root;
  };

  const auto make_filter_data_with_value_length = [](wtf_size_t n) {
    auto array = std::make_unique<JSONArray>();
    array->PushString(String(std::string(n, 'a')));

    auto root = std::make_unique<JSONObject>();
    root->SetArray("a", std::move(array));
    return root;
  };

  const struct {
    String description;
    std::unique_ptr<JSONValue> json;
    mojom::blink::AttributionFilterDataPtr expected;
  } kTestCases[] = {
      {
          "Null",
          nullptr,
          AttributionFilterDataBuilder().Build(),
      },
      {
          "empty",
          ParseJSON(R"json({})json"),
          AttributionFilterDataBuilder().Build(),
      },
      {
          "source_type",
          ParseJSON(R"json({"source_type": []})json"),
          AttributionFilterDataBuilder().AddFilter("source_type", {}).Build(),
      },
      {
          "multiple",
          ParseJSON(R"json({
            "a": ["b"],
            "c": ["e", "d"]
          })json"),
          AttributionFilterDataBuilder()
              .AddFilter("a", {"b"})
              .AddFilter("c", {"e", "d"})
              .Build(),
      },
      {
          "not_dictionary",
          ParseJSON(R"json(true)json"),
          nullptr,
      },
      {
          "value_not_array",
          ParseJSON(R"json({"a": true})json"),
          nullptr,
      },
      {
          "array_element_not_string",
          ParseJSON(R"json({"a": [true]})json"),
          nullptr,
      },
      {
          "too_many_keys",
          make_filter_data_with_keys(51),
          nullptr,
      },
      {
          "key_too_long",
          make_filter_data_with_key_length(26),
          nullptr,
      },
      {
          "too_many_values",
          make_filter_data_with_values(51),
          nullptr,
      },
      {
          "value_too_long",
          make_filter_data_with_value_length(26),
          nullptr,
      },
  };

  for (const auto& test_case : kTestCases) {
    mojom::blink::AttributionFilterData filter_data;

    bool valid = ParseAttributionFilterData(test_case.json.get(), filter_data);
    EXPECT_EQ(valid, !test_case.expected.is_null()) << test_case.description;

    if (test_case.expected) {
      EXPECT_EQ(*test_case.expected, filter_data) << test_case.description;
    }
  }

  {
    std::unique_ptr<JSONValue> json = make_filter_data_with_keys(50);
    mojom::blink::AttributionFilterData filter_data;
    EXPECT_TRUE(ParseAttributionFilterData(json.get(), filter_data));
  }

  {
    std::unique_ptr<JSONValue> json = make_filter_data_with_key_length(25);
    mojom::blink::AttributionFilterData filter_data;
    EXPECT_TRUE(ParseAttributionFilterData(json.get(), filter_data));
  }

  {
    std::unique_ptr<JSONValue> json = make_filter_data_with_values(50);
    mojom::blink::AttributionFilterData filter_data;
    EXPECT_TRUE(ParseAttributionFilterData(json.get(), filter_data));
  }

  {
    std::unique_ptr<JSONValue> json = make_filter_data_with_value_length(25);
    mojom::blink::AttributionFilterData filter_data;
    EXPECT_TRUE(ParseAttributionFilterData(json.get(), filter_data));
  }
}

TEST(AttributionResponseParsingTest, ParseAggregatableDedupKey) {
  const auto reporting_origin =
      SecurityOrigin::CreateFromString("https://r.test");

  const struct {
    String description;
    String json;
    mojom::blink::AttributionTriggerDataPtr expected;
  } kTestCases[] = {
      {"no_aggregatable_dedup_key", R"json({})json",
       mojom::blink::AttributionTriggerData::New(
           reporting_origin, WTF::Vector<mojom::blink::EventTriggerDataPtr>(),
           /*filters=*/AttributionFilterDataBuilder().Build(),
           /*not_filters=*/AttributionFilterDataBuilder().Build(),
           WTF::Vector<mojom::blink::AttributionAggregatableTriggerDataPtr>(),
           WTF::HashMap<String, uint32_t>(),
           /*debug_key=*/nullptr,
           /*aggregatable_dedup_key=*/nullptr)},
      {"valid_aggregatable_dedup_key",
       R"json({
        "aggregatable_deduplication_key": "3"
      })json",
       mojom::blink::AttributionTriggerData::New(
           reporting_origin, WTF::Vector<mojom::blink::EventTriggerDataPtr>(),
           /*filters=*/AttributionFilterDataBuilder().Build(),
           /*not_filters=*/AttributionFilterDataBuilder().Build(),
           WTF::Vector<mojom::blink::AttributionAggregatableTriggerDataPtr>(),
           WTF::HashMap<String, uint32_t>(),
           /*debug_key=*/nullptr,
           /*aggregatable_dedup_key=*/
           mojom::blink::AttributionTriggerDedupKey::New(3))},
      {"aggregatable_dedup_key_not_string",
       R"json({
        "aggregatable_deduplication_key": 3
      })json",
       mojom::blink::AttributionTriggerData::New(
           reporting_origin, WTF::Vector<mojom::blink::EventTriggerDataPtr>(),
           /*filters=*/AttributionFilterDataBuilder().Build(),
           /*not_filters=*/AttributionFilterDataBuilder().Build(),
           WTF::Vector<mojom::blink::AttributionAggregatableTriggerDataPtr>(),
           WTF::HashMap<String, uint32_t>(),
           /*debug_key=*/nullptr,
           /*aggregatable_dedup_key=*/nullptr)},
      {"invalid_aggregatable_dedup_key",
       R"json({
        "aggregatable_deduplication_key": "abc"
      })json",
       mojom::blink::AttributionTriggerData::New(
           reporting_origin, WTF::Vector<mojom::blink::EventTriggerDataPtr>(),
           /*filters=*/AttributionFilterDataBuilder().Build(),
           /*not_filters=*/AttributionFilterDataBuilder().Build(),
           WTF::Vector<mojom::blink::AttributionAggregatableTriggerDataPtr>(),
           WTF::HashMap<String, uint32_t>(),
           /*debug_key=*/nullptr,
           /*aggregatable_dedup_key=*/nullptr)},
  };

  for (const auto& test_case : kTestCases) {
    mojom::blink::AttributionTriggerData trigger_data;
    // This field is not populated by `ParseTriggerRegistrationHeader()`, so
    // just set it to an arbitrary origin so we can ensure it isn't changed.
    trigger_data.reporting_origin = reporting_origin;

    bool valid = ParseTriggerRegistrationHeader(test_case.json, trigger_data);
    EXPECT_EQ(valid, !test_case.expected.is_null()) << test_case.description;

    if (test_case.expected) {
      EXPECT_EQ(test_case.expected->reporting_origin->ToUrlOrigin(),
                trigger_data.reporting_origin->ToUrlOrigin())
          << test_case.description;

      EXPECT_EQ(test_case.expected->aggregatable_dedup_key,
                trigger_data.aggregatable_dedup_key)
          << test_case.description;
    }
  }
}

TEST(AttributionResponseParsingTest, ParseSourceRegistrationHeader) {
  const auto reporting_origin =
      SecurityOrigin::CreateFromString("https://r.test");

  const struct {
    String description;
    String json;
    mojom::blink::AttributionSourceDataPtr expected;
  } kTestCases[] = {
      {
          "invalid_json",
          "!",
          nullptr,
      },
      {
          "root_not_object",
          R"json([])json",
          nullptr,
      },
      {
          "required_fields_only",
          R"json({
            "destination": "https://d.test"
          })json",
          mojom::blink::AttributionSourceData::New(
              /*destination=*/SecurityOrigin::CreateFromString(
                  "https://d.test"),
              /*reporting_origin=*/reporting_origin,
              /*source_event_id=*/0,
              /*expiry=*/absl::nullopt,
              /*priority=*/0,
              /*debug_key=*/nullptr,
              /*filter_data=*/AttributionFilterDataBuilder().Build(),
              /*aggregation_keys=*/WTF::HashMap<String, absl::uint128>()),
      },
      {
          "missing_destination",
          R"json({
            "source_event_id": "1"
          })json",
          nullptr,
      },
      {
          "source_event_id_not_string",
          R"json({
            "source_event_id": 1,
            "destination": "https://d.test"
          })json",
          mojom::blink::AttributionSourceData::New(
              /*destination=*/SecurityOrigin::CreateFromString(
                  "https://d.test"),
              /*reporting_origin=*/reporting_origin,
              /*source_event_id=*/0,
              /*expiry=*/absl::nullopt,
              /*priority=*/0,
              /*debug_key=*/nullptr,
              /*filter_data=*/AttributionFilterDataBuilder().Build(),
              /*aggregation_keys=*/WTF::HashMap<String, absl::uint128>()),
      },
      {
          "invalid_source_event_id",
          R"json({
            "source_event_id": "-5",
            "destination": "https://d.test"
          })json",
          mojom::blink::AttributionSourceData::New(
              /*destination=*/SecurityOrigin::CreateFromString(
                  "https://d.test"),
              /*reporting_origin=*/reporting_origin,
              /*source_event_id=*/0,
              /*expiry=*/absl::nullopt,
              /*priority=*/0,
              /*debug_key=*/nullptr,
              /*filter_data=*/AttributionFilterDataBuilder().Build(),
              /*aggregation_keys=*/WTF::HashMap<String, absl::uint128>()),
      },
      {
          "valid_source_event_id",
          R"json({
            "source_event_id": "1",
            "destination": "https://d.test"
          })json",
          mojom::blink::AttributionSourceData::New(
              /*destination=*/SecurityOrigin::CreateFromString(
                  "https://d.test"),
              /*reporting_origin=*/reporting_origin,
              /*source_event_id=*/1,
              /*expiry=*/absl::nullopt,
              /*priority=*/0,
              /*debug_key=*/nullptr,
              /*filter_data=*/AttributionFilterDataBuilder().Build(),
              /*aggregation_keys=*/WTF::HashMap<String, absl::uint128>()),
      },
      {
          "destination_not_string",
          R"json({
            "destination": 4
          })json",
          nullptr,
      },
      {
          "destination_not_potentially_trustworthy",
          R"json({
            "destination": "http://d.test"
          })json",
          nullptr,
      },
      {
          "valid_priority",
          R"json({
            "destination": "https://d.test",
            "priority": "5"
          })json",
          mojom::blink::AttributionSourceData::New(
              /*destination=*/SecurityOrigin::CreateFromString(
                  "https://d.test"),
              /*reporting_origin=*/reporting_origin,
              /*source_event_id=*/0,
              /*expiry=*/absl::nullopt,
              /*priority=*/5,
              /*debug_key=*/nullptr,
              /*filter_data=*/AttributionFilterDataBuilder().Build(),
              /*aggregation_keys=*/WTF::HashMap<String, absl::uint128>()),
      },
      {
          "priority_not_string",
          R"json({
            "destination": "https://d.test",
            "priority": 5
          })json",
          mojom::blink::AttributionSourceData::New(
              /*destination=*/SecurityOrigin::CreateFromString(
                  "https://d.test"),
              /*reporting_origin=*/reporting_origin,
              /*source_event_id=*/0,
              /*expiry=*/absl::nullopt,
              /*priority=*/0,
              /*debug_key=*/nullptr,
              /*filter_data=*/AttributionFilterDataBuilder().Build(),
              /*aggregation_keys=*/WTF::HashMap<String, absl::uint128>()),
      },
      {
          "invalid_priority",
          R"json({
            "destination": "https://d.test",
            "priority": "abc"
          })json",
          mojom::blink::AttributionSourceData::New(
              /*destination=*/SecurityOrigin::CreateFromString(
                  "https://d.test"),
              /*reporting_origin=*/reporting_origin,
              /*source_event_id=*/0,
              /*expiry=*/absl::nullopt,
              /*priority=*/0,
              /*debug_key=*/nullptr,
              /*filter_data=*/AttributionFilterDataBuilder().Build(),
              /*aggregation_keys=*/WTF::HashMap<String, absl::uint128>()),
      },
      {
          "valid_expiry",
          R"json({
            "destination": "https://d.test",
            "expiry": "5"
          })json",
          mojom::blink::AttributionSourceData::New(
              /*destination=*/SecurityOrigin::CreateFromString(
                  "https://d.test"),
              /*reporting_origin=*/reporting_origin,
              /*source_event_id=*/0,
              /*expiry=*/base::Seconds(5),
              /*priority=*/0,
              /*debug_key=*/nullptr,
              /*filter_data=*/AttributionFilterDataBuilder().Build(),
              /*aggregation_keys=*/WTF::HashMap<String, absl::uint128>()),
      },
      {
          "expiry_not_string",
          R"json({
            "destination": "https://d.test",
            "expiry": 5
          })json",
          mojom::blink::AttributionSourceData::New(
              /*destination=*/SecurityOrigin::CreateFromString(
                  "https://d.test"),
              /*reporting_origin=*/reporting_origin,
              /*source_event_id=*/0,
              /*expiry=*/absl::nullopt,
              /*priority=*/0,
              /*debug_key=*/nullptr,
              /*filter_data=*/AttributionFilterDataBuilder().Build(),
              /*aggregation_keys=*/WTF::HashMap<String, absl::uint128>()),
      },
      {
          "invalid_expiry",
          R"json({
            "destination": "https://d.test",
            "expiry": "abc"
          })json",
          mojom::blink::AttributionSourceData::New(
              /*destination=*/SecurityOrigin::CreateFromString(
                  "https://d.test"),
              /*reporting_origin=*/reporting_origin,
              /*source_event_id=*/0,
              /*expiry=*/absl::nullopt,
              /*priority=*/0,
              /*debug_key=*/nullptr,
              /*filter_data=*/AttributionFilterDataBuilder().Build(),
              /*aggregation_keys=*/WTF::HashMap<String, absl::uint128>()),
      },
      {
          "valid_debug_key",
          R"json({
            "destination": "https://d.test",
            "debug_key": "5"
          })json",
          mojom::blink::AttributionSourceData::New(
              /*destination=*/SecurityOrigin::CreateFromString(
                  "https://d.test"),
              /*reporting_origin=*/reporting_origin,
              /*source_event_id=*/0,
              /*expiry=*/absl::nullopt,
              /*priority=*/0,
              /*debug_key=*/mojom::blink::AttributionDebugKey::New(5),
              /*filter_data=*/AttributionFilterDataBuilder().Build(),
              /*aggregation_keys=*/WTF::HashMap<String, absl::uint128>()),
      },
      {
          "valid_filter_data",
          R"json({
            "destination": "https://d.test",
            "filter_data": {"SOURCE_TYPE": []}
          })json",
          mojom::blink::AttributionSourceData::New(
              /*destination=*/SecurityOrigin::CreateFromString(
                  "https://d.test"),
              /*reporting_origin=*/reporting_origin,
              /*source_event_id=*/0,
              /*expiry=*/absl::nullopt,
              /*priority=*/0,
              /*debug_key=*/nullptr,
              /*filter_data=*/
              AttributionFilterDataBuilder()
                  .AddFilter("SOURCE_TYPE", {})
                  .Build(),
              /*aggregation_keys=*/WTF::HashMap<String, absl::uint128>()),
      },
      {
          "invalid_source_type_key_in_filter_data",
          R"json({
            "destination": "https://d.test",
            "filter_data": {"source_type": []}
          })json",
          nullptr,
      },
      {
          "unknown_field",
          R"json({
            "destination": "https://d.test",
            "a": {"b": {"c": {"d": "e"}}}
          })json",
          mojom::blink::AttributionSourceData::New(
              /*destination=*/SecurityOrigin::CreateFromString(
                  "https://d.test"),
              /*reporting_origin=*/reporting_origin,
              /*source_event_id=*/0,
              /*expiry=*/absl::nullopt,
              /*priority=*/0,
              /*debug_key=*/nullptr,
              /*filter_data=*/AttributionFilterDataBuilder().Build(),
              /*aggregation_keys=*/WTF::HashMap<String, absl::uint128>()),
      },
  };

  for (const auto& test_case : kTestCases) {
    mojom::blink::AttributionSourceData source_data;
    // This field is not populated by `ParseSourceRegistrationHeader()`, so just
    // set it to an arbitrary origin so we can ensure it isn't changed.
    source_data.reporting_origin = reporting_origin;

    bool valid = ParseSourceRegistrationHeader(test_case.json, source_data);
    EXPECT_EQ(valid, !test_case.expected.is_null()) << test_case.description;

    if (test_case.expected) {
      EXPECT_EQ(test_case.expected->destination->ToUrlOrigin(),
                source_data.destination->ToUrlOrigin())
          << test_case.description;

      EXPECT_EQ(test_case.expected->reporting_origin->ToUrlOrigin(),
                source_data.reporting_origin->ToUrlOrigin())
          << test_case.description;

      EXPECT_EQ(test_case.expected->source_event_id,
                source_data.source_event_id)
          << test_case.description;

      EXPECT_EQ(test_case.expected->expiry, source_data.expiry)
          << test_case.description;

      EXPECT_EQ(test_case.expected->priority, source_data.priority)
          << test_case.description;

      EXPECT_EQ(test_case.expected->debug_key, source_data.debug_key)
          << test_case.description;

      EXPECT_EQ(test_case.expected->filter_data, source_data.filter_data)
          << test_case.description;

      // This field is not populated by `ParseSourceRegistrationHeader()`, but
      // check it for equality with the test case anyway.
      EXPECT_EQ(test_case.expected->aggregation_keys,
                source_data.aggregation_keys)
          << test_case.description;
    }
  }
}

TEST(AttributionResponseParsingTest, ParseDebugKey) {
  EXPECT_FALSE(ParseDebugKey(String()));  // null string
  EXPECT_FALSE(ParseDebugKey(""));
  EXPECT_FALSE(ParseDebugKey("-1"));
  EXPECT_FALSE(ParseDebugKey("0x5"));

  EXPECT_EQ(ParseDebugKey("123"), mojom::blink::AttributionDebugKey::New(123));
  EXPECT_EQ(ParseDebugKey("18446744073709551615"),
            mojom::blink::AttributionDebugKey::New(
                std::numeric_limits<uint64_t>::max()));
}

TEST(AttributionResponseParsingTest, ParseEventTriggerData) {
  const struct {
    String description;
    std::unique_ptr<JSONValue> json;
    bool valid;
    Vector<mojom::blink::EventTriggerDataPtr> expected;
  } kTestCases[] = {
      {
          "Null",
          nullptr,
          true,
          {},
      },
      {
          "root_not_array",
          ParseJSON(R"json({})json"),
          false,
          {},
      },
      {
          "empty",
          ParseJSON(R"json([])json"),
          true,
          {},
      },
      {
          "too_many_values",
          ParseJSON(R"json([{},{},{},{},{},{},{},{},{},{},{}])json"),
          false,
          {},
      },
      {
          "value_not_object",
          ParseJSON(R"json([123])json"),
          false,
          {},
      },
      {
          "missing_trigger_data",
          ParseJSON(R"json([{}])json"),
          true,
          VectorBuilder<mojom::blink::EventTriggerDataPtr>()
              .Add(mojom::blink::EventTriggerData::New(
                  /*data=*/0,
                  /*priority=*/0,
                  /*dedup_key=*/nullptr,
                  /*filters=*/AttributionFilterDataBuilder().Build(),
                  /*not_filters=*/AttributionFilterDataBuilder().Build()))
              .Build(),
      },
      {
          "trigger_data_not_string",
          ParseJSON(R"json([{"trigger_data": 1}])json"),
          true,
          VectorBuilder<mojom::blink::EventTriggerDataPtr>()
              .Add(mojom::blink::EventTriggerData::New(
                  /*data=*/0,
                  /*priority=*/0,
                  /*dedup_key=*/nullptr,
                  /*filters=*/AttributionFilterDataBuilder().Build(),
                  /*not_filters=*/AttributionFilterDataBuilder().Build()))
              .Build(),
      },
      {
          "invalid_trigger_data",
          ParseJSON(R"json([{"trigger_data": "-5"}])json"),
          true,
          VectorBuilder<mojom::blink::EventTriggerDataPtr>()
              .Add(mojom::blink::EventTriggerData::New(
                  /*data=*/0,
                  /*priority=*/0,
                  /*dedup_key=*/nullptr,
                  /*filters=*/AttributionFilterDataBuilder().Build(),
                  /*not_filters=*/AttributionFilterDataBuilder().Build()))
              .Build(),
      },
      {
          "valid_trigger_data",
          ParseJSON(R"json([{"trigger_data": "5"}])json"),
          true,
          VectorBuilder<mojom::blink::EventTriggerDataPtr>()
              .Add(mojom::blink::EventTriggerData::New(
                  /*data=*/5,
                  /*priority=*/0,
                  /*dedup_key=*/nullptr,
                  /*filters=*/AttributionFilterDataBuilder().Build(),
                  /*not_filters=*/AttributionFilterDataBuilder().Build()))
              .Build(),
      },
      {
          "multiple",
          ParseJSON(R"json([
            {"trigger_data": "5"},
            {"trigger_data": "3"},
            {"trigger_data": "4"}
          ])json"),
          true,
          VectorBuilder<mojom::blink::EventTriggerDataPtr>()
              .Add(mojom::blink::EventTriggerData::New(
                  /*data=*/5,
                  /*priority=*/0,
                  /*dedup_key=*/nullptr,
                  /*filters=*/AttributionFilterDataBuilder().Build(),
                  /*not_filters=*/AttributionFilterDataBuilder().Build()))
              .Add(mojom::blink::EventTriggerData::New(
                  /*data=*/3,
                  /*priority=*/0,
                  /*dedup_key=*/nullptr,
                  /*filters=*/AttributionFilterDataBuilder().Build(),
                  /*not_filters=*/AttributionFilterDataBuilder().Build()))
              .Add(mojom::blink::EventTriggerData::New(
                  /*data=*/4,
                  /*priority=*/0,
                  /*dedup_key=*/nullptr,
                  /*filters=*/AttributionFilterDataBuilder().Build(),
                  /*not_filters=*/AttributionFilterDataBuilder().Build()))
              .Build(),
      },
      {
          "valid_priority",
          ParseJSON(R"json([{
            "trigger_data": "5",
            "priority": "3"
          }])json"),
          true,
          VectorBuilder<mojom::blink::EventTriggerDataPtr>()
              .Add(mojom::blink::EventTriggerData::New(
                  /*data=*/5,
                  /*priority=*/3,
                  /*dedup_key=*/nullptr,
                  /*filters=*/AttributionFilterDataBuilder().Build(),
                  /*not_filters=*/AttributionFilterDataBuilder().Build()))
              .Build(),
      },
      {
          "priority_not_string",
          ParseJSON(R"json([{
            "trigger_data": "5",
            "priority": 3
          }])json"),
          true,
          VectorBuilder<mojom::blink::EventTriggerDataPtr>()
              .Add(mojom::blink::EventTriggerData::New(
                  /*data=*/5,
                  /*priority=*/0,
                  /*dedup_key=*/nullptr,
                  /*filters=*/AttributionFilterDataBuilder().Build(),
                  /*not_filters=*/AttributionFilterDataBuilder().Build()))
              .Build(),
      },
      {
          "invalid_priority",
          ParseJSON(R"json([{
            "trigger_data": "5",
            "priority": "abc"
          }])json"),
          true,
          VectorBuilder<mojom::blink::EventTriggerDataPtr>()
              .Add(mojom::blink::EventTriggerData::New(
                  /*data=*/5,
                  /*priority=*/0,
                  /*dedup_key=*/nullptr,
                  /*filters=*/AttributionFilterDataBuilder().Build(),
                  /*not_filters=*/AttributionFilterDataBuilder().Build()))
              .Build(),
      },
      {
          "valid_dedup_key",
          ParseJSON(R"json([{
            "trigger_data": "5",
            "deduplication_key": "3"
          }])json"),
          true,
          VectorBuilder<mojom::blink::EventTriggerDataPtr>()
              .Add(mojom::blink::EventTriggerData::New(
                  /*data=*/5,
                  /*priority=*/0,
                  /*dedup_key=*/
                  mojom::blink::AttributionTriggerDedupKey::New(3),
                  /*filters=*/AttributionFilterDataBuilder().Build(),
                  /*not_filters=*/AttributionFilterDataBuilder().Build()))
              .Build(),
      },
      {
          "dedup_key_not_string",
          ParseJSON(R"json([{
            "trigger_data": "5",
            "deduplication_key": 3
          }])json"),
          true,
          VectorBuilder<mojom::blink::EventTriggerDataPtr>()
              .Add(mojom::blink::EventTriggerData::New(
                  /*data=*/5,
                  /*priority=*/0,
                  /*dedup_key=*/nullptr,
                  /*filters=*/AttributionFilterDataBuilder().Build(),
                  /*not_filters=*/AttributionFilterDataBuilder().Build()))
              .Build(),
      },
      {
          "invalid_dedup_Key",
          ParseJSON(R"json([{
            "trigger_data": "5",
            "deduplication_key": "abc"
          }])json"),
          true,
          VectorBuilder<mojom::blink::EventTriggerDataPtr>()
              .Add(mojom::blink::EventTriggerData::New(
                  /*data=*/5,
                  /*priority=*/0,
                  /*dedup_key=*/nullptr,
                  /*filters=*/AttributionFilterDataBuilder().Build(),
                  /*not_filters=*/AttributionFilterDataBuilder().Build()))
              .Build(),
      },
      {
          "valid_filters",
          ParseJSON(R"json([{
            "trigger_data": "5",
            "filters": {"source_type": ["navigation"]}
          }])json"),
          true,
          VectorBuilder<mojom::blink::EventTriggerDataPtr>()
              .Add(mojom::blink::EventTriggerData::New(
                  /*data=*/5,
                  /*priority=*/0,
                  /*dedup_key=*/nullptr,
                  /*filters=*/
                  AttributionFilterDataBuilder()
                      .AddFilter("source_type", {"navigation"})
                      .Build(),
                  /*not_filters=*/AttributionFilterDataBuilder().Build()))
              .Build(),
      },
      {
          "invalid_filters",
          ParseJSON(R"json([{
            "trigger_data": "5",
            "filters": 1
          }])json"),
          false,
          {},
      },
      {
          "valid_not_filters",
          ParseJSON(R"json([{
            "trigger_data": "5",
            "not_filters": {"source_type": ["navigation"]}
          }])json"),
          true,
          VectorBuilder<mojom::blink::EventTriggerDataPtr>()
              .Add(mojom::blink::EventTriggerData::New(
                  /*data=*/5,
                  /*priority=*/0,
                  /*dedup_key=*/nullptr,
                  /*filters=*/AttributionFilterDataBuilder().Build(),
                  /*not_filters=*/
                  AttributionFilterDataBuilder()
                      .AddFilter("source_type", {"navigation"})
                      .Build()))
              .Build(),
      },
      {
          "invalid_not_filters",
          ParseJSON(R"json([{
            "trigger_data": "5",
            "not_filters": 1
          }])json"),
          false,
          {},
      },
  };

  for (const auto& test_case : kTestCases) {
    Vector<mojom::blink::EventTriggerDataPtr> actual;
    bool valid = ParseEventTriggerData(test_case.json.get(), actual);
    EXPECT_EQ(valid, test_case.valid) << test_case.description;
    EXPECT_EQ(actual, test_case.expected) << test_case.description;
  }
}

TEST(AttributionResponseParsingTest, FilterValuesHistogram) {
  const auto make_filter_data = [](wtf_size_t n) {
    auto array = std::make_unique<JSONArray>();
    for (wtf_size_t i = 0; i < n; ++i) {
      array->PushString("x");
    }

    auto object = std::make_unique<JSONObject>();
    object->SetArray("a", std::move(array));
    return object;
  };

  const struct {
    wtf_size_t size;
    bool expected;
  } kTestCases[] = {
      {0, true},
      {kMaxValuesPerAttributionFilter, true},
      {kMaxValuesPerAttributionFilter + 1, false},
  };

  for (const auto& test_case : kTestCases) {
    base::HistogramTester histograms;
    std::unique_ptr<JSONValue> json = make_filter_data(test_case.size);
    mojom::blink::AttributionFilterData filter_data;
    ParseAttributionFilterData(json.get(), filter_data);
    histograms.ExpectUniqueSample("Conversions.ValuesPerFilter", test_case.size,
                                  test_case.expected);
  }
}

TEST(AttributionResponseParsingTest, FiltersSizeHistogram) {
  const auto make_filter_data = [](wtf_size_t n) {
    auto object = std::make_unique<JSONObject>();
    for (wtf_size_t i = 0; i < n; ++i) {
      object->SetArray(String::Number(i), std::make_unique<JSONArray>());
    }
    return object;
  };

  const struct {
    wtf_size_t size;
    bool expected;
  } kTestCases[] = {
      {0, true},
      {kMaxAttributionFiltersPerSource, true},
      {kMaxAttributionFiltersPerSource + 1, false},
  };

  for (const auto& test_case : kTestCases) {
    base::HistogramTester histograms;
    std::unique_ptr<JSONValue> json = make_filter_data(test_case.size);
    mojom::blink::AttributionFilterData filter_data;
    ParseAttributionFilterData(json.get(), filter_data);
    histograms.ExpectUniqueSample("Conversions.FiltersPerFilterData",
                                  test_case.size, test_case.expected);
  }
}

TEST(AttributionResponseParsingTest, SourceAggregationKeysHistogram) {
  const auto make_aggregatable_source_with_keys = [](wtf_size_t n) {
    auto object = std::make_unique<JSONObject>();
    for (wtf_size_t i = 0; i < n; ++i) {
      object->SetString(String::Number(i), "0x1");
    }
    return object;
  };

  const struct {
    wtf_size_t size;
    bool expected;
  } kTestCases[] = {
      {0, true},
      {kMaxAttributionAggregationKeysPerSourceOrTrigger, true},
      {kMaxAttributionAggregationKeysPerSourceOrTrigger + 1, false},
  };

  for (const auto& test_case : kTestCases) {
    base::HistogramTester histograms;
    auto json = make_aggregatable_source_with_keys(test_case.size);
    WTF::HashMap<String, absl::uint128> aggregation_keys;
    ParseAggregationKeys(json.get(), aggregation_keys);
    histograms.ExpectUniqueSample("Conversions.AggregatableKeysPerSource",
                                  test_case.size, test_case.expected);
  }
}

TEST(AttributionResponseParsingTest, AggregatableTriggerDataHistogram) {
  const auto make_aggregatable_trigger_with_trigger_data = [](wtf_size_t n) {
    auto array = std::make_unique<JSONArray>();
    for (wtf_size_t i = 0; i < n; ++i) {
      auto object = std::make_unique<JSONObject>();
      object->SetString("key_piece", "0x1");
      object->SetArray("source_keys", std::make_unique<JSONArray>());
      array->PushObject(std::move(object));
    }
    return array;
  };

  const struct {
    wtf_size_t size;
    bool expected;
  } kTestCases[] = {
      {0, true},
      {kMaxAttributionAggregatableTriggerDataPerTrigger, true},
      {kMaxAttributionAggregatableTriggerDataPerTrigger + 1, false},
  };

  for (const auto& test_case : kTestCases) {
    base::HistogramTester histograms;
    std::unique_ptr<JSONValue> json =
        make_aggregatable_trigger_with_trigger_data(test_case.size);
    WTF::Vector<mojom::blink::AttributionAggregatableTriggerDataPtr>
        trigger_data;
    ParseAttributionAggregatableTriggerData(json.get(), trigger_data);
    histograms.ExpectUniqueSample("Conversions.AggregatableTriggerDataLength",
                                  test_case.size, test_case.expected);
  }
}

}  // namespace blink::attribution_response_parsing
