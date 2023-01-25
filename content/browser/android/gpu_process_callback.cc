// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/android/scoped_java_ref.h"
#include "base/android/unguessable_token_android.h"
#include "base/functional/overloaded.h"
#include "content/browser/android/scoped_surface_request_manager.h"
#include "content/common/android/surface_wrapper.h"
#include "content/public/android/content_jni_headers/GpuProcessCallback_jni.h"
#include "content/public/browser/browser_thread.h"
#include "gpu/ipc/common/gpu_surface_tracker.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

namespace content {

void JNI_GpuProcessCallback_CompleteScopedSurfaceRequest(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& token,
    const base::android::JavaParamRef<jobject>& surface) {
  absl::optional<base::UnguessableToken> requestToken =
      base::android::UnguessableTokenAndroid::FromJavaUnguessableToken(env,
                                                                       token);
  if (!requestToken) {
    DLOG(ERROR) << "Received invalid surface request token.";
    return;
  }

  DCHECK(!BrowserThread::CurrentlyOn(BrowserThread::UI));

  base::android::ScopedJavaGlobalRef<jobject> jsurface;
  jsurface.Reset(env, surface);
  ScopedSurfaceRequestManager::GetInstance()->FulfillScopedSurfaceRequest(
      requestToken.value(),
      gl::ScopedJavaSurface(jsurface, /*auto_release=*/true));
}

base::android::ScopedJavaLocalRef<jobject>
JNI_GpuProcessCallback_GetViewSurface(
    JNIEnv* env,
    jint surface_id) {
  base::android::ScopedJavaLocalRef<jobject> j_surface_wrapper;
  bool can_be_used_with_surface_control = false;
  auto surface_variant =
      gpu::GpuSurfaceTracker::GetInstance()->AcquireJavaSurface(
          surface_id, &can_be_used_with_surface_control);
  absl::visit(
      base::Overloaded{[&](gl::ScopedJavaSurface&& scoped_java_surface) {
                         if (!scoped_java_surface.IsEmpty()) {
                           j_surface_wrapper = JNI_SurfaceWrapper_create(
                               env, scoped_java_surface.j_surface(),
                               can_be_used_with_surface_control);
                         }
                       },
                       [&](gl::ScopedJavaSurfaceControl&& surface_control) {
                         j_surface_wrapper =
                             JNI_SurfaceWrapper_createFromSurfaceControl(
                                 env, std::move(surface_control));
                       }},
      std::move(surface_variant));
  return j_surface_wrapper;
}

}  // namespace content
