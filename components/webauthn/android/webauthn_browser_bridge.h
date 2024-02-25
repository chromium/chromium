// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAUTHN_ANDROID_WEBAUTHN_BROWSER_BRIDGE_H_
#define COMPONENTS_WEBAUTHN_ANDROID_WEBAUTHN_BROWSER_BRIDGE_H_

#include "base/android/scoped_java_ref.h"

namespace webauthn {

class WebauthnBrowserBridge {
 public:
  WebauthnBrowserBridge(JNIEnv* env,
                        const base::android::JavaParamRef<jobject>& jbridge);

  WebauthnBrowserBridge(const WebauthnBrowserBridge&) = delete;
  WebauthnBrowserBridge& operator=(const WebauthnBrowserBridge&) = delete;

  ~WebauthnBrowserBridge();

  void OnCredentialsDetailsListReceived(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>&,
      const base::android::JavaParamRef<jobjectArray>& credentials,
      const base::android::JavaParamRef<jobject>& jframe_host,
      jboolean is_conditional_request,
      const base::android::JavaParamRef<jobject>& jgetAssertionCallback,
      const base::android::JavaParamRef<jobject>& jhybridCallback) const;

  void CleanupRequest(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jframe_host) const;

  void CleanupCredManRequest(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jframe_host) const;

  void OnCredManConditionalRequestPending(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jframe_host,
      jboolean jhas_results,
      const base::android::JavaParamRef<jobject>& jfull_request_runnable);

  void OnCredManUiClosed(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jframe_host,
      jboolean jsuccess);

  void OnPasswordCredentialReceived(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jframe_host,
      const base::android::JavaParamRef<jstring>& jusername,
      const base::android::JavaParamRef<jstring>& jpassword);

  void Destroy(JNIEnv* env);

 private:
  // Java object that owns this WebauthnBrowserBridge.
  base::android::ScopedJavaGlobalRef<jobject> owner_;
};

}  // namespace webauthn

#endif  // COMPONENTS_WEBAUTHN_ANDROID_WEBAUTHN_BROWSER_BRIDGE_H_
