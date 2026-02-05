// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_RENDER_FRAME_HOST_ANDROID_H_
#define CONTENT_BROWSER_RENDERER_HOST_RENDER_FRAME_HOST_ANDROID_H_

#include <jni.h>

#include <optional>

#include "base/android/jni_android.h"
#include "base/android/jni_weak_ref.h"
#include "base/android/scoped_java_ref.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/supports_user_data.h"
#include "base/unguessable_token.h"

class GURL;

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
      base::OnceCallback<void(const std::optional<GURL>&)> callback) const;

  std::vector<jni_zero::ScopedJavaLocalRef<jobject>> GetAllRenderFrameHosts(
      JNIEnv* env) const;

  bool IsFeatureEnabled(JNIEnv* env, int32_t feature) const;

  base::UnguessableToken GetAndroidOverlayRoutingToken(JNIEnv* env) const;

  void NotifyUserActivation(JNIEnv* env);

  void NotifyWebAuthnAssertionRequestSucceeded(JNIEnv* env);

  bool IsCloseWatcherActive(JNIEnv* env) const;

  bool SignalCloseWatcherIfActive(JNIEnv* env) const;

  bool IsRenderFrameLive(JNIEnv* env) const;

  void GetInterfaceToRendererFrame(
      JNIEnv* env,
      const base::android::JavaRef<jstring>& interface_name,
      int64_t message_pipe_handle) const;

  void TerminateRendererDueToBadMessage(JNIEnv* env, int32_t reason) const;

  bool IsProcessBlocked(JNIEnv* env) const;

  void PerformGetAssertionWebAuthSecurityChecks(
      JNIEnv* env,
      const base::android::JavaRef<jstring>&,
      const base::android::JavaRef<jobject>&,
      bool is_payment_credential_get_assertion,
      const base::android::JavaRef<jobject>&
          remote_desktop_client_override_origin,
      const base::android::JavaRef<jobject>& callback) const;

  void PerformMakeCredentialWebAuthSecurityChecks(
      JNIEnv* env,
      const base::android::JavaRef<jstring>&,
      const base::android::JavaRef<jobject>&,
      bool is_payment_credential_creation,
      const base::android::JavaRef<jobject>&
          remote_desktop_client_override_origin,
      const base::android::JavaRef<jobject>& callback) const;

  void PerformReportWebAuthSecurityChecks(
      JNIEnv* env,
      const base::android::JavaRef<jstring>&,
      const base::android::JavaRef<jobject>&,
      const base::android::JavaRef<jobject>& callback) const;

  int32_t GetLifecycleState(JNIEnv* env) const;

  void InsertVisualStateCallback(base::OnceCallback<void(bool)> callback);

  void ExecuteJavaScriptInIsolatedWorld(
      JNIEnv* env,
      const base::android::JavaRef<jstring>& jstring,
      int32_t jworldId,
      const base::android::JavaRef<jobject>& jcallback);

  bool HasHitTestDataForTesting(JNIEnv* env);

  void ViewSource(JNIEnv* env);

  RenderFrameHostImpl* render_frame_host() const { return render_frame_host_; }

 private:
  const raw_ptr<RenderFrameHostImpl> render_frame_host_;
  JavaObjectWeakGlobalRef obj_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_RENDER_FRAME_HOST_ANDROID_H_
