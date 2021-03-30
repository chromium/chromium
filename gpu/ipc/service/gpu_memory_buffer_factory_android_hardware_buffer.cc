// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/service/gpu_memory_buffer_factory_android_hardware_buffer.h"

#include "base/android/android_hardware_buffer_compat.h"
#include "base/android/scoped_hardware_buffer_handle.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "build/build_config.h"
#include "components/viz/common/resources/resource_format_utils.h"
#include "gpu/command_buffer/service/ahardwarebuffer_utils.h"
#include "gpu/ipc/common/gpu_memory_buffer_impl_android_hardware_buffer.h"
#include "ui/gfx/android/android_surface_control_compat.h"
#include "ui/gl/gl_image_ahardwarebuffer.h"

namespace gpu {
namespace {

AHardwareBuffer_Desc GetBufferDescription(const gfx::Size& size,
                                          viz::ResourceFormat format,
                                          gfx::BufferUsage usage) {
  AHardwareBuffer_Desc hwb_desc;
  hwb_desc.width = size.width();
  hwb_desc.height = size.height();
  hwb_desc.format = AHardwareBufferFormat(format);

  hwb_desc.usage = AHARDWAREBUFFER_USAGE_GPU_SAMPLED_IMAGE |
                   AHARDWAREBUFFER_USAGE_GPU_COLOR_OUTPUT;
  if (usage == gfx::BufferUsage::SCANOUT)
    hwb_desc.usage |= gfx::SurfaceControl::RequiredUsage();

  // Number of images in an image array.
  hwb_desc.layers = 1;

  // The following three are not used here.
  hwb_desc.stride = 0;
  hwb_desc.rfu0 = 0;
  hwb_desc.rfu1 = 0;

  return hwb_desc;
}

}  // namespace

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
  return this;
}

bool GpuMemoryBufferFactoryAndroidHardwareBuffer::SupportsCreateAnonymousImage()
    const {
  return true;
}

scoped_refptr<gl::GLImage>
GpuMemoryBufferFactoryAndroidHardwareBuffer::CreateAnonymousImage(
    const gfx::Size& size,
    gfx::BufferFormat format,
    gfx::BufferUsage usage,
    SurfaceHandle surface_handle,
    bool* is_cleared) {
  auto resource_format = viz::GetResourceFormat(format);
  if (!AHardwareBufferSupportedFormat(resource_format)) {
    LOG(ERROR) << "Requested format not supported by AHB : " << resource_format;
    return nullptr;
  }

  AHardwareBuffer* buffer = nullptr;
  AHardwareBuffer_Desc hwb_desc =
      GetBufferDescription(size, resource_format, usage);
  base::AndroidHardwareBufferCompat::GetInstance().Allocate(&hwb_desc, &buffer);
  if (!buffer) {
    LOG(ERROR) << "Failed to allocate AHB";
  }
  auto handle = base::android::ScopedHardwareBufferHandle::Adopt(buffer);

  scoped_refptr<gl::GLImageAHardwareBuffer> gl_image =
      new gl::GLImageAHardwareBuffer(size);
  if (!gl_image->Initialize(handle.get(), false)) {
    LOG(ERROR) << "Failed to initialize GLImageAHardwareBuffer";
    return nullptr;
  }

  *is_cleared = false;
  return gl_image;
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
