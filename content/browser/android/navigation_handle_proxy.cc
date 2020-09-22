// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/navigation_handle_proxy.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "content/public/android/content_jni_headers/NavigationHandle_jni.h"
#include "content/public/browser/navigation_handle.h"

using base::android::AttachCurrentThread;
using base::android::ConvertUTF8ToJavaString;
using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;

namespace content {

NavigationHandleProxy::NavigationHandleProxy(
    NavigationHandle* cpp_navigation_handle)
    : cpp_navigation_handle_(cpp_navigation_handle) {
  JNIEnv* env = AttachCurrentThread();
  java_navigation_handle_ = Java_NavigationHandle_Constructor(
      env, reinterpret_cast<jlong>(this),
      ConvertUTF8ToJavaString(env, cpp_navigation_handle_->GetURL().spec()),
      cpp_navigation_handle_->IsInMainFrame(),
      cpp_navigation_handle_->IsSameDocument(),
      cpp_navigation_handle_->IsRendererInitiated());
}

void NavigationHandleProxy::DidRedirect() {
  JNIEnv* env = AttachCurrentThread();
  Java_NavigationHandle_didRedirect(
      env, java_navigation_handle_,
      ConvertUTF8ToJavaString(env, cpp_navigation_handle_->GetURL().spec()));
}

void NavigationHandleProxy::DidFinish() {
  JNIEnv* env = AttachCurrentThread();
  // Matches logic in
  // components/navigation_interception/navigation_params_android.cc
  ScopedJavaLocalRef<jstring> jstring_url(ConvertUTF8ToJavaString(
      env, cpp_navigation_handle_->GetBaseURLForDataURL().is_empty()
               ? cpp_navigation_handle_->GetURL().spec()
               : cpp_navigation_handle_->GetBaseURLForDataURL()
                     .possibly_invalid_spec()));

  bool is_fragment_navigation = cpp_navigation_handle_->IsSameDocument();

  if (cpp_navigation_handle_->HasCommitted()) {
    // See http://crbug.com/251330 for why it's determined this way.
    url::Replacements<char> replacements;
    replacements.ClearRef();
    bool urls_same_ignoring_fragment =
        cpp_navigation_handle_->GetURL().ReplaceComponents(replacements) ==
        cpp_navigation_handle_->GetPreviousURL().ReplaceComponents(
            replacements);
    is_fragment_navigation &= urls_same_ignoring_fragment;
  }

  bool is_valid_search_form_url =
      cpp_navigation_handle_->GetSearchableFormURL() != ""
          ? cpp_navigation_handle_->GetSearchableFormURL().is_valid()
          : false;

  Java_NavigationHandle_didFinish(
      env, java_navigation_handle_, jstring_url,
      cpp_navigation_handle_->IsErrorPage(),
      cpp_navigation_handle_->HasCommitted(), is_fragment_navigation,
      cpp_navigation_handle_->IsDownload(), is_valid_search_form_url,
      cpp_navigation_handle_->HasCommitted()
          ? cpp_navigation_handle_->GetPageTransition()
          : -1,
      cpp_navigation_handle_->GetNetErrorCode(),
      // TODO(shaktisahu): Change default status to -1 after fixing
      // crbug/690041.
      cpp_navigation_handle_->GetResponseHeaders()
          ? cpp_navigation_handle_->GetResponseHeaders()->response_code()
          : 200);
}

NavigationHandleProxy::~NavigationHandleProxy() {
  JNIEnv* env = AttachCurrentThread();
  Java_NavigationHandle_release(env, java_navigation_handle_);
}

// Called from Java.
void NavigationHandleProxy::SetRequestHeader(
    JNIEnv* env,
    const JavaParamRef<jstring>& name,
    const JavaParamRef<jstring>& value) {
  cpp_navigation_handle_->SetRequestHeader(ConvertJavaStringToUTF8(name),
                                           ConvertJavaStringToUTF8(value));
}

// Called from Java.
void NavigationHandleProxy::RemoveRequestHeader(
    JNIEnv* env,
    const JavaParamRef<jstring>& name) {
  cpp_navigation_handle_->RemoveRequestHeader(ConvertJavaStringToUTF8(name));
}

}  // namespace content
