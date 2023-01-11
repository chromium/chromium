// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBXR_ANDROID_ARCORE_JAVA_UTILS_H_
#define COMPONENTS_WEBXR_ANDROID_ARCORE_JAVA_UTILS_H_

#include <jni.h>

#include "base/android/scoped_java_ref.h"
#include "base/functional/callback.h"
#include "components/webxr/android/ar_compositor_delegate_provider.h"
#include "device/vr/android/arcore/arcore_session_utils.h"

namespace webxr {

class ArCoreJavaUtils : public device::ArCoreSessionUtils {
 public:
  explicit ArCoreJavaUtils(
      webxr::ArCompositorDelegateProvider compositor_delegate_provider);
  ~ArCoreJavaUtils() override;

  // ArCoreSessionUtils:
  void RequestArSession(
      int render_process_id,
      int render_frame_id,
      bool use_overlay,
      bool can_render_dom_content,
      device::SurfaceReadyCallback ready_callback,
      device::SurfaceTouchCallback touch_callback,
      device::SurfaceDestroyedCallback destroyed_callback) override;
  void EndSession() override;
  bool EnsureLoaded() override;
  base::android::ScopedJavaLocalRef<jobject> GetApplicationContext() override;

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
  void OnDrawingSurfaceDestroyed(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);

 private:
  base::android::ScopedJavaGlobalRef<jobject> j_arcore_java_utils_;

  webxr::ArCompositorDelegateProvider compositor_delegate_provider_;

  device::SurfaceReadyCallback surface_ready_callback_;
  device::SurfaceTouchCallback surface_touch_callback_;
  device::SurfaceDestroyedCallback surface_destroyed_callback_;
};

}  // namespace webxr

#endif  // COMPONENTS_WEBXR_ANDROID_ARCORE_JAVA_UTILS_H_
