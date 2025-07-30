// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/service/gpu_memory_buffer_factory_native_pixmap.h"

#include "base/logging.h"
#include "build/build_config.h"
#include "components/viz/common/gpu/vulkan_context_provider.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gfx/buffer_usage_util.h"
#include "ui/gfx/client_native_pixmap.h"
#include "ui/gfx/linux/native_pixmap_dmabuf.h"
#include "ui/gfx/native_pixmap.h"
#include "ui/gl/gl_implementation.h"
#include "ui/ozone/public/ozone_platform.h"
#include "ui/ozone/public/surface_factory_ozone.h"

#if BUILDFLAG(ENABLE_VULKAN)
#include "gpu/vulkan/vulkan_device_queue.h"
#endif

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
    gfx::BufferFormat format,
    gfx::BufferUsage usage) {
  scoped_refptr<gfx::NativePixmap> pixmap =
      ui::OzonePlatform::GetInstance()
          ->GetSurfaceFactoryOzone()
          ->CreateNativePixmap(gpu::kNullSurfaceHandle, GetVulkanDeviceQueue(),
                               size, format, usage, size);
  return CreateNativeGmbHandleFromNativePixmap(size, format, usage,
                                               std::move(pixmap));
}

bool GpuMemoryBufferFactoryNativePixmap::
    FillSharedMemoryRegionWithBufferContents(
        gfx::GpuMemoryBufferHandle buffer_handle,
        base::UnsafeSharedMemoryRegion shared_memory) {
  return false;
}

VulkanDeviceQueue* GpuMemoryBufferFactoryNativePixmap::GetVulkanDeviceQueue() {
#if BUILDFLAG(ENABLE_VULKAN)
  if (vulkan_context_provider_)
    return vulkan_context_provider_->GetDeviceQueue();
#endif

  return nullptr;
}

gfx::GpuMemoryBufferHandle
GpuMemoryBufferFactoryNativePixmap::CreateNativeGmbHandleFromNativePixmap(
    const gfx::Size& size,
    gfx::BufferFormat format,
    gfx::BufferUsage usage,
    scoped_refptr<gfx::NativePixmap> pixmap) {
  if (!pixmap.get()) {
    DLOG(ERROR) << "Failed to create pixmap " << size.ToString() << ",  "
                << gfx::BufferFormatToString(format) << ", usage "
                << gfx::BufferUsageToString(usage);
    return gfx::GpuMemoryBufferHandle();
  }

  gfx::NativePixmapHandle native_pixmap_handle = pixmap->ExportHandle();
  if (native_pixmap_handle.planes.empty()) {
    return gfx::GpuMemoryBufferHandle();
  }

  return gfx::GpuMemoryBufferHandle(std::move(native_pixmap_handle));
}

}  // namespace gpu
