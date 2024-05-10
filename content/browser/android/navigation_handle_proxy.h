// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ANDROID_NAVIGATION_HANDLE_PROXY_H_
#define CONTENT_BROWSER_ANDROID_NAVIGATION_HANDLE_PROXY_H_

#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/memory/raw_ptr.h"
#include "net/http/http_request_headers.h"

namespace content {

class NavigationHandle;

// JNI Bridge in between:
// - [C++] NavigationHandle
// - [Java] NavigationHandle
class NavigationHandleProxy final {
 public:
  explicit NavigationHandleProxy(NavigationHandle* cpp_navigation_handle);
  ~NavigationHandleProxy();

  const base::android::JavaRef<jobject>& java_navigation_handle() const {
    return java_navigation_handle_;
  }

  // |DidStart|, |DidRedirect| and |DidFinish| update the NavigationHandle on
  // the java side with the state from the C++ side.
  void DidStart();
  void DidRedirect();
  void DidFinish();

 private:
  std::string GetMimeType() const;

  base::android::ScopedJavaGlobalRef<jobject> java_navigation_handle_;
  raw_ptr<NavigationHandle> cpp_navigation_handle_ = nullptr;
};

}  // namespace content

#endif  // CONTENT_BROWSER_ANDROID_NAVIGATION_HANDLE_PROXY_H_
