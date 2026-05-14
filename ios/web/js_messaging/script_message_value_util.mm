// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/js_messaging/script_message_value_util.h"

#import "base/notreached.h"
#import "base/strings/sys_string_conversions.h"
#import "base/values.h"
#import "ios/web/public/js_messaging/script_message_value.h"

namespace web {

std::unique_ptr<ScriptMessageValue> CreateScriptMessageValue(id element) {
  CFTypeID type_id = CFGetTypeID((__bridge CFTypeRef)element);
  if (type_id == CFStringGetTypeID()) {
    return std::make_unique<ScriptMessageValue>(
        base::SysNSStringToUTF16(element));
  }
  if (type_id == CFNumberGetTypeID()) {
    if (CFNumberGetType((CFNumberRef)element) == kCFNumberIntType) {
      return std::make_unique<ScriptMessageValue>(
          base::Value([element intValue]));
    }

    return std::make_unique<ScriptMessageValue>([element doubleValue]);
  }
  if (type_id == CFBooleanGetTypeID()) {
    return std::make_unique<ScriptMessageValue>([element boolValue]);
  }
  if (type_id == CFDictionaryGetTypeID()) {
    return std::make_unique<ScriptMessageValue>((NSDictionary*)element);
  }
  NOTREACHED();
}

}  // namespace web
