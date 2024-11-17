// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/common/gpu_surface_tracker.h"

#include <utility>

#include "base/check.h"
#include "base/containers/contains.h"
#include "base/functional/overloaded.h"
#include "build/build_config.h"
#include "ui/gl/android/scoped_java_surface.h"

namespace gpu {

GpuSurfaceTracker::GpuSurfaceTracker()
    : next_surface_handle_(1) {
  gpu::GpuSurfaceLookup::InitInstance(this);
}

GpuSurfaceTracker::~GpuSurfaceTracker() {
  gpu::GpuSurfaceLookup::InitInstance(nullptr);
}

GpuSurfaceTracker* GpuSurfaceTracker::GetInstance() {
  return base::Singleton<GpuSurfaceTracker>::get();
}

int GpuSurfaceTracker::AddSurfaceForNativeWidget(SurfaceRecord record) {
  base::AutoLock lock(surface_map_lock_);
  gpu::SurfaceHandle surface_handle = next_surface_handle_++;
  surface_map_.emplace(surface_handle, std::move(record));
  return surface_handle;
}

bool GpuSurfaceTracker::IsValidSurfaceHandle(
    gpu::SurfaceHandle surface_handle) const {
  base::AutoLock lock(surface_map_lock_);
  return base::Contains(surface_map_, surface_handle);
}

void GpuSurfaceTracker::RemoveSurface(gpu::SurfaceHandle surface_handle) {
  base::AutoLock lock(surface_map_lock_);
  DCHECK(base::Contains(surface_map_, surface_handle));
  surface_map_.erase(surface_handle);
}

SurfaceRecord GpuSurfaceTracker::AcquireJavaSurface(
    gpu::SurfaceHandle surface_handle) {
  base::AutoLock lock(surface_map_lock_);
  SurfaceMap::const_iterator it = surface_map_.find(surface_handle);
  if (it == surface_map_.end())
    return SurfaceRecord(gl::ScopedJavaSurface(),
                         /*can_be_used_with_surface_control=*/false);

  return absl::visit(
      base::Overloaded{
          [&](const gl::ScopedJavaSurface& surface) {
            DCHECK(surface.IsValid());
            return SurfaceRecord(surface.CopyRetainOwnership(),
                                 it->second.can_be_used_with_surface_control,
                                 it->second.host_input_token);
          },
          [&](const gl::ScopedJavaSurfaceControl& surface_control) {
            return SurfaceRecord(surface_control.CopyRetainOwnership());
          }},
      it->second.surface_variant);
}

std::size_t GpuSurfaceTracker::GetSurfaceCount() {
  base::AutoLock lock(surface_map_lock_);
  return surface_map_.size();
}

}  // namespace gpu
