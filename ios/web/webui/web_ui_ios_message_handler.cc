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

bool WebUIIOSMessageHandler::ExtractDoubleValue(const base::ListValue* value,
                                                double* out_value) {
  std::string string_value;
  if (value->GetString(0, &string_value))
    return base::StringToDouble(string_value, out_value);
  if (value->GetDouble(0, out_value))
    return true;
  NOTREACHED();
  return false;
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
