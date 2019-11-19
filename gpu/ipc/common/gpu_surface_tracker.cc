// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/common/gpu_surface_tracker.h"

#include "base/logging.h"
#include "build/build_config.h"

#if defined(OS_ANDROID)
#include <android/native_window_jni.h>
#include "ui/gl/android/scoped_java_surface.h"
#endif  // defined(OS_ANDROID)

namespace gpu {

#if defined(OS_ANDROID)
GpuSurfaceTracker::SurfaceRecord::SurfaceRecord(
    gfx::AcceleratedWidget widget,
    jobject j_surface,
    bool can_be_used_with_surface_control)
    : widget(widget),
      can_be_used_with_surface_control(can_be_used_with_surface_control) {
  // TODO(liberato): It would be nice to assert |surface != nullptr|, but we
  // can't.  in_process_context_factory.cc (for tests) actually calls us without
  // a Surface from java.  Presumably, nobody uses it.  crbug.com/712717 .
  if (j_surface != nullptr)
    surface = gl::ScopedJavaSurface::AcquireExternalSurface(j_surface);
}
#else   // defined(OS_ANDROID)
GpuSurfaceTracker::SurfaceRecord::SurfaceRecord(gfx::AcceleratedWidget widget)
    : widget(widget) {}
#endif  // !defined(OS_ANDROID)

GpuSurfaceTracker::SurfaceRecord::SurfaceRecord(SurfaceRecord&&) = default;

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
  return surface_map_.find(surface_handle) != surface_map_.end();
}

void GpuSurfaceTracker::RemoveSurface(gpu::SurfaceHandle surface_handle) {
  base::AutoLock lock(surface_map_lock_);
  DCHECK(surface_map_.find(surface_handle) != surface_map_.end());
  surface_map_.erase(surface_handle);
}

gfx::AcceleratedWidget GpuSurfaceTracker::AcquireNativeWidget(
    gpu::SurfaceHandle surface_handle,
    bool* can_be_used_with_surface_control) {
  base::AutoLock lock(surface_map_lock_);
  SurfaceMap::iterator it = surface_map_.find(surface_handle);
  if (it == surface_map_.end())
    return gfx::kNullAcceleratedWidget;

#if defined(OS_ANDROID)
  if (it->second.widget != gfx::kNullAcceleratedWidget)
    ANativeWindow_acquire(it->second.widget);
  *can_be_used_with_surface_control =
      it->second.can_be_used_with_surface_control;
#endif  // defined(OS_ANDROID)

  return it->second.widget;
}

#if defined(OS_ANDROID)
gl::ScopedJavaSurface GpuSurfaceTracker::AcquireJavaSurface(
    gpu::SurfaceHandle surface_handle,
    bool* can_be_used_with_surface_control) {
  base::AutoLock lock(surface_map_lock_);
  SurfaceMap::const_iterator it = surface_map_.find(surface_handle);
  if (it == surface_map_.end())
    return gl::ScopedJavaSurface();

  const gl::ScopedJavaSurface& j_surface = it->second.surface;
  DCHECK(j_surface.IsValid());

  *can_be_used_with_surface_control =
      it->second.can_be_used_with_surface_control;
  return gl::ScopedJavaSurface::AcquireExternalSurface(
      j_surface.j_surface().obj());
}
#endif

std::size_t GpuSurfaceTracker::GetSurfaceCount() {
  base::AutoLock lock(surface_map_lock_);
  return surface_map_.size();
}

}  // namespace gpu
