// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBXR_ANDROID_XR_SESSION_COORDINATOR_H_
#define COMPONENTS_WEBXR_ANDROID_XR_SESSION_COORDINATOR_H_

#include <jni.h>

#include "base/android/scoped_java_ref.h"
#include "base/functional/callback.h"
#include "device/vr/android/xr_java_coordinator.h"

namespace webxr {

using ActivityReadyCallback = base::OnceCallback<void(
    const base::android::JavaParamRef<jobject>& activity)>;

class XrSessionCoordinator : public device::XrJavaCoordinator {
 public:
  // Used to return the ContextUtils.applicationContext, which may not be the
  // activity.
  static base::android::ScopedJavaLocalRef<jobject> GetApplicationContext();

  // Used to query the current Activity from the specified WebContents.
  static base::android::ScopedJavaLocalRef<jobject> GetActivity(
      base::android::ScopedJavaLocalRef<jobject> web_contents);

  explicit XrSessionCoordinator();
  ~XrSessionCoordinator() override;

  // XrJavaCoordinator:
  void RequestArSession(
      int render_process_id,
      int render_frame_id,
      bool use_overlay,
      bool can_render_dom_content,
      const device::CompositorDelegateProvider& compositor_delegate_provider,
      device::SurfaceReadyCallback ready_callback,
      device::SurfaceTouchCallback touch_callback,
      device::JavaShutdownCallback destroyed_callback) override;
  void RequestVrSession(
      int render_process_id,
      int render_frame_id,
      const device::CompositorDelegateProvider& compositor_delegate_provider,
      device::SurfaceReadyCallback ready_callback,
      device::SurfaceTouchCallback touch_callback,
      device::JavaShutdownCallback destroyed_callback,
      device::XrSessionButtonTouchedCallback button_touched_callback) override;
  void EndSession() override;
  bool EnsureARCoreLoaded() override;
  base::android::ScopedJavaLocalRef<jobject> GetCurrentActivityContext()
      override;
  base::android::ScopedJavaLocalRef<jobject> GetActivityFrom(
      int render_process_id,
      int render_frame_id) override;

  void RequestXrSession(ActivityReadyCallback ready_callback,
                        device::JavaShutdownCallback shutdown_callback);

  // Methods called from the Java side.
  void OnDrawingSurfaceReady(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jobject>& surface,
      const base::android::JavaParamRef<jobject>& root_window,
      int rotation,
      int width,
      int height);
  void OnDrawingSurfaceTouch(JNIEnv* env,
                             const base::android::JavaParamRef<jobject>& obj,
                             bool primary,
                             bool touching,
                             int32_t pointer_id,
                             float x,
                             float y);
  void OnJavaShutdown(JNIEnv* env,
                      const base::android::JavaParamRef<jobject>& obj);
  void OnXrSessionButtonTouched(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);
  void OnXrHostActivityReady(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jobject>& activity);

 private:
  base::android::ScopedJavaGlobalRef<jobject> j_xr_session_coordinator_;

  device::SurfaceReadyCallback surface_ready_callback_;
  device::SurfaceTouchCallback surface_touch_callback_;
  device::JavaShutdownCallback java_shutdown_callback_;
  device::XrSessionButtonTouchedCallback xr_button_touched_callback_;
  ActivityReadyCallback activity_ready_callback_;
};

}  // namespace webxr

#endif  // COMPONENTS_WEBXR_ANDROID_XR_SESSION_COORDINATOR_H_
