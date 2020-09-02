// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/web_test/common/web_test_string_util.h"

#include <stddef.h>

#include "base/lazy_instance.h"
#include "base/macros.h"
#include "base/rand_util.h"
#include "third_party/blink/public/web/web_navigation_policy.h"
#include "url/gurl.h"

namespace web_test_string_util {

namespace {

const char web_tests_pattern[] = "/web_tests/";
const std::string::size_type web_tests_pattern_size =
    sizeof(web_tests_pattern) - 1;
const char file_url_pattern[] = "file:/";
const char file_test_prefix[] = "(file test):";
const char data_url_pattern[] = "data:";
const std::string::size_type data_url_pattern_size =
    sizeof(data_url_pattern) - 1;
const char* kPolicyDownload = "download";
const char* kPolicyCurrentTab = "current tab";
const char* kPolicyNewBackgroundTab = "new background tab";
const char* kPolicyNewForegroundTab = "new foreground tab";
const char* kPolicyNewWindow = "new window";
const char* kPolicyNewPopup = "new popup";

}  // namespace

const char* kIllegalString = "illegal value";

std::string NormalizeWebTestURL(const std::string& url) {
  std::string result = url;
  size_t pos;
  if (!url.find(file_url_pattern) &&
      ((pos = url.find(web_tests_pattern)) != std::string::npos)) {
    // adjust file URLs to match upstream results.
    result.replace(0, pos + web_tests_pattern_size, file_test_prefix);
  } else if (!url.find(data_url_pattern)) {
    // URL-escape data URLs to match results upstream.
    std::string path = url.substr(data_url_pattern_size);
    result.replace(data_url_pattern_size, url.length(), path);
  }
  return result;
}

std::string URLDescription(const GURL& url) {
  if (url.SchemeIs(url::kFileScheme))
    return url.ExtractFileName();
  return url.possibly_invalid_spec();
}

const char* WebNavigationPolicyToString(
    const blink::WebNavigationPolicy& policy) {
  switch (policy) {
    case blink::kWebNavigationPolicyDownload:
      return kPolicyDownload;
    case blink::kWebNavigationPolicyCurrentTab:
      return kPolicyCurrentTab;
    case blink::kWebNavigationPolicyNewBackgroundTab:
      return kPolicyNewBackgroundTab;
    case blink::kWebNavigationPolicyNewForegroundTab:
      return kPolicyNewForegroundTab;
    case blink::kWebNavigationPolicyNewWindow:
      return kPolicyNewWindow;
    case blink::kWebNavigationPolicyNewPopup:
      return kPolicyNewPopup;
    default:
      return kIllegalString;
  }
}

blink::WebString V8StringToWebString(v8::Isolate* isolate,
                                     v8::Local<v8::String> v8_str) {
  int length = v8_str->Utf8Length(isolate) + 1;
  std::unique_ptr<char[]> chars(new char[length]);
  v8_str->WriteUtf8(isolate, chars.get(), length);
  return blink::WebString::FromUTF8(chars.get());
}

}  // namespace web_test_string_util
