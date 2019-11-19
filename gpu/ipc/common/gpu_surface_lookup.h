// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_IPC_COMMON_GPU_SURFACE_LOOKUP_H_
#define GPU_IPC_COMMON_GPU_SURFACE_LOOKUP_H_

#include "base/macros.h"
#include "gpu/gpu_export.h"
#include "gpu/ipc/common/surface_handle.h"
#include "ui/gfx/native_widget_types.h"

#if defined(OS_ANDROID)
#include "ui/gl/android/scoped_java_surface.h"
#endif

namespace gpu {

// This class provides an interface to look up window surface handles
// that cannot be sent through the IPC channel.
class GPU_EXPORT GpuSurfaceLookup {
 public:
  GpuSurfaceLookup() {}
  virtual ~GpuSurfaceLookup() {}

  static GpuSurfaceLookup* GetInstance();
  static void InitInstance(GpuSurfaceLookup* lookup);

  virtual gfx::AcceleratedWidget AcquireNativeWidget(
      gpu::SurfaceHandle surface_handle,
      bool* can_be_used_with_surface_control) = 0;

#if defined(OS_ANDROID)
  virtual gl::ScopedJavaSurface AcquireJavaSurface(
      int surface_id,
      bool* can_be_used_with_surface_control) = 0;
#endif

 private:
  DISALLOW_COPY_AND_ASSIGN(GpuSurfaceLookup);
};

}  // namespace gpu

#endif  // GPU_IPC_COMMON_GPU_SURFACE_LOOKUP_H_
