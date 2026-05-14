// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_JS_MESSAGING_SCRIPT_MESSAGE_VALUE_UTIL_H_
#define IOS_WEB_JS_MESSAGING_SCRIPT_MESSAGE_VALUE_UTIL_H_

#include "ios/web/public/js_messaging/script_message_value.h"

namespace web {

// Creates a ScriptMessageValue from an Objective-C object.
std::unique_ptr<ScriptMessageValue> CreateScriptMessageValue(id element);

}  // namespace web

#endif  // IOS_WEB_JS_MESSAGING_SCRIPT_MESSAGE_VALUE_UTIL_H_
