// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/service/gpu_memory_buffer_factory_android_hardware_buffer.h"

namespace gpu {

GpuMemoryBufferFactoryAndroidHardwareBuffer::
    GpuMemoryBufferFactoryAndroidHardwareBuffer() = default;

GpuMemoryBufferFactoryAndroidHardwareBuffer::
    ~GpuMemoryBufferFactoryAndroidHardwareBuffer() = default;

gfx::GpuMemoryBufferHandle
GpuMemoryBufferFactoryAndroidHardwareBuffer::CreateGpuMemoryBuffer(
    gfx::GpuMemoryBufferId id,
    const gfx::Size& size,
    const gfx::Size& framebuffer_size,
    gfx::BufferFormat format,
    gfx::BufferUsage usage,
    int client_id,
    SurfaceHandle surface_handle) {
  return gfx::GpuMemoryBufferHandle();
}

void GpuMemoryBufferFactoryAndroidHardwareBuffer::DestroyGpuMemoryBuffer(
    gfx::GpuMemoryBufferId id,
    int client_id) {}

bool GpuMemoryBufferFactoryAndroidHardwareBuffer::
    FillSharedMemoryRegionWithBufferContents(
        gfx::GpuMemoryBufferHandle buffer_handle,
        base::UnsafeSharedMemoryRegion shared_memory) {
  // Not implemented.
  return false;
}

}  // namespace gpu
