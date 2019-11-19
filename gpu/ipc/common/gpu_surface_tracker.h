// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_IPC_COMMON_GPU_SURFACE_TRACKER_H_
#define GPU_IPC_COMMON_GPU_SURFACE_TRACKER_H_

#include <stddef.h>

#include <map>

#include "base/macros.h"
#include "base/memory/singleton.h"
#include "base/synchronization/lock.h"
#include "gpu/gpu_export.h"
#include "gpu/ipc/common/gpu_surface_lookup.h"
#include "gpu/ipc/common/surface_handle.h"
#include "ui/gfx/native_widget_types.h"

namespace gpu {

// This class is used on Android and Mac, and is responsible for tracking native
// window surfaces exposed to the GPU process. Every surface gets registered to
// this class, and gets a handle.  The handle can be passed to
// CommandBufferProxyImpl::Create or to
// GpuMemoryBufferManager::CreateGpuMemoryBuffer.
// On Android, the handle is used in the GPU process to get a reference to the
// ANativeWindow, using GpuSurfaceLookup (implemented by
// ChildProcessSurfaceManager).  We require that an Android Surface is provided
// with the ANativeWindow, so one must provide an explicit GpuSurfaceTracker::
// SurfaceRecord when adding it.
// On Mac, the handle just passes through the GPU process, and is sent back via
// GpuCommandBufferMsg_SwapBuffersCompleted to reference the surface.
// This class is thread safe.
class GPU_EXPORT GpuSurfaceTracker : public gpu::GpuSurfaceLookup {
 public:
  struct SurfaceRecord {
#if defined(OS_ANDROID)
    SurfaceRecord(gfx::AcceleratedWidget widget,
                  jobject j_surface,
                  bool can_be_used_with_surface_control);
#else   // defined(OS_ANDROID)
    explicit SurfaceRecord(gfx::AcceleratedWidget widget);
#endif  // !defined(OS_ANDROID)

    SurfaceRecord(SurfaceRecord&&);
    SurfaceRecord(const SurfaceRecord&) = delete;

    gfx::AcceleratedWidget widget;
#if defined(OS_ANDROID)
    gl::ScopedJavaSurface surface;
    bool can_be_used_with_surface_control;
#endif
  };

  // GpuSurfaceLookup implementation:
  // Returns the native widget associated with a given surface_handle.
  // On Android, this adds a reference on the ANativeWindow.
  gfx::AcceleratedWidget AcquireNativeWidget(
      gpu::SurfaceHandle surface_handle,
      bool* can_be_used_with_surface_control) override;

#if defined(OS_ANDROID)
  gl::ScopedJavaSurface AcquireJavaSurface(
      gpu::SurfaceHandle surface_handle,
      bool* can_be_used_with_surface_control) override;
#endif

  // Gets the global instance of the surface tracker.
  static GpuSurfaceTracker* Get() { return GetInstance(); }

  // Adds a surface for a native widget. Returns the surface ID.
  int AddSurfaceForNativeWidget(SurfaceRecord record);

  // Return true if the surface handle is registered with the tracker.
  bool IsValidSurfaceHandle(gpu::SurfaceHandle surface_handle) const;

  // Removes a given existing surface.
  void RemoveSurface(gpu::SurfaceHandle surface_handle);

  // Returns the number of surfaces currently registered with the tracker.
  std::size_t GetSurfaceCount();

  // Gets the global instance of the surface tracker. Identical to Get(), but
  // named that way for the implementation of Singleton.
  static GpuSurfaceTracker* GetInstance();

 private:
  using SurfaceMap = std::map<gpu::SurfaceHandle, SurfaceRecord>;

  friend struct base::DefaultSingletonTraits<GpuSurfaceTracker>;

  GpuSurfaceTracker();
  ~GpuSurfaceTracker() override;

  mutable base::Lock surface_map_lock_;
  SurfaceMap surface_map_;
  int next_surface_handle_;

  DISALLOW_COPY_AND_ASSIGN(GpuSurfaceTracker);
};

}  // namespace ui

#endif  // GPU_IPC_COMMON_GPU_SURFACE_TRACKER_H_

