// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_IPC_COMMON_GPU_SURFACE_LOOKUP_H_
#define GPU_IPC_COMMON_GPU_SURFACE_LOOKUP_H_

#include "base/android/scoped_java_ref.h"
#include "gpu/gpu_export.h"
#include "gpu/ipc/common/surface_handle.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gl/android/scoped_java_surface.h"
#include "ui/gl/android/scoped_java_surface_control.h"

namespace gpu {

using JavaSurfaceVariant =
    absl::variant<gl::ScopedJavaSurface, gl::ScopedJavaSurfaceControl>;

struct GPU_EXPORT SurfaceRecord {
  SurfaceRecord(
      gl::ScopedJavaSurface surface,
      bool can_be_used_with_surface_control,
      const base::android::JavaRef<jobject>& host_input_token = nullptr);
  explicit SurfaceRecord(gl::ScopedJavaSurfaceControl surface_control);
  ~SurfaceRecord();

  SurfaceRecord(SurfaceRecord&&);
  SurfaceRecord(const SurfaceRecord&) = delete;

  JavaSurfaceVariant surface_variant;
  bool can_be_used_with_surface_control = false;
  // Host input transfer token gotten from Android Window's root surface
  // control.
  base::android::ScopedJavaGlobalRef<jobject> host_input_token;
};
// This class provides an interface to look up window surface handles
// that cannot be sent through the IPC channel.
class GPU_EXPORT GpuSurfaceLookup {
 public:
  GpuSurfaceLookup() {}

  GpuSurfaceLookup(const GpuSurfaceLookup&) = delete;
  GpuSurfaceLookup& operator=(const GpuSurfaceLookup&) = delete;

  virtual ~GpuSurfaceLookup() {}

  static GpuSurfaceLookup* GetInstance();
  static void InitInstance(GpuSurfaceLookup* lookup);

  virtual SurfaceRecord AcquireJavaSurface(int surface_id) = 0;
};

}  // namespace gpu

#endif  // GPU_IPC_COMMON_GPU_SURFACE_LOOKUP_H_
