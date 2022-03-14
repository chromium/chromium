// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/attribution_response_parsing.h"

#include <limits>
#include <string>
#include <utility>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/attribution_reporting/constants.h"
#include "third_party/blink/public/mojom/conversions/attribution_data_host.mojom-blink.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink::attribution_response_parsing {

namespace {

class AggregatableSourceBuilder {
 public:
  AggregatableSourceBuilder() = default;
  ~AggregatableSourceBuilder() = default;

  AggregatableSourceBuilder& AddKey(
      String key_id,
      mojom::blink::AttributionAggregatableKeyPtr key) {
    source_.keys.insert(std::move(key_id), std::move(key));
    return *this;
  }

  mojom::blink::AttributionAggregatableSourcePtr Build() const {
    return source_.Clone();
  }

 private:
  mojom::blink::AttributionAggregatableSource source_;
};

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

}  // namespace

TEST(AttributionResponseParsingTest, ParseAttributionAggregatableSource) {
  const struct {
    String description;
    AtomicString header;
    bool valid;
    mojom::blink::AttributionAggregatableSourcePtr source;
  } kTestCases[] = {
      {"Empty header", "", false,
       mojom::blink::AttributionAggregatableSource::New()},
      {"Invalid JSON", "{", false,
       mojom::blink::AttributionAggregatableSource::New()},
      {"Missing id field", R"([{"key_piece":"0x159"}])", false,
       mojom::blink::AttributionAggregatableSource::New()},
      {"Missing key_piece field", R"([{"id":"key"}])", false,
       mojom::blink::AttributionAggregatableSource::New()},
      {"Invalid key", R"([{"id":"key","key_piece":"0xG59"}])", false,
       mojom::blink::AttributionAggregatableSource::New()},
      {"One valid key", R"([{"id":"key","key_piece":"0x159"}])", true,
       AggregatableSourceBuilder()
           .AddKey(/*key_id=*/"key",
                   mojom::blink::AttributionAggregatableKey::New(
                       /*high_bits=*/0, /*low_bits=*/345))
           .Build()},
      {"Two valid keys",
       AtomicString(R"([{"id":"key1","key_piece":"0x159"},)") +
           R"({"id":"key2","key_piece":"0x50000000000000159"}])",
       true,
       AggregatableSourceBuilder()
           .AddKey(/*key_id=*/"key1",
                   mojom::blink::AttributionAggregatableKey::New(
                       /*high_bits=*/0, /*low_bits=*/345))
           .AddKey(/*key_id=*/"key2",
                   mojom::blink::AttributionAggregatableKey::New(
                       /*high_bits=*/5, /*low_bits=*/345))
           .Build()},
      {"Second key invalid",
       AtomicString(R"([{"id":"key1","key_piece":"0x159"},)") +
           R"({"id":"key2","key_piece":""}])",
       false, mojom::blink::AttributionAggregatableSource::New()},
  };

  for (const auto& test_case : kTestCases) {
    auto source = mojom::blink::AttributionAggregatableSource::New();
    bool valid = ParseAttributionAggregatableSource(test_case.header, *source);
    EXPECT_EQ(test_case.valid, valid) << test_case.description;
    if (test_case.valid)
      EXPECT_EQ(test_case.source, source) << test_case.description;
  }
}

TEST(AttributionResponseParsingTest,
     ParseAttributionAggregatableSource_CheckSize) {
  struct AttributionAggregatableSourceSizeTestCase {
    String description;
    bool valid;
    wtf_size_t key_count;
    wtf_size_t key_size;

    AtomicString GetHeader() const {
      StringBuilder builder;

      const char* separator = "";
      for (wtf_size_t i = 0u; i < key_count; ++i) {
        builder.Append(separator);
        builder.Append("{\"key_piece\":\"0x1\",\"id\":\"");
        builder.Append(GetKey(i));
        builder.Append("\"}");
        separator = ",";
      }

      return "[" + builder.ToAtomicString() + "]";
    }

    mojom::blink::AttributionAggregatableSourcePtr GetSource() const {
      AggregatableSourceBuilder builder;
      if (!valid)
        return builder.Build();

      for (wtf_size_t i = 0u; i < key_count; ++i) {
        builder.AddKey(GetKey(i), mojom::blink::AttributionAggregatableKey::New(
                                      /*high_bits=*/0, /*low_bits=*/1));
      }

      return builder.Build();
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
    auto source = mojom::blink::AttributionAggregatableSource::New();
    bool valid =
        ParseAttributionAggregatableSource(test_case.GetHeader(), *source);
    EXPECT_EQ(test_case.valid, valid) << test_case.description;
    if (test_case.valid)
      EXPECT_EQ(test_case.GetSource(), source) << test_case.description;
  }
}

TEST(AttributionResponseParsingTest, ParseAttributionAggregatableTrigger) {
  const struct {
    String description;
    AtomicString header;
    bool valid;
    mojom::blink::AttributionAggregatableTriggerPtr trigger;
  } kTestCases[] = {
      {"Empty header", "", false,
       mojom::blink::AttributionAggregatableTrigger::New()},
      {"Invalid JSON", "{", false,
       mojom::blink::AttributionAggregatableTrigger::New()},
      {"Missing source_keys field", R"([{"key_piece":"0x400"}])", false,
       mojom::blink::AttributionAggregatableTrigger::New()},
      {"Missing key_piece field", R"([{"source_keys":["key"]}])", false,
       mojom::blink::AttributionAggregatableTrigger::New()},
      {"Invalid key", R"([{"key_piece":"0xG00","source_keys":["key"]}])", false,
       mojom::blink::AttributionAggregatableTrigger::New()},
      {"Valid trigger", R"([{"key_piece":"0x400","source_keys":["key"]}])",
       true,
       AggregatableTriggerBuilder()
           .AddTriggerData(
               mojom::blink::AttributionAggregatableTriggerData::New(
                   mojom::blink::AttributionAggregatableKey::New(
                       /*high_bits=*/0, /*low_bits=*/1024),
                   /*source_keys=*/Vector<String>{"key"},
                   /*filters=*/mojom::blink::AttributionFilterData::New(),
                   /*not_filters=*/mojom::blink::AttributionFilterData::New()))
           .Build()},
      {"Valid trigger with filters",
       AtomicString(R"([{"key_piece":"0x400","source_keys":["key"],)") +
           R"("filters":{"filter":["value1"]},"not_filters":{"filter":["value2"]}}])",
       true,
       AggregatableTriggerBuilder()
           .AddTriggerData(
               mojom::blink::AttributionAggregatableTriggerData::New(
                   mojom::blink::AttributionAggregatableKey::New(
                       /*high_bits=*/0, /*low_bits=*/1024),
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
       AtomicString(R"([{"key_piece":"0x400","source_keys":["key1"]},)") +
           R"({"key_piece":"0xA80","source_keys":["key2"]}])",
       true,
       AggregatableTriggerBuilder()
           .AddTriggerData(
               mojom::blink::AttributionAggregatableTriggerData::New(
                   mojom::blink::AttributionAggregatableKey::New(
                       /*high_bits=*/0, /*low_bits=*/1024),
                   /*source_keys=*/Vector<String>{"key1"},
                   /*filters=*/mojom::blink::AttributionFilterData::New(),
                   /*not_filters=*/mojom::blink::AttributionFilterData::New()))
           .AddTriggerData(
               mojom::blink::AttributionAggregatableTriggerData::New(
                   mojom::blink::AttributionAggregatableKey::New(
                       /*high_bits=*/0, /*low_bits=*/2688),
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
    EXPECT_EQ(test_case.valid, valid) << test_case.description;
    if (test_case.valid) {
      EXPECT_EQ(test_case.trigger->trigger_data, trigger_data)
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

    AtomicString GetHeader() const {
      StringBuilder builder;
      String key = GetKey();

      const char* separator = "";
      for (wtf_size_t i = 0u; i < trigger_data_count; ++i) {
        builder.Append(separator);
        builder.Append("{\"key_piece\":\"0x1\",\"source_keys\":[");

        const char* sub_separator = "";
        for (wtf_size_t j = 0u; j < key_count; ++j) {
          builder.Append(sub_separator);
          builder.Append("\"");
          builder.Append(key);
          builder.Append("\"");
          sub_separator = ",";
        }

        builder.Append("]}");
        separator = ",";
      }

      return "[" + builder.ToAtomicString() + "]";
    }

    WTF::Vector<mojom::blink::AttributionAggregatableTriggerDataPtr>
    GetTriggerData() const {
      AggregatableTriggerBuilder builder;
      if (!valid)
        return {};

      for (wtf_size_t i = 0u; i < trigger_data_count; ++i) {
        builder.AddTriggerData(
            mojom::blink::AttributionAggregatableTriggerData::New(
                mojom::blink::AttributionAggregatableKey::New(
                    /*high_bits=*/0, /*low_bits=*/1),
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
    AtomicString description;
    AtomicString header;
    bool valid;
    WTF::HashMap<String, uint32_t> values;
  } kTestCases[] = {
      {"Empty header", "", false, {}},
      {"Invalid JSON", "{", false, {}},
      {"Invalid value", R"({"key":-1})", false, {}},
      {"Valid value", R"({"key":123})", true, {{"key", 123}}},
      {"Two valid values",
       R"({"key1":123,"key2":456})",
       true,
       {{"key1", 123}, {"key2", 456}}},
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

    AtomicString GetHeader() const {
      StringBuilder builder;
      const char* separator = "";
      for (wtf_size_t i = 0u; i < key_count; ++i) {
        builder.Append(separator);
        builder.Append("\"");
        builder.Append(GetKey(i));
        builder.Append("\":");
        builder.AppendNumber(i + 1);
        separator = ",";
      }
      return "{" + builder.ToAtomicString() + "}";
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
  // TODO(apaseltiner): Add comprehensive test cases.
  const struct {
    String description;
    AtomicString json;
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
  };

  for (const auto& test_case : kTestCases) {
    mojom::blink::AttributionFilterData filter_data;

    bool valid = ParseFilters(test_case.json, filter_data);
    EXPECT_EQ(valid, !test_case.expected.is_null()) << test_case.description;

    if (test_case.expected) {
      EXPECT_EQ(*test_case.expected, filter_data) << test_case.description;
    }
  }
}

TEST(AttributionResponseParsingTest, ParseSourceRegistrationHeader) {
  const auto reporting_origin =
      SecurityOrigin::CreateFromString("https://r.test");

  // TODO(apaseltiner): Add comprehensive test cases.
  const struct {
    String description;
    AtomicString json;
    mojom::blink::AttributionSourceDataPtr expected;
  } kTestCases[] = {
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
              /*aggregatable_source=*/AggregatableSourceBuilder().Build()),
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
              /*aggregatable_source=*/AggregatableSourceBuilder().Build()),
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

      EXPECT_EQ(test_case.expected->filter_data, source_data.filter_data)
          << test_case.description;

      // TODO(apaseltiner): Check remaining fields here.
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

}  // namespace blink::attribution_response_parsing
