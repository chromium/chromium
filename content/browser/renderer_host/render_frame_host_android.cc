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
#include "content/browser/bad_message.h"
#include "content/browser/renderer_host/close_listener_host.h"
#include "content/browser/renderer_host/render_frame_host_delegate.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/public/android/content_jni_headers/RenderFrameHostImpl_jni.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/site_instance.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/blink/public/mojom/frame/user_activation_notification_type.mojom.h"
#include "third_party/blink/public/mojom/permissions_policy/permissions_policy_feature.mojom-shared.h"
#include "third_party/blink/public/mojom/webauthn/authenticator.mojom.h"
#include "url/android/gurl_android.h"
#include "url/origin.h"

using base::android::AttachCurrentThread;
using base::android::ConvertUTF8ToJavaString;
using base::android::JavaParamRef;
using base::android::JavaRef;
using base::android::ScopedJavaLocalRef;

namespace content {

namespace {
void OnGetCanonicalUrlForSharing(
    const base::android::JavaRef<jobject>& jcallback,
    const absl::optional<GURL>& url) {
  JNIEnv* env = base::android::AttachCurrentThread();
  if (!url) {
    base::android::RunObjectCallbackAndroid(jcallback,
                                            url::GURLAndroid::EmptyGURL(env));
    return;
  }

  base::android::RunObjectCallbackAndroid(
      jcallback, url::GURLAndroid::FromNativeGURL(env, url.value()));
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
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) const {
  return url::GURLAndroid::FromNativeGURL(
      env, render_frame_host_->GetLastCommittedURL());
}

ScopedJavaLocalRef<jobject> RenderFrameHostAndroid::GetLastCommittedOrigin(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  return render_frame_host_->GetLastCommittedOrigin().CreateJavaObject();
}

void RenderFrameHostAndroid::GetCanonicalUrlForSharing(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>&,
    const base::android::JavaParamRef<jobject>& jcallback) const {
  render_frame_host_->GetCanonicalUrl(base::BindOnce(
      &OnGetCanonicalUrlForSharing,
      base::android::ScopedJavaGlobalRef<jobject>(env, jcallback)));
}

ScopedJavaLocalRef<jobjectArray> RenderFrameHostAndroid::GetAllRenderFrameHosts(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) const {
  std::vector<RenderFrameHostImpl*> frames;
  render_frame_host_->ForEachRenderFrameHost(
      [&frames](RenderFrameHostImpl* rfh) { frames.push_back(rfh); });
  jclass clazz =
      org_chromium_content_browser_framehost_RenderFrameHostImpl_clazz(env);
  jobjectArray jframes = env->NewObjectArray(frames.size(), clazz, nullptr);
  for (size_t i = 0; i < frames.size(); i++) {
    ScopedJavaLocalRef<jobject> frame = frames[i]->GetJavaRenderFrameHost();
    env->SetObjectArrayElement(jframes, i, frame.obj());
  }
  return ScopedJavaLocalRef<jobjectArray>(env, jframes);
}

bool RenderFrameHostAndroid::IsFeatureEnabled(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>&,
    jint feature) const {
  return render_frame_host_->IsFeatureEnabled(
      static_cast<blink::mojom::PermissionsPolicyFeature>(feature));
}

ScopedJavaLocalRef<jobject>
RenderFrameHostAndroid::GetAndroidOverlayRoutingToken(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) const {
  return base::android::UnguessableTokenAndroid::Create(
      env, render_frame_host_->GetOverlayRoutingToken());
}

void RenderFrameHostAndroid::NotifyUserActivation(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>&) {
  render_frame_host_->GetAssociatedLocalFrame()->NotifyUserActivation(
      blink::mojom::UserActivationNotificationType::kVoiceSearch);
}

jboolean RenderFrameHostAndroid::SignalCloseWatcherIfActive(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>&) const {
  auto* close_listener_host =
      CloseListenerHost::GetOrCreateForCurrentDocument(render_frame_host_);
  return close_listener_host->SignalIfActive();
}

jboolean RenderFrameHostAndroid::IsRenderFrameLive(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>&) const {
  return render_frame_host_->IsRenderFrameLive();
}

void RenderFrameHostAndroid::GetInterfaceToRendererFrame(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>&,
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
    const base::android::JavaParamRef<jobject>&,
    jint reason) const {
  DCHECK_LT(reason, bad_message::BAD_MESSAGE_MAX);
  ReceivedBadMessage(render_frame_host_->GetProcess(),
                     static_cast<bad_message::BadMessageReason>(reason));
}

jboolean RenderFrameHostAndroid::IsProcessBlocked(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>&) const {
  return render_frame_host_->GetProcess()->IsBlocked();
}

ScopedJavaLocalRef<jobject>
RenderFrameHostAndroid::PerformGetAssertionWebAuthSecurityChecks(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>&,
    const base::android::JavaParamRef<jstring>& relying_party_id,
    const base::android::JavaParamRef<jobject>& effective_origin,
    jboolean is_payment_credential_get_assertion) const {
  url::Origin origin = url::Origin::FromJavaObject(effective_origin);
  std::pair<blink::mojom::AuthenticatorStatus, bool> results =
      render_frame_host_->PerformGetAssertionWebAuthSecurityChecks(
          ConvertJavaStringToUTF8(env, relying_party_id), origin,
          is_payment_credential_get_assertion, nullptr);
  return Java_RenderFrameHostImpl_createWebAuthSecurityChecksResults(
      env, static_cast<jint>(results.first), results.second);
}

jint RenderFrameHostAndroid::PerformMakeCredentialWebAuthSecurityChecks(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>&,
    const base::android::JavaParamRef<jstring>& relying_party_id,
    const base::android::JavaParamRef<jobject>& effective_origin,
    jboolean is_payment_credential_creation) const {
  url::Origin origin = url::Origin::FromJavaObject(effective_origin);
  return static_cast<int32_t>(
      render_frame_host_->PerformMakeCredentialWebAuthSecurityChecks(
          ConvertJavaStringToUTF8(env, relying_party_id), origin,
          is_payment_credential_creation, nullptr));
}

jint RenderFrameHostAndroid::GetLifecycleState(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>&) const {
  return static_cast<jint>(render_frame_host_->GetLifecycleState());
}

void RenderFrameHostAndroid::InsertVisualStateCallback(
    JNIEnv* env,
    const JavaParamRef<jobject>& jcallback) {
  render_frame_host()->InsertVisualStateCallback(
      base::BindOnce(&base::android::RunBooleanCallbackAndroid,
                     base::android::ScopedJavaGlobalRef<jobject>(jcallback)));
}

}  // namespace content
