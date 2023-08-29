// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAUTHN_ANDROID_INTERNAL_AUTHENTICATOR_ANDROID_H_
#define COMPONENTS_WEBAUTHN_ANDROID_INTERNAL_AUTHENTICATOR_ANDROID_H_

#include "base/android/scoped_java_ref.h"
#include "components/webauthn/core/browser/internal_authenticator.h"
#include "content/public/browser/global_routing_id.h"
#include "third_party/blink/public/mojom/webauthn/authenticator.mojom.h"

namespace url {
class Origin;
}

namespace content {
class RenderFrameHost;
}  // namespace content

namespace webauthn {

// Implementation of the public InternalAuthenticator interface.
// This class is meant only for trusted and internal components of Chrome to
// use. The Android implementation is in
// org.chromium.chrome.browser.webauth.AuthenticatorImpl.
// When MakeCredential() or GetAssertion() is called, the Java implementation
// passes the response through InvokeMakeCredentialResponse() and
// InvokeGetAssertionResponse(), which eventually invokes the callback given by
// the original caller.
class InternalAuthenticatorAndroid : public webauthn::InternalAuthenticator {
 public:
  explicit InternalAuthenticatorAndroid(
      content::RenderFrameHost* render_frame_host);

  ~InternalAuthenticatorAndroid() override;

  // InternalAuthenticator:
  void SetEffectiveOrigin(const url::Origin& origin) override;
  void SetPaymentOptions(blink::mojom::PaymentOptionsPtr payment) override;
  void MakeCredential(
      blink::mojom::PublicKeyCredentialCreationOptionsPtr options,
      blink::mojom::Authenticator::MakeCredentialCallback callback) override;
  void GetAssertion(
      blink::mojom::PublicKeyCredentialRequestOptionsPtr options,
      blink::mojom::Authenticator::GetAssertionCallback callback) override;
  void IsUserVerifyingPlatformAuthenticatorAvailable(
      blink::mojom::Authenticator::
          IsUserVerifyingPlatformAuthenticatorAvailableCallback callback)
      override;
  bool IsGetMatchingCredentialIdsSupported() override;
  void GetMatchingCredentialIds(
      const std::string& relying_party_id,
      const std::vector<std::vector<uint8_t>>& credential_ids,
      bool require_third_party_payment_bit,
      webauthn::GetMatchingCredentialIdsCallback callback) override;
  void Cancel() override;
  content::RenderFrameHost* GetRenderFrameHost() override;

  void InvokeMakeCredentialResponse(
      JNIEnv* env,
      jint status,
      const base::android::JavaParamRef<jobject>& byte_buffer);
  void InvokeGetAssertionResponse(
      JNIEnv* env,
      jint status,
      const base::android::JavaParamRef<jobject>& byte_buffer);
  void InvokeIsUserVerifyingPlatformAuthenticatorAvailableResponse(
      JNIEnv* env,
      jboolean is_uvpaa);
  void InvokeGetMatchingCredentialIdsResponse(
      JNIEnv* env,
      const base::android::JavaParamRef<jobjectArray>& credential_ids_array);

 private:
  // Returns the associated AuthenticatorImpl Java object. Initializes new
  // instance if not done so already in order to avoid possibility of any null
  // pointer issues.
  base::android::JavaRef<jobject>& GetJavaObject();

  const content::GlobalRenderFrameHostId render_frame_host_id_;
  base::android::ScopedJavaGlobalRef<jobject> java_internal_authenticator_ref_;
  blink::mojom::Authenticator::MakeCredentialCallback
      make_credential_response_callback_;
  blink::mojom::Authenticator::GetAssertionCallback
      get_assertion_response_callback_;
  blink::mojom::Authenticator::
      IsUserVerifyingPlatformAuthenticatorAvailableCallback is_uvpaa_callback_;
  webauthn::GetMatchingCredentialIdsCallback
      get_matching_credential_ids_callback_;
};

}  // namespace webauthn

#endif  // COMPONENTS_WEBAUTHN_ANDROID_INTERNAL_AUTHENTICATOR_ANDROID_H_
