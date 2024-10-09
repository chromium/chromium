// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/render_frame_host_android.h"

#include <jni.h>
#include <utility>

#include "base/android/callback_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/unguessable_token_android.h"
#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/json/json_writer.h"
#include "content/browser/bad_message.h"
#include "content/browser/closewatcher/close_listener_host.h"
#include "content/browser/renderer_host/render_frame_host_delegate.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/site_instance.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/blink/public/mojom/frame/user_activation_notification_type.mojom.h"
#include "third_party/blink/public/mojom/permissions_policy/permissions_policy_feature.mojom-shared.h"
#include "third_party/blink/public/mojom/webauthn/authenticator.mojom.h"
#include "url/android/gurl_android.h"
#include "url/origin.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "content/public/android/content_jni_headers/RenderFrameHostImpl_jni.h"

using base::android::AttachCurrentThread;
using base::android::ConvertJavaStringToUTF16;
using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertUTF8ToJavaString;
using base::android::JavaParamRef;
using base::android::JavaRef;
using base::android::ScopedJavaLocalRef;

namespace content {

namespace {
void OnGetCanonicalUrlForSharing(
    const base::android::JavaRef<jobject>& jcallback,
    const std::optional<GURL>& url) {
  JNIEnv* env = base::android::AttachCurrentThread();
  if (!url) {
    base::android::RunObjectCallbackAndroid(jcallback,
                                            url::GURLAndroid::EmptyGURL(env));
    return;
  }

  base::android::RunObjectCallbackAndroid(
      jcallback, url::GURLAndroid::FromNativeGURL(env, url.value()));
}

void JavaScriptResultCallback(
    const base::android::ScopedJavaGlobalRef<jobject>& callback,
    base::Value result) {
  JNIEnv* env = base::android::AttachCurrentThread();
  std::string json;
  base::JSONWriter::Write(result, &json);
  base::android::ScopedJavaLocalRef<jstring> j_json =
      ConvertUTF8ToJavaString(env, json);
  Java_RenderFrameHostImpl_onEvaluateJavaScriptResult(env, j_json, callback);
}

}  // namespace

// static
RenderFrameHost* RenderFrameHost::FromJavaRenderFrameHost(
    const JavaRef<jobject>& jrender_frame_host_android) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (jrender_frame_host_android.is_null())
    return nullptr;

  RenderFrameHostAndroid* render_frame_host_android =
      reinterpret_cast<RenderFrameHostAndroid*>(
          Java_RenderFrameHostImpl_getNativePointer(
              AttachCurrentThread(), jrender_frame_host_android));
  if (!render_frame_host_android)
    return nullptr;
  return render_frame_host_android->render_frame_host();
}

RenderFrameHostAndroid::RenderFrameHostAndroid(
    RenderFrameHostImpl* render_frame_host)
    : render_frame_host_(render_frame_host) {}

RenderFrameHostAndroid::~RenderFrameHostAndroid() {
  // Avoid unnecessarily creating the java object from the destructor.
  if (obj_.is_uninitialized())
    return;

  ScopedJavaLocalRef<jobject> jobj = GetJavaObject();
  if (!jobj.is_null()) {
    Java_RenderFrameHostImpl_clearNativePtr(AttachCurrentThread(), jobj);
    obj_.reset();
  }
}

base::android::ScopedJavaLocalRef<jobject>
RenderFrameHostAndroid::GetJavaObject() {
  JNIEnv* env = base::android::AttachCurrentThread();
  if (obj_.is_uninitialized()) {
    const bool is_incognito = render_frame_host_->GetSiteInstance()
                                  ->GetBrowserContext()
                                  ->IsOffTheRecord();
    const GlobalRenderFrameHostId rfh_id = render_frame_host_->GetGlobalId();
    ScopedJavaLocalRef<jobject> local_ref = Java_RenderFrameHostImpl_create(
        env, reinterpret_cast<intptr_t>(this),
        render_frame_host_->delegate()->GetJavaRenderFrameHostDelegate(),
        is_incognito, rfh_id.child_id, rfh_id.frame_routing_id);
    obj_ = JavaObjectWeakGlobalRef(env, local_ref);
    return local_ref;
  }
  return obj_.get(env);
}

ScopedJavaLocalRef<jobject> RenderFrameHostAndroid::GetLastCommittedURL(
    JNIEnv* env) const {
  return url::GURLAndroid::FromNativeGURL(
      env, render_frame_host_->GetLastCommittedURL());
}

ScopedJavaLocalRef<jobject> RenderFrameHostAndroid::GetLastCommittedOrigin(
    JNIEnv* env) {
  return render_frame_host_->GetLastCommittedOrigin().ToJavaObject(env);
}

ScopedJavaLocalRef<jobject> RenderFrameHostAndroid::GetMainFrame(JNIEnv* env) {
  return render_frame_host_->GetMainFrame()->GetJavaRenderFrameHost();
}

void RenderFrameHostAndroid::GetCanonicalUrlForSharing(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcallback) const {
  render_frame_host_->GetCanonicalUrl(base::BindOnce(
      &OnGetCanonicalUrlForSharing,
      base::android::ScopedJavaGlobalRef<jobject>(env, jcallback)));
}

std::vector<ScopedJavaLocalRef<jobject>>
RenderFrameHostAndroid::GetAllRenderFrameHosts(JNIEnv* env) const {
  std::vector<ScopedJavaLocalRef<jobject>> ret;
  render_frame_host_->ForEachRenderFrameHost([&ret](RenderFrameHostImpl* rfh) {
    ret.push_back(rfh->GetJavaRenderFrameHost());
  });
  return ret;
}

bool RenderFrameHostAndroid::IsFeatureEnabled(
    JNIEnv* env,
    jint feature) const {
  return render_frame_host_->IsFeatureEnabled(
      static_cast<blink::mojom::PermissionsPolicyFeature>(feature));
}

base::UnguessableToken RenderFrameHostAndroid::GetAndroidOverlayRoutingToken(
    JNIEnv* env) const {
  return render_frame_host_->GetOverlayRoutingToken();
}

void RenderFrameHostAndroid::NotifyUserActivation(JNIEnv* env) {
  render_frame_host_->GetAssociatedLocalFrame()->NotifyUserActivation(
      blink::mojom::UserActivationNotificationType::kVoiceSearch);
}

void RenderFrameHostAndroid::NotifyWebAuthnAssertionRequestSucceeded(
    JNIEnv* env) {
  render_frame_host_->WebAuthnAssertionRequestSucceeded();
}

jboolean RenderFrameHostAndroid::IsCloseWatcherActive(JNIEnv* env) const {
  auto* close_listener_host =
      CloseListenerHost::GetForCurrentDocument(render_frame_host_);
  return close_listener_host && close_listener_host->IsActive();
}

jboolean RenderFrameHostAndroid::SignalCloseWatcherIfActive(JNIEnv* env) const {
  auto* close_listener_host =
      CloseListenerHost::GetForCurrentDocument(render_frame_host_);
  return close_listener_host && close_listener_host->SignalIfActive();
}

jboolean RenderFrameHostAndroid::IsRenderFrameLive(JNIEnv* env) const {
  return render_frame_host_->IsRenderFrameLive();
}

void RenderFrameHostAndroid::GetInterfaceToRendererFrame(
    JNIEnv* env,
    const base::android::JavaParamRef<jstring>& interface_name,
    jlong message_pipe_raw_handle) const {
  DCHECK(render_frame_host_->IsRenderFrameLive());
  render_frame_host_->GetRemoteInterfaces()->GetInterfaceByName(
      ConvertJavaStringToUTF8(env, interface_name),
      mojo::ScopedMessagePipeHandle(
          mojo::MessagePipeHandle(message_pipe_raw_handle)));
}

void RenderFrameHostAndroid::TerminateRendererDueToBadMessage(
    JNIEnv* env,
    jint reason) const {
  DCHECK_LT(reason, bad_message::BAD_MESSAGE_MAX);
  ReceivedBadMessage(render_frame_host_->GetProcess(),
                     static_cast<bad_message::BadMessageReason>(reason));
}

jboolean RenderFrameHostAndroid::IsProcessBlocked(JNIEnv* env) const {
  return render_frame_host_->GetProcess()->IsBlocked();
}

void RenderFrameHostAndroid::PerformGetAssertionWebAuthSecurityChecks(
    JNIEnv* env,
    const base::android::JavaParamRef<jstring>& relying_party_id,
    const base::android::JavaParamRef<jobject>& effective_origin,
    jboolean is_payment_credential_get_assertion,
    const base::android::JavaParamRef<jobject>& callback) const {
  url::Origin origin = url::Origin::FromJavaObject(env, effective_origin);
  render_frame_host_->PerformGetAssertionWebAuthSecurityChecks(
      ConvertJavaStringToUTF8(env, relying_party_id), origin,
      is_payment_credential_get_assertion,
      base::BindOnce(
          [](base::android::ScopedJavaGlobalRef<jobject> callback,
             blink::mojom::AuthenticatorStatus status, bool is_cross_origin) {
            base::android::RunObjectCallbackAndroid(
                callback,
                Java_RenderFrameHostImpl_createWebAuthSecurityChecksResults(
                    base::android::AttachCurrentThread(),
                    static_cast<jint>(status), is_cross_origin));
          },
          base::android::ScopedJavaGlobalRef<jobject>(callback)));
}

void RenderFrameHostAndroid::PerformMakeCredentialWebAuthSecurityChecks(
    JNIEnv* env,
    const base::android::JavaParamRef<jstring>& relying_party_id,
    const base::android::JavaParamRef<jobject>& effective_origin,
    jboolean is_payment_credential_creation,
    const base::android::JavaParamRef<jobject>& callback) const {
  url::Origin origin = url::Origin::FromJavaObject(env, effective_origin);
  render_frame_host_->PerformMakeCredentialWebAuthSecurityChecks(
      ConvertJavaStringToUTF8(env, relying_party_id), origin,
      is_payment_credential_creation,
      base::BindOnce(
          [](base::android::ScopedJavaGlobalRef<jobject> callback,
             blink::mojom::AuthenticatorStatus status, bool is_cross_origin) {
            base::android::RunObjectCallbackAndroid(
                callback,
                Java_RenderFrameHostImpl_createWebAuthSecurityChecksResults(
                    base::android::AttachCurrentThread(),
                    static_cast<jint>(status), is_cross_origin));
          },
          base::android::ScopedJavaGlobalRef<jobject>(callback)));
}

jint RenderFrameHostAndroid::GetLifecycleState(JNIEnv* env) const {
  return static_cast<jint>(render_frame_host_->GetLifecycleState());
}

void RenderFrameHostAndroid::ExecuteJavaScriptInIsolatedWorld(
    JNIEnv* env,
    const base::android::JavaParamRef<jstring>& jscript,
    jint jworldId,
    const base::android::JavaParamRef<jobject>& jcallback) {
  if (!jcallback) {
    render_frame_host()->ExecuteJavaScriptInIsolatedWorld(
        ConvertJavaStringToUTF16(env, jscript), base::DoNothing(), jworldId);
    return;
  }
  // Secure the Java callback in a scoped object and give ownership of it to the
  // base::OnceCallback below.
  base::android::ScopedJavaGlobalRef<jobject> java_callback;
  java_callback.Reset(env, jcallback);

  render_frame_host()->ExecuteJavaScriptInIsolatedWorld(
      ConvertJavaStringToUTF16(env, jscript),
      base::BindOnce(&JavaScriptResultCallback, java_callback), jworldId);
}

void RenderFrameHostAndroid::InsertVisualStateCallback(
    JNIEnv* env,
    const JavaParamRef<jobject>& jcallback) {
  render_frame_host()->InsertVisualStateCallback(
      base::BindOnce(&base::android::RunBooleanCallbackAndroid,
                     base::android::ScopedJavaGlobalRef<jobject>(jcallback)));
}

}  // namespace content
