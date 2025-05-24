// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/rand_util.h"
#include "base/test/fuzztest_support.h"
#include "third_party/fuzztest/src/fuzztest/fuzztest.h"
#include "third_party/inspector_protocol/crdtp/json.h"

namespace content {

void ConvertsJSONToCBORCorrectly(std::string_view input) {
  std::vector<uint8_t> cbor;
  crdtp::json::ConvertJSONToCBOR(base::as_byte_span(input), &cbor);
}

auto JsonDomain() {
  return fuzztest::ReversibleMap(
      // The mapping function maps a base::Value to its JSON string
      // representation.
      [](base::Value value) {
        std::string res;
        base::JSONWriter::Write(std::move(value), &res);
        return res;
      },
      // The inverse mapping function maps the JSON string representation to
      // a tuple of base::Value. The return value is additionally wrapped in
      // std::optional.
      [](const std::string& value) -> std::optional<std::tuple<base::Value>> {
        auto res = base::JSONReader::Read(value);
        if (!res) {
          return std::nullopt;
        }
        // We use a tuple because the FuzzTest API requires it, since the
        // inverse mapping can map one input value to multiple output values.
        return std::tuple{std::move(*res)};
      },
      fuzztest::Arbitrary<base::Value>());
}

FUZZ_TEST(DevToolsProtocolFuzzer, ConvertsJSONToCBORCorrectly)
    .WithDomains(fuzztest::OneOf(JsonDomain(),
                                 fuzztest::Arbitrary<std::string>().WithSeeds(
                                     []() -> std::vector<std::string> {
                                       auto domain = JsonDomain();
                                       return {domain.GetRandomValue(
                                           base::RandomBitGenerator())};
                                     })));
}  // namespace content
