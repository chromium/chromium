// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/common/gpu_surface_lookup.h"

#include <jni.h>

#include "base/check.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "ui/gl/surface_jni_headers/InputTransferToken_jni.h"

namespace gpu {

namespace {
GpuSurfaceLookup* g_instance = nullptr;
}  // anonymous namespace

SurfaceRecord::SurfaceRecord(
    gl::ScopedJavaSurface surface,
    bool can_be_used_with_surface_control,
    const base::android::JavaRef<jobject>& host_input_token)
    : surface_variant(std::move(surface)),
      can_be_used_with_surface_control(can_be_used_with_surface_control),
      host_input_token(host_input_token) {
#if DCHECK_IS_ON()
  JNIEnv* env = jni_zero::AttachCurrentThread();
  if (host_input_token) {
    DCHECK(env->IsInstanceOf(host_input_token.obj(),
                             android_window_InputTransferToken_clazz(env)));
  }
#endif  // DCHECK_IS_ON()
}

SurfaceRecord::SurfaceRecord(gl::ScopedJavaSurfaceControl surface_control)
    : surface_variant(std::move(surface_control)),
      can_be_used_with_surface_control(true) {}

SurfaceRecord::~SurfaceRecord() = default;
SurfaceRecord::SurfaceRecord(SurfaceRecord&&) = default;

// static
GpuSurfaceLookup* GpuSurfaceLookup::GetInstance() {
  DCHECK(g_instance);
  return g_instance;
}

// static
void GpuSurfaceLookup::InitInstance(GpuSurfaceLookup* lookup) {
  DCHECK(!g_instance || !lookup);
  g_instance = lookup;
}

}  // namespace gpu
