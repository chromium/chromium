// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/navigation_handle_proxy.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/common/content_client.h"
#include "net/http/http_response_headers.h"
#include "url/android/gurl_android.h"
#include "url/gurl.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "content/public/android/content_jni_headers/NavigationHandle_jni.h"

using base::android::AttachCurrentThread;
using base::android::JavaParamRef;

namespace content {

NavigationHandleProxy::NavigationHandleProxy(
    NavigationHandle* cpp_navigation_handle)
    : cpp_navigation_handle_(cpp_navigation_handle) {
  JNIEnv* env = AttachCurrentThread();

  java_navigation_handle_ = Java_NavigationHandle_Constructor(
      env, reinterpret_cast<jlong>(cpp_navigation_handle));
}

void NavigationHandleProxy::DidStart() {
  JNIEnv* env = AttachCurrentThread();

  // Set all these methods on the Java side over JNI with a new JNI method.
  Java_NavigationHandle_initialize(
      env, java_navigation_handle_, reinterpret_cast<jlong>(this),
      url::GURLAndroid::FromNativeGURL(env, cpp_navigation_handle_->GetURL()),
      url::GURLAndroid::FromNativeGURL(
          env, cpp_navigation_handle_->GetReferrer().url),
      url::GURLAndroid::FromNativeGURL(
          env, cpp_navigation_handle_->GetBaseURLForDataURL()),
      cpp_navigation_handle_->IsInPrimaryMainFrame(),
      cpp_navigation_handle_->IsSameDocument(),
      cpp_navigation_handle_->IsRendererInitiated(),
      cpp_navigation_handle_->GetInitiatorOrigin()
          ? cpp_navigation_handle_->GetInitiatorOrigin()->ToJavaObject(env)
          : nullptr,
      cpp_navigation_handle_->GetPageTransition(),
      cpp_navigation_handle_->IsPost(),
      cpp_navigation_handle_->HasUserGesture(),
      cpp_navigation_handle_->WasServerRedirect(),
      cpp_navigation_handle_->IsExternalProtocol(),
      cpp_navigation_handle_->GetNavigationId(),
      cpp_navigation_handle_->IsPageActivation(),
      cpp_navigation_handle_->GetReloadType() != content::ReloadType::NONE,
      cpp_navigation_handle_->IsPdf(),
      base::android::ConvertUTF8ToJavaString(env, GetMimeType()),
      GetContentClient()->browser()->IsSaveableNavigation(
          cpp_navigation_handle_));
}

void NavigationHandleProxy::DidRedirect() {
  JNIEnv* env = AttachCurrentThread();
  Java_NavigationHandle_didRedirect(
      env, java_navigation_handle_,
      url::GURLAndroid::FromNativeGURL(env, cpp_navigation_handle_->GetURL()),
      cpp_navigation_handle_->IsExternalProtocol());
}

void NavigationHandleProxy::DidFinish() {
  JNIEnv* env = AttachCurrentThread();
  // Matches logic in
  // components/navigation_interception/navigation_params_android.cc
  const GURL& gurl = cpp_navigation_handle_->GetBaseURLForDataURL().is_empty()
                         ? cpp_navigation_handle_->GetURL()
                         : cpp_navigation_handle_->GetBaseURLForDataURL();

  bool is_primary_main_frame_fragment_navigation =
      cpp_navigation_handle_->IsInPrimaryMainFrame() &&
      cpp_navigation_handle_->IsSameDocument();

  if (is_primary_main_frame_fragment_navigation &&
      cpp_navigation_handle_->HasCommitted()) {
    // See http://crbug.com/251330 for why it's determined this way.
    bool urls_same_ignoring_fragment =
        cpp_navigation_handle_->GetURL().EqualsIgnoringRef(
            cpp_navigation_handle_->GetPreviousPrimaryMainFrameURL());
    is_primary_main_frame_fragment_navigation = urls_same_ignoring_fragment;
  }

  bool is_valid_search_form_url =
      cpp_navigation_handle_->GetSearchableFormURL() != ""
          ? cpp_navigation_handle_->GetSearchableFormURL().is_valid()
          : false;

  Java_NavigationHandle_didFinish(
      env, java_navigation_handle_, url::GURLAndroid::FromNativeGURL(env, gurl),
      cpp_navigation_handle_->IsErrorPage(),
      cpp_navigation_handle_->HasCommitted(),
      is_primary_main_frame_fragment_navigation,
      cpp_navigation_handle_->IsDownload(), is_valid_search_form_url,
      cpp_navigation_handle_->GetPageTransition(),
      cpp_navigation_handle_->GetNetErrorCode(),
      // TODO(shaktisahu): Change default status to -1 after fixing
      // crbug/690041.
      cpp_navigation_handle_->GetResponseHeaders()
          ? cpp_navigation_handle_->GetResponseHeaders()->response_code()
          : 200,
      cpp_navigation_handle_->IsExternalProtocol(),
      cpp_navigation_handle_->IsPdf(),
      base::android::ConvertUTF8ToJavaString(env, GetMimeType()),
      GetContentClient()->browser()->IsSaveableNavigation(
          cpp_navigation_handle_));
}

NavigationHandleProxy::~NavigationHandleProxy() {
  JNIEnv* env = AttachCurrentThread();
  Java_NavigationHandle_release(env, java_navigation_handle_);
}

std::string NavigationHandleProxy::GetMimeType() const {
  std::string mime_type;
  if (cpp_navigation_handle_->GetResponseHeaders()) {
    cpp_navigation_handle_->GetResponseHeaders()->GetMimeType(&mime_type);
  }
  return mime_type;
}

}  // namespace content
