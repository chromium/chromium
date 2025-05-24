// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ANDROID_PAGE_PROXY_H_
#define CONTENT_BROWSER_ANDROID_PAGE_PROXY_H_

#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/memory/raw_ptr.h"
#include "net/http/http_request_headers.h"

namespace content {

class PageImpl;

// JNI Bridge in between:
// - [C++] Page
// - [Java] Page
class PageProxy final {
 public:
  explicit PageProxy(PageImpl* cpp_page);
  ~PageProxy();

  void WillDeletePage(bool is_prerendering);

  const base::android::JavaRef<jobject>& java_page() const {
    return java_page_;
  }

 private:
  base::android::ScopedJavaGlobalRef<jobject> java_page_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_ANDROID_PAGE_PROXY_H_
