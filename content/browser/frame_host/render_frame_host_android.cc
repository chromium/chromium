// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/frame_host/render_frame_host_android.h"

#include <utility>

#include "base/android/callback_android.h"
#include "base/android/jni_string.h"
#include "base/android/unguessable_token_android.h"
#include "base/bind.h"
#include "base/logging.h"
#include "content/browser/frame_host/render_frame_host_delegate.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/public/android/content_jni_headers/RenderFrameHostImpl_jni.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/site_instance.h"
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

RenderFrameHostAndroid::RenderFrameHostAndroid(
    RenderFrameHostImpl* render_frame_host,
    service_manager::mojom::InterfaceProviderPtr interface_provider_ptr)
    : render_frame_host_(render_frame_host),
      interface_provider_ptr_(std::move(interface_provider_ptr)) {}

RenderFrameHostAndroid::~RenderFrameHostAndroid() {
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
        is_incognito,
        interface_provider_ptr_.PassInterface().PassHandle().release().value());
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
  render_frame_host_->GetAssociatedLocalFrame()->NotifyUserActivation();
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

}  // namespace content
