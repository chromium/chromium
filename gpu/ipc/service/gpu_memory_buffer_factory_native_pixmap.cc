// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/service/gpu_memory_buffer_factory_native_pixmap.h"

#include "build/build_config.h"
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
#include "ui/gl/gl_implementation.h"

#if defined(USE_X11) || defined(USE_OZONE)
#include "ui/base/ui_base_features.h"
#endif

#if defined(USE_OZONE)
#include "ui/ozone/public/ozone_platform.h"
#include "ui/ozone/public/surface_factory_ozone.h"
#endif

#if defined(USE_X11)
#include "ui/gfx/linux/gbm_buffer.h"                     // nogncheck
#include "ui/gfx/linux/gpu_memory_buffer_support_x11.h"  // nogncheck
#include "ui/gl/gl_image_glx_native_pixmap.h"            // nogncheck
#endif

namespace gpu {

namespace {

// The boilerplate code to initialize each GLImage that we need is the same, but
// the Initialize() methods are not virtual, so a template is needed.
template <class Image, class Pixmap>
scoped_refptr<Image> CreateImageFromPixmap(const gfx::Size& size,
                                           gfx::BufferFormat format,
                                           scoped_refptr<Pixmap> pixmap) {
  auto image = base::MakeRefCounted<Image>(size, format);
  if (!image->Initialize(std::move(pixmap))) {
    LOG(ERROR) << "Failed to create GLImage " << size.ToString() << ", "
               << gfx::BufferFormatToString(format);
    return nullptr;
  }
  return image;
}

}  // namespace

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
    const gfx::Size& framebuffer_size,
    gfx::BufferFormat format,
    gfx::BufferUsage usage,
    int client_id,
    SurfaceHandle surface_handle) {
#if defined(USE_OZONE)
  if (features::IsUsingOzonePlatform()) {
    scoped_refptr<gfx::NativePixmap> pixmap =
        ui::OzonePlatform::GetInstance()
            ->GetSurfaceFactoryOzone()
            ->CreateNativePixmap(surface_handle, GetVulkanDevice(), size,
                                 format, usage, framebuffer_size);
    return CreateGpuMemoryBufferFromNativePixmap(id, size, format, usage,
                                                 client_id, std::move(pixmap));
  }
#endif
  DCHECK_EQ(framebuffer_size, size);

#if defined(USE_X11)
  DCHECK(!features::IsUsingOzonePlatform());
  std::unique_ptr<ui::GbmBuffer> buffer =
      ui::GpuMemoryBufferSupportX11::GetInstance()->CreateBuffer(format, size,
                                                                 usage);
  if (!buffer)
    return gfx::GpuMemoryBufferHandle();
  gfx::NativePixmapHandle handle = buffer->ExportHandle();
  scoped_refptr<gfx::NativePixmapDmaBuf> pixmap =
      base::MakeRefCounted<gfx::NativePixmapDmaBuf>(size, format,
                                                    std::move(handle));
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
  if (features::IsUsingOzonePlatform()) {
    ui::OzonePlatform::GetInstance()
        ->GetSurfaceFactoryOzone()
        ->CreateNativePixmapAsync(
            surface_handle, GetVulkanDevice(), size, format, usage,
            base::BindOnce(
                &GpuMemoryBufferFactoryNativePixmap::OnNativePixmapCreated, id,
                size, format, usage, client_id, std::move(callback),
                weak_factory_.GetWeakPtr()));
    return;
  }
#endif

#if defined(USE_X11)
  std::move(callback).Run(
      CreateGpuMemoryBuffer(id, size, /*framebuffer_size=*/size, format, usage,
                            client_id, surface_handle));
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

bool GpuMemoryBufferFactoryNativePixmap::
    FillSharedMemoryRegionWithBufferContents(
        gfx::GpuMemoryBufferHandle buffer_handle,
        base::UnsafeSharedMemoryRegion shared_memory) {
  return false;
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
    if (features::IsUsingOzonePlatform()) {
      pixmap = ui::OzonePlatform::GetInstance()
                   ->GetSurfaceFactoryOzone()
                   ->CreateNativePixmapFromHandle(
                       surface_handle, size, format,
                       std::move(handle.native_pixmap_handle));
    }
#endif
#if !defined(OS_FUCHSIA)
    if (!pixmap) {
      DCHECK_EQ(surface_handle, gpu::kNullSurfaceHandle);
      pixmap = base::WrapRefCounted(new gfx::NativePixmapDmaBuf(
          size, format, std::move(handle.native_pixmap_handle)));
    }
#endif

    if (!pixmap) {
      DLOG(ERROR) << "Failed to create pixmap from handle";
      return nullptr;
    }
  }

  switch (gl::GetGLImplementation()) {
    case gl::kGLImplementationEGLGLES2:
    case gl::kGLImplementationEGLANGLE:
      // EGL
      return CreateImageFromPixmap<gl::GLImageNativePixmap>(size, format,
                                                            pixmap);
#if defined(USE_X11)
    case gl::kGLImplementationDesktopGL:
#if defined(USE_OZONE)
      if (features::IsUsingOzonePlatform()) {
        NOTREACHED();
        return nullptr;
      }
#endif
      // GLX
      return CreateImageFromPixmap<gl::GLImageGLXNativePixmap>(size, format,
                                                               pixmap);
#endif
    default:
      NOTREACHED();
      return nullptr;
  }
}

bool GpuMemoryBufferFactoryNativePixmap::SupportsCreateAnonymousImage() const {
#if defined(USE_OZONE)
  if (features::IsUsingOzonePlatform())
    return true;
#endif
  return false;
}

scoped_refptr<gl::GLImage>
GpuMemoryBufferFactoryNativePixmap::CreateAnonymousImage(
    const gfx::Size& size,
    gfx::BufferFormat format,
    gfx::BufferUsage usage,
    SurfaceHandle surface_handle,
    bool* is_cleared) {
  scoped_refptr<gfx::NativePixmap> pixmap;
#if defined(USE_OZONE)
  if (features::IsUsingOzonePlatform()) {
    pixmap = ui::OzonePlatform::GetInstance()
                 ->GetSurfaceFactoryOzone()
                 ->CreateNativePixmap(surface_handle, GetVulkanDevice(), size,
                                      format, usage);
  }
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
