/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/platform/json/json_values.h"

#include <algorithm>
#include <cmath>

#include "third_party/blink/renderer/platform/wtf/decimal.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

namespace {

inline bool EscapeChar(UChar c, StringBuilder* dst) {
  switch (c) {
    case '\b':
      dst->Append("\\b");
      break;
    case '\f':
      dst->Append("\\f");
      break;
    case '\n':
      dst->Append("\\n");
      break;
    case '\r':
      dst->Append("\\r");
      break;
    case '\t':
      dst->Append("\\t");
      break;
    case '\\':
      dst->Append("\\\\");
      break;
    case '"':
      dst->Append("\\\"");
      break;
    default:
      return false;
  }
  return true;
}

const LChar kHexDigits[17] = "0123456789ABCDEF";

void AppendUnsignedAsHex(UChar number, StringBuilder* dst) {
  dst->Append("\\u");
  for (size_t i = 0; i < 4; ++i) {
    dst->Append(kHexDigits[(number & 0xF000) >> 12]);
    number <<= 4;
  }
}

void WriteIndent(int depth, StringBuilder* output) {
  for (int i = 0; i < depth; ++i)
    output->Append("  ");
}

}  // anonymous namespace

const char kJSONNullString[] = "null";
const char kJSONTrueString[] = "true";
const char kJSONFalseString[] = "false";

void EscapeStringForJSON(const String& str, StringBuilder* dst) {
  for (unsigned i = 0; i < str.length(); ++i) {
    UChar c = str[i];
    if (!EscapeChar(c, dst)) {
      if (c < 32 || c > 126 || c == '<' || c == '>') {
        // 1. Escaping <, > to prevent script execution.
        // 2. Technically, we could also pass through c > 126 as UTF8, but this
        //    is also optional. It would also be a pain to implement here.
        AppendUnsignedAsHex(c, dst);
      } else {
        dst->Append(c);
      }
    }
  }
}

void DoubleQuoteStringForJSON(const String& str, StringBuilder* dst) {
  dst->Append('"');
  EscapeStringForJSON(str, dst);
  dst->Append('"');
}

String JSONValue::QuoteString(const String& input) {
  StringBuilder builder;
  DoubleQuoteStringForJSON(input, &builder);
  return builder.ToString();
}

bool JSONValue::AsBoolean(bool*) const {
  return false;
}

bool JSONValue::AsDouble(double*) const {
  return false;
}

bool JSONValue::AsInteger(int*) const {
  return false;
}

bool JSONValue::AsString(String*) const {
  return false;
}

String JSONValue::ToJSONString() const {
  StringBuilder result;
  result.ReserveCapacity(512);
  WriteJSON(&result);
  return result.ToString();
}

String JSONValue::ToPrettyJSONString() const {
  StringBuilder result;
  result.ReserveCapacity(512);
  PrettyWriteJSON(&result);
  return result.ToString();
}

void JSONValue::WriteJSON(StringBuilder* output) const {
  DCHECK(type_ == kTypeNull);
  output->Append(kJSONNullString, 4);
}

void JSONValue::PrettyWriteJSON(StringBuilder* output) const {
  PrettyWriteJSONInternal(output, 0);
  output->Append('\n');
}

void JSONValue::PrettyWriteJSONInternal(StringBuilder* output,
                                        int depth) const {
  WriteJSON(output);
}

std::unique_ptr<JSONValue> JSONValue::Clone() const {
  return JSONValue::Null();
}

bool JSONBasicValue::AsBoolean(bool* output) const {
  if (GetType() != kTypeBoolean)
    return false;
  *output = bool_value_;
  return true;
}

bool JSONBasicValue::AsDouble(double* output) const {
  if (GetType() == kTypeDouble) {
    *output = double_value_;
    return true;
  }
  if (GetType() == kTypeInteger) {
    *output = integer_value_;
    return true;
  }
  return false;
}

bool JSONBasicValue::AsInteger(int* output) const {
  if (GetType() != kTypeInteger)
    return false;
  *output = integer_value_;
  return true;
}

void JSONBasicValue::WriteJSON(StringBuilder* output) const {
  DCHECK(GetType() == kTypeBoolean || GetType() == kTypeInteger ||
         GetType() == kTypeDouble);
  if (GetType() == kTypeBoolean) {
    if (bool_value_)
      output->Append(kJSONTrueString, 4);
    else
      output->Append(kJSONFalseString, 5);
  } else if (GetType() == kTypeDouble) {
    if (!std::isfinite(double_value_)) {
      output->Append(kJSONNullString, 4);
      return;
    }
    output->Append(Decimal::FromDouble(double_value_).ToString());
  } else if (GetType() == kTypeInteger) {
    output->Append(String::Number(integer_value_));
  }
}

std::unique_ptr<JSONValue> JSONBasicValue::Clone() const {
  switch (GetType()) {
    case kTypeDouble:
      return std::make_unique<JSONBasicValue>(double_value_);
    case kTypeInteger:
      return std::make_unique<JSONBasicValue>(integer_value_);
    case kTypeBoolean:
      return std::make_unique<JSONBasicValue>(bool_value_);
    default:
      NOTREACHED();
  }
  return nullptr;
}

bool JSONString::AsString(String* output) const {
  *output = string_value_;
  return true;
}

void JSONString::WriteJSON(StringBuilder* output) const {
  DCHECK(GetType() == kTypeString);
  DoubleQuoteStringForJSON(string_value_, output);
}

std::unique_ptr<JSONValue> JSONString::Clone() const {
  return std::make_unique<JSONString>(string_value_);
}

JSONObject::~JSONObject() = default;

void JSONObject::SetBoolean(const String& name, bool value) {
  SetValue(name, std::make_unique<JSONBasicValue>(value));
}

void JSONObject::SetInteger(const String& name, int value) {
  SetValue(name, std::make_unique<JSONBasicValue>(value));
}

void JSONObject::SetDouble(const String& name, double value) {
  SetValue(name, std::make_unique<JSONBasicValue>(value));
}

void JSONObject::SetString(const String& name, const String& value) {
  SetValue(name, std::make_unique<JSONString>(value));
}

void JSONObject::SetValue(const String& name,
                          std::unique_ptr<JSONValue> value) {
  Set(name, value);
}

void JSONObject::SetObject(const String& name,
                           std::unique_ptr<JSONObject> value) {
  Set(name, value);
}

void JSONObject::SetArray(const String& name,
                          std::unique_ptr<JSONArray> value) {
  Set(name, value);
}

bool JSONObject::GetBoolean(const String& name, bool* output) const {
  JSONValue* value = Get(name);
  if (!value)
    return false;
  return value->AsBoolean(output);
}

bool JSONObject::GetInteger(const String& name, int* output) const {
  JSONValue* value = Get(name);
  if (!value)
    return false;
  return value->AsInteger(output);
}

bool JSONObject::GetDouble(const String& name, double* output) const {
  JSONValue* value = Get(name);
  if (!value)
    return false;
  return value->AsDouble(output);
}

bool JSONObject::GetString(const String& name, String* output) const {
  JSONValue* value = Get(name);
  if (!value)
    return false;
  return value->AsString(output);
}

JSONObject* JSONObject::GetJSONObject(const String& name) const {
  return JSONObject::Cast(Get(name));
}

JSONArray* JSONObject::GetArray(const String& name) const {
  return JSONArray::Cast(Get(name));
}

JSONValue* JSONObject::Get(const String& name) const {
  Dictionary::const_iterator it = data_.find(name);
  if (it == data_.end())
    return nullptr;
  return it->value.get();
}

JSONObject::Entry JSONObject::at(wtf_size_t index) const {
  const String key = order_[index];
  return std::make_pair(key, data_.find(key)->value.get());
}

bool JSONObject::BooleanProperty(const String& name, bool default_value) const {
  bool result = default_value;
  GetBoolean(name, &result);
  return result;
}

int JSONObject::IntegerProperty(const String& name, int default_value) const {
  int result = default_value;
  GetInteger(name, &result);
  return result;
}

double JSONObject::DoubleProperty(const String& name,
                                  double default_value) const {
  double result = default_value;
  GetDouble(name, &result);
  return result;
}

void JSONObject::Remove(const String& name) {
  data_.erase(name);
  for (wtf_size_t i = 0; i < order_.size(); ++i) {
    if (order_[i] == name) {
      order_.EraseAt(i);
      break;
    }
  }
}

void JSONObject::WriteJSON(StringBuilder* output) const {
  output->Append('{');
  for (wtf_size_t i = 0; i < order_.size(); ++i) {
    Dictionary::const_iterator it = data_.find(order_[i]);
    CHECK(it != data_.end());
    if (i)
      output->Append(',');
    DoubleQuoteStringForJSON(it->key, output);
    output->Append(':');
    it->value->WriteJSON(output);
  }
  output->Append('}');
}

void JSONObject::PrettyWriteJSONInternal(StringBuilder* output,
                                         int depth) const {
  output->Append("{\n");
  for (wtf_size_t i = 0; i < order_.size(); ++i) {
    Dictionary::const_iterator it = data_.find(order_[i]);
    CHECK(it != data_.end());
    if (i)
      output->Append(",\n");
    WriteIndent(depth + 1, output);
    DoubleQuoteStringForJSON(it->key, output);
    output->Append(": ");
    it->value->PrettyWriteJSONInternal(output, depth + 1);
  }
  output->Append('\n');
  WriteIndent(depth, output);
  output->Append('}');
}

std::unique_ptr<JSONValue> JSONObject::Clone() const {
  auto result = std::make_unique<JSONObject>();
  for (const String& key : order_) {
    Dictionary::const_iterator value = data_.find(key);
    DCHECK(value != data_.end() && value->value);
    result->SetValue(key, value->value->Clone());
  }
  return std::move(result);
}

JSONObject::JSONObject() : JSONValue(kTypeObject), data_(), order_() {}

JSONArray::~JSONArray() = default;

void JSONArray::WriteJSON(StringBuilder* output) const {
  output->Append('[');
  bool first = true;
  for (const std::unique_ptr<JSONValue>& value : data_) {
    if (!first)
      output->Append(',');
    value->WriteJSON(output);
    first = false;
  }
  output->Append(']');
}

void JSONArray::PrettyWriteJSONInternal(StringBuilder* output,
                                        int depth) const {
  output->Append('[');
  bool first = true;
  bool last_inserted_new_line = false;
  for (const std::unique_ptr<JSONValue>& value : data_) {
    bool insert_new_line = value->GetType() == JSONValue::kTypeObject ||
                           value->GetType() == JSONValue::kTypeArray ||
                           value->GetType() == JSONValue::kTypeString;
    if (first) {
      if (insert_new_line) {
        output->Append('\n');
        WriteIndent(depth + 1, output);
      }
      first = false;
    } else {
      output->Append(',');
      if (last_inserted_new_line) {
        output->Append('\n');
        WriteIndent(depth + 1, output);
      } else {
        output->Append(' ');
      }
    }
    value->PrettyWriteJSONInternal(output, depth + 1);
    last_inserted_new_line = insert_new_line;
  }
  if (last_inserted_new_line) {
    output->Append('\n');
    WriteIndent(depth, output);
  }
  output->Append(']');
}

std::unique_ptr<JSONValue> JSONArray::Clone() const {
  auto result = std::make_unique<JSONArray>();
  for (const std::unique_ptr<JSONValue>& value : data_)
    result->PushValue(value->Clone());
  return std::move(result);
}

JSONArray::JSONArray() : JSONValue(kTypeArray) {}

void JSONArray::PushBoolean(bool value) {
  data_.push_back(std::make_unique<JSONBasicValue>(value));
}

void JSONArray::PushInteger(int value) {
  data_.push_back(std::make_unique<JSONBasicValue>(value));
}

void JSONArray::PushDouble(double value) {
  data_.push_back(std::make_unique<JSONBasicValue>(value));
}

void JSONArray::PushString(const String& value) {
  data_.push_back(std::make_unique<JSONString>(value));
}

void JSONArray::PushValue(std::unique_ptr<JSONValue> value) {
  DCHECK(value);
  data_.push_back(std::move(value));
}

void JSONArray::PushObject(std::unique_ptr<JSONObject> value) {
  DCHECK(value);
  data_.push_back(std::move(value));
}

void JSONArray::PushArray(std::unique_ptr<JSONArray> value) {
  DCHECK(value);
  data_.push_back(std::move(value));
}

JSONValue* JSONArray::at(wtf_size_t index) const {
  DCHECK_LT(index, data_.size());
  return data_[index].get();
}

}  // namespace blink
