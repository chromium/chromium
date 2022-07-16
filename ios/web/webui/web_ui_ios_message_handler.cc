// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/public/webui/web_ui_ios_message_handler.h"

#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"

namespace web {

bool WebUIIOSMessageHandler::ExtractIntegerValue(const base::ListValue* value,
                                                 int* out_int) {
  const base::Value& single_element = value->GetList()[0];
  absl::optional<double> double_value = single_element.GetIfDouble();
  if (double_value) {
    *out_int = static_cast<int>(*double_value);
    return true;
  }

  return base::StringToInt(single_element.GetString(), out_int);
}

bool WebUIIOSMessageHandler::ExtractDoubleValue(const base::ListValue* value,
                                                double* out_value) {
  const base::Value& single_element = value->GetList()[0];
  absl::optional<double> double_value = single_element.GetIfDouble();
  if (double_value) {
    *out_value = *double_value;
    return true;
  }

  return base::StringToDouble(single_element.GetString(), out_value);
}

std::u16string WebUIIOSMessageHandler::ExtractStringValue(
    const base::ListValue* value) {
  std::u16string string16_value;
  if (value->GetString(0, &string16_value))
    return string16_value;
  NOTREACHED();
  return std::u16string();
}

}  // namespace web
