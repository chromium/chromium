// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/service/gpu_memory_buffer_factory_native_pixmap.h"

#include "components/viz/common/gpu/vulkan_context_provider.h"
#include "gpu/command_buffer/common/gpu_memory_buffer_support.h"
#include "gpu/vulkan/vulkan_device_queue.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gfx/buffer_usage_util.h"
#include "ui/gfx/client_native_pixmap.h"
#include "ui/gfx/linux/native_pixmap_dmabuf.h"
#include "ui/gfx/native_pixmap.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_enums.h"
#include "ui/gl/gl_image_native_pixmap.h"

#if defined(USE_OZONE)
#include "ui/ozone/public/ozone_platform.h"
#include "ui/ozone/public/surface_factory_ozone.h"
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
GpuMemoryBufferFactoryNativePixmap::CreateGpuMemoryBuffer(
    gfx::GpuMemoryBufferId id,
    const gfx::Size& size,
    gfx::BufferFormat format,
    gfx::BufferUsage usage,
    int client_id,
    SurfaceHandle surface_handle) {
#if defined(USE_OZONE)
  scoped_refptr<gfx::NativePixmap> pixmap =
      ui::OzonePlatform::GetInstance()
          ->GetSurfaceFactoryOzone()
          ->CreateNativePixmap(surface_handle, GetVulkanDevice(), size, format,
                               usage);
  return CreateGpuMemoryBufferFromNativePixmap(id, size, format, usage,
                                               client_id, std::move(pixmap));
#else
  NOTIMPLEMENTED();
  return gfx::GpuMemoryBufferHandle();
#endif
}

void GpuMemoryBufferFactoryNativePixmap::CreateGpuMemoryBufferAsync(
    gfx::GpuMemoryBufferId id,
    const gfx::Size& size,
    gfx::BufferFormat format,
    gfx::BufferUsage usage,
    int client_id,
    SurfaceHandle surface_handle,
    CreateGpuMemoryBufferAsyncCallback callback) {
#if defined(USE_OZONE)
  ui::OzonePlatform::GetInstance()
      ->GetSurfaceFactoryOzone()
      ->CreateNativePixmapAsync(
          surface_handle, GetVulkanDevice(), size, format, usage,
          base::BindOnce(
              &GpuMemoryBufferFactoryNativePixmap::OnNativePixmapCreated, id,
              size, format, usage, client_id, std::move(callback),
              weak_factory_.GetWeakPtr()));
#else
  NOTIMPLEMENTED();
  std::move(callback).Run(gfx::GpuMemoryBufferHandle());
#endif
}

void GpuMemoryBufferFactoryNativePixmap::DestroyGpuMemoryBuffer(
    gfx::GpuMemoryBufferId id,
    int client_id) {
  base::AutoLock lock(native_pixmaps_lock_);
  NativePixmapMapKey key(id.id, client_id);
  native_pixmaps_.erase(key);
}

ImageFactory* GpuMemoryBufferFactoryNativePixmap::AsImageFactory() {
  return this;
}

scoped_refptr<gl::GLImage>
GpuMemoryBufferFactoryNativePixmap::CreateImageForGpuMemoryBuffer(
    gfx::GpuMemoryBufferHandle handle,
    const gfx::Size& size,
    gfx::BufferFormat format,
    int client_id,
    SurfaceHandle surface_handle) {
  if (handle.type != gfx::NATIVE_PIXMAP)
    return nullptr;

  scoped_refptr<gfx::NativePixmap> pixmap;

  // If CreateGpuMemoryBuffer was used to allocate this buffer then avoid
  // creating a new native pixmap for it.
  {
    base::AutoLock lock(native_pixmaps_lock_);
    NativePixmapMapKey key(handle.id.id, client_id);
    auto it = native_pixmaps_.find(key);
    if (it != native_pixmaps_.end())
      pixmap = it->second;
  }

  // Create new pixmap from handle if one doesn't already exist.
  if (!pixmap) {
#if defined(USE_OZONE)
    pixmap = ui::OzonePlatform::GetInstance()
                 ->GetSurfaceFactoryOzone()
                 ->CreateNativePixmapFromHandle(
                     surface_handle, size, format,
                     std::move(handle.native_pixmap_handle));
#else
    DCHECK_EQ(surface_handle, gpu::kNullSurfaceHandle);
    pixmap = base::WrapRefCounted(new gfx::NativePixmapDmaBuf(
        size, format, std::move(handle.native_pixmap_handle)));
#endif
    if (!pixmap.get()) {
      DLOG(ERROR) << "Failed to create pixmap from handle";
      return nullptr;
    }
  }

  auto image = base::MakeRefCounted<gl::GLImageNativePixmap>(size, format);
  if (!image->Initialize(std::move(pixmap))) {
    LOG(ERROR) << "Failed to create GLImage " << size.ToString() << ", "
               << gfx::BufferFormatToString(format);
    return nullptr;
  }
  return image;
}

bool GpuMemoryBufferFactoryNativePixmap::SupportsCreateAnonymousImage() const {
#if defined(USE_OZONE)
  return true;
#else
  return false;
#endif
}

scoped_refptr<gl::GLImage>
GpuMemoryBufferFactoryNativePixmap::CreateAnonymousImage(
    const gfx::Size& size,
    gfx::BufferFormat format,
    gfx::BufferUsage usage,
    bool* is_cleared) {
  scoped_refptr<gfx::NativePixmap> pixmap;
#if defined(USE_OZONE)
  pixmap = ui::OzonePlatform::GetInstance()
               ->GetSurfaceFactoryOzone()
               ->CreateNativePixmap(gpu::kNullSurfaceHandle, GetVulkanDevice(),
                                    size, format, usage);
#else
  NOTIMPLEMENTED();
#endif
  if (!pixmap.get()) {
    LOG(ERROR) << "Failed to create pixmap " << size.ToString() << ", "
               << gfx::BufferFormatToString(format) << ", usage "
               << gfx::BufferUsageToString(usage);
    return nullptr;
  }
  auto image = base::MakeRefCounted<gl::GLImageNativePixmap>(size, format);
  if (!image->Initialize(std::move(pixmap))) {
    LOG(ERROR) << "Failed to create GLImage " << size.ToString() << ", "
               << gfx::BufferFormatToString(format) << ", usage "
               << gfx::BufferUsageToString(usage);
    return nullptr;
  }
  *is_cleared = true;
  return image;
}

unsigned GpuMemoryBufferFactoryNativePixmap::RequiredTextureType() {
  return GL_TEXTURE_2D;
}

VkDevice GpuMemoryBufferFactoryNativePixmap::GetVulkanDevice() {
  return vulkan_context_provider_
             ? vulkan_context_provider_->GetDeviceQueue()->GetVulkanDevice()
             : VK_NULL_HANDLE;
}

// static
void GpuMemoryBufferFactoryNativePixmap::OnNativePixmapCreated(
    gfx::GpuMemoryBufferId id,
    const gfx::Size& size,
    gfx::BufferFormat format,
    gfx::BufferUsage usage,
    int client_id,
    CreateGpuMemoryBufferAsyncCallback callback,
    base::WeakPtr<GpuMemoryBufferFactoryNativePixmap> weak_ptr,
    scoped_refptr<gfx::NativePixmap> pixmap) {
  if (weak_ptr) {
    std::move(callback).Run(weak_ptr->CreateGpuMemoryBufferFromNativePixmap(
        id, size, format, usage, client_id, pixmap));
  } else {
    std::move(callback).Run(gfx::GpuMemoryBufferHandle());
  }
}

gfx::GpuMemoryBufferHandle
GpuMemoryBufferFactoryNativePixmap::CreateGpuMemoryBufferFromNativePixmap(
    gfx::GpuMemoryBufferId id,
    const gfx::Size& size,
    gfx::BufferFormat format,
    gfx::BufferUsage usage,
    int client_id,
    scoped_refptr<gfx::NativePixmap> pixmap) {
  if (!pixmap.get()) {
    DLOG(ERROR) << "Failed to create pixmap " << size.ToString() << ",  "
                << gfx::BufferFormatToString(format) << ", usage "
                << gfx::BufferUsageToString(usage);
    return gfx::GpuMemoryBufferHandle();
  }

  gfx::GpuMemoryBufferHandle new_handle;
  new_handle.type = gfx::NATIVE_PIXMAP;
  new_handle.id = id;
  new_handle.native_pixmap_handle = pixmap->ExportHandle();

  // TODO(reveman): Remove this once crbug.com/628334 has been fixed.
  {
    base::AutoLock lock(native_pixmaps_lock_);
    NativePixmapMapKey key(id.id, client_id);
    DCHECK(native_pixmaps_.find(key) == native_pixmaps_.end());
    native_pixmaps_[key] = pixmap;
  }

  return new_handle;
}

}  // namespace gpu
