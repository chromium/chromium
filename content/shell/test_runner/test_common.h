// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_TEST_RUNNER_TEST_COMMON_H_
#define CONTENT_SHELL_TEST_RUNNER_TEST_COMMON_H_

#include <string>

#include "base/strings/string_util.h"
#include "content/shell/test_runner/test_runner_export.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/web/web_navigation_policy.h"
#include "v8/include/v8.h"

class GURL;

namespace test_runner {

extern const char* kIllegalString;

inline bool IsASCIIAlpha(char ch) {
  return base::IsAsciiLower(ch | 0x20);
}

inline bool IsNotASCIIAlpha(char ch) {
  return !IsASCIIAlpha(ch);
}

TEST_RUNNER_EXPORT std::string NormalizeLayoutTestURL(const std::string& url);

std::string URLDescription(const GURL& url);
const char* WebNavigationPolicyToString(
    const blink::WebNavigationPolicy& policy);

blink::WebString V8StringToWebString(v8::Isolate* isolate,
                                     v8::Local<v8::String> v8_str);

}  // namespace test_runner

#endif  // CONTENT_SHELL_TEST_RUNNER_TEST_COMMON_H_
