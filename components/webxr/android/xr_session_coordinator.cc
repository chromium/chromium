// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webxr/android/xr_session_coordinator.h"

#include <memory>
#include <utility>

#include "base/android/jni_string.h"
#include "components/webxr/android/webxr_utils.h"
#include "device/vr/android/compositor_delegate_provider.h"
#include "device/vr/buildflags/buildflags.h"
#include "gpu/ipc/common/gpu_surface_tracker.h"
#include "ui/android/window_android.h"
#include "ui/gl/android/scoped_a_native_window.h"
#include "ui/gl/android/scoped_java_surface.h"

#if BUILDFLAG(ENABLE_ARCORE)
#include "base/android/bundle_utils.h"
#include "device/vr/android/arcore/arcore_shim.h"
#endif

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/webxr/android/xr_jni_headers/XrSessionCoordinator_jni.h"

using base::android::AttachCurrentThread;
using base::android::ScopedJavaLocalRef;

namespace webxr {

XrSessionCoordinator::XrSessionCoordinator() {
  JNIEnv* env = AttachCurrentThread();
  if (!env) {
    return;
  }
  ScopedJavaLocalRef<jobject> j_xr_session_coordinator =
      Java_XrSessionCoordinator_create(env, (jlong)this);
  if (j_xr_session_coordinator.is_null()) {
    return;
  }
  j_xr_session_coordinator_.Reset(j_xr_session_coordinator);
}

XrSessionCoordinator::~XrSessionCoordinator() {
  JNIEnv* env = AttachCurrentThread();
  Java_XrSessionCoordinator_onNativeDestroy(env, j_xr_session_coordinator_);
}

void XrSessionCoordinator::RequestArSession(
    int render_process_id,
    int render_frame_id,
    bool use_overlay,
    bool can_render_dom_content,
    const device::CompositorDelegateProvider& compositor_delegate_provider,
    device::SurfaceReadyCallback ready_callback,
    device::SurfaceTouchCallback touch_callback,
    device::JavaShutdownCallback destroyed_callback) {
  DVLOG(1) << __func__;
  JNIEnv* env = AttachCurrentThread();

  surface_ready_callback_ = std::move(ready_callback);
  surface_touch_callback_ = std::move(touch_callback);
  java_shutdown_callback_ = std::move(destroyed_callback);

  Java_XrSessionCoordinator_startArSession(
      env, j_xr_session_coordinator_,
      compositor_delegate_provider.GetJavaObject(),
      webxr::GetJavaWebContents(render_process_id, render_frame_id),
      use_overlay, can_render_dom_content);
}

void XrSessionCoordinator::RequestVrSession(
    int render_process_id,
    int render_frame_id,
    const device::CompositorDelegateProvider& compositor_delegate_provider,
    device::SurfaceReadyCallback ready_callback,
    device::SurfaceTouchCallback touch_callback,
    device::JavaShutdownCallback destroyed_callback,
    device::XrSessionButtonTouchedCallback button_touched_callback) {
  DVLOG(1) << __func__;
  JNIEnv* env = AttachCurrentThread();

  surface_ready_callback_ = std::move(ready_callback);
  surface_touch_callback_ = std::move(touch_callback);
  java_shutdown_callback_ = std::move(destroyed_callback);
  xr_button_touched_callback_ = std::move(button_touched_callback);

  Java_XrSessionCoordinator_startVrSession(
      env, j_xr_session_coordinator_,
      compositor_delegate_provider.GetJavaObject(),
      webxr::GetJavaWebContents(render_process_id, render_frame_id));
}

void XrSessionCoordinator::RequestXrSession(
    ActivityReadyCallback ready_callback,
    device::JavaShutdownCallback shutdown_callback) {
  DVLOG(1) << __func__;
  JNIEnv* env = AttachCurrentThread();

  activity_ready_callback_ = std::move(ready_callback);
  java_shutdown_callback_ = std::move(shutdown_callback);

  Java_XrSessionCoordinator_startXrSession(env, j_xr_session_coordinator_);
}

void XrSessionCoordinator::EndSession() {
  DVLOG(1) << __func__;
  JNIEnv* env = AttachCurrentThread();

  Java_XrSessionCoordinator_endSession(env, j_xr_session_coordinator_);
}

void XrSessionCoordinator::OnDrawingSurfaceReady(
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
          gpu::SurfaceRecord(std::move(scoped_surface),
                             /*can_be_used_with_surface_control=*/false));
  ui::WindowAndroid* root_window =
      ui::WindowAndroid::FromJavaWindowAndroid(java_root_window);
  display::Display::Rotation display_rotation =
      static_cast<display::Display::Rotation>(rotation);
  surface_ready_callback_.Run(window.a_native_window(), surface_handle,
                              root_window, display_rotation, {width, height});
}

void XrSessionCoordinator::OnDrawingSurfaceTouch(
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

void XrSessionCoordinator::OnJavaShutdown(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  DVLOG(1) << __func__ << ":::";
  if (java_shutdown_callback_) {
    std::move(java_shutdown_callback_).Run();
  }
}

void XrSessionCoordinator::OnXrSessionButtonTouched(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  DVLOG(1) << __func__ << ":::";
  if (xr_button_touched_callback_) {
    std::move(xr_button_touched_callback_).Run();
  }
}

void XrSessionCoordinator::OnXrHostActivityReady(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    const base::android::JavaParamRef<jobject>& activity) {
  DVLOG(1) << __func__;
  if (activity_ready_callback_) {
    std::move(activity_ready_callback_).Run(activity);
  }
}

bool XrSessionCoordinator::EnsureARCoreLoaded() {
#if BUILDFLAG(ENABLE_ARCORE)
  DCHECK(device::IsArCoreSupported());

  // TODO(crbug.com/41414239): Allow loading the ARCore shim by name instead of
  // by absolute path.
  std::string path = base::android::BundleUtils::ResolveLibraryPath(
      /*library_name=*/"arcore_sdk_c", /*split_name=*/"");

  // Crash in debug builds if `path` is empty but handle this situation in
  // release builds. This is done by design - the `path` will be empty only if
  // there was a regression introduced to our gn/gni files w/o causing a build
  // break. In release builds, this approach will result in the site not being
  // able to request an AR session. We need to ensure that both loadable_modules
  // and secondary_abi_loadable_modules are set correctly when building and that
  // we're in the split we expect them to be (currently not passing a split
  // name).
  if (path.empty()) {
    LOG(DFATAL) << "Unable to find path to ARCore SDK library";
    return false;
  }

  return device::LoadArCoreSdk(path);
#else  // BUILDFLAG(ENABLE_ARCORE)
  return false;
#endif
}

ScopedJavaLocalRef<jobject> XrSessionCoordinator::GetCurrentActivityContext() {
  JNIEnv* env = AttachCurrentThread();
  return Java_XrSessionCoordinator_getCurrentActivityContext(env);
}

ScopedJavaLocalRef<jobject> XrSessionCoordinator::GetActivityFrom(
    int render_process_id,
    int render_frame_id) {
  return GetActivity(
      webxr::GetJavaWebContents(render_process_id, render_frame_id));
}

// static
ScopedJavaLocalRef<jobject> XrSessionCoordinator::GetApplicationContext() {
  JNIEnv* env = AttachCurrentThread();
  return Java_XrSessionCoordinator_getApplicationContext(env);
}

// static
ScopedJavaLocalRef<jobject> XrSessionCoordinator::GetActivity(
    ScopedJavaLocalRef<jobject> web_contents) {
  JNIEnv* env = AttachCurrentThread();
  return Java_XrSessionCoordinator_getActivity(env, web_contents);
}

}  // namespace webxr
