// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/attribution_response_parsing.h"

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

class AggregatableSourcesBuilder {
 public:
  AggregatableSourcesBuilder() = default;
  ~AggregatableSourcesBuilder() = default;

  AggregatableSourcesBuilder& AddKey(
      String key_id,
      mojom::blink::AttributionAggregatableKeyPtr key) {
    sources_.sources.insert(std::move(key_id), std::move(key));
    return *this;
  }

  mojom::blink::AttributionAggregatableSourcesPtr Build() const {
    return sources_.Clone();
  }

 private:
  mojom::blink::AttributionAggregatableSources sources_;
};

}  // namespace

TEST(AttributionResponseParsingTest, ParseAttributionAggregatableSources) {
  const struct {
    String description;
    AtomicString header;
    ResponseParseStatus status;
    mojom::blink::AttributionAggregatableSourcesPtr value;
  } kTestCases[] = {
      {"Empty header", "", ResponseParseStatus::kNotFound,
       mojom::blink::AttributionAggregatableSources::New()},
      {"Invalid JSON", "{", ResponseParseStatus::kParseError,
       mojom::blink::AttributionAggregatableSources::New()},
      {"Missing id field", R"([{"key_piece":"0x159"}])",
       ResponseParseStatus::kInvalidFormat,
       mojom::blink::AttributionAggregatableSources::New()},
      {"Missing key_piece field", R"([{"id":"key"}])",
       ResponseParseStatus::kInvalidFormat,
       mojom::blink::AttributionAggregatableSources::New()},
      {"Invalid key", R"([{"id":"key","key_piece":"0xG59"}])",
       ResponseParseStatus::kInvalidFormat,
       mojom::blink::AttributionAggregatableSources::New()},
      {"One valid key", R"([{"id":"key","key_piece":"0x159"}])",
       ResponseParseStatus::kSuccess,
       AggregatableSourcesBuilder()
           .AddKey(/*key_id=*/"key",
                   mojom::blink::AttributionAggregatableKey::New(
                       /*high_bits=*/0, /*low_bits=*/345))
           .Build()},
      {"Two valid keys",
       AtomicString(R"([{"id":"key1","key_piece":"0x159"},)") +
           R"({"id":"key2","key_piece":"0x50000000000000159"}])",
       ResponseParseStatus::kSuccess,
       AggregatableSourcesBuilder()
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
       ResponseParseStatus::kInvalidFormat,
       mojom::blink::AttributionAggregatableSources::New()},
  };

  for (const auto& test_case : kTestCases) {
    auto result = ParseAttributionAggregatableSources(test_case.header);
    EXPECT_EQ(test_case.status, result.status) << test_case.description;
    EXPECT_EQ(test_case.value, result.value) << test_case.description;
  }
}

TEST(AttributionResponseParsingTest,
     ParseAttributionAggregatableSources_CheckSize) {
  struct AttributionAggregatableSourcesSizeTestCase {
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

    mojom::blink::AttributionAggregatableSourcesPtr GetValue() const {
      AggregatableSourcesBuilder builder;
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
      // `blink::kMaxAttributionAggregatableKeysPerSource` varies which might
      // generate invalid JSON.
      return String(
          std::string(key_size, 'A' + index % 26 + 32 * (index / 26)));
    }
  };

  const AttributionAggregatableSourcesSizeTestCase kTestCases[] = {
      {"empty", true, 0, 0},
      {"max_keys", true, blink::kMaxAttributionAggregatableKeysPerSource, 1},
      {"too_many_keys", false,
       blink::kMaxAttributionAggregatableKeysPerSource + 1, 1},
      {"max_key_size", true, 1,
       blink::kMaxBytesPerAttributionAggregatableKeyId},
      {"excessive_key_size", false, 1,
       blink::kMaxBytesPerAttributionAggregatableKeyId + 1},
  };

  for (const auto& test_case : kTestCases) {
    auto result = ParseAttributionAggregatableSources(test_case.GetHeader());
    EXPECT_EQ(result.status, test_case.valid
                                 ? ResponseParseStatus::kSuccess
                                 : ResponseParseStatus::kInvalidFormat)
        << test_case.description;
    EXPECT_EQ(result.value, test_case.GetValue()) << test_case.description;
  }
}

}  // namespace blink::attribution_response_parsing
