// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/services/auction_worklet/public/cpp/cbor_test_util.h"

#include <stdint.h>

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/containers/span_writer.h"
#include "base/test/values_test_util.h"
#include "base/values.h"
#include "components/cbor/values.h"
#include "components/cbor/writer.h"

namespace auction_worklet::test {
namespace {

// If `convert_content_to_binary_cbor_string` is true, converts "content" values
// in dictionaries to binary CBOR strings. See documentation of
// ToKVv2ResponseCborString() for more details.
cbor::Value ToCbor(const base::Value& value,
                   bool convert_content_to_binary_cbor_string = false) {
  switch (value.type()) {
    case base::Value::Type::NONE:
      return cbor::Value();
    case base::Value::Type::BOOLEAN:
      return cbor::Value(value.GetBool());
    case base::Value::Type::INTEGER:
      return cbor::Value(value.GetInt());
    case base::Value::Type::DOUBLE:
      return cbor::Value(value.GetDouble());
    case base::Value::Type::STRING:
      return cbor::Value(value.GetString());
    case base::Value::Type::BINARY:
      return cbor::Value(value.GetBlob());
    case base::Value::Type::DICT: {
      cbor::Value::MapValue map;
      for (auto pair : value.GetDict()) {
        if (convert_content_to_binary_cbor_string && pair.first == "content") {
          std::optional<std::vector<uint8_t>> cbor_blob =
              cbor::Writer::Write(ToCbor(pair.second));
          CHECK(cbor_blob);
          map.try_emplace(cbor::Value(pair.first),
                          cbor::Value(std::move(cbor_blob).value()));
          continue;
        }

        map.try_emplace(
            cbor::Value(pair.first),
            ToCbor(pair.second, convert_content_to_binary_cbor_string));
      }
      return cbor::Value(std::move(map));
    }
    case base::Value::Type::LIST: {
      cbor::Value::ArrayValue array;
      for (const auto& entry : value.GetList()) {
        array.emplace_back(
            ToCbor(entry, convert_content_to_binary_cbor_string));
      }
      return cbor::Value(std::move(array));
    }
    default:
      // Use a default since this is test-only code, and to avoid causing issues
      // if a new type is added to base::Value.
      NOTREACHED();
  }
}

}  // namespace

std::vector<uint8_t> ToCborVector(std::string_view json) {
  std::optional<std::vector<uint8_t>> out =
      cbor::Writer::Write(ToCbor(base::test::ParseJson(json)));
  CHECK(out);
  return std::move(out).value();
}

std::string ToCborString(std::string_view json) {
  std::vector<uint8_t> vector = ToCborVector(json);
  return std::string(vector.begin(), vector.end());
}

std::string ToKVv2ResponseCborString(std::string_view json) {
  std::optional<std::vector<uint8_t>> vector = cbor::Writer::Write(
      ToCbor(base::test::ParseJson(json),
             /*convert_content_to_binary_cbor_string=*/true));
  CHECK(vector);
  return std::string(vector->begin(), vector->end());
}

std::string CreateKVv2ResponseBody(std::string_view cbor_response_body,
                                   std::optional<size_t> advertised_cbor_length,
                                   size_t padding_length,
                                   uint8_t compression_scheme) {
  if (!advertised_cbor_length.has_value()) {
    advertised_cbor_length = cbor_response_body.length();
  }

  std::string response_body(5 + cbor_response_body.size() + padding_length, 0);
  base::SpanWriter writer(
      base::as_writable_bytes(base::make_span(response_body)));
  writer.WriteU8BigEndian(compression_scheme);
  writer.WriteU32BigEndian(*advertised_cbor_length);
  writer.Write(base::as_bytes(base::make_span(cbor_response_body)));
  return response_body;
}

}  // namespace auction_worklet::test
