// Copyright 2020 The Chromium Authors
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
#include "components/viz/common/resources/shared_image_format_utils.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/dawn_context_provider.h"
#include "gpu/command_buffer/service/service_utils.h"
#include "gpu/command_buffer/service/shared_image/ozone_image_backing.h"
#include "gpu/command_buffer/service/shared_image/shared_image_format_service_utils.h"
#include "gpu/command_buffer/service/shared_memory_region_wrapper.h"
#include "gpu/config/gpu_finch_features.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/gpu_memory_buffer.h"
#include "ui/gfx/native_pixmap.h"
#include "ui/gl/buildflags.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_fence.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_surface_egl.h"
#include "ui/ozone/public/ozone_platform.h"
#include "ui/ozone/public/surface_factory_ozone.h"

#if BUILDFLAG(ENABLE_VULKAN)
#include "components/viz/common/gpu/vulkan_context_provider.h"
#include "gpu/vulkan/vulkan_device_queue.h"
#endif  // BUILDFLAG(ENABLE_VULKAN)

namespace gpu {
namespace {

gfx::BufferUsage GetBufferUsage(SharedImageUsageSet usage) {
  if (usage.Has(SHARED_IMAGE_USAGE_PROTECTED_VIDEO)) {
    return gfx::BufferUsage::PROTECTED_SCANOUT;
  } else if (usage.HasAny(SHARED_IMAGE_USAGE_WEBGPU_READ |
                          SHARED_IMAGE_USAGE_WEBGPU_WRITE)) {
    // Just use SCANOUT for WebGPU since the memory doesn't need to be linear.
    return gfx::BufferUsage::SCANOUT;
  } else if (usage.Has(SHARED_IMAGE_USAGE_SCANOUT)) {
    if (usage.Has(SHARED_IMAGE_USAGE_CONCURRENT_READ_WRITE)) {
      // Example usage here is low latency (desynchronized) 2d canvas. Note that
      // this does not imply CPU read/write.
      return gfx::BufferUsage::SCANOUT_FRONT_RENDERING;
    }
    return gfx::BufferUsage::SCANOUT;
  } else {
    return gfx::BufferUsage::GPU_READ;
  }
}

constexpr SharedImageUsageSet kSupportedUsage =
    SHARED_IMAGE_USAGE_GLES2_READ | SHARED_IMAGE_USAGE_GLES2_WRITE |
    SHARED_IMAGE_USAGE_GLES2_FOR_RASTER_ONLY |
    SHARED_IMAGE_USAGE_DISPLAY_WRITE | SHARED_IMAGE_USAGE_DISPLAY_READ |
    SHARED_IMAGE_USAGE_RASTER_READ | SHARED_IMAGE_USAGE_RASTER_WRITE |
    SHARED_IMAGE_USAGE_RASTER_OVER_GLES2_ONLY |
    SHARED_IMAGE_USAGE_OOP_RASTERIZATION | SHARED_IMAGE_USAGE_SCANOUT |
    SHARED_IMAGE_USAGE_WEBGPU_READ | SHARED_IMAGE_USAGE_WEBGPU_WRITE |
    SHARED_IMAGE_USAGE_CONCURRENT_READ_WRITE | SHARED_IMAGE_USAGE_VIDEO_DECODE |
    SHARED_IMAGE_USAGE_WEBGPU_SWAP_CHAIN_TEXTURE |
    SHARED_IMAGE_USAGE_RASTER_DELEGATED_COMPOSITING |
    SHARED_IMAGE_USAGE_HIGH_PERFORMANCE_GPU | SHARED_IMAGE_USAGE_CPU_UPLOAD |
    SHARED_IMAGE_USAGE_CPU_WRITE | SHARED_IMAGE_USAGE_WEBGPU_STORAGE_TEXTURE |
    SHARED_IMAGE_USAGE_PROTECTED_VIDEO;

}  // namespace

OzoneImageBackingFactory::OzoneImageBackingFactory(
    scoped_refptr<SharedContextState> shared_context_state,
    const GpuDriverBugWorkarounds& workarounds)
    : SharedImageBackingFactory(kSupportedUsage),
      shared_context_state_(std::move(shared_context_state)),
      workarounds_(workarounds) {}

OzoneImageBackingFactory::~OzoneImageBackingFactory() = default;

std::unique_ptr<OzoneImageBacking>
OzoneImageBackingFactory::CreateSharedImageInternal(
    const Mailbox& mailbox,
    viz::SharedImageFormat format,
    SurfaceHandle surface_handle,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
    SharedImageUsageSet usage,
    std::string debug_label,
    std::optional<gfx::BufferUsage> buffer_usage) {
  gfx::BufferFormat buffer_format = ToBufferFormat(format);
  VulkanDeviceQueue* device_queue = nullptr;
#if BUILDFLAG(ENABLE_VULKAN)
  DCHECK(shared_context_state_);
  if (shared_context_state_->vk_context_provider()) {
    device_queue =
        shared_context_state_->vk_context_provider()->GetDeviceQueue();
  }
#endif  // BUILDFLAG(ENABLE_VULKAN)
  ui::SurfaceFactoryOzone* surface_factory =
      ui::OzonePlatform::GetInstance()->GetSurfaceFactoryOzone();

  // Note that when |buffer_usage| is passed as a parameter and is not null, it
  // should be used instead of converting |usage| to it via GetBufferUsage().
  scoped_refptr<gfx::NativePixmap> pixmap = surface_factory->CreateNativePixmap(
      surface_handle, device_queue, size, buffer_format,
      buffer_usage.value_or(GetBufferUsage(usage)));
  // Fallback to GPU_READ if cannot create pixmap with SCANOUT
  if (!pixmap) {
    pixmap = surface_factory->CreateNativePixmap(surface_handle, device_queue,
                                                 size, buffer_format,
                                                 gfx::BufferUsage::GPU_READ);
  }
  if (!pixmap) {
    DLOG(ERROR) << "Failed to create native pixmap";
    return nullptr;
  }
  return std::make_unique<OzoneImageBacking>(
      mailbox, format, size, color_space, surface_origin, alpha_type, usage,
      std::move(debug_label), shared_context_state_, std::move(pixmap),
      workarounds_, std::move(buffer_usage));
}

std::unique_ptr<SharedImageBacking> OzoneImageBackingFactory::CreateSharedImage(
    const Mailbox& mailbox,
    viz::SharedImageFormat format,
    SurfaceHandle surface_handle,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
    SharedImageUsageSet usage,
    std::string debug_label,
    bool is_thread_safe) {
  CHECK(!is_thread_safe);
  return CreateSharedImageInternal(mailbox, format, surface_handle, size,
                                   color_space, surface_origin, alpha_type,
                                   usage, std::move(debug_label));
}

std::unique_ptr<SharedImageBacking> OzoneImageBackingFactory::CreateSharedImage(
    const Mailbox& mailbox,
    viz::SharedImageFormat format,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
    SharedImageUsageSet usage,
    std::string debug_label,
    bool is_thread_safe,
    base::span<const uint8_t> pixel_data) {
  CHECK(!is_thread_safe);
  SurfaceHandle surface_handle = SurfaceHandle();
  auto backing = CreateSharedImageInternal(
      mailbox, format, surface_handle, size, color_space, surface_origin,
      alpha_type, usage, std::move(debug_label));

  if (!backing) {
    return nullptr;
  }
  if (!pixel_data.empty()) {
    SkImageInfo info = backing->AsSkImageInfo();
    if (pixel_data.size() != info.computeMinByteSize()) {
      DLOG(ERROR) << "Invalid initial pixel data size";
      return nullptr;
    }
    SkPixmap pixmap(info, pixel_data.data(), info.minRowBytes());

    if (!backing->UploadFromMemory({pixmap})) {
      return nullptr;
    }
  }

  return backing;
}

std::unique_ptr<SharedImageBacking> OzoneImageBackingFactory::CreateSharedImage(
    const Mailbox& mailbox,
    viz::SharedImageFormat format,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
    SharedImageUsageSet usage,
    std::string debug_label,
    gfx::GpuMemoryBufferHandle handle) {
  DCHECK_EQ(handle.type, gfx::NATIVE_PIXMAP);

  ui::SurfaceFactoryOzone* surface_factory =
      ui::OzonePlatform::GetInstance()->GetSurfaceFactoryOzone();
  scoped_refptr<gfx::NativePixmap> pixmap =
      surface_factory->CreateNativePixmapFromHandle(
          kNullSurfaceHandle, size, ToBufferFormat(format),
          std::move(handle.native_pixmap_handle));
  if (!pixmap) {
    return nullptr;
  }

  auto backing = std::make_unique<OzoneImageBacking>(
      mailbox, format, size, color_space, surface_origin, alpha_type, usage,
      std::move(debug_label), shared_context_state_, std::move(pixmap),
      workarounds_);
  backing->SetCleared();

  return backing;
}

std::unique_ptr<SharedImageBacking> OzoneImageBackingFactory::CreateSharedImage(
    const Mailbox& mailbox,
    viz::SharedImageFormat format,
    SurfaceHandle surface_handle,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
    SharedImageUsageSet usage,
    std::string debug_label,
    bool is_thread_safe,
    gfx::BufferUsage buffer_usage) {
  DCHECK(!is_thread_safe);
  return CreateSharedImageInternal(mailbox, format, surface_handle, size,
                                   color_space, surface_origin, alpha_type,
                                   usage, std::move(debug_label), buffer_usage);
}

bool OzoneImageBackingFactory::IsSupported(
    SharedImageUsageSet usage,
    viz::SharedImageFormat format,
    const gfx::Size& size,
    bool thread_safe,
    gfx::GpuMemoryBufferType gmb_type,
    GrContextType gr_context_type,
    base::span<const uint8_t> pixel_data) {
  if (gmb_type != gfx::EMPTY_BUFFER && gmb_type != gfx::NATIVE_PIXMAP) {
    return false;
  }

  if (usage.Has(SHARED_IMAGE_USAGE_CPU_WRITE) &&
      gmb_type != gfx::NATIVE_PIXMAP) {
    // Only CPU writable when the client provides a NativePixmap.
    return false;
  }

  bool used_by_skia = usage.HasAny(
      SHARED_IMAGE_USAGE_RASTER_READ | SHARED_IMAGE_USAGE_RASTER_WRITE |
      SHARED_IMAGE_USAGE_DISPLAY_READ | SHARED_IMAGE_USAGE_DISPLAY_WRITE);
  bool used_by_vulkan =
      used_by_skia && gr_context_type == GrContextType::kVulkan;
  bool used_by_webgpu = usage.HasAny(SHARED_IMAGE_USAGE_WEBGPU_READ |
                                     SHARED_IMAGE_USAGE_WEBGPU_WRITE);
  bool used_by_gl = (HasGLES2ReadOrWriteUsage(usage)) ||
                    (used_by_skia && gr_context_type == GrContextType::kGL);
  if (used_by_vulkan && !CanImportNativePixmapToVulkan()) {
    return false;
  }
  if (used_by_webgpu && !CanImportNativePixmapToWebGPU()) {
    return false;
  }
  auto* factory = ui::OzonePlatform::GetInstance()->GetSurfaceFactoryOzone();
  if (viz::HasEquivalentBufferFormat(format) &&
      !factory->CanCreateNativePixmapForFormat(ToBufferFormat(format))) {
    return false;
  }

  ui::GLOzone* gl_ozone = factory->GetCurrentGLOzone();
  if (used_by_gl &&
      (!gl_ozone || !gl_ozone->CanImportNativePixmap(ToBufferFormat(format)))) {
    return false;
  }

  bool platform_supports_overlays = ui::OzonePlatform::GetInstance()
                                        ->GetPlatformRuntimeProperties()
                                        .supports_overlays;
  // If overlays are not supported by the Ozone platform, then only display
  // compositor output images allocated through OzoneImageBacking may use
  // OverlayRepresentation.
  bool used_by_overlay = usage.Has(SHARED_IMAGE_USAGE_SCANOUT) &&
                         (platform_supports_overlays ||
                          usage.Has(SHARED_IMAGE_USAGE_DISPLAY_WRITE));
  // We may rely on implicit synchronization for GL/Overlay synchronization in
  // case GpuFence support is not available.
  bool gl_overlay_requires_fence_sync =
      gl::GetANGLEImplementation() == gl::ANGLEImplementation::kVulkan;
  bool used_by_multiple =
      used_by_vulkan + used_by_webgpu + used_by_gl + used_by_overlay > 1;
  bool require_gpu_fence_sync =
      used_by_multiple &&
      (gl_overlay_requires_fence_sync || used_by_vulkan || used_by_webgpu);

  if (require_gpu_fence_sync) {
    if (used_by_vulkan && !CanVulkanSynchronizeGpuFence()) {
      return false;
    }

    if (used_by_gl && !gl::GLFence::IsGpuFenceSupported()) {
      return false;
    }

    if (used_by_webgpu && !CanWebGPUSynchronizeGpuFence()) {
      return false;
    }
  }

#if BUILDFLAG(IS_FUCHSIA)
  if (gr_context_type != GrContextType::kVulkan) {
    return false;
  }

  // For now just use OzoneImageBacking for primary plane buffers.
  // TODO(crbug.com/40219694): When Vulkan/GL interop is supported on Fuchsia
  // OzoneImageBacking should be used for all scanout buffers.
  constexpr uint32_t kPrimaryPlaneUsageFlags =
      SHARED_IMAGE_USAGE_DISPLAY_READ | SHARED_IMAGE_USAGE_DISPLAY_WRITE |
      SHARED_IMAGE_USAGE_SCANOUT;
  if (usage != kPrimaryPlaneUsageFlags || gmb_type != gfx::EMPTY_BUFFER) {
    return false;
  }
#endif

  return true;
}

bool OzoneImageBackingFactory::CanImportNativePixmapToVulkan() {
#if BUILDFLAG(ENABLE_VULKAN)
  if (!shared_context_state_->vk_context_provider()) {
    return false;
  }
  auto* vk_device =
      shared_context_state_->vk_context_provider()->GetDeviceQueue();
  return shared_context_state_->vk_context_provider()
      ->GetVulkanImplementation()
      ->CanImportGpuMemoryBuffer(vk_device, gfx::NATIVE_PIXMAP);
#else
  return false;
#endif  // BUILDFLAG(ENABLE_VULKAN)
}

bool OzoneImageBackingFactory::CanVulkanSynchronizeGpuFence() {
#if BUILDFLAG(ENABLE_VULKAN)
#if BUILDFLAG(IS_FUCHSIA)
  constexpr auto kGpuFenceExternalSemaphoreHandleType =
      VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_ZIRCON_EVENT_BIT_FUCHSIA;
#else
  constexpr auto kGpuFenceExternalSemaphoreHandleType =
      VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_SYNC_FD_BIT;
#endif
  if (!shared_context_state_->vk_context_provider()) {
    return false;
  }
  auto* vk_device =
      shared_context_state_->vk_context_provider()->GetDeviceQueue();
  auto* vk_implementation =
      shared_context_state_->vk_context_provider()->GetVulkanImplementation();
  return vk_implementation->GetExternalSemaphoreHandleType() ==
             kGpuFenceExternalSemaphoreHandleType &&
         vk_implementation->IsExternalSemaphoreSupported(vk_device);
#else
  return false;
#endif  // BUILDFLAG(ENABLE_VULKAN)
}

bool OzoneImageBackingFactory::CanImportNativePixmapToWebGPU() {
#if BUILDFLAG(IS_CHROMEOS)
  // Safe to always return true here, as it's not possible to create a WebGPU
  // adapter that doesn't support importing native pixmaps:
  // https://source.chromium.org/chromium/chromium/src/+/main:gpu/command_buffer/service/webgpu_decoder_impl.cc;drc=daed597d580d450d36578c0cc53b4f72d3b507da;l=1291
  // TODO(crbug.com/40855765): To check it without vk_context_provider.
  return true;
#else
  // Disable all WebGPU ozone usage for non-Chromeos Ozone (Fuchsia, Linux).
  // WebGPU on non-ChromeOS will now go through the ExternalVkImageBacking. Long
  // term we will return to using the ozone backing on devices that have sync
  // fences.
  // TODO(crbug.com/330385376): Support dynamic switching of fence types in
  // dawn and runtime extension testing.
  // This testing in runtime can be done where graphite is enabled by checking
  // against features in the 'dawn_context_provider' in the
  // 'shared_context_state_'.
  return false;
#endif
}

bool OzoneImageBackingFactory::CanWebGPUSynchronizeGpuFence() {
#if BUILDFLAG(IS_CHROMEOS)
  // Dawn always use sync files on ChromeOS so it's safe to unconditionally
  // return true here.
  return true;
#else
  // TODO: somehow check if Dawn is using sync files.
  return false;
#endif
}

SharedImageBackingType OzoneImageBackingFactory::GetBackingType() {
  return SharedImageBackingType::kOzone;
}

}  // namespace gpu
