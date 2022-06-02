// Copyright 2022 The Chromium Authors. All rights reserved.
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

class AggregatableTriggerBuilder {
 public:
  AggregatableTriggerBuilder() = default;
  ~AggregatableTriggerBuilder() = default;

  AggregatableTriggerBuilder& AddTriggerData(
      mojom::blink::AttributionAggregatableTriggerDataPtr trigger) {
    trigger_.trigger_data.push_back(std::move(trigger));
    return *this;
  }

  mojom::blink::AttributionAggregatableTriggerPtr Build() const {
    return trigger_.Clone();
  }

 private:
  mojom::blink::AttributionAggregatableTrigger trigger_;
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

TEST(AttributionResponseParsingTest, ParseAttributionAggregatableSource) {
  const struct {
    String description;
    String header;
    bool valid;
    WTF::HashMap<String, absl::uint128> expected;
  } kTestCases[] = {
      {"Empty header", "", false, {}},
      {"Invalid JSON", "{", false, {}},
      {"Not a dictionary", "[]", false, {}},
      {"key not a string", R"({"key":123})", false, {}},
      {"Invalid key", R"({"key":"0xG59"})", false, {}},
      {"One valid key",
       R"({"key":"0x159"})",
       true,
       {{"key", absl::MakeUint128(/*high=*/0, /*low=*/345)}}},
      {"Two valid keys",
       R"({"key1":"0x159","key2":"0x50000000000000159"})",
       true,
       {
           {"key1", absl::MakeUint128(/*high=*/0, /*low=*/345)},
           {"key2", absl::MakeUint128(/*high=*/5, /*low=*/345)},
       }},
      {"Second key invalid", R"({"key1":"0x159","key2":""})", false, {}},
  };

  for (const auto& test_case : kTestCases) {
    WTF::HashMap<String, absl::uint128> actual;
    bool valid = ParseAttributionAggregatableSource(test_case.header, actual);
    EXPECT_EQ(test_case.valid, valid) << test_case.description;
    if (test_case.valid)
      EXPECT_EQ(test_case.expected, actual) << test_case.description;
  }
}

TEST(AttributionResponseParsingTest,
     ParseAttributionAggregatableSource_CheckSize) {
  struct AttributionAggregatableSourceSizeTestCase {
    String description;
    bool valid;
    wtf_size_t key_count;
    wtf_size_t key_size;

    String GetHeader() const {
      JSONObject object;
      for (wtf_size_t i = 0u; i < key_count; ++i) {
        object.SetString(GetKey(i), "0x1");
      }
      return object.ToJSONString();
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
      // `blink::kMaxAttributionAggregatableKeysPerSourceOrTrigger` varies which
      // might generate invalid JSON.
      return String(
          std::string(key_size, 'A' + index % 26 + 32 * (index / 26)));
    }
  };

  const AttributionAggregatableSourceSizeTestCase kTestCases[] = {
      {"empty", true, 0, 0},
      {"max_keys", true,
       blink::kMaxAttributionAggregatableKeysPerSourceOrTrigger, 1},
      {"too_many_keys", false,
       blink::kMaxAttributionAggregatableKeysPerSourceOrTrigger + 1, 1},
      {"max_key_size", true, 1,
       blink::kMaxBytesPerAttributionAggregatableKeyId},
      {"excessive_key_size", false, 1,
       blink::kMaxBytesPerAttributionAggregatableKeyId + 1},
  };

  for (const auto& test_case : kTestCases) {
    WTF::HashMap<String, absl::uint128> actual;
    bool valid =
        ParseAttributionAggregatableSource(test_case.GetHeader(), actual);
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
    String header;
    mojom::blink::AttributionAggregatableTriggerPtr expected;
  } kTestCases[] = {
      {"Empty header", "", nullptr},
      {"Invalid JSON", "{", nullptr},
      {"Not an array", "{}", nullptr},
      {"Element not a dictionary", "[123]", nullptr},
      {"Missing source_keys field", R"([{"key_piece":"0x400"}])", nullptr},
      {"source_keys not an array",
       R"([{"key_piece":"0x400","source_keys":"key"}])", nullptr},
      {"source_keys element not a string",
       R"([{"key_piece":"0x400","source_keys":[123]}])"},
      {"Missing key_piece field", R"([{"source_keys":["key"]}])", nullptr},
      {"Invalid key", R"([{"key_piece":"0xG00","source_keys":["key"]}])",
       nullptr},
      {"Valid trigger", R"([{"key_piece":"0x400","source_keys":["key"]}])",
       AggregatableTriggerBuilder()
           .AddTriggerData(
               mojom::blink::AttributionAggregatableTriggerData::New(
                   absl::MakeUint128(/*high=*/0, /*low=*/1024),
                   /*source_keys=*/Vector<String>{"key"},
                   /*filters=*/mojom::blink::AttributionFilterData::New(),
                   /*not_filters=*/mojom::blink::AttributionFilterData::New()))
           .Build()},
      {"Valid trigger with filters",
       R"([{
         "key_piece": "0x400",
         "source_keys": ["key"],
         "filters": {"filter": ["value1"]},
         "not_filters": {"filter": ["value2"]}
       }])",
       AggregatableTriggerBuilder()
           .AddTriggerData(
               mojom::blink::AttributionAggregatableTriggerData::New(
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
       R"([{"key_piece":"0x400","source_keys":["key1"]},
           {"key_piece":"0xA80","source_keys":["key2"]}])",
       AggregatableTriggerBuilder()
           .AddTriggerData(
               mojom::blink::AttributionAggregatableTriggerData::New(
                   absl::MakeUint128(/*high=*/0, /*low=*/1024),
                   /*source_keys=*/Vector<String>{"key1"},
                   /*filters=*/mojom::blink::AttributionFilterData::New(),
                   /*not_filters=*/mojom::blink::AttributionFilterData::New()))
           .AddTriggerData(
               mojom::blink::AttributionAggregatableTriggerData::New(
                   absl::MakeUint128(/*high=*/0, /*low=*/2688),
                   /*source_keys=*/Vector<String>{"key2"},
                   /*filters=*/mojom::blink::AttributionFilterData::New(),
                   /*not_filters=*/mojom::blink::AttributionFilterData::New()))
           .Build()},
  };

  for (const auto& test_case : kTestCases) {
    WTF::Vector<mojom::blink::AttributionAggregatableTriggerDataPtr>
        trigger_data;
    bool valid =
        ParseAttributionAggregatableTriggerData(test_case.header, trigger_data);
    EXPECT_EQ(!test_case.expected.is_null(), valid) << test_case.description;
    if (test_case.expected) {
      EXPECT_EQ(test_case.expected->trigger_data, trigger_data)
          << test_case.description;
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

    String GetHeader() const {
      String key = GetKey();

      JSONArray array;
      for (wtf_size_t i = 0u; i < trigger_data_count; ++i) {
        auto object = std::make_unique<JSONObject>();
        object->SetString("key_piece", "0x1");

        auto keys = std::make_unique<JSONArray>();
        for (wtf_size_t j = 0u; j < key_count; ++j) {
          keys->PushString(key);
        }
        object->SetArray("source_keys", std::move(keys));

        array.PushObject(std::move(object));
      }

      return array.ToJSONString();
    }

    WTF::Vector<mojom::blink::AttributionAggregatableTriggerDataPtr>
    GetTriggerData() const {
      AggregatableTriggerBuilder builder;
      if (!valid)
        return {};

      for (wtf_size_t i = 0u; i < trigger_data_count; ++i) {
        builder.AddTriggerData(
            mojom::blink::AttributionAggregatableTriggerData::New(
                absl::MakeUint128(/*high=*/0, /*low=*/1),
                /*source_keys=*/Vector<String>(key_count, GetKey()),
                /*filters=*/mojom::blink::AttributionFilterData::New(),
                /*not_filters=*/mojom::blink::AttributionFilterData::New()));
      }
      return std::move(builder.Build()->trigger_data);
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
       blink::kMaxAttributionAggregatableKeysPerSourceOrTrigger, 0},
      {"too many keys", false, 1,
       blink::kMaxAttributionAggregatableKeysPerSourceOrTrigger + 1, 0},
      {"max_key_size", true, 1, 1, kMaxBytesPerAttributionAggregatableKeyId},
      {"excessive_key_size", false, 1, 1,
       kMaxBytesPerAttributionAggregatableKeyId + 1},
  };

  for (const auto& test_case : kTestCases) {
    WTF::Vector<mojom::blink::AttributionAggregatableTriggerDataPtr>
        trigger_data;
    bool valid = ParseAttributionAggregatableTriggerData(test_case.GetHeader(),
                                                         trigger_data);

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
    String header;
    bool valid;
    WTF::HashMap<String, uint32_t> values;
  } kTestCases[] = {
      {"Empty header", "", false, {}},
      {"Invalid JSON", "{", false, {}},
      {"Value not an integer", R"({"key":"1"})", false, {}},
      {"Invalid value", R"({"key":-1})", false, {}},
      {"Valid value", R"({"key":123})", true, {{"key", 123}}},
      {"Two valid values",
       R"({"key1":123,"key2":456})",
       true,
       {{"key1", 123}, {"key2", 456}}},
      {"Max valid value", R"({"key":65536})", true, {{"key", 65536}}},
      {"Value out of range", R"({"key":65537})", false, {}},
  };

  for (const auto& test_case : kTestCases) {
    WTF::HashMap<String, uint32_t> values;
    bool valid = ParseAttributionAggregatableValues(test_case.header, values);
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

    String GetHeader() const {
      JSONObject object;
      for (wtf_size_t i = 0u; i < key_count; ++i) {
        object.SetInteger(GetKey(i), i + 1);
      }
      return object.ToJSONString();
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
      // `blink::kMaxAttributionAggregatableKeysPerSourceOrTrigger` varies which
      // might generate invalid JSON characters.
      return String(
          std::string(key_size, 'A' + index % 26 + 32 * (index / 26)));
    }
  };

  const AttributionAggregatableValuesSizeTestCase kTestCases[] = {
      {"empty", true, 0, 0},
      {"max_keys", true,
       blink::kMaxAttributionAggregatableKeysPerSourceOrTrigger, 1},
      {"too_many_keys", false,
       blink::kMaxAttributionAggregatableKeysPerSourceOrTrigger + 1, 1},
      {"max_key_size", true, 1,
       blink::kMaxBytesPerAttributionAggregatableKeyId},
      {"excessive_key_size", false, 1,
       blink::kMaxBytesPerAttributionAggregatableKeyId + 1},
  };

  for (const auto& test_case : kTestCases) {
    WTF::HashMap<String, uint32_t> values;
    bool valid =
        ParseAttributionAggregatableValues(test_case.GetHeader(), values);

    EXPECT_EQ(test_case.valid, valid) << test_case.description;
    if (test_case.valid)
      EXPECT_EQ(test_case.GetValues(), values) << test_case.description;
  }
}

TEST(AttributionResponseParsingTest, ParseFilters) {
  const auto make_filter_data_with_keys = [](wtf_size_t n) {
    JSONObject root;
    for (wtf_size_t i = 0; i < n; ++i) {
      root.SetArray(String::Number(i), std::make_unique<JSONArray>());
    }
    return root.ToJSONString();
  };

  const auto make_filter_data_with_key_length = [](wtf_size_t n) {
    JSONObject root;
    root.SetArray(String(std::string(n, 'a')), std::make_unique<JSONArray>());
    return root.ToJSONString();
  };

  const auto make_filter_data_with_values = [](wtf_size_t n) {
    auto array = std::make_unique<JSONArray>();
    for (wtf_size_t i = 0; i < n; ++i) {
      array->PushString("x");
    }

    JSONObject root;
    root.SetArray("a", std::move(array));
    return root.ToJSONString();
  };

  const auto make_filter_data_with_value_length = [](wtf_size_t n) {
    auto array = std::make_unique<JSONArray>();
    array->PushString(String(std::string(n, 'a')));

    JSONObject root;
    root.SetArray("a", std::move(array));
    return root.ToJSONString();
  };

  const struct {
    String description;
    String json;
    mojom::blink::AttributionFilterDataPtr expected;
  } kTestCases[] = {
      {
          "empty",
          R"json({})json",
          AttributionFilterDataBuilder().Build(),
      },
      {
          "source_type",
          R"json({"source_type": []})json",
          AttributionFilterDataBuilder().AddFilter("source_type", {}).Build(),
      },
      {
          "multiple",
          R"json({
            "a": ["b"],
            "c": ["e", "d"]
          })json",
          AttributionFilterDataBuilder()
              .AddFilter("a", {"b"})
              .AddFilter("c", {"e", "d"})
              .Build(),
      },
      {
          "invalid_json",
          "!",
          nullptr,
      },
      {
          "not_dictionary",
          R"json(true)json",
          nullptr,
      },
      {
          "value_not_array",
          R"json({"a": true})json",
          nullptr,
      },
      {
          "array_element_not_string",
          R"json({"a": [true]})json",
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

    bool valid = ParseFilters(test_case.json, filter_data);
    EXPECT_EQ(valid, !test_case.expected.is_null()) << test_case.description;

    if (test_case.expected) {
      EXPECT_EQ(*test_case.expected, filter_data) << test_case.description;
    }
  }

  {
    mojom::blink::AttributionFilterData filter_data;
    EXPECT_TRUE(ParseFilters(make_filter_data_with_keys(50), filter_data));
  }

  {
    mojom::blink::AttributionFilterData filter_data;
    EXPECT_TRUE(
        ParseFilters(make_filter_data_with_key_length(25), filter_data));
  }

  {
    mojom::blink::AttributionFilterData filter_data;
    EXPECT_TRUE(ParseFilters(make_filter_data_with_values(50), filter_data));
  }

  {
    mojom::blink::AttributionFilterData filter_data;
    EXPECT_TRUE(
        ParseFilters(make_filter_data_with_value_length(25), filter_data));
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
          "missing_source_event_id",
          R"json({
            "destination": "https://d.test"
          })json",
          nullptr,
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
          nullptr,
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
          "destination_not_string",
          R"json({
            "source_event_id": "1",
            "destination": 4
          })json",
          nullptr,
      },
      {
          "destination_not_potentially_trustworthy",
          R"json({
            "source_event_id": "1",
            "destination": "http://d.test"
          })json",
          nullptr,
      },
      {
          "valid_priority",
          R"json({
            "source_event_id": "1",
            "destination": "https://d.test",
            "priority": "5"
          })json",
          mojom::blink::AttributionSourceData::New(
              /*destination=*/SecurityOrigin::CreateFromString(
                  "https://d.test"),
              /*reporting_origin=*/reporting_origin,
              /*source_event_id=*/1,
              /*expiry=*/absl::nullopt,
              /*priority=*/5,
              /*debug_key=*/nullptr,
              /*filter_data=*/AttributionFilterDataBuilder().Build(),
              /*aggregation_keys=*/WTF::HashMap<String, absl::uint128>()),
      },
      {
          "priority_not_string",
          R"json({
            "source_event_id": "1",
            "destination": "https://d.test",
            "priority": 5
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
          "invalid_priority",
          R"json({
            "source_event_id": "1",
            "destination": "https://d.test",
            "priority": "abc"
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
          "valid_expiry",
          R"json({
            "source_event_id": "1",
            "destination": "https://d.test",
            "expiry": "5"
          })json",
          mojom::blink::AttributionSourceData::New(
              /*destination=*/SecurityOrigin::CreateFromString(
                  "https://d.test"),
              /*reporting_origin=*/reporting_origin,
              /*source_event_id=*/1,
              /*expiry=*/base::Seconds(5),
              /*priority=*/0,
              /*debug_key=*/nullptr,
              /*filter_data=*/AttributionFilterDataBuilder().Build(),
              /*aggregation_keys=*/WTF::HashMap<String, absl::uint128>()),
      },
      {
          "expiry_not_string",
          R"json({
            "source_event_id": "1",
            "destination": "https://d.test",
            "expiry": 5
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
          "invalid_expiry",
          R"json({
            "source_event_id": "1",
            "destination": "https://d.test",
            "expiry": "abc"
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
          "valid_debug_key",
          R"json({
            "source_event_id": "1",
            "destination": "https://d.test",
            "debug_key": "5"
          })json",
          mojom::blink::AttributionSourceData::New(
              /*destination=*/SecurityOrigin::CreateFromString(
                  "https://d.test"),
              /*reporting_origin=*/reporting_origin,
              /*source_event_id=*/1,
              /*expiry=*/absl::nullopt,
              /*priority=*/0,
              /*debug_key=*/mojom::blink::AttributionDebugKey::New(5),
              /*filter_data=*/AttributionFilterDataBuilder().Build(),
              /*aggregation_keys=*/WTF::HashMap<String, absl::uint128>()),
      },
      {
          "valid_filter_data",
          R"json({
            "source_event_id": "1",
            "destination": "https://d.test",
            "filter_data": {"SOURCE_TYPE": []}
          })json",
          mojom::blink::AttributionSourceData::New(
              /*destination=*/SecurityOrigin::CreateFromString(
                  "https://d.test"),
              /*reporting_origin=*/reporting_origin,
              /*source_event_id=*/1,
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
            "source_event_id": "1",
            "destination": "https://d.test",
            "filter_data": {"source_type": []}
          })json",
          nullptr,
      },
      {
          "unknown_field",
          R"json({
            "source_event_id": "1",
            "destination": "https://d.test",
            "a": {"b": {"c": {"d": "e"}}}
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
    String json;
    bool valid;
    Vector<mojom::blink::EventTriggerDataPtr> expected;
  } kTestCases[] = {
      {
          "invalid_json",
          "!",
          false,
          {},
      },
      {
          "root_not_array",
          R"json({})json",
          false,
          {},
      },
      {
          "empty",
          R"json([])json",
          true,
          {},
      },
      {
          "too_many_values",
          R"json([{},{},{},{},{},{},{},{},{},{},{}])json",
          false,
          {},
      },
      {
          "value_not_object",
          R"json([123])json",
          false,
          {},
      },
      {
          "missing_trigger_data",
          R"json([{}])json",
          false,
          {},
      },
      {
          "trigger_data_not_string",
          R"json([{"trigger_data": 1}])json",
          false,
          {},
      },
      {
          "invalid_trigger_data",
          R"json([{"trigger_data": "-5"}])json",
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
          R"json([{"trigger_data": "5"}])json",
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
          R"json([
            {"trigger_data": "5"},
            {"trigger_data": "3"},
            {"trigger_data": "4"}
          ])json",
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
          R"json([{
            "trigger_data": "5",
            "priority": "3"
          }])json",
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
          R"json([{
            "trigger_data": "5",
            "priority": 3
          }])json",
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
          R"json([{
            "trigger_data": "5",
            "priority": "abc"
          }])json",
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
          R"json([{
            "trigger_data": "5",
            "deduplication_key": "3"
          }])json",
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
          R"json([{
            "trigger_data": "5",
            "deduplication_key": 3
          }])json",
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
          R"json([{
            "trigger_data": "5",
            "deduplication_key": "abc"
          }])json",
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
          R"json([{
            "trigger_data": "5",
            "filters": {"source_type": ["navigation"]}
          }])json",
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
          R"json([{
            "trigger_data": "5",
            "filters": 1
          }])json",
          false,
          {},
      },
      {
          "valid_not_filters",
          R"json([{
            "trigger_data": "5",
            "not_filters": {"source_type": ["navigation"]}
          }])json",
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
          R"json([{
            "trigger_data": "5",
            "not_filters": 1
          }])json",
          false,
          {},
      },
  };

  for (const auto& test_case : kTestCases) {
    Vector<mojom::blink::EventTriggerDataPtr> actual;
    bool valid = ParseEventTriggerData(test_case.json, actual);
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

    JSONObject object;
    object.SetArray("a", std::move(array));
    return object.ToJSONString();
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
    mojom::blink::AttributionFilterData filter_data;
    ParseFilters(make_filter_data(test_case.size), filter_data);
    histograms.ExpectUniqueSample("Conversions.ValuesPerFilter", test_case.size,
                                  test_case.expected);
  }
}

TEST(AttributionResponseParsingTest, FiltersSizeHistogram) {
  const auto make_filter_data = [](wtf_size_t n) {
    JSONObject object;
    for (wtf_size_t i = 0; i < n; ++i) {
      object.SetArray(String::Number(i), std::make_unique<JSONArray>());
    }
    return object.ToJSONString();
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
    mojom::blink::AttributionFilterData filter_data;
    ParseFilters(make_filter_data(test_case.size), filter_data);
    histograms.ExpectUniqueSample("Conversions.FiltersPerFilterData",
                                  test_case.size, test_case.expected);
  }
}

TEST(AttributionResponseParsingTest, SourceAggregatableKeysHistogram) {
  const auto make_aggregatable_source_with_keys = [](wtf_size_t n) {
    JSONObject object;
    for (wtf_size_t i = 0; i < n; ++i) {
      object.SetString(String::Number(i), "0x1");
    }
    return object.ToJSONString();
  };

  const struct {
    wtf_size_t size;
    bool expected;
  } kTestCases[] = {
      {0, true},
      {kMaxAttributionAggregatableKeysPerSourceOrTrigger, true},
      {kMaxAttributionAggregatableKeysPerSourceOrTrigger + 1, false},
  };

  for (const auto& test_case : kTestCases) {
    base::HistogramTester histograms;
    WTF::HashMap<String, absl::uint128> aggregation_keys;
    ParseAttributionAggregatableSource(
        make_aggregatable_source_with_keys(test_case.size), aggregation_keys);
    histograms.ExpectUniqueSample("Conversions.AggregatableKeysPerSource",
                                  test_case.size, test_case.expected);
  }
}

TEST(AttributionResponseParsingTest, AggregatableTriggerDataHistogram) {
  const auto make_aggregatable_trigger_with_trigger_data = [](wtf_size_t n) {
    JSONArray array;
    for (wtf_size_t i = 0; i < n; ++i) {
      auto object = std::make_unique<JSONObject>();
      object->SetString("key_piece", "0x1");
      object->SetArray("source_keys", std::make_unique<JSONArray>());
      array.PushObject(std::move(object));
    }
    return array.ToJSONString();
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
    WTF::Vector<mojom::blink::AttributionAggregatableTriggerDataPtr>
        trigger_data;
    ParseAttributionAggregatableTriggerData(
        make_aggregatable_trigger_with_trigger_data(test_case.size),
        trigger_data);
    histograms.ExpectUniqueSample("Conversions.AggregatableTriggerDataLength",
                                  test_case.size, test_case.expected);
  }
}

}  // namespace blink::attribution_response_parsing
