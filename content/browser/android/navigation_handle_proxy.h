// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ANDROID_NAVIGATION_HANDLE_PROXY_H_
#define CONTENT_BROWSER_ANDROID_NAVIGATION_HANDLE_PROXY_H_

#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "net/http/http_request_headers.h"

#include <string>
#include <vector>

namespace content {

class NavigationHandle;

// JNI Bridge in between:
// - [C++] NavigationHandle
// - [Java] NavigationHandle
class NavigationHandleProxy final {
 public:
  explicit NavigationHandleProxy(NavigationHandle* cpp_navigation_handle);
  ~NavigationHandleProxy();

  NavigationHandle* cpp_navigation_handle() const {
    return cpp_navigation_handle_;
  }
  base::android::ScopedJavaGlobalRef<jobject> java_navigation_handle() const {
    return java_navigation_handle_;
  }

  // |DidRedirect| and |DidFinish| updates the NavigationHandle on the java side
  // with the state from the C++ side.
  void DidRedirect();
  void DidFinish();

  // Called from Java.
  void SetRequestHeader(JNIEnv* env,
                        const base::android::JavaParamRef<jstring>& name,
                        const base::android::JavaParamRef<jstring>& value);

  // Called from Java.
  void RemoveRequestHeader(JNIEnv* env,
                           const base::android::JavaParamRef<jstring>& name);

 private:
  base::android::ScopedJavaGlobalRef<jobject> java_navigation_handle_;
  NavigationHandle* cpp_navigation_handle_ = nullptr;
};

}  // namespace content

#endif  // CONTENT_BROWSER_ANDROID_NAVIGATION_HANDLE_PROXY_H_
