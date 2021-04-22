// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/web_ui_message_handler.h"

#include "base/check.h"
#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"

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
  std::string string_value;
  if (value->GetString(0, &string_value))
    return base::StringToInt(string_value, out_int);
  double double_value;
  if (value->GetDouble(0, &double_value)) {
    *out_int = static_cast<int>(double_value);
    return true;
  }
  NOTREACHED();
  return false;
}

bool WebUIMessageHandler::ExtractDoubleValue(const base::ListValue* value,
                                             double* out_value) {
  std::string string_value;
  if (value->GetString(0, &string_value))
    return base::StringToDouble(string_value, out_value);
  if (value->GetDouble(0, out_value))
    return true;
  NOTREACHED();
  return false;
}

std::u16string WebUIMessageHandler::ExtractStringValue(
    const base::ListValue* value) {
  std::u16string string16_value;
  if (value->GetString(0, &string16_value))
    return string16_value;
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
