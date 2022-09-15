// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/libfuzzer/proto/json_proto_converter.h"

namespace json_proto {

void JsonProtoConverter::AppendArray(const ArrayValue& array_value) {
  data_ << '[';
  bool need_comma = false;
  for (const auto& value : array_value.value()) {
    // Trailing comma inside of an array makes JSON invalid, avoid adding that.
    if (need_comma)
      data_ << ',';
    else
      need_comma = true;

    AppendValue(value);
  }
  data_ << ']';
}

void JsonProtoConverter::AppendNumber(const NumberValue& number_value) {
  if (number_value.has_float_value()) {
    data_ << number_value.float_value().value();
  } else if (number_value.has_exponent_value()) {
    auto value = number_value.exponent_value();
    data_ << value.base();
    data_ << (value.use_uppercase() ? 'E' : 'e');
    data_ << value.exponent();
  } else if (number_value.has_exponent_frac_value()) {
    auto value = number_value.exponent_value();
    data_ << value.base();
    data_ << (value.use_uppercase() ? 'E' : 'e');
    data_ << value.exponent();
  } else {
    data_ << number_value.integer_value().value();
  }
}

void JsonProtoConverter::AppendObject(const JsonObject& json_object) {
  data_ << '{';
  bool leading_comma = false;
  for (const auto& field : json_object.field()) {
    if (leading_comma) {
      data_ << ",";
    }
    leading_comma = true;
    data_ << '"' << field.name() << '"' << ':';
    AppendValue(field.value());
  }
  data_ << '}';
}

void JsonProtoConverter::AppendValue(const JsonValue& json_value) {
  if (json_value.has_object_value()) {
    AppendObject(json_value.object_value());
  } else if (json_value.has_array_value()) {
    AppendArray(json_value.array_value());
  } else if (json_value.has_number_value()) {
    AppendNumber(json_value.number_value());
  } else if (json_value.has_string_value()) {
    data_ << '"' << json_value.string_value().value() << '"';
  } else if (json_value.has_boolean_value()) {
    data_ << (json_value.boolean_value().value() ? "true" : "false");
  } else {
    data_ << "null";
  }
}

std::string JsonProtoConverter::Convert(const JsonValue& json_value) {
  AppendValue(json_value);
  return data_.str();
}

std::string JsonProtoConverter::Convert(const JsonObject& json_object) {
  AppendObject(json_object);
  return data_.str();
}

std::string JsonProtoConverter::Convert(
    const json_proto::ArrayValue& json_array) {
  AppendArray(json_array);
  return data_.str();
}

}  // namespace json_proto
