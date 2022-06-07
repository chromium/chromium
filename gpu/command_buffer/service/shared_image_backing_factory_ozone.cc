// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image_backing_factory_ozone.h"

#include <dawn/dawn_proc_table.h>
#include <dawn/native/DawnNative.h>

#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "build/build_config.h"
#include "build/chromecast_buildflags.h"
#include "build/chromeos_buildflags.h"
#include "components/viz/common/gpu/vulkan_context_provider.h"
#include "components/viz/common/resources/resource_format_utils.h"
#include "gpu/command_buffer/common/gpu_memory_buffer_support.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/shared_image_backing_ozone.h"
#include "gpu/command_buffer/service/shared_memory_region_wrapper.h"
#include "gpu/vulkan/vulkan_device_queue.h"
#include "ui/gfx/gpu_memory_buffer.h"
#include "ui/gfx/native_pixmap.h"
#include "ui/gl/buildflags.h"
#include "ui/gl/gl_surface_egl.h"
#include "ui/ozone/public/ozone_platform.h"
#include "ui/ozone/public/surface_factory_ozone.h"

namespace gpu {
namespace {

gfx::BufferUsage GetBufferUsage(uint32_t usage) {
  if (usage & SHARED_IMAGE_USAGE_WEBGPU) {
    // Just use SCANOUT for WebGPU since the memory doesn't need to be linear.
    return gfx::BufferUsage::SCANOUT;
  } else if (usage & SHARED_IMAGE_USAGE_SCANOUT) {
    return gfx::BufferUsage::SCANOUT;
  } else {
    return gfx::BufferUsage::GPU_READ;
  }
}

}  // namespace

SharedImageBackingFactoryOzone::SharedImageBackingFactoryOzone(
    SharedContextState* shared_context_state)
    : shared_context_state_(shared_context_state) {
#if BUILDFLAG(USE_DAWN)
  dawn_procs_ = base::MakeRefCounted<base::RefCountedData<DawnProcTable>>(
      dawn::native::GetProcs());
#endif  // BUILDFLAG(USE_DAWN)
}

SharedImageBackingFactoryOzone::~SharedImageBackingFactoryOzone() = default;

std::unique_ptr<SharedImageBackingOzone>
SharedImageBackingFactoryOzone::CreateSharedImageInternal(
    const Mailbox& mailbox,
    viz::ResourceFormat format,
    SurfaceHandle surface_handle,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
    uint32_t usage) {
  gfx::BufferFormat buffer_format = viz::BufferFormat(format);
  VkDevice vk_device = VK_NULL_HANDLE;
  DCHECK(shared_context_state_);
  if (shared_context_state_->vk_context_provider()) {
    vk_device = shared_context_state_->vk_context_provider()
                    ->GetDeviceQueue()
                    ->GetVulkanDevice();
  }
  ui::SurfaceFactoryOzone* surface_factory =
      ui::OzonePlatform::GetInstance()->GetSurfaceFactoryOzone();
  scoped_refptr<gfx::NativePixmap> pixmap = surface_factory->CreateNativePixmap(
      surface_handle, vk_device, size, buffer_format, GetBufferUsage(usage));
  // Fallback to GPU_READ if cannot create pixmap with SCANOUT
  if (!pixmap) {
    pixmap = surface_factory->CreateNativePixmap(surface_handle, vk_device,
                                                 size, buffer_format,
                                                 gfx::BufferUsage::GPU_READ);
  }
  if (!pixmap) {
    return nullptr;
  }
  return std::make_unique<SharedImageBackingOzone>(
      mailbox, format, gfx::BufferPlane::DEFAULT, size, color_space,
      surface_origin, alpha_type, usage, shared_context_state_.get(),
      std::move(pixmap), dawn_procs_);
}

std::unique_ptr<SharedImageBacking>
SharedImageBackingFactoryOzone::CreateSharedImage(
    const Mailbox& mailbox,
    viz::ResourceFormat format,
    SurfaceHandle surface_handle,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
    uint32_t usage,
    bool is_thread_safe) {
  DCHECK(!is_thread_safe);
  return CreateSharedImageInternal(mailbox, format, surface_handle, size,
                                   color_space, surface_origin, alpha_type,
                                   usage);
}

std::unique_ptr<SharedImageBacking>
SharedImageBackingFactoryOzone::CreateSharedImage(
    const Mailbox& mailbox,
    viz::ResourceFormat format,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
    uint32_t usage,
    base::span<const uint8_t> pixel_data) {
  SurfaceHandle surface_handle = SurfaceHandle();
  auto backing =
      CreateSharedImageInternal(mailbox, format, surface_handle, size,
                                color_space, surface_origin, alpha_type, usage);

  if (!pixel_data.empty() &&
      !backing->WritePixels(pixel_data, shared_context_state_, format, size,
                            alpha_type)) {
    return nullptr;
  }

  return backing;
}

std::unique_ptr<SharedImageBacking>
SharedImageBackingFactoryOzone::CreateSharedImage(
    const Mailbox& mailbox,
    int client_id,
    gfx::GpuMemoryBufferHandle handle,
    gfx::BufferFormat buffer_format,
    gfx::BufferPlane plane,
    SurfaceHandle surface_handle,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
    uint32_t usage) {
  viz::ResourceFormat format = viz::GetResourceFormat(buffer_format);
  std::unique_ptr<SharedImageBackingOzone> backing;
  if (handle.type == gfx::NATIVE_PIXMAP) {
    ui::SurfaceFactoryOzone* surface_factory =
        ui::OzonePlatform::GetInstance()->GetSurfaceFactoryOzone();
    scoped_refptr<gfx::NativePixmap> pixmap =
        surface_factory->CreateNativePixmapFromHandle(
            surface_handle, size, buffer_format,
            std::move(handle.native_pixmap_handle));
    if (!pixmap) {
      return nullptr;
    }

    const gfx::Size plane_size = gpu::GetPlaneSize(plane, size);
    const viz::ResourceFormat plane_format =
        viz::GetResourceFormat(GetPlaneBufferFormat(plane, buffer_format));
    backing = std::make_unique<SharedImageBackingOzone>(
        mailbox, plane_format, plane, plane_size, color_space, surface_origin,
        alpha_type, usage, shared_context_state_.get(), std::move(pixmap),
        dawn_procs_);
    backing->SetCleared();
  } else if (handle.type == gfx::SHARED_MEMORY_BUFFER) {
    SharedMemoryRegionWrapper shm_wrapper;
    if (!shm_wrapper.Initialize(handle, size, format)) {
      return nullptr;
    }

    backing = CreateSharedImageInternal(mailbox, format, surface_handle, size,
                                        color_space, surface_origin, alpha_type,
                                        usage);
    if (!backing->WritePixels(shm_wrapper.GetMemoryAsSpan(),
                              shared_context_state_, format, size,
                              alpha_type)) {
      DLOG(ERROR) << "Failed to write pixels for shared memory.";
      return nullptr;
    }
    backing->SetSharedMemoryWrapper(std::move(shm_wrapper));
  }

  return backing;
}

bool SharedImageBackingFactoryOzone::IsSupported(
    uint32_t usage,
    viz::ResourceFormat format,
    bool thread_safe,
    gfx::GpuMemoryBufferType gmb_type,
    GrContextType gr_context_type,
    bool* allow_legacy_mailbox,
    bool is_pixel_used) {
  // TODO(crbug.com/969114): Not all shared image factory implementations
  // support concurrent read/write usage.
  if (usage & SHARED_IMAGE_USAGE_CONCURRENT_READ_WRITE) {
    return false;
  }

  if (gmb_type != gfx::EMPTY_BUFFER && gmb_type != gfx::NATIVE_PIXMAP &&
      gmb_type != gfx::SHARED_MEMORY_BUFFER) {
    return false;
  }

#if BUILDFLAG(IS_FUCHSIA)
  DCHECK_EQ(gr_context_type, GrContextType::kVulkan);

  // For now just use SharedImageBackingOzone for primary plane buffers.
  // TODO(crbug.com/1310026): When Vulkan/GL interop is supported on Fuchsia
  // SharedImageBackingOzone should be used for all scanout buffers.
  constexpr uint32_t kPrimaryPlaneUsageFlags = SHARED_IMAGE_USAGE_DISPLAY |
                                               SHARED_IMAGE_USAGE_SCANOUT |
                                               SHARED_IMAGE_USAGE_RASTER;
  if (usage != kPrimaryPlaneUsageFlags ||
      !CanImportGpuMemoryBufferToVulkan(gmb_type)) {
    return false;
  }
#elif BUILDFLAG(IS_LINUX) && !BUILDFLAG(IS_CHROMEOS_ASH) && \
    !BUILDFLAG(IS_CHROMEOS_LACROS) && !BUILDFLAG(IS_CHROMECAST)
  bool used_by_skia = (usage & SHARED_IMAGE_USAGE_RASTER) ||
                      (usage & SHARED_IMAGE_USAGE_DISPLAY);
  bool used_by_vulkan =
      used_by_skia && gr_context_type == GrContextType::kVulkan;
  bool used_by_webgpu = usage & SHARED_IMAGE_USAGE_WEBGPU;
  bool used_by_gl = (usage & SHARED_IMAGE_USAGE_GLES2) ||
                    (used_by_skia && gr_context_type == GrContextType::kGL);
  if (used_by_vulkan && !CanImportGpuMemoryBufferToVulkan(gfx::NATIVE_PIXMAP)) {
    return false;
  }
  if (used_by_webgpu && !CanImportNativePixmapToWebGPU()) {
    return false;
  }
  if (used_by_gl &&
      !gl::GLSurfaceEGL::GetGLDisplayEGL()->HasEGLExtension("EGL_KHR_image")) {
    return false;
  }
#else
  // TODO(hitawala): Until SharedImageBackingOzone supports all use cases prefer
  // using SharedImageBackingGLImage instead
  bool needs_interop_factory = (gr_context_type == GrContextType::kVulkan &&
                                (usage & SHARED_IMAGE_USAGE_DISPLAY)) ||
                               (usage & SHARED_IMAGE_USAGE_WEBGPU) ||
                               (usage & SHARED_IMAGE_USAGE_VIDEO_DECODE);
  if (!needs_interop_factory) {
    return false;
  }
#endif

  *allow_legacy_mailbox = false;
  return true;
}

bool SharedImageBackingFactoryOzone::CanImportGpuMemoryBufferToVulkan(
    gfx::GpuMemoryBufferType memory_buffer_type) {
  if (!shared_context_state_->vk_context_provider()) {
    return false;
  }
  auto* vk_device =
      shared_context_state_->vk_context_provider()->GetDeviceQueue();
  return shared_context_state_->vk_context_provider()
      ->GetVulkanImplementation()
      ->CanImportGpuMemoryBuffer(vk_device, memory_buffer_type);
}

bool SharedImageBackingFactoryOzone::CanImportNativePixmapToWebGPU() {
  // Assume that if skia/vulkan vkDevice supports the Vulkan extensions
  // (external_memory_dma_buf, image_drm_format_modifier), then Dawn/WebGPU also
  // support the extensions until there is capability to check the extensions
  // from Dawn vkDevice when they are exposed.
  return CanImportGpuMemoryBufferToVulkan(gfx::NATIVE_PIXMAP);
}

}  // namespace gpu
