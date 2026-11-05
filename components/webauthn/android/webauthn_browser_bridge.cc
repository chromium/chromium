// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webauthn/android/webauthn_browser_bridge.h"

#include <jni.h>

#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/android/callback_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/time/time.h"
#include "components/webauthn/android/webauthn_client_android.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "device/fido/discoverable_credential_metadata.h"
#include "device/fido/public/public_key_credential_user_entity.h"
#include "third_party/jni_zero/default_conversions.h"
#include "third_party/jni_zero/jni_zero.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/webauthn/android/jni_headers/WebauthnBrowserBridge_jni.h"

using base::android::AttachCurrentThread;
using base::android::ScopedJavaGlobalRef;

namespace jni_zero {

// Converts a Java WebauthnCredentialDetails to its native counterpart. This is
// used by @JniType for the credential list passed to
// WebauthnBrowserBridge::OnCredentialsDetailsListReceived().
template <>
device::DiscoverableCredentialMetadata
FromJniType<device::DiscoverableCredentialMetadata>(
    JNIEnv* env,
    const JavaRef<jobject>& j_credential) {
  device::DiscoverableCredentialMetadata credential;
  credential.cred_id = webauthn::
      Java_WebauthnBrowserBridge_getWebauthnCredentialDetailsCredentialId(
          env, j_credential);
  credential.user.id =
      webauthn::Java_WebauthnBrowserBridge_getWebauthnCredentialDetailsUserId(
          env, j_credential);
  credential.user.name =
      webauthn::Java_WebauthnBrowserBridge_getWebauthnCredentialDetailsUserName(
          env, j_credential);
  credential.user.display_name = webauthn::
      Java_WebauthnBrowserBridge_getWebauthnCredentialDetailsUserDisplayName(
          env, j_credential);
  int64_t last_used_time_ms = webauthn::
      Java_WebauthnBrowserBridge_getWebauthnCredentialDetailsLastUsedTimeMs(
          env, j_credential);
  if (last_used_time_ms != 0) {
    credential.last_used_time =
        base::Time::FromMillisecondsSinceUnixEpoch(last_used_time_ms);
  }
  return credential;
}

}  // namespace jni_zero

namespace webauthn {

void OnWebauthnCredentialSelected(
    const base::android::JavaRef<jobject>& jcallback,
    const std::vector<uint8_t>& credential_id) {
  JNIEnv* env = AttachCurrentThread();
  base::android::RunObjectCallbackAndroid(
      jcallback, Java_WebauthnBrowserBridge_createSelectedPasskeyCredential(
                     env, credential_id));
}

void OnPasswordCredentialSelected(
    const base::android::JavaRef<jobject>& jcallback,
    std::u16string_view username,
    std::u16string_view password) {
  JNIEnv* env = AttachCurrentThread();
  base::android::RunObjectCallbackAndroid(
      jcallback, Java_WebauthnBrowserBridge_createSelectedPasswordCredential(
                     env, username, password));
}

void OnNonCredentialReturn(const base::android::JavaRef<jobject>& jcallback,
                           NonCredentialReturnReason return_reason) {
  base::android::RunIntCallbackAndroid(jcallback,
                                       static_cast<int32_t>(return_reason));
}

void OnHybridAssertionInvoked(
    const base::android::JavaRef<jobject>& jcallback) {
  jni_zero::RunRunnable(jcallback);
}

static jni_zero::JniUniquePtr<WebauthnBrowserBridge>
JNI_WebauthnBrowserBridge_CreateNativeWebauthnBrowserBridge(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& jbridge) {
  return jni_zero::MakeUnique<WebauthnBrowserBridge>(env, jbridge);
}

WebauthnBrowserBridge::WebauthnBrowserBridge(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& jbridge)
    : owner_(env, jbridge) {}

WebauthnBrowserBridge::~WebauthnBrowserBridge() = default;

void WebauthnBrowserBridge::OnCredentialsDetailsListReceived(
    JNIEnv* env,
    std::vector<device::DiscoverableCredentialMetadata>&& credentials,
    content::RenderFrameHost* render_frame_host,
    int32_t mediation_type,
    const base::android::JavaRef<jobject>& jcredential_callback,
    const base::android::JavaRef<jobject>& jhybrid_callback,
    const base::android::JavaRef<jobject>& jnon_credential_callback) const {
  auto* client = WebAuthnClientAndroid::GetClient();
  // A null client indicates the embedder does not support Conditional UI.
  // Also, crash reports suggest that there can be null WebContents at this
  // point, presumably indicating that a tab is being closed while the
  // listCredentials call is outstanding. See https://crbug.com/1399887.
  if (!client || !render_frame_host ||
      !content::WebContents::FromRenderFrameHost(render_frame_host)) {
    base::android::RunIntCallbackAndroid(
        jnon_credential_callback,
        static_cast<int32_t>(NonCredentialReturnReason::kError));
    return;
  }

  CHECK(AssertionMediationType(mediation_type) !=
            AssertionMediationType::kImmediatePasskeysOnly ||
        !credentials.empty());

  base::RepeatingCallback<void(const std::vector<uint8_t>&)> passkey_callback =
      base::BindRepeating(
          &OnWebauthnCredentialSelected,
          ScopedJavaGlobalRef<jobject>(env, jcredential_callback));

  base::RepeatingCallback<void(std::u16string_view, std::u16string_view)>
      password_callback = base::BindRepeating(
          &OnPasswordCredentialSelected,
          ScopedJavaGlobalRef<jobject>(env, jcredential_callback));

  base::RepeatingClosure hybrid_closure;
  if (!jhybrid_callback.is_null()) {
    hybrid_closure = base::BindRepeating(
        &OnHybridAssertionInvoked,
        ScopedJavaGlobalRef<jobject>(env, jhybrid_callback));
  }

  base::RepeatingCallback<void(NonCredentialReturnReason)>
      non_credential_callback = base::BindRepeating(
          &OnNonCredentialReturn,
          ScopedJavaGlobalRef<jobject>(env, jnon_credential_callback));

  client->OnWebAuthnRequestPending(
      render_frame_host, std::move(credentials),
      AssertionMediationType(mediation_type), std::move(passkey_callback),
      std::move(password_callback), std::move(hybrid_closure),
      std::move(non_credential_callback));
}

void WebauthnBrowserBridge::OnCredManConditionalRequestPending(
    content::RenderFrameHost* render_frame_host,
    bool has_results,
    base::RepeatingCallback<void(bool)>&& full_request_callback) {
  auto* client = WebAuthnClientAndroid::GetClient();
  if (!client || !render_frame_host ||
      !content::WebContents::FromRenderFrameHost(render_frame_host)) {
    return;
  }
  client->OnCredManConditionalRequestPending(render_frame_host, has_results,
                                             std::move(full_request_callback));
}

void WebauthnBrowserBridge::OnCredManUiClosed(
    content::RenderFrameHost* render_frame_host,
    bool success) {
  auto* client = WebAuthnClientAndroid::GetClient();
  if (!client || !render_frame_host ||
      !content::WebContents::FromRenderFrameHost(render_frame_host)) {
    return;
  }
  client->OnCredManUiClosed(render_frame_host, success);
}

void WebauthnBrowserBridge::OnPasswordCredentialReceived(
    content::RenderFrameHost* render_frame_host,
    std::u16string&& username,
    std::u16string&& password) {
  auto* client = WebAuthnClientAndroid::GetClient();
  if (!client || !render_frame_host ||
      !content::WebContents::FromRenderFrameHost(render_frame_host)) {
    return;
  }
  client->OnPasswordCredentialReceived(render_frame_host, std::move(username),
                                       std::move(password));
}

void WebauthnBrowserBridge::CleanupRequest(
    content::RenderFrameHost* render_frame_host) const {
  auto* client = WebAuthnClientAndroid::GetClient();

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
    content::RenderFrameHost* render_frame_host) const {
  auto* client = WebAuthnClientAndroid::GetClient();

  if (!client || !render_frame_host ||
      !content::WebContents::FromRenderFrameHost(render_frame_host)) {
    return;
  }
  client->CleanupCredManRequest(render_frame_host);
}

static bool JNI_WebauthnBrowserBridge_ShouldDisallowCredentialRequest(
    content::RenderFrameHost* render_frame_host) {
  if (!render_frame_host) {
    return false;
  }
  auto* client = WebAuthnClientAndroid::GetClient();
  if (!client) {
    return false;
  }
  return client->ShouldDisallowCredentialRequest(render_frame_host);
}

}  // namespace webauthn

DEFINE_JNI(WebauthnBrowserBridge)
