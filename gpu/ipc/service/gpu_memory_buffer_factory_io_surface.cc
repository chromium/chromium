// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/service/gpu_memory_buffer_factory_io_surface.h"

#include <vector>

#include "base/logging.h"
#include "gpu/ipc/common/gpu_client_ids.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gfx/mac/io_surface.h"

namespace gpu {

namespace {

// A GpuMemoryBuffer with client_id = 0 behaves like anonymous shared memory.
const int kAnonymousClientId = 0;

}  // namespace

GpuMemoryBufferFactoryIOSurface::GpuMemoryBufferFactoryIOSurface() {
}

GpuMemoryBufferFactoryIOSurface::~GpuMemoryBufferFactoryIOSurface() {
}

gfx::GpuMemoryBufferHandle
GpuMemoryBufferFactoryIOSurface::CreateGpuMemoryBuffer(
    gfx::GpuMemoryBufferId id,
    const gfx::Size& size,
    const gfx::Size& framebuffer_size,
    gfx::BufferFormat format,
    gfx::BufferUsage usage,
    int client_id,
    SurfaceHandle surface_handle) {
  DCHECK_NE(client_id, kAnonymousClientId);
  DCHECK_EQ(framebuffer_size, size);

  bool should_clear = true;
  base::apple::ScopedCFTypeRef<IOSurfaceRef> io_surface =
      gfx::CreateIOSurface(size, format, should_clear);
  if (!io_surface) {
    LOG(ERROR) << "Failed to allocate IOSurface.";
    return gfx::GpuMemoryBufferHandle();
  }

  gfx::GpuMemoryBufferHandle handle;
  handle.type = gfx::IO_SURFACE_BUFFER;
  handle.id = id;
  handle.io_surface = io_surface;

  return handle;
}

void GpuMemoryBufferFactoryIOSurface::DestroyGpuMemoryBuffer(
    gfx::GpuMemoryBufferId id,
    int client_id) {}

bool GpuMemoryBufferFactoryIOSurface::FillSharedMemoryRegionWithBufferContents(
    gfx::GpuMemoryBufferHandle buffer_handle,
    base::UnsafeSharedMemoryRegion shared_memory) {
  return false;
}

}  // namespace gpu
