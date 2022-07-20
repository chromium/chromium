// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image/ozone_image_backing_factory.h"

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
#include "gpu/command_buffer/service/shared_image/ozone_image_backing.h"
#include "gpu/command_buffer/service/shared_memory_region_wrapper.h"
#include "gpu/vulkan/vulkan_device_queue.h"
#include "ui/gfx/gpu_memory_buffer.h"
#include "ui/gfx/native_pixmap.h"
#include "ui/gl/buildflags.h"
#include "ui/gl/gl_bindings.h"
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

OzoneImageBackingFactory::OzoneImageBackingFactory(
    SharedContextState* shared_context_state,
    const GpuDriverBugWorkarounds& workarounds)
    : shared_context_state_(shared_context_state), workarounds_(workarounds) {
#if BUILDFLAG(USE_DAWN)
  dawn_procs_ = base::MakeRefCounted<base::RefCountedData<DawnProcTable>>(
      dawn::native::GetProcs());
#endif  // BUILDFLAG(USE_DAWN)
}

OzoneImageBackingFactory::~OzoneImageBackingFactory() = default;

std::unique_ptr<OzoneImageBacking>
OzoneImageBackingFactory::CreateSharedImageInternal(
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
    DLOG(ERROR) << "Failed to create native pixmap";
    return nullptr;
  }
  return std::make_unique<OzoneImageBacking>(
      mailbox, format, gfx::BufferPlane::DEFAULT, size, color_space,
      surface_origin, alpha_type, usage, shared_context_state_.get(),
      std::move(pixmap), dawn_procs_, workarounds_);
}

std::unique_ptr<SharedImageBacking> OzoneImageBackingFactory::CreateSharedImage(
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

std::unique_ptr<SharedImageBacking> OzoneImageBackingFactory::CreateSharedImage(
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

  if (!backing) {
    return nullptr;
  }
  if (!pixel_data.empty()) {
    SkImageInfo info = backing->AsSkImageInfo();
    SkPixmap pixmap(info, pixel_data.data(), info.minRowBytes());

    if (!backing->UploadFromMemory(pixmap))
      return nullptr;
  }

  return backing;
}

std::unique_ptr<SharedImageBacking> OzoneImageBackingFactory::CreateSharedImage(
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
  DCHECK_EQ(handle.type, gfx::NATIVE_PIXMAP);

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
  auto backing = std::make_unique<OzoneImageBacking>(
      mailbox, plane_format, plane, plane_size, color_space, surface_origin,
      alpha_type, usage, shared_context_state_.get(), std::move(pixmap),
      dawn_procs_, workarounds_);
  backing->SetCleared();

  return backing;
}

bool OzoneImageBackingFactory::IsSupported(uint32_t usage,
                                           viz::ResourceFormat format,
                                           bool thread_safe,
                                           gfx::GpuMemoryBufferType gmb_type,
                                           GrContextType gr_context_type,
                                           bool* allow_legacy_mailbox,
                                           bool is_pixel_used) {
  if (gmb_type != gfx::EMPTY_BUFFER && gmb_type != gfx::NATIVE_PIXMAP) {
    return false;
  }

  bool used_by_skia = (usage & SHARED_IMAGE_USAGE_RASTER) ||
                      (usage & SHARED_IMAGE_USAGE_DISPLAY);
  bool used_by_vulkan =
      used_by_skia && gr_context_type == GrContextType::kVulkan;
  bool used_by_webgpu = usage & SHARED_IMAGE_USAGE_WEBGPU;
  bool used_by_gl = (usage & SHARED_IMAGE_USAGE_GLES2) ||
                    (used_by_skia && gr_context_type == GrContextType::kGL);
  if (used_by_vulkan && !CanImportNativePixmapToVulkan()) {
    return false;
  }
  if (used_by_webgpu && !CanImportNativePixmapToWebGPU()) {
    return false;
  }
  ui::GLOzone* gl_ozone = ui::OzonePlatform::GetInstance()
                              ->GetSurfaceFactoryOzone()
                              ->GetCurrentGLOzone();
  if (used_by_gl && (!gl_ozone || !gl_ozone->CanImportNativePixmap())) {
    return false;
  }

#if BUILDFLAG(IS_FUCHSIA)
  DCHECK_EQ(gr_context_type, GrContextType::kVulkan);

  // For now just use OzoneImageBacking for primary plane buffers.
  // TODO(crbug.com/1310026): When Vulkan/GL interop is supported on Fuchsia
  // OzoneImageBacking should be used for all scanout buffers.
  constexpr uint32_t kPrimaryPlaneUsageFlags = SHARED_IMAGE_USAGE_DISPLAY |
                                               SHARED_IMAGE_USAGE_SCANOUT |
                                               SHARED_IMAGE_USAGE_RASTER;
  if (usage != kPrimaryPlaneUsageFlags || gmb_type != gfx::NATIVE_PIXMAP) {
    return false;
  }
#endif

  *allow_legacy_mailbox = false;
  return true;
}

bool OzoneImageBackingFactory::CanImportNativePixmapToVulkan() {
  if (!shared_context_state_->vk_context_provider()) {
    return false;
  }
  auto* vk_device =
      shared_context_state_->vk_context_provider()->GetDeviceQueue();
  return shared_context_state_->vk_context_provider()
      ->GetVulkanImplementation()
      ->CanImportGpuMemoryBuffer(vk_device, gfx::NATIVE_PIXMAP);
}

bool OzoneImageBackingFactory::CanImportNativePixmapToWebGPU() {
  // Assume that if skia/vulkan vkDevice supports the Vulkan extensions
  // (external_memory_dma_buf, image_drm_format_modifier), then Dawn/WebGPU also
  // support the extensions until there is capability to check the extensions
  // from Dawn vkDevice when they are exposed.
  return CanImportNativePixmapToVulkan();
}

}  // namespace gpu
