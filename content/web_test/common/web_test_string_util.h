// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_WEB_TEST_COMMON_WEB_TEST_STRING_UTIL_H_
#define CONTENT_WEB_TEST_COMMON_WEB_TEST_STRING_UTIL_H_

#include <string>

#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/web/web_navigation_policy.h"
#include "ui/base/window_open_disposition.h"
#include "v8/include/v8.h"

class GURL;

namespace web_test_string_util {

extern const char* kIllegalString;

// Converts a web test url into a string that is invariant with the testing
// environment (e.g. the absolute file path of the chrome repository), called
// when the url will be output in the text result of a test.
std::string NormalizeWebTestURLForTextOutput(const std::string& url);

std::string URLDescription(const GURL& url);
const char* WebNavigationPolicyToString(
    const blink::WebNavigationPolicy& policy);
const char* WindowOpenDispositionToString(WindowOpenDisposition disposition);

blink::WebString V8StringToWebString(v8::Isolate* isolate,
                                     v8::Local<v8::String> v8_str);

}  // namespace web_test_string_util

#endif  // CONTENT_WEB_TEST_COMMON_WEB_TEST_STRING_UTIL_H_
