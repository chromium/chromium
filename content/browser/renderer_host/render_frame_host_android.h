// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_RENDER_FRAME_HOST_ANDROID_H_
#define CONTENT_BROWSER_RENDERER_HOST_RENDER_FRAME_HOST_ANDROID_H_

#include <jni.h>

#include "base/android/jni_android.h"
#include "base/android/jni_weak_ref.h"
#include "base/android/scoped_java_ref.h"
#include "base/memory/raw_ptr.h"
#include "base/supports_user_data.h"
#include "base/unguessable_token.h"

namespace content {

class RenderFrameHostImpl;

// Android wrapper around RenderFrameHost that provides safer passage from java
// and back to native and provides java with a means of communicating with its
// native counterpart.
class RenderFrameHostAndroid : public base::SupportsUserData::Data {
 public:
  RenderFrameHostAndroid(RenderFrameHostImpl* render_frame_host);

  RenderFrameHostAndroid(const RenderFrameHostAndroid&) = delete;
  RenderFrameHostAndroid& operator=(const RenderFrameHostAndroid&) = delete;

  ~RenderFrameHostAndroid() override;

  base::android::ScopedJavaLocalRef<jobject> GetJavaObject();

  // Methods called from Java
  base::android::ScopedJavaLocalRef<jobject> GetLastCommittedURL(
      JNIEnv* env) const;

  base::android::ScopedJavaLocalRef<jobject> GetLastCommittedOrigin(
      JNIEnv* env);

  base::android::ScopedJavaLocalRef<jobject> GetMainFrame(JNIEnv* env);

  void GetCanonicalUrlForSharing(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jcallback) const;

  std::vector<jni_zero::ScopedJavaLocalRef<jobject>> GetAllRenderFrameHosts(
      JNIEnv* env) const;

  bool IsFeatureEnabled(JNIEnv* env,
                        jint feature) const;

  base::UnguessableToken GetAndroidOverlayRoutingToken(JNIEnv* env) const;

  void NotifyUserActivation(JNIEnv* env);

  void NotifyWebAuthnAssertionRequestSucceeded(JNIEnv* env);

  jboolean IsCloseWatcherActive(JNIEnv* env) const;

  jboolean SignalCloseWatcherIfActive(JNIEnv* env) const;

  jboolean IsRenderFrameLive(JNIEnv* env) const;

  void GetInterfaceToRendererFrame(
      JNIEnv* env,
      const base::android::JavaParamRef<jstring>& interface_name,
      jlong message_pipe_handle) const;

  void TerminateRendererDueToBadMessage(
      JNIEnv* env,
      jint reason) const;

  jboolean IsProcessBlocked(JNIEnv* env) const;

  void PerformGetAssertionWebAuthSecurityChecks(
      JNIEnv* env,
      const base::android::JavaParamRef<jstring>&,
      const base::android::JavaParamRef<jobject>&,
      jboolean is_payment_credential_get_assertion,
      const base::android::JavaParamRef<jobject>& callback) const;

  void PerformMakeCredentialWebAuthSecurityChecks(
      JNIEnv* env,
      const base::android::JavaParamRef<jstring>&,
      const base::android::JavaParamRef<jobject>&,
      jboolean is_payment_credential_creation,
      const base::android::JavaParamRef<jobject>& callback) const;

  jint GetLifecycleState(JNIEnv* env) const;

  void InsertVisualStateCallback(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jcallback);

  void ExecuteJavaScriptInIsolatedWorld(
      JNIEnv* env,
      const base::android::JavaParamRef<jstring>& jstring,
      jint jworldId,
      const base::android::JavaParamRef<jobject>& jcallback);

  RenderFrameHostImpl* render_frame_host() const { return render_frame_host_; }

 private:
  const raw_ptr<RenderFrameHostImpl> render_frame_host_;
  JavaObjectWeakGlobalRef obj_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_RENDER_FRAME_HOST_ANDROID_H_
