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
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>&) const;

  base::android::ScopedJavaLocalRef<jobject> GetLastCommittedOrigin(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>&);

  void GetCanonicalUrlForSharing(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>&,
      const base::android::JavaParamRef<jobject>& jcallback) const;

  base::android::ScopedJavaLocalRef<jobjectArray> GetAllRenderFrameHosts(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>&) const;

  bool IsFeatureEnabled(JNIEnv* env,
                        const base::android::JavaParamRef<jobject>&,
                        jint feature) const;

  // Returns UnguessableToken.
  base::android::ScopedJavaLocalRef<jobject> GetAndroidOverlayRoutingToken(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>&) const;

  void NotifyUserActivation(JNIEnv* env,
                            const base::android::JavaParamRef<jobject>&);

  void NotifyWebAuthnAssertionRequestSucceeded(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>&);

  jboolean IsCloseWatcherActive(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>&) const;

  jboolean SignalCloseWatcherIfActive(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>&) const;

  jboolean IsRenderFrameLive(JNIEnv* env,
                             const base::android::JavaParamRef<jobject>&) const;

  void GetInterfaceToRendererFrame(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>&,
      const base::android::JavaParamRef<jstring>& interface_name,
      jlong message_pipe_handle) const;

  void TerminateRendererDueToBadMessage(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>&,
      jint reason) const;

  jboolean IsProcessBlocked(JNIEnv* env,
                            const base::android::JavaParamRef<jobject>&) const;

  base::android::ScopedJavaLocalRef<jobject>
  PerformGetAssertionWebAuthSecurityChecks(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>&,
      const base::android::JavaParamRef<jstring>&,
      const base::android::JavaParamRef<jobject>&,
      jboolean is_payment_credential_get_assertion) const;

  jint PerformMakeCredentialWebAuthSecurityChecks(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>&,
      const base::android::JavaParamRef<jstring>&,
      const base::android::JavaParamRef<jobject>&,
      jboolean is_payment_credential_creation) const;

  jint GetLifecycleState(JNIEnv* env,
                         const base::android::JavaParamRef<jobject>&) const;

  void InsertVisualStateCallback(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jcallback);

  RenderFrameHostImpl* render_frame_host() const { return render_frame_host_; }

 private:
  const raw_ptr<RenderFrameHostImpl> render_frame_host_;
  JavaObjectWeakGlobalRef obj_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_RENDER_FRAME_HOST_ANDROID_H_
