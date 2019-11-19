// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <android/native_window_jni.h>
#include <cpu-features.h>

#include "base/android/jni_array.h"
#include "base/android/library_loader/library_loader_hooks.h"
#include "base/android/memory_pressure_listener_android.h"
#include "base/android/unguessable_token_android.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/unguessable_token.h"
#include "content/child/child_thread_impl.h"
#include "content/common/android/surface_wrapper.h"
#include "content/public/android/content_jni_headers/ContentChildProcessServiceDelegate_jni.h"
#include "content/public/common/content_descriptors.h"
#include "content/public/common/content_switches.h"
#include "gpu/command_buffer/service/texture_owner.h"
#include "gpu/ipc/common/android/scoped_surface_request_conduit.h"
#include "gpu/ipc/common/gpu_surface_lookup.h"
#include "services/service_manager/embedder/shared_file_util.h"
#include "services/service_manager/embedder/switches.h"
#include "ui/gl/android/scoped_java_surface.h"
#include "ui/gl/android/surface_texture.h"

using base::android::AttachCurrentThread;
using base::android::JavaParamRef;

namespace content {

namespace {

// TODO(sievers): Use two different implementations of this depending on if
// we're in a renderer or gpu process.
class ChildProcessSurfaceManager : public gpu::ScopedSurfaceRequestConduit,
                                   public gpu::GpuSurfaceLookup {
 public:
  ChildProcessSurfaceManager() {}
  ~ChildProcessSurfaceManager() override {}

  // |service_impl| is the instance of
  // org.chromium.content.app.ChildProcessService.
  void SetServiceImpl(const base::android::JavaRef<jobject>& service_impl) {
    service_impl_.Reset(service_impl);
  }

  // Overriden from ScopedSurfaceRequestConduit:
  void ForwardSurfaceOwnerForSurfaceRequest(
      const base::UnguessableToken& request_token,
      const gpu::TextureOwner* texture_owner) override {
    JNIEnv* env = base::android::AttachCurrentThread();

    content::
        Java_ContentChildProcessServiceDelegate_forwardSurfaceForSurfaceRequest(
            env, service_impl_,
            base::android::UnguessableTokenAndroid::Create(env, request_token),
            texture_owner->CreateJavaSurface().j_surface());
  }

  // Overridden from GpuSurfaceLookup:
  gfx::AcceleratedWidget AcquireNativeWidget(
      int surface_id,
      bool* can_be_used_with_surface_control) override {
    JNIEnv* env = base::android::AttachCurrentThread();
    base::android::ScopedJavaLocalRef<jobject> surface_wrapper =
        content::Java_ContentChildProcessServiceDelegate_getViewSurface(
            env, service_impl_, surface_id);
    if (!surface_wrapper)
      return gfx::kNullAcceleratedWidget;

    gl::ScopedJavaSurface surface(
        content::JNI_SurfaceWrapper_getSurface(env, surface_wrapper));
    DCHECK(!surface.j_surface().is_null());

    // Note: This ensures that any local references used by
    // ANativeWindow_fromSurface are released immediately. This is needed as a
    // workaround for https://code.google.com/p/android/issues/detail?id=68174
    base::android::ScopedJavaLocalFrame scoped_local_reference_frame(env);
    ANativeWindow* native_window =
        ANativeWindow_fromSurface(env, surface.j_surface().obj());

    *can_be_used_with_surface_control =
        content::JNI_SurfaceWrapper_canBeUsedWithSurfaceControl(
            env, surface_wrapper);
    return native_window;
  }

  // Overridden from GpuSurfaceLookup:
  gl::ScopedJavaSurface AcquireJavaSurface(
      int surface_id,
      bool* can_be_used_with_surface_control) override {
    JNIEnv* env = base::android::AttachCurrentThread();
    base::android::ScopedJavaLocalRef<jobject> surface_wrapper =
        content::Java_ContentChildProcessServiceDelegate_getViewSurface(
            env, service_impl_, surface_id);
    if (!surface_wrapper)
      return gl::ScopedJavaSurface();

    gl::ScopedJavaSurface surface(
        content::JNI_SurfaceWrapper_getSurface(env, surface_wrapper));
    DCHECK(!surface.j_surface().is_null());

    *can_be_used_with_surface_control =
        content::JNI_SurfaceWrapper_canBeUsedWithSurfaceControl(
            env, surface_wrapper);
    return surface;
  }

 private:
  friend struct base::LazyInstanceTraitsBase<ChildProcessSurfaceManager>;
  // The instance of org.chromium.content.app.ChildProcessService.
  base::android::ScopedJavaGlobalRef<jobject> service_impl_;

  DISALLOW_COPY_AND_ASSIGN(ChildProcessSurfaceManager);
};

base::LazyInstance<ChildProcessSurfaceManager>::Leaky
    g_child_process_surface_manager = LAZY_INSTANCE_INITIALIZER;

// Chrome actually uses the renderer code path for all of its child
// processes such as renderers, plugins, etc.
void JNI_ContentChildProcessServiceDelegate_InternalInitChildProcess(
    JNIEnv* env,
    const JavaParamRef<jobject>& service_impl,
    jint cpu_count,
    jlong cpu_features) {
  // Set the CPU properties.
  android_setCpu(cpu_count, cpu_features);

  g_child_process_surface_manager.Get().SetServiceImpl(service_impl);

  gpu::GpuSurfaceLookup::InitInstance(
      g_child_process_surface_manager.Pointer());
  gpu::ScopedSurfaceRequestConduit::SetInstance(
      g_child_process_surface_manager.Pointer());

  base::android::MemoryPressureListenerAndroid::Initialize(env);
}

}  // namespace

void JNI_ContentChildProcessServiceDelegate_InitChildProcess(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jint cpu_count,
    jlong cpu_features) {
  JNI_ContentChildProcessServiceDelegate_InternalInitChildProcess(
      env, obj, cpu_count, cpu_features);
}

void JNI_ContentChildProcessServiceDelegate_RetrieveFileDescriptorsIdsToKeys(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  std::map<int, std::string> ids_to_keys;
  std::string file_switch_value =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          service_manager::switches::kSharedFiles);

  std::vector<int> ids;
  std::vector<std::string> keys;
  if (!file_switch_value.empty()) {
    base::Optional<std::map<int, std::string>> ids_to_keys_from_command_line =
        service_manager::ParseSharedFileSwitchValue(file_switch_value);
    if (ids_to_keys_from_command_line) {
      for (auto iter : *ids_to_keys_from_command_line) {
        ids.push_back(iter.first);
        keys.push_back(iter.second);
      }
    }
  }

  Java_ContentChildProcessServiceDelegate_setFileDescriptorsIdsToKeys(
      env, obj, base::android::ToJavaIntArray(env, ids),
      base::android::ToJavaArrayOfStrings(env, keys));
}

}  // namespace content
