// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/web_test/common/web_test_string_util.h"

#include <stddef.h>

#include <string_view>

#include "base/containers/heap_array.h"
#include "base/lazy_instance.h"
#include "base/rand_util.h"
#include "base/strings/string_util.h"
#include "third_party/blink/public/web/web_navigation_policy.h"
#include "url/gurl.h"

namespace web_test_string_util {

namespace {

constexpr std::string_view kWebTestsPattern = "/web_tests/";
constexpr std::string_view kFileURLPattern = "file://";
const char* kFileTestPrefix = "(file test):";
const char* kPolicyDownload = "download";
const char* kPolicyCurrentTab = "current tab";
const char* kPolicyNewBackgroundTab = "new background tab";
const char* kPolicyNewForegroundTab = "new foreground tab";
const char* kPolicyNewWindow = "new window";
const char* kPolicyNewPopup = "new popup";
const char* kPolicyPictureInPicture = "picture in picture";

}  // namespace

const char* kIllegalString = "illegal value";

std::string NormalizeWebTestURLForTextOutput(const std::string& url) {
  std::string result = url;
  if (base::StartsWith(url, kFileURLPattern)) {
    // Adjust the file URL by removing the part depending on the testing
    // environment.
    size_t pos = std::string_view(url).find(kWebTestsPattern);
    if (pos != std::string::npos)
      result.replace(0, pos + kWebTestsPattern.size(), kFileTestPrefix);
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
    case blink::kWebNavigationPolicyPictureInPicture:
      return kPolicyPictureInPicture;
    default:
      return kIllegalString;
  }
}

const char* WindowOpenDispositionToString(WindowOpenDisposition disposition) {
  switch (disposition) {
    case WindowOpenDisposition::SAVE_TO_DISK:
      return kPolicyDownload;
    case WindowOpenDisposition::CURRENT_TAB:
      return kPolicyCurrentTab;
    case WindowOpenDisposition::NEW_BACKGROUND_TAB:
      return kPolicyNewBackgroundTab;
    case WindowOpenDisposition::NEW_FOREGROUND_TAB:
      return kPolicyNewForegroundTab;
    case WindowOpenDisposition::NEW_WINDOW:
      return kPolicyNewWindow;
    case WindowOpenDisposition::NEW_POPUP:
      return kPolicyNewPopup;
    default:
      return kIllegalString;
  }
}

blink::WebString V8StringToWebString(v8::Isolate* isolate,
                                     v8::Local<v8::String> v8_str) {
  int length = v8_str->Utf8Length(isolate) + 1;
  auto chars = base::HeapArray<char>::WithSize(length);
  v8_str->WriteUtf8(isolate, chars.data(), chars.size());
  return blink::WebString::FromUTF8(chars.data());
}

}  // namespace web_test_string_util
