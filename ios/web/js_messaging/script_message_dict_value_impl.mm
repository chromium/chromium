// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/strings/sys_string_conversions.h"
#import "ios/web/js_messaging/script_message_value_util.h"
#import "ios/web/js_messaging/web_view_js_utils.h"
#import "ios/web/public/js_messaging/script_message_dict_value.h"
#import "ios/web/public/js_messaging/script_message_list_value.h"
#import "ios/web/public/js_messaging/script_message_value.h"

namespace {

// Returns the object associated with `key` in `dict` if the type of that
// object matches `expected_type`. Otherwise returns `nullptr`.
id GetDictElementAndMatchType(NSDictionary* dict,
                              std::string_view key,
                              CFTypeID expected_type) {
  id element = [dict objectForKey:base::SysUTF8ToNSString(key)];
  if (!element || CFGetTypeID((__bridge CFTypeRef)element) != expected_type) {
    return nullptr;
  }

  return element;
}

}  // namespace

namespace web {

ScriptMessageDictValue::ScriptMessageDictValue(ScriptMessageDictValue&&) =
    default;
ScriptMessageDictValue& ScriptMessageDictValue::operator=(
    ScriptMessageDictValue&&) = default;

ScriptMessageDictValue::ScriptMessageDictValue(NSDictionary* value)
    : data_(value) {}

ScriptMessageDictValue::~ScriptMessageDictValue() = default;

bool ScriptMessageDictValue::empty() const {
  return [data_ count] == 0;
}

size_t ScriptMessageDictValue::size() const {
  return [data_ count];
}

bool ScriptMessageDictValue::contains(std::string_view key) const {
  return [data_ objectForKey:base::SysUTF8ToNSString(key)] != nil;
}

std::unique_ptr<ScriptMessageValue> ScriptMessageDictValue::Find(
    std::string_view key) {
  id element = [data_ objectForKey:base::SysUTF8ToNSString(key)];
  if (element == nil) {
    return nullptr;
  }

  return CreateScriptMessageValue(element);
}

std::optional<bool> ScriptMessageDictValue::FindBool(
    std::string_view key) const {
  id element = GetDictElementAndMatchType(data_, key, CFBooleanGetTypeID());
  if (!element) {
    return std::nullopt;
  }
  return [element boolValue];
}

std::optional<int> ScriptMessageDictValue::FindInt(std::string_view key) const {
  id element = GetDictElementAndMatchType(data_, key, CFNumberGetTypeID());
  if (!element) {
    return std::nullopt;
  }
  return [element intValue];
}

std::optional<double> ScriptMessageDictValue::FindDouble(
    std::string_view key) const {
  id element = GetDictElementAndMatchType(data_, key, CFNumberGetTypeID());
  if (!element) {
    return std::nullopt;
  }
  return [element doubleValue];
}

std::optional<std::string> ScriptMessageDictValue::FindString(
    std::string_view key) const {
  id element = GetDictElementAndMatchType(data_, key, CFStringGetTypeID());
  if (!element) {
    return std::nullopt;
  }
  return base::SysNSStringToUTF8(element);
}

std::unique_ptr<ScriptMessageDictValue> ScriptMessageDictValue::FindDict(
    std::string_view key) const {
  id element = GetDictElementAndMatchType(data_, key, CFDictionaryGetTypeID());
  if (!element) {
    return nullptr;
  }
  return std::make_unique<ScriptMessageDictValue>((NSDictionary*)element);
}

std::unique_ptr<ScriptMessageListValue> ScriptMessageDictValue::FindList(
    std::string_view key) const {
  id element = GetDictElementAndMatchType(data_, key, CFArrayGetTypeID());
  if (!element) {
    return nullptr;
  }
  return std::make_unique<ScriptMessageListValue>((NSArray*)element);
}

}  // namespace web
