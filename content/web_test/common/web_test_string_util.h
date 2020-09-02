// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_WEB_TEST_COMMON_WEB_TEST_STRING_UTIL_H_
#define CONTENT_WEB_TEST_COMMON_WEB_TEST_STRING_UTIL_H_

#include <string>

#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/web/web_navigation_policy.h"
#include "v8/include/v8.h"

class GURL;

namespace web_test_string_util {

extern const char* kIllegalString;

std::string NormalizeWebTestURL(const std::string& url);

std::string URLDescription(const GURL& url);
const char* WebNavigationPolicyToString(
    const blink::WebNavigationPolicy& policy);

blink::WebString V8StringToWebString(v8::Isolate* isolate,
                                     v8::Local<v8::String> v8_str);

}  // namespace web_test_string_util

#endif  // CONTENT_WEB_TEST_COMMON_WEB_TEST_STRING_UTIL_H_
