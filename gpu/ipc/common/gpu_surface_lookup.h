// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_IPC_COMMON_GPU_SURFACE_LOOKUP_H_
#define GPU_IPC_COMMON_GPU_SURFACE_LOOKUP_H_

#include "gpu/gpu_export.h"
#include "gpu/ipc/common/surface_handle.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gl/android/scoped_java_surface.h"

namespace gpu {

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

  virtual gl::ScopedJavaSurface AcquireJavaSurface(
      int surface_id,
      bool* can_be_used_with_surface_control) = 0;
};

}  // namespace gpu

#endif  // GPU_IPC_COMMON_GPU_SURFACE_LOOKUP_H_
