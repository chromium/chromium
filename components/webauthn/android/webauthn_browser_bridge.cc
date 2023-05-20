// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webauthn/android/webauthn_browser_bridge.h"

#include <jni.h>

#include "base/android/callback_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "components/webauthn/android/jni_headers/WebAuthnBrowserBridge_jni.h"
#include "components/webauthn/android/webauthn_client_android.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "device/fido/discoverable_credential_metadata.h"
#include "device/fido/public_key_credential_user_entity.h"

using base::android::ScopedJavaLocalRef;

device::DiscoverableCredentialMetadata ConvertJavaCredentialDetailsToMetadata(
    JNIEnv* env,
    ScopedJavaLocalRef<jobject> j_credential) {
  device::DiscoverableCredentialMetadata credential;
  base::android::JavaByteArrayToByteVector(
      env,
      Java_WebAuthnBrowserBridge_getWebAuthnCredentialDetailsCredentialId(
          env, j_credential),
      &credential.cred_id);
  base::android::JavaByteArrayToByteVector(
      env,
      Java_WebAuthnBrowserBridge_getWebAuthnCredentialDetailsUserId(
          env, j_credential),
      &credential.user.id);
  credential.user.name = ConvertJavaStringToUTF8(
      env, Java_WebAuthnBrowserBridge_getWebAuthnCredentialDetailsUserName(
               env, j_credential));
  credential.user.display_name = ConvertJavaStringToUTF8(
      env,
      Java_WebAuthnBrowserBridge_getWebAuthnCredentialDetailsUserDisplayName(
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

void OnWebAuthnCredentialSelected(
    const base::android::JavaRef<jobject>& jcallback,
    const std::vector<uint8_t>& credential_id) {
  base::android::RunObjectCallbackAndroid(
      jcallback, base::android::ToJavaByteArray(
                     base::android::AttachCurrentThread(), credential_id.data(),
                     credential_id.size()));
}

static jlong JNI_WebAuthnBrowserBridge_CreateNativeWebAuthnBrowserBridge(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jbridge) {
  return reinterpret_cast<jlong>(new WebAuthnBrowserBridge(env, jbridge));
}

WebAuthnBrowserBridge::WebAuthnBrowserBridge(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jbridge)
    : owner_(env, jbridge) {}

WebAuthnBrowserBridge::~WebAuthnBrowserBridge() = default;

void WebAuthnBrowserBridge::OnCredentialsDetailsListReceived(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>&,
    const base::android::JavaParamRef<jobjectArray>& credentials,
    const base::android::JavaParamRef<jobject>& jframe_host,
    jboolean is_conditional_request,
    const base::android::JavaParamRef<jobject>& jcallback) const {
  auto* client = components::WebAuthnClientAndroid::GetClient();
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
        jcallback, base::android::ToJavaByteArray(
                       base::android::AttachCurrentThread(),
                       credential_id.data(), credential_id.size()));
    return;
  }

  std::vector<device::DiscoverableCredentialMetadata> credentials_metadata;
  ConvertJavaCredentialArrayToMetadataVector(env, credentials,
                                             &credentials_metadata);
  client->OnWebAuthnRequestPending(
      render_frame_host, credentials_metadata, is_conditional_request,
      base::BindRepeating(
          &OnWebAuthnCredentialSelected,
          base::android::ScopedJavaGlobalRef<jobject>(env, jcallback)));
}

void TriggerFullRequest(
    const base::android::JavaRef<jobject>& jfull_request_runnable) {
  base::android::RunRunnableAndroid(jfull_request_runnable);
}

void WebAuthnBrowserBridge::OnCredManConditionalRequestPending(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jframe_host,
    jboolean jhas_results,
    const base::android::JavaParamRef<jobject>& jfull_request_runnable) {
  auto* client = components::WebAuthnClientAndroid::GetClient();
  auto* render_frame_host =
      content::RenderFrameHost::FromJavaRenderFrameHost(jframe_host);
  if (!client || !render_frame_host ||
      !content::WebContents::FromRenderFrameHost(render_frame_host)) {
    return;
  }
  client->OnCredManConditionalRequestPending(
      render_frame_host, jhas_results,
      base::BindRepeating(&TriggerFullRequest,
                          base::android::ScopedJavaGlobalRef<jobject>(
                              env, jfull_request_runnable)));
}

void WebAuthnBrowserBridge::OnCredManUiClosed(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jframe_host,
    jboolean jsuccess) {
  auto* client = components::WebAuthnClientAndroid::GetClient();
  auto* render_frame_host =
      content::RenderFrameHost::FromJavaRenderFrameHost(jframe_host);
  if (!client || !render_frame_host ||
      !content::WebContents::FromRenderFrameHost(render_frame_host)) {
    return;
  }
  client->OnCredManUiClosed(render_frame_host, jsuccess);
}

void WebAuthnBrowserBridge::CleanupRequest(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jframe_host) const {
  auto* client = components::WebAuthnClientAndroid::GetClient();
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
