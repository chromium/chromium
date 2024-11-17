// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_IPC_COMMON_GPU_SURFACE_TRACKER_H_
#define GPU_IPC_COMMON_GPU_SURFACE_TRACKER_H_

#include <stddef.h>

#include <map>

#include "base/memory/singleton.h"
#include "base/synchronization/lock.h"
#include "gpu/gpu_export.h"
#include "gpu/ipc/common/gpu_surface_lookup.h"
#include "gpu/ipc/common/surface_handle.h"
#include "ui/gl/android/scoped_java_surface.h"
#include "ui/gl/android/scoped_java_surface_control.h"

namespace gpu {

// This class is used on Android, and is responsible for tracking native
// window surfaces exposed to the GPU process. Every surface gets registered to
// this class, and gets a handle.  The handle can be passed to
// CommandBufferProxyImpl::Create or to
// GpuMemoryBufferManager::CreateGpuMemoryBuffer.
// On Android, the handle is used in the GPU process to get a reference to the
// ScopedJavaSurface, using GpuSurfaceLookup (implemented by
// ChildProcessSurfaceManager).
// This class is thread safe.
class GPU_EXPORT GpuSurfaceTracker : public gpu::GpuSurfaceLookup {
 public:
  SurfaceRecord AcquireJavaSurface(gpu::SurfaceHandle surface_handle) override;

  // Gets the global instance of the surface tracker.
  static GpuSurfaceTracker* Get() { return GetInstance(); }

  GpuSurfaceTracker(const GpuSurfaceTracker&) = delete;
  GpuSurfaceTracker& operator=(const GpuSurfaceTracker&) = delete;

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
};

}  // namespace ui

#endif  // GPU_IPC_COMMON_GPU_SURFACE_TRACKER_H_

