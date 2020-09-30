// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/render_frame_host_android.h"

#include <utility>

#include "base/android/callback_android.h"
#include "base/android/jni_string.h"
#include "base/android/unguessable_token_android.h"
#include "base/bind.h"
#include "base/check_op.h"
#include "content/browser/renderer_host/render_frame_host_delegate.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/public/android/content_jni_headers/RenderFrameHostImpl_jni.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/site_instance.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "third_party/blink/public/mojom/feature_policy/feature_policy_feature.mojom-shared.h"
#include "third_party/blink/public/mojom/frame/user_activation_notification_type.mojom.h"
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
    const base::Optional<GURL>& url) {
  if (!url) {
    base::android::RunObjectCallbackAndroid(jcallback,
                                            ScopedJavaLocalRef<jstring>());
    return;
  }

  base::android::RunStringCallbackAndroid(jcallback, url->spec());
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
    RenderFrameHostImpl* render_frame_host,
    mojo::PendingRemote<service_manager::mojom::InterfaceProvider>
        interface_provider_remote)
    : render_frame_host_(render_frame_host),
      interface_provider_remote_(std::move(interface_provider_remote)) {}

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
    bool is_incognito = render_frame_host_->GetSiteInstance()
                            ->GetBrowserContext()
                            ->IsOffTheRecord();
    ScopedJavaLocalRef<jobject> local_ref = Java_RenderFrameHostImpl_create(
        env, reinterpret_cast<intptr_t>(this),
        render_frame_host_->delegate()->GetJavaRenderFrameHostDelegate(),
        is_incognito, interface_provider_remote_.PassPipe().release().value());
    obj_ = JavaObjectWeakGlobalRef(env, local_ref);
    return local_ref;
  }
  return obj_.get(env);
}

ScopedJavaLocalRef<jstring> RenderFrameHostAndroid::GetLastCommittedURL(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) const {
  return ConvertUTF8ToJavaString(
      env, render_frame_host_->GetLastCommittedURL().spec());
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
  render_frame_host_->GetCanonicalUrlForSharing(base::BindOnce(
      &OnGetCanonicalUrlForSharing,
      base::android::ScopedJavaGlobalRef<jobject>(env, jcallback)));
}

bool RenderFrameHostAndroid::IsFeatureEnabled(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>&,
    jint feature) const {
  return render_frame_host_->IsFeatureEnabled(
      static_cast<blink::mojom::FeaturePolicyFeature>(feature));
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

jboolean RenderFrameHostAndroid::IsRenderFrameCreated(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>&) const {
  return render_frame_host_->IsRenderFrameCreated();
}

jboolean RenderFrameHostAndroid::IsProcessBlocked(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>&) const {
  return render_frame_host_->GetProcess()->IsBlocked();
}

jint RenderFrameHostAndroid::PerformGetAssertionWebAuthSecurityChecks(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>&,
    const base::android::JavaParamRef<jstring>& relying_party_id,
    const base::android::JavaParamRef<jobject>& effective_origin) const {
  url::Origin origin = url::Origin::FromJavaObject(effective_origin);
  return static_cast<int32_t>(
      render_frame_host_->PerformGetAssertionWebAuthSecurityChecks(
          ConvertJavaStringToUTF8(env, relying_party_id), origin));
}

jint RenderFrameHostAndroid::PerformMakeCredentialWebAuthSecurityChecks(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>&,
    const base::android::JavaParamRef<jstring>& relying_party_id,
    const base::android::JavaParamRef<jobject>& effective_origin) const {
  url::Origin origin = url::Origin::FromJavaObject(effective_origin);
  return static_cast<int32_t>(
      render_frame_host_->PerformMakeCredentialWebAuthSecurityChecks(
          ConvertJavaStringToUTF8(env, relying_party_id), origin));
}

}  // namespace content
