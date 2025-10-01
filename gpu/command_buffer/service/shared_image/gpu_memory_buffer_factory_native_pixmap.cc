// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image/gpu_memory_buffer_factory_native_pixmap.h"

#include "build/build_config.h"
#include "components/viz/common/gpu/vulkan_context_provider.h"
#include "gpu/command_buffer/service/shared_image/ozone_image_backing_factory.h"

namespace gpu {

GpuMemoryBufferFactoryNativePixmap::GpuMemoryBufferFactoryNativePixmap()
    : GpuMemoryBufferFactoryNativePixmap(nullptr) {}

GpuMemoryBufferFactoryNativePixmap::GpuMemoryBufferFactoryNativePixmap(
    viz::VulkanContextProvider* vulkan_context_provider)
    : vulkan_context_provider_(vulkan_context_provider) {}

GpuMemoryBufferFactoryNativePixmap::~GpuMemoryBufferFactoryNativePixmap() =
    default;

gfx::GpuMemoryBufferHandle
GpuMemoryBufferFactoryNativePixmap::CreateNativeGmbHandle(
    const gfx::Size& size,
    viz::SharedImageFormat format,
    gfx::BufferUsage usage) {
  return OzoneImageBackingFactory::CreateGpuMemoryBufferHandle(
      vulkan_context_provider_.get(), size, format, usage);
}

}  // namespace gpu
