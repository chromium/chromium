// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <variant>

#import "base/check.h"
#import "base/check_op.h"
#import "base/no_destructor.h"
#import "base/notreached.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/web/js_messaging/web_view_js_utils.h"
#import "ios/web/public/js_messaging/script_message_value.h"

namespace web {

ScriptMessageValue::ScriptMessageValue() = default;

ScriptMessageValue::ScriptMessageValue(ScriptMessageValue&&) = default;
ScriptMessageValue& ScriptMessageValue::operator=(ScriptMessageValue&&) =
    default;

ScriptMessageValue::ScriptMessageValue(std::string&& value)
    : data_(base::Value(std::move(value))) {}
ScriptMessageValue::ScriptMessageValue(std::string_view value)
    : data_(base::Value(value)) {}
ScriptMessageValue::ScriptMessageValue(std::u16string_view value)
    : data_(base::Value(value)) {}
ScriptMessageValue::ScriptMessageValue(int value) : data_(base::Value(value)) {}
ScriptMessageValue::ScriptMessageValue(double value)
    : data_(base::Value(value)) {}
ScriptMessageValue::ScriptMessageValue(bool value)
    : data_(base::Value(value)) {}
ScriptMessageValue::ScriptMessageValue(NSDictionary* value)
    : ScriptMessageValue(ScriptMessageDictValue(value)) {}
ScriptMessageValue::ScriptMessageValue(ScriptMessageDictValue value)
    : data_(std::move(value)) {}
ScriptMessageValue::ScriptMessageValue(NSArray* value)
    : ScriptMessageValue(ScriptMessageListValue(value)) {}
ScriptMessageValue::ScriptMessageValue(ScriptMessageListValue value)
    : data_(std::move(value)) {}

ScriptMessageValue::~ScriptMessageValue() = default;

base::Value::Type ScriptMessageValue::type() const {
  if (std::holds_alternative<std::monostate>(data_)) {
    return base::Value::Type::NONE;
  }

  if (std::holds_alternative<base::Value>(data_)) {
    return std::get<base::Value>(data_).type();
  } else if (std::holds_alternative<ScriptMessageDictValue>(data_)) {
    return base::Value::Type::DICT;
  } else if (std::holds_alternative<ScriptMessageListValue>(data_)) {
    return base::Value::Type::LIST;
  }

  NOTREACHED();
}

const base::Value& ScriptMessageValue::GetValue() const {
  if (std::holds_alternative<std::monostate>(data_)) {
    static const base::NoDestructor<base::Value> kNoneValue;
    return *kNoneValue;
  }

  CHECK(std::holds_alternative<base::Value>(data_));
  return std::get<base::Value>(data_);
}

const ScriptMessageDictValue& ScriptMessageValue::GetDict() const {
  CHECK(std::holds_alternative<ScriptMessageDictValue>(data_));
  return std::get<ScriptMessageDictValue>(data_);
}

const ScriptMessageListValue& ScriptMessageValue::GetList() const {
  CHECK(std::holds_alternative<ScriptMessageListValue>(data_));
  return std::get<ScriptMessageListValue>(data_);
}

std::unique_ptr<base::Value> ScriptMessageValue::ConvertToValue() const {
  switch (type()) {
    case base::Value::Type::NONE:
      return std::make_unique<base::Value>(base::Value());
    case base::Value::Type::DICT:
      return ValueResultFromWKResult(GetDict().data());
    case base::Value::Type::LIST:
      return ValueResultFromWKResult(GetList().data());
    default:
      return std::make_unique<base::Value>(GetValue().Clone());
  }
}

}  // namespace web
