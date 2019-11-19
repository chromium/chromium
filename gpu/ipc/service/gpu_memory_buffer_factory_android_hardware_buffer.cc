// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/service/gpu_memory_buffer_factory_android_hardware_buffer.h"

#include "base/android/android_hardware_buffer_compat.h"
#include "base/logging.h"
#include "base/memory/shared_memory_handle.h"
#include "base/stl_util.h"
#include "build/build_config.h"
#include "gpu/ipc/common/gpu_memory_buffer_impl_android_hardware_buffer.h"
#include "ui/gl/gl_image_ahardwarebuffer.h"

namespace gpu {

GpuMemoryBufferFactoryAndroidHardwareBuffer::
    GpuMemoryBufferFactoryAndroidHardwareBuffer() = default;

GpuMemoryBufferFactoryAndroidHardwareBuffer::
    ~GpuMemoryBufferFactoryAndroidHardwareBuffer() = default;

gfx::GpuMemoryBufferHandle
GpuMemoryBufferFactoryAndroidHardwareBuffer::CreateGpuMemoryBuffer(
    gfx::GpuMemoryBufferId id,
    const gfx::Size& size,
    gfx::BufferFormat format,
    gfx::BufferUsage usage,
    int client_id,
    SurfaceHandle surface_handle) {
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

ImageFactory* GpuMemoryBufferFactoryAndroidHardwareBuffer::AsImageFactory() {
  return this;
}

scoped_refptr<gl::GLImage>
GpuMemoryBufferFactoryAndroidHardwareBuffer::CreateImageForGpuMemoryBuffer(
    gfx::GpuMemoryBufferHandle handle,
    const gfx::Size& size,
    gfx::BufferFormat format,
    int client_id,
    SurfaceHandle surface_handle) {
  if (handle.type != gfx::ANDROID_HARDWARE_BUFFER)
    return nullptr;

  base::android::ScopedHardwareBufferHandle& buffer =
      handle.android_hardware_buffer;
  DCHECK(buffer.is_valid());

  scoped_refptr<gl::GLImageAHardwareBuffer> image(
      new gl::GLImageAHardwareBuffer(size));
  if (!image->Initialize(buffer.get(),
                         /* preserved */ false)) {
    DLOG(ERROR) << "Failed to create GLImage " << size.ToString();
    image = nullptr;
  }

  // The underlying AHardwareBuffer's reference count was incremented by
  // RecvHandleFromUnixSocket which implicitly acquired it. Apparently
  // eglCreateImageKHR acquires the AHardwareBuffer on construction and
  // releases on destruction (this isn't really documented), so we need to
  // release here to avoid an excess reference. We want to pass ownership to
  // the image. Also release in the failure case to ensure we consistently
  // consume the GpuMemoryBufferHandle.
  buffer.reset();

  return image;
}

unsigned GpuMemoryBufferFactoryAndroidHardwareBuffer::RequiredTextureType() {
  return GL_TEXTURE_EXTERNAL_OES;
}

}  // namespace gpu
