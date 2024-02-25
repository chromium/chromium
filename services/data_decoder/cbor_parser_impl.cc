// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/data_decoder/cbor_parser_impl.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/value_iterators.h"
#include "base/values.h"
#include "components/cbor/reader.h"
#include "components/cbor/values.h"
#include "mojo/public/cpp/base/big_buffer.h"

namespace data_decoder {

namespace {

std::optional<base::Value> ConvertToBaseValue(const cbor::Value& cbor_value) {
  switch (cbor_value.type()) {
    case cbor::Value::Type::UNSIGNED:
    case cbor::Value::Type::NEGATIVE:
      return base::Value(static_cast<int>(cbor_value.GetInteger()));

    case cbor::Value::Type::ARRAY: {
      const std::vector<cbor::Value>& input_array = cbor_value.GetArray();
      base::Value::List output_array;

      for (const auto& el : input_array) {
        std::optional<base::Value> converted_el = ConvertToBaseValue(el);
        if (!converted_el.has_value()) {
          return std::nullopt;
        }
        output_array.Append(*std::move(converted_el));
      }
      return base::Value(std::move(output_array));
    }

    case cbor::Value::Type::MAP: {
      const cbor::Value::MapValue& input_map = cbor_value.GetMap();
      base::Value::Dict output_map;

      for (const auto& el : input_map) {
        std::string key;
        if (el.first.is_string()) {
          key = el.first.GetString();
        } else if (el.first.is_bytestring()) {
          key = el.first.GetBytestringAsString();
        } else {
          // not supporting anything that is not a string or a bytestring at the
          // moment.
          return std::nullopt;
        }

        std::optional<base::Value> converted_value =
            ConvertToBaseValue(el.second);

        if (!converted_value.has_value()) {
          return std::nullopt;
        }
        output_map.Set(key, *std::move(converted_value));
      }

      return base::Value(std::move(output_map));
    }
    case cbor::Value::Type::SIMPLE_VALUE:
      switch (cbor_value.GetSimpleValue()) {
        case cbor::Value::SimpleValue::FALSE_VALUE:
          return base::Value(false);

        case cbor::Value::SimpleValue::TRUE_VALUE:
          return base::Value(true);

        case cbor::Value::SimpleValue::UNDEFINED:
        case cbor::Value::SimpleValue::NULL_VALUE:
          return std::nullopt;
      }

    case cbor::Value::Type::STRING:
      return base::Value(cbor_value.GetString());

    case cbor::Value::Type::BYTE_STRING:
      return base::Value(cbor_value.GetBytestring());

    case cbor::Value::Type::FLOAT_VALUE:
      return base::Value(cbor_value.GetDouble());

    default:
      return std::nullopt;
  }
}

}  // namespace

CborParserImpl::CborParserImpl() = default;

CborParserImpl::~CborParserImpl() = default;

void CborParserImpl::Parse(mojo_base::BigBuffer cbor, ParseCallback callback) {
  cbor::Reader::DecoderError error;
  cbor::Reader::Config config;
  config.error_code_out = &error;
  config.allow_floating_point = true;

  auto ret = cbor::Reader::Read(cbor, config);

  if (!ret.has_value()) {
    std::move(callback).Run(std::nullopt,
                            cbor::Reader::ErrorCodeToString(error));
    return;
  }

  std::optional<::base::Value> temp_value = ConvertToBaseValue(*ret);

  if (temp_value.has_value()) {
    std::move(callback).Run(std::move(*temp_value), std::nullopt);
  } else {
    std::move(callback).Run(std::nullopt, "Error unexpected CBOR value.");
  }
}
}  // namespace data_decoder
