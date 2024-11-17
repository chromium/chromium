// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webauthn/android/webauthn_browser_bridge.h"

#include <jni.h>

#include "base/android/callback_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "components/webauthn/android/webauthn_client_android.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "device/fido/discoverable_credential_metadata.h"
#include "device/fido/public_key_credential_user_entity.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/webauthn/android/jni_headers/WebauthnBrowserBridge_jni.h"

using base::android::ConvertJavaStringToUTF8;
using base::android::ScopedJavaGlobalRef;
using base::android::ScopedJavaLocalRef;

namespace webauthn {

device::DiscoverableCredentialMetadata ConvertJavaCredentialDetailsToMetadata(
    JNIEnv* env,
    ScopedJavaLocalRef<jobject> j_credential) {
  device::DiscoverableCredentialMetadata credential;
  base::android::JavaByteArrayToByteVector(
      env,
      Java_WebauthnBrowserBridge_getWebauthnCredentialDetailsCredentialId(
          env, j_credential),
      &credential.cred_id);
  base::android::JavaByteArrayToByteVector(
      env,
      Java_WebauthnBrowserBridge_getWebauthnCredentialDetailsUserId(
          env, j_credential),
      &credential.user.id);
  credential.user.name = ConvertJavaStringToUTF8(
      env, Java_WebauthnBrowserBridge_getWebauthnCredentialDetailsUserName(
               env, j_credential));
  credential.user.display_name = ConvertJavaStringToUTF8(
      env,
      Java_WebauthnBrowserBridge_getWebauthnCredentialDetailsUserDisplayName(
          env, j_credential));
  return credential;
}

void ConvertJavaCredentialArrayToMetadataVector(
    JNIEnv* env,
    const base::android::JavaRef<jobjectArray>& array,
    std::vector<device::DiscoverableCredentialMetadata>* out) {
  jsize jlength = env->GetArrayLength(array.obj());
  // GetArrayLength() returns -1 if |array| is not a valid Java array.
  DCHECK_GE(jlength, 0) << "Invalid array length: " << jlength;
  size_t length = static_cast<size_t>(std::max(0, jlength));
  for (size_t i = 0; i < length; ++i) {
    ScopedJavaLocalRef<jobject> j_credential(
        env, static_cast<jobject>(env->GetObjectArrayElement(array.obj(), i)));
    out->emplace_back(
        ConvertJavaCredentialDetailsToMetadata(env, j_credential));
  }
}

void OnWebauthnCredentialSelected(
    const base::android::JavaRef<jobject>& jcallback,
    const std::vector<uint8_t>& credential_id) {
  base::android::RunObjectCallbackAndroid(
      jcallback, base::android::ToJavaByteArray(jni_zero::AttachCurrentThread(),
                                                credential_id));
}

void OnHybridAssertionInvoked(
    const base::android::JavaRef<jobject>& jcallback) {
  base::android::RunRunnableAndroid(jcallback);
}

static jlong JNI_WebauthnBrowserBridge_CreateNativeWebauthnBrowserBridge(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jbridge) {
  return reinterpret_cast<jlong>(new WebauthnBrowserBridge(env, jbridge));
}

WebauthnBrowserBridge::WebauthnBrowserBridge(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jbridge)
    : owner_(env, jbridge) {}

WebauthnBrowserBridge::~WebauthnBrowserBridge() = default;

void WebauthnBrowserBridge::OnCredentialsDetailsListReceived(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>&,
    const base::android::JavaParamRef<jobjectArray>& credentials,
    const base::android::JavaParamRef<jobject>& jframe_host,
    jboolean is_conditional_request,
    const base::android::JavaParamRef<jobject>& jget_assertion_callback,
    const base::android::JavaParamRef<jobject>& jhybrid_callback) const {
  auto* client = WebAuthnClientAndroid::GetClient();
  auto* render_frame_host =
      content::RenderFrameHost::FromJavaRenderFrameHost(jframe_host);
  // A null client indicates the embedder does not support Conditional UI.
  // Also, crash reports suggest that there can be null WebContents at this
  // point, presumably indicating that a tab is being closed while the
  // listCredentials call is outstanding. See https://crbug.com/1399887.
  if (!client || !render_frame_host ||
      !content::WebContents::FromRenderFrameHost(render_frame_host)) {
    std::vector<uint8_t> credential_id = {};
    base::android::RunObjectCallbackAndroid(
        jget_assertion_callback,
        base::android::ToJavaByteArray(jni_zero::AttachCurrentThread(),
                                       credential_id));
    return;
  }

  std::vector<device::DiscoverableCredentialMetadata> credentials_metadata;
  ConvertJavaCredentialArrayToMetadataVector(env, credentials,
                                             &credentials_metadata);

  base::RepeatingCallback<void()> hybrid_callback;
  if (jhybrid_callback != nullptr) {
    hybrid_callback = base::BindRepeating(
        &OnHybridAssertionInvoked,
        ScopedJavaGlobalRef<jobject>(env, jhybrid_callback));
  }

  client->OnWebAuthnRequestPending(
      render_frame_host, credentials_metadata, is_conditional_request,
      base::BindRepeating(
          &OnWebauthnCredentialSelected,
          ScopedJavaGlobalRef<jobject>(env, jget_assertion_callback)),
      std::move(hybrid_callback));
}

void TriggerFullRequest(
    const base::android::JavaRef<jobject>& jfull_request_runnable,
    bool request_passwords) {
  base::android::RunBooleanCallbackAndroid(jfull_request_runnable,
                                           request_passwords);
}

void WebauthnBrowserBridge::OnCredManConditionalRequestPending(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jframe_host,
    jboolean jhas_results,
    const base::android::JavaParamRef<jobject>& jfull_request_runnable) {
  auto* client = WebAuthnClientAndroid::GetClient();
  auto* render_frame_host =
      content::RenderFrameHost::FromJavaRenderFrameHost(jframe_host);
  if (!client || !render_frame_host ||
      !content::WebContents::FromRenderFrameHost(render_frame_host)) {
    return;
  }
  client->OnCredManConditionalRequestPending(
      render_frame_host, jhas_results,
      base::BindRepeating(
          &TriggerFullRequest,
          ScopedJavaGlobalRef<jobject>(env, jfull_request_runnable)));
}

void WebauthnBrowserBridge::OnCredManUiClosed(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jframe_host,
    jboolean jsuccess) {
  auto* client = WebAuthnClientAndroid::GetClient();
  auto* render_frame_host =
      content::RenderFrameHost::FromJavaRenderFrameHost(jframe_host);
  if (!client || !render_frame_host ||
      !content::WebContents::FromRenderFrameHost(render_frame_host)) {
    return;
  }
  client->OnCredManUiClosed(render_frame_host, jsuccess);
}

void WebauthnBrowserBridge::OnPasswordCredentialReceived(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jframe_host,
    const base::android::JavaParamRef<jstring>& jusername,
    const base::android::JavaParamRef<jstring>& jpassword) {
  auto* client = WebAuthnClientAndroid::GetClient();
  auto* render_frame_host =
      content::RenderFrameHost::FromJavaRenderFrameHost(jframe_host);
  if (!client || !render_frame_host ||
      !content::WebContents::FromRenderFrameHost(render_frame_host)) {
    return;
  }
  client->OnPasswordCredentialReceived(
      render_frame_host,
      base::android::ConvertJavaStringToUTF16(env, jusername),
      base::android::ConvertJavaStringToUTF16(env, jpassword));
}

void WebauthnBrowserBridge::CleanupRequest(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jframe_host) const {
  auto* client = WebAuthnClientAndroid::GetClient();
  auto* render_frame_host =
      content::RenderFrameHost::FromJavaRenderFrameHost(jframe_host);

  // Crash reports indicate that there can be null WebContents at this point,
  // although it isn't clear how, since the Cancel message was received from
  // renderer and is processed synchronously. The null check exists to mitigate
  // downstream dereferences. See https://crbug.com/1399887.
  if (!render_frame_host ||
      !content::WebContents::FromRenderFrameHost(render_frame_host)) {
    return;
  }

  client->CleanupWebAuthnRequest(render_frame_host);
}

void WebauthnBrowserBridge::CleanupCredManRequest(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jframe_host) const {
  auto* client = WebAuthnClientAndroid::GetClient();
  auto* render_frame_host =
      content::RenderFrameHost::FromJavaRenderFrameHost(jframe_host);

  if (!client || !render_frame_host ||
      !content::WebContents::FromRenderFrameHost(render_frame_host)) {
    return;
  }
  client->CleanupCredManRequest(render_frame_host);
}

void WebauthnBrowserBridge::Destroy(JNIEnv* env) {
  delete this;
}

}  // namespace webauthn
