// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/js_messaging/script_message_value_util.h"
#import "ios/web/public/js_messaging/script_message_list_value.h"
#import "ios/web/public/js_messaging/script_message_value.h"

namespace web {

ScriptMessageListValue::ScriptMessageListValue(ScriptMessageListValue&&) =
    default;
ScriptMessageListValue& ScriptMessageListValue::operator=(
    ScriptMessageListValue&&) = default;

ScriptMessageListValue::ScriptMessageListValue(NSArray* value) : data_(value) {}

ScriptMessageListValue::~ScriptMessageListValue() = default;

bool ScriptMessageListValue::Empty() const {
  return [data_ count] == 0;
}

size_t ScriptMessageListValue::Size() const {
  return [data_ count];
}

std::unique_ptr<ScriptMessageValue> ScriptMessageListValue::Front() const {
  return CreateScriptMessageValue([data_ firstObject]);
}

std::unique_ptr<ScriptMessageValue> ScriptMessageListValue::Back() const {
  return CreateScriptMessageValue([data_ lastObject]);
}

}  // namespace web
