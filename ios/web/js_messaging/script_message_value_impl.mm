// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <variant>

#import "base/notreached.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/web/js_messaging/web_view_js_utils.h"
#import "ios/web/public/js_messaging/script_message_value.h"

namespace web {

ScriptMessageValue::ScriptMessageValue() = default;

ScriptMessageValue::ScriptMessageValue(ScriptMessageValue&&) = default;
ScriptMessageValue& ScriptMessageValue::operator=(ScriptMessageValue&&) =
    default;

ScriptMessageValue::ScriptMessageValue(base::Value value)
    : data_(std::move(value)) {}
ScriptMessageValue::ScriptMessageValue(std::u16string_view value)
    : data_(base::Value(value)) {}
ScriptMessageValue::ScriptMessageValue(double value)
    : data_(base::Value(value)) {}
ScriptMessageValue::ScriptMessageValue(bool value)
    : data_(base::Value(value)) {}
ScriptMessageValue::ScriptMessageValue(ScriptMessageDictValue value)
    : data_(std::move(value)) {}
ScriptMessageValue::ScriptMessageValue(NSDictionary* value)
    : data_(ScriptMessageDictValue(value)) {}
ScriptMessageValue::ScriptMessageValue(ScriptMessageListValue value)
    : data_(std::move(value)) {}
ScriptMessageValue::ScriptMessageValue(NSArray* value)
    : data_(ScriptMessageListValue(value)) {}

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

const base::Value& ScriptMessageValue::GetValue() {
  if (std::holds_alternative<std::monostate>(data_)) {
    data_.emplace<base::Value>(base::Value());
  }

  CHECK(std::holds_alternative<base::Value>(data_));
  return std::get<base::Value>(data_);
}

const ScriptMessageDictValue& ScriptMessageValue::GetDict() const {
  CHECK(type() == base::Value::Type::DICT);
  return std::get<ScriptMessageDictValue>(data_);
}

const ScriptMessageListValue& ScriptMessageValue::GetList() const {
  CHECK(type() == base::Value::Type::LIST);
  return std::get<ScriptMessageListValue>(data_);
}

}  // namespace web
