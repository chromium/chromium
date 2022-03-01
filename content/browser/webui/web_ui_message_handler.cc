// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/web_ui_message_handler.h"

#include "base/check.h"
#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace content {

void WebUIMessageHandler::AllowJavascriptForTesting() {
  AllowJavascript();
}

void WebUIMessageHandler::AllowJavascript() {
  if (javascript_allowed_)
    return;

  javascript_allowed_ = true;
  CHECK(IsJavascriptAllowed());

  OnJavascriptAllowed();
}

void WebUIMessageHandler::DisallowJavascript() {
  if (!javascript_allowed_)
    return;

  javascript_allowed_ = false;
  DCHECK(!IsJavascriptAllowed());

  OnJavascriptDisallowed();
}

bool WebUIMessageHandler::IsJavascriptAllowed() {
  return javascript_allowed_ && web_ui() && web_ui()->CanCallJavascript();
}

bool WebUIMessageHandler::ExtractIntegerValue(const base::ListValue* value,
                                              int* out_int) {
  return WebUIMessageHandler::ExtractIntegerValue(value->GetList(), out_int);
}

bool WebUIMessageHandler::ExtractIntegerValue(const base::Value::List& list,
                                              int* out_int) {
  const base::Value& single_element = list[0];
  absl::optional<double> double_value = single_element.GetIfDouble();
  if (double_value) {
    *out_int = static_cast<int>(*double_value);
    return true;
  }

  return base::StringToInt(single_element.GetString(), out_int);
}

bool WebUIMessageHandler::ExtractDoubleValue(const base::ListValue* value,
                                             double* out_value) {
  return WebUIMessageHandler::ExtractDoubleValue(value->GetList(), out_value);
}

bool WebUIMessageHandler::ExtractDoubleValue(const base::Value::List& list,
                                             double* out_value) {
  const base::Value& single_element = list[0];
  absl::optional<double> double_value = single_element.GetIfDouble();
  if (double_value) {
    *out_value = *double_value;
    return true;
  }

  return base::StringToDouble(single_element.GetString(), out_value);
}

std::u16string WebUIMessageHandler::ExtractStringValue(
    const base::ListValue* value) {
  return WebUIMessageHandler::ExtractStringValue(value->GetList());
}

std::u16string WebUIMessageHandler::ExtractStringValue(
    const base::Value::List& list) {
  if (0u < list.size() && list[0].is_string())
    return base::UTF8ToUTF16(list[0].GetString());

  NOTREACHED();
  return std::u16string();
}

void WebUIMessageHandler::ResolveJavascriptCallback(
    const base::Value& callback_id,
    const base::Value& response) {
  // cr.webUIResponse is a global JS function exposed from cr.js.
  CallJavascriptFunction("cr.webUIResponse", callback_id, base::Value(true),
                         response);
}

void WebUIMessageHandler::RejectJavascriptCallback(
    const base::Value& callback_id,
    const base::Value& response) {
  // cr.webUIResponse is a global JS function exposed from cr.js.
  CallJavascriptFunction("cr.webUIResponse", callback_id, base::Value(false),
                         response);
}

}  // namespace content
