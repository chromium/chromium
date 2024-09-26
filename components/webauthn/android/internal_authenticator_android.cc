// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webauthn/android/internal_authenticator_android.h"

#include <string>
#include <utility>
#include <vector>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_bytebuffer.h"
#include "base/android/jni_string.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "url/origin.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/webauthn/android/jni_headers/InternalAuthenticator_jni.h"

using base::android::AttachCurrentThread;
using base::android::ConvertUTF8ToJavaString;
using base::android::JavaArrayOfByteArrayToBytesVector;
using base::android::JavaRef;
using base::android::ScopedJavaLocalRef;
using base::android::ToJavaArrayOfByteArray;

namespace webauthn {

InternalAuthenticatorAndroid::InternalAuthenticatorAndroid(
    content::RenderFrameHost* render_frame_host)
    : render_frame_host_id_(render_frame_host->GetGlobalId()) {
  JNIEnv* env = AttachCurrentThread();
  java_internal_authenticator_ref_ = Java_InternalAuthenticator_create(
      env, reinterpret_cast<intptr_t>(this),
      render_frame_host->GetJavaRenderFrameHost());
}

InternalAuthenticatorAndroid::~InternalAuthenticatorAndroid() {
  JNIEnv* env = AttachCurrentThread();
  DCHECK(!java_internal_authenticator_ref_.is_null());
  Java_InternalAuthenticator_clearNativePtr(env,
                                            java_internal_authenticator_ref_);
}

void InternalAuthenticatorAndroid::SetEffectiveOrigin(
    const url::Origin& origin) {
  JNIEnv* env = AttachCurrentThread();
  JavaRef<jobject>& obj = GetJavaObject();
  DCHECK(!obj.is_null());

  Java_InternalAuthenticator_setEffectiveOrigin(env, obj,
                                                origin.ToJavaObject(env));
}

void InternalAuthenticatorAndroid::SetPaymentOptions(
    blink::mojom::PaymentOptionsPtr payment) {
  JNIEnv* env = AttachCurrentThread();
  JavaRef<jobject>& obj = GetJavaObject();
  DCHECK(!obj.is_null());

  std::vector<uint8_t> byte_vector =
      blink::mojom::PaymentOptions::Serialize(&payment);
  ScopedJavaLocalRef<jobject> byte_buffer = ScopedJavaLocalRef<jobject>(
      env, env->NewDirectByteBuffer(byte_vector.data(), byte_vector.size()));
  base::android::CheckException(env);

  Java_InternalAuthenticator_setPaymentOptions(env, obj, byte_buffer);
}

void InternalAuthenticatorAndroid::MakeCredential(
    blink::mojom::PublicKeyCredentialCreationOptionsPtr options,
    blink::mojom::Authenticator::MakeCredentialCallback callback) {
  JNIEnv* env = AttachCurrentThread();
  JavaRef<jobject>& obj = GetJavaObject();
  DCHECK(!obj.is_null());

  make_credential_response_callback_ = std::move(callback);

  std::vector<uint8_t> byte_vector =
      blink::mojom::PublicKeyCredentialCreationOptions::Serialize(&options);
  ScopedJavaLocalRef<jobject> byte_buffer = ScopedJavaLocalRef<jobject>(
      env, env->NewDirectByteBuffer(byte_vector.data(), byte_vector.size()));
  base::android::CheckException(env);

  Java_InternalAuthenticator_makeCredential(env, obj, byte_buffer);
}

void InternalAuthenticatorAndroid::GetAssertion(
    blink::mojom::PublicKeyCredentialRequestOptionsPtr options,
    blink::mojom::Authenticator::GetAssertionCallback callback) {
  JNIEnv* env = AttachCurrentThread();
  JavaRef<jobject>& obj = GetJavaObject();
  DCHECK(!obj.is_null());

  get_assertion_response_callback_ = std::move(callback);

  std::vector<uint8_t> byte_vector =
      blink::mojom::PublicKeyCredentialRequestOptions::Serialize(&options);
  ScopedJavaLocalRef<jobject> byte_buffer = ScopedJavaLocalRef<jobject>(
      env, env->NewDirectByteBuffer(byte_vector.data(), byte_vector.size()));
  base::android::CheckException(env);

  Java_InternalAuthenticator_getAssertion(env, obj, byte_buffer);
}

void InternalAuthenticatorAndroid::
    IsUserVerifyingPlatformAuthenticatorAvailable(
        blink::mojom::Authenticator::
            IsUserVerifyingPlatformAuthenticatorAvailableCallback callback) {
  JNIEnv* env = AttachCurrentThread();
  JavaRef<jobject>& obj = GetJavaObject();
  DCHECK(!obj.is_null());

  is_uvpaa_callback_ = std::move(callback);
  Java_InternalAuthenticator_isUserVerifyingPlatformAuthenticatorAvailable(env,
                                                                           obj);
}

bool InternalAuthenticatorAndroid::IsGetMatchingCredentialIdsSupported() {
  JNIEnv* env = AttachCurrentThread();
  JavaRef<jobject>& obj = GetJavaObject();
  DCHECK(!obj.is_null());

  return Java_InternalAuthenticator_isGetMatchingCredentialIdsSupported(env,
                                                                        obj);
}

void InternalAuthenticatorAndroid::GetMatchingCredentialIds(
    const std::string& relying_party_id,
    const std::vector<std::vector<uint8_t>>& credential_ids,
    bool require_third_party_payment_bit,
    webauthn::GetMatchingCredentialIdsCallback callback) {
  JNIEnv* env = AttachCurrentThread();
  JavaRef<jobject>& obj = GetJavaObject();
  DCHECK(!obj.is_null());

  get_matching_credential_ids_callback_ = std::move(callback);
  Java_InternalAuthenticator_getMatchingCredentialIds(
      env, obj, ConvertUTF8ToJavaString(env, relying_party_id),
      ToJavaArrayOfByteArray(env, std::move(credential_ids)),
      require_third_party_payment_bit);
}

void InternalAuthenticatorAndroid::Cancel() {
  JNIEnv* env = AttachCurrentThread();
  JavaRef<jobject>& obj = GetJavaObject();
  DCHECK(!obj.is_null());

  Java_InternalAuthenticator_cancel(env, obj);
}

content::RenderFrameHost* InternalAuthenticatorAndroid::GetRenderFrameHost() {
  return content::RenderFrameHost::FromID(render_frame_host_id_);
}

void InternalAuthenticatorAndroid::InvokeMakeCredentialResponse(
    JNIEnv* env,
    jint status,
    const base::android::JavaParamRef<jobject>& byte_buffer) {
  blink::mojom::MakeCredentialAuthenticatorResponsePtr response;

  // |byte_buffer| may be null if authentication failed.
  if (byte_buffer) {
    auto span = base::android::JavaByteBufferToSpan(env, byte_buffer.obj());
    blink::mojom::MakeCredentialAuthenticatorResponse::Deserialize(
        span.data(), span.size(), &response);
  }

  DCHECK_NE(
      status,
      static_cast<int>(
          blink::mojom::AuthenticatorStatus::ERROR_WITH_DOM_EXCEPTION_DETAILS));
  std::move(make_credential_response_callback_)
      .Run(static_cast<blink::mojom::AuthenticatorStatus>(status),
           std::move(response), /*dom_exception_details=*/nullptr);
}

void InternalAuthenticatorAndroid::InvokeGetAssertionResponse(
    JNIEnv* env,
    jint status,
    const base::android::JavaParamRef<jobject>& byte_buffer) {
  blink::mojom::GetAssertionAuthenticatorResponsePtr response;

  // |byte_buffer| may be null if authentication failed.
  if (byte_buffer) {
    auto span = base::android::JavaByteBufferToSpan(env, byte_buffer.obj());
    blink::mojom::GetAssertionAuthenticatorResponse::Deserialize(
        span.data(), span.size(), &response);
  }

  DCHECK_NE(
      status,
      static_cast<int>(
          blink::mojom::AuthenticatorStatus::ERROR_WITH_DOM_EXCEPTION_DETAILS));
  std::move(get_assertion_response_callback_)
      .Run(static_cast<blink::mojom::AuthenticatorStatus>(status),
           std::move(response), /*dom_exception_details=*/nullptr);
}

void InternalAuthenticatorAndroid::
    InvokeIsUserVerifyingPlatformAuthenticatorAvailableResponse(
        JNIEnv* env,
        jboolean is_uvpaa) {
  std::move(is_uvpaa_callback_).Run(static_cast<bool>(is_uvpaa));
}

void InternalAuthenticatorAndroid::InvokeGetMatchingCredentialIdsResponse(
    JNIEnv* env,
    const base::android::JavaParamRef<jobjectArray>& credential_ids_array) {
  std::vector<std::vector<uint8_t>> credential_ids;
  JavaArrayOfByteArrayToBytesVector(env, credential_ids_array, &credential_ids);
  std::move(get_matching_credential_ids_callback_)
      .Run(std::move(credential_ids));
}

JavaRef<jobject>& InternalAuthenticatorAndroid::GetJavaObject() {
  if (java_internal_authenticator_ref_.is_null()) {
    JNIEnv* env = AttachCurrentThread();
    java_internal_authenticator_ref_ = Java_InternalAuthenticator_create(
        env, reinterpret_cast<intptr_t>(this),
        GetRenderFrameHost()->GetJavaRenderFrameHost());
  }
  return java_internal_authenticator_ref_;
}

}  // namespace webauthn
