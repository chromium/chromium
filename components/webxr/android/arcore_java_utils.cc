// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webxr/android/arcore_java_utils.h"

#include <memory>
#include <utility>

#include "base/android/jni_string.h"
#include "components/webxr/android/ar_jni_headers/ArCoreJavaUtils_jni.h"
#include "components/webxr/android/webxr_utils.h"
#include "device/vr/android/arcore/arcore_shim.h"
#include "gpu/ipc/common/gpu_surface_tracker.h"
#include "ui/android/window_android.h"
#include "ui/gl/android/scoped_a_native_window.h"
#include "ui/gl/android/scoped_java_surface.h"

using base::android::AttachCurrentThread;
using base::android::ScopedJavaLocalRef;

namespace webxr {

ArCoreJavaUtils::ArCoreJavaUtils(
    webxr::ArCompositorDelegateProvider compositor_delegate_provider)
    : compositor_delegate_provider_(compositor_delegate_provider) {
  JNIEnv* env = AttachCurrentThread();
  if (!env)
    return;
  ScopedJavaLocalRef<jobject> j_arcore_java_utils =
      Java_ArCoreJavaUtils_create(env, (jlong)this);
  if (j_arcore_java_utils.is_null())
    return;
  j_arcore_java_utils_.Reset(j_arcore_java_utils);
}

ArCoreJavaUtils::~ArCoreJavaUtils() {
  JNIEnv* env = AttachCurrentThread();
  Java_ArCoreJavaUtils_onNativeDestroy(env, j_arcore_java_utils_);
}

void ArCoreJavaUtils::RequestArSession(
    int render_process_id,
    int render_frame_id,
    bool use_overlay,
    bool can_render_dom_content,
    device::SurfaceReadyCallback ready_callback,
    device::SurfaceTouchCallback touch_callback,
    device::SurfaceDestroyedCallback destroyed_callback) {
  DVLOG(1) << __func__;
  JNIEnv* env = AttachCurrentThread();

  surface_ready_callback_ = std::move(ready_callback);
  surface_touch_callback_ = std::move(touch_callback);
  surface_destroyed_callback_ = std::move(destroyed_callback);

  Java_ArCoreJavaUtils_startSession(
      env, j_arcore_java_utils_, compositor_delegate_provider_.GetJavaObject(),
      webxr::GetJavaWebContents(render_process_id, render_frame_id),
      use_overlay, can_render_dom_content);
}

void ArCoreJavaUtils::EndSession() {
  DVLOG(1) << __func__;
  JNIEnv* env = AttachCurrentThread();

  Java_ArCoreJavaUtils_endSession(env, j_arcore_java_utils_);
}

void ArCoreJavaUtils::OnDrawingSurfaceReady(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    const base::android::JavaParamRef<jobject>& surface,
    const base::android::JavaParamRef<jobject>& java_root_window,
    int rotation,
    int width,
    int height) {
  DVLOG(1) << __func__ << ": width=" << width << " height=" << height
           << " rotation=" << rotation;
  gl::ScopedJavaSurface scoped_surface(surface, /*auto_release=*/false);
  gl::ScopedANativeWindow window(scoped_surface);
  gpu::SurfaceHandle surface_handle =
      gpu::GpuSurfaceTracker::Get()->AddSurfaceForNativeWidget(
          gpu::GpuSurfaceTracker::SurfaceRecord(
              std::move(scoped_surface),
              /*can_be_used_with_surface_control=*/false));
  ui::WindowAndroid* root_window =
      ui::WindowAndroid::FromJavaWindowAndroid(java_root_window);
  display::Display::Rotation display_rotation =
      static_cast<display::Display::Rotation>(rotation);
  surface_ready_callback_.Run(window.a_native_window(), surface_handle,
                              root_window, display_rotation, {width, height});
}

void ArCoreJavaUtils::OnDrawingSurfaceTouch(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    bool primary,
    bool touching,
    int32_t pointer_id,
    float x,
    float y) {
  DVLOG(3) << __func__ << ": pointer_id=" << pointer_id
           << " primary=" << primary << " touching=" << touching;
  surface_touch_callback_.Run(primary, touching, pointer_id, {x, y});
}

void ArCoreJavaUtils::OnDrawingSurfaceDestroyed(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  DVLOG(1) << __func__ << ":::";
  if (surface_destroyed_callback_) {
    std::move(surface_destroyed_callback_).Run();
  }
}

bool ArCoreJavaUtils::EnsureLoaded() {
  DCHECK(device::IsArCoreSupported());

  JNIEnv* env = AttachCurrentThread();

  // TODO(crbug.com/884780): Allow loading the ARCore shim by name instead of by
  // absolute path.
  ScopedJavaLocalRef<jstring> java_path =
      Java_ArCoreJavaUtils_getArCoreShimLibraryPath(env);

  // Crash in debug builds if `java_path` is a null pointer but handle this
  // situation in release builds. This is done by design - the `java_path` will
  // be null only if there was a regression introduced to our gn/gni files w/o
  // causing a build break. In release builds, this approach will result in the
  // site not being able to request an AR session.
  DCHECK(java_path)
      << "Unable to find path to ARCore SDK library - please ensure that "
         "loadable_modules and secondary_abi_loadable_modules are set "
         "correctly when building";
  if (!java_path) {
    LOG(ERROR) << "Unable to find path to ARCore SDK library";
    return false;
  }

  return device::LoadArCoreSdk(
      base::android::ConvertJavaStringToUTF8(env, java_path));
}

ScopedJavaLocalRef<jobject> ArCoreJavaUtils::GetApplicationContext() {
  JNIEnv* env = AttachCurrentThread();
  return Java_ArCoreJavaUtils_getApplicationContext(env);
}

}  // namespace webxr
