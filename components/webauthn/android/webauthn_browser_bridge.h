// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAUTHN_ANDROID_WEBAUTHN_BROWSER_BRIDGE_H_
#define COMPONENTS_WEBAUTHN_ANDROID_WEBAUTHN_BROWSER_BRIDGE_H_

#include <jni.h>

#include "base/android/scoped_java_ref.h"

namespace webauthn {

class WebauthnBrowserBridge {
 public:
  WebauthnBrowserBridge(JNIEnv* env,
                        const base::android::JavaRef<jobject>& jbridge);

  WebauthnBrowserBridge(const WebauthnBrowserBridge&) = delete;
  WebauthnBrowserBridge& operator=(const WebauthnBrowserBridge&) = delete;

  ~WebauthnBrowserBridge();

  void OnCredentialsDetailsListReceived(
      JNIEnv* env,
      const base::android::JavaRef<jobjectArray>& credentials,
      const base::android::JavaRef<jobject>& jframe_host,
      int32_t mediation_type,
      const base::android::JavaRef<jobject>& jcredential_callback,
      const base::android::JavaRef<jobject>& jhybrid_callback,
      const base::android::JavaRef<jobject>& jnon_credential_callback) const;

  void CleanupRequest(JNIEnv* env,
                      const base::android::JavaRef<jobject>& jframe_host) const;

  void CleanupCredManRequest(
      JNIEnv* env,
      const base::android::JavaRef<jobject>& jframe_host) const;

  void OnCredManConditionalRequestPending(
      JNIEnv* env,
      const base::android::JavaRef<jobject>& jframe_host,
      bool jhas_results,
      const base::android::JavaRef<jobject>& jfull_request_runnable);

  void OnCredManUiClosed(JNIEnv* env,
                         const base::android::JavaRef<jobject>& jframe_host,
                         bool jsuccess);

  void OnPasswordCredentialReceived(
      JNIEnv* env,
      const base::android::JavaRef<jobject>& jframe_host,
      const base::android::JavaRef<jstring>& jusername,
      const base::android::JavaRef<jstring>& jpassword);

  void Destroy(JNIEnv* env);

 private:
  // Java object that owns this WebauthnBrowserBridge.
  base::android::ScopedJavaGlobalRef<jobject> owner_;
};

}  // namespace webauthn

#endif  // COMPONENTS_WEBAUTHN_ANDROID_WEBAUTHN_BROWSER_BRIDGE_H_
