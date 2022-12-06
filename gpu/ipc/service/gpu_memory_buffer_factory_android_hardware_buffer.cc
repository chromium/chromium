// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/service/gpu_memory_buffer_factory_android_hardware_buffer.h"

#include "base/android/android_hardware_buffer_compat.h"
#include "base/android/scoped_hardware_buffer_handle.h"
#include "base/containers/contains.h"
#include "base/logging.h"
#include "build/build_config.h"
#include "components/viz/common/resources/resource_format_utils.h"
#include "gpu/command_buffer/common/gpu_memory_buffer_support.h"
#include "gpu/command_buffer/service/ahardwarebuffer_utils.h"
#include "gpu/ipc/common/gpu_memory_buffer_impl_android_hardware_buffer.h"
#include "ui/gfx/android/android_surface_control_compat.h"

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
  DCHECK_EQ(framebuffer_size, size);
  auto buffer = GpuMemoryBufferImplAndroidHardwareBuffer::Create(
      id, size, format, usage, GpuMemoryBufferImpl::DestructionCallback());
  if (!buffer) {
    LOG(ERROR) << "Error creating new GpuMemoryBuffer";
    return gfx::GpuMemoryBufferHandle();
  }
  auto handle = buffer->CloneHandle();

  {
    base::AutoLock lock(lock_);
    BufferMapKey key(id, client_id);
    DLOG_IF(ERROR, base::Contains(buffer_map_, key))
        << "Created GpuMemoryBuffer with duplicate id";
    buffer_map_[key] = std::move(buffer);
  }
  return handle;
}

void GpuMemoryBufferFactoryAndroidHardwareBuffer::DestroyGpuMemoryBuffer(
    gfx::GpuMemoryBufferId id,
    int client_id) {
  base::AutoLock lock(lock_);
  BufferMapKey key(id, client_id);
  buffer_map_.erase(key);
}

bool GpuMemoryBufferFactoryAndroidHardwareBuffer::
    FillSharedMemoryRegionWithBufferContents(
        gfx::GpuMemoryBufferHandle buffer_handle,
        base::UnsafeSharedMemoryRegion shared_memory) {
  // Not implemented.
  return false;
}

ImageFactory* GpuMemoryBufferFactoryAndroidHardwareBuffer::AsImageFactory() {
  return nullptr;
}

}  // namespace gpu
