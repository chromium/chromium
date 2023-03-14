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
#include "components/viz/common/resources/resource_format_utils.h"
#include "gpu/command_buffer/common/gpu_memory_buffer_support.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/service_utils.h"
#include "gpu/command_buffer/service/shared_image/ozone_image_backing.h"
#include "gpu/command_buffer/service/shared_image/shared_image_format_utils.h"
#include "gpu/command_buffer/service/shared_memory_region_wrapper.h"
#include "ui/gfx/gpu_memory_buffer.h"
#include "ui/gfx/native_pixmap.h"
#include "ui/gl/buildflags.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_surface_egl.h"
#include "ui/ozone/public/ozone_platform.h"
#include "ui/ozone/public/surface_factory_ozone.h"

#if BUILDFLAG(ENABLE_VULKAN)
#include "components/viz/common/gpu/vulkan_context_provider.h"
#include "gpu/vulkan/vulkan_device_queue.h"
#endif  // BUILDFLAG(ENABLE_VULKAN)

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

constexpr uint32_t kSupportedUsage =
    SHARED_IMAGE_USAGE_GLES2 | SHARED_IMAGE_USAGE_GLES2_FRAMEBUFFER_HINT |
    SHARED_IMAGE_USAGE_DISPLAY_WRITE | SHARED_IMAGE_USAGE_DISPLAY_READ |
    SHARED_IMAGE_USAGE_RASTER | SHARED_IMAGE_USAGE_OOP_RASTERIZATION |
    SHARED_IMAGE_USAGE_SCANOUT | SHARED_IMAGE_USAGE_WEBGPU |
    SHARED_IMAGE_USAGE_CONCURRENT_READ_WRITE | SHARED_IMAGE_USAGE_VIDEO_DECODE |
    SHARED_IMAGE_USAGE_WEBGPU_SWAP_CHAIN_TEXTURE |
    SHARED_IMAGE_USAGE_RASTER_DELEGATED_COMPOSITING |
    SHARED_IMAGE_USAGE_HIGH_PERFORMANCE_GPU | SHARED_IMAGE_USAGE_CPU_UPLOAD |
    SHARED_IMAGE_USAGE_CPU_WRITE;

}  // namespace

OzoneImageBackingFactory::OzoneImageBackingFactory(
    SharedContextState* shared_context_state,
    const GpuDriverBugWorkarounds& workarounds,
    const GpuPreferences& gpu_preferences)
    : SharedImageBackingFactory(kSupportedUsage),
      shared_context_state_(shared_context_state),
      workarounds_(workarounds),
      use_passthrough_(gpu_preferences.use_passthrough_cmd_decoder &&
                       gles2::PassthroughCommandDecoderSupported()) {
#if BUILDFLAG(USE_DAWN)
  dawn_procs_ = base::MakeRefCounted<base::RefCountedData<DawnProcTable>>(
      dawn::native::GetProcs());
#endif  // BUILDFLAG(USE_DAWN)
}

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
    uint32_t usage) {
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
  scoped_refptr<gfx::NativePixmap> pixmap = surface_factory->CreateNativePixmap(
      surface_handle, device_queue, size, buffer_format, GetBufferUsage(usage));
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
      mailbox, format, gfx::BufferPlane::DEFAULT, size, color_space,
      surface_origin, alpha_type, usage, shared_context_state_.get(),
      std::move(pixmap), dawn_procs_, workarounds_, use_passthrough_);
}

std::unique_ptr<SharedImageBacking> OzoneImageBackingFactory::CreateSharedImage(
    const Mailbox& mailbox,
    viz::SharedImageFormat format,
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
    viz::SharedImageFormat format,
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

    if (!backing->UploadFromMemory({pixmap})) {
      return nullptr;
    }
  }

  return backing;
}

std::unique_ptr<SharedImageBacking> OzoneImageBackingFactory::CreateSharedImage(
    const Mailbox& mailbox,
    gfx::GpuMemoryBufferHandle handle,
    gfx::BufferFormat buffer_format,
    gfx::BufferPlane plane,
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
          kNullSurfaceHandle, size, buffer_format,
          std::move(handle.native_pixmap_handle));
  if (!pixmap) {
    return nullptr;
  }

  const gfx::Size plane_size = gpu::GetPlaneSize(plane, size);
  const viz::ResourceFormat plane_format =
      viz::GetResourceFormat(GetPlaneBufferFormat(plane, buffer_format));
  auto backing = std::make_unique<OzoneImageBacking>(
      mailbox, viz::SharedImageFormat::SinglePlane(plane_format), plane,
      plane_size, color_space, surface_origin, alpha_type, usage,
      shared_context_state_.get(), std::move(pixmap), dawn_procs_, workarounds_,
      use_passthrough_);
  backing->SetCleared();

  return backing;
}

std::unique_ptr<SharedImageBacking> OzoneImageBackingFactory::CreateSharedImage(
    const Mailbox& mailbox,
    viz::SharedImageFormat format,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
    uint32_t usage,
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
      mailbox, format, gfx::BufferPlane::DEFAULT, size, color_space,
      surface_origin, alpha_type, usage, shared_context_state_.get(),
      std::move(pixmap), dawn_procs_, workarounds_, use_passthrough_);
  backing->SetCleared();

  return backing;
}

bool OzoneImageBackingFactory::IsSupported(
    uint32_t usage,
    viz::SharedImageFormat format,
    const gfx::Size& size,
    bool thread_safe,
    gfx::GpuMemoryBufferType gmb_type,
    GrContextType gr_context_type,
    base::span<const uint8_t> pixel_data) {
  if (gmb_type != gfx::EMPTY_BUFFER && gmb_type != gfx::NATIVE_PIXMAP) {
    return false;
  }

  bool used_by_skia = (usage & SHARED_IMAGE_USAGE_RASTER) ||
                      (usage & SHARED_IMAGE_USAGE_DISPLAY_READ) ||
                      (usage & SHARED_IMAGE_USAGE_DISPLAY_WRITE);
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
  auto* factory = ui::OzonePlatform::GetInstance()->GetSurfaceFactoryOzone();
  if (!factory->CanCreateNativePixmapForFormat(ToBufferFormat(format)))
    return false;

  ui::GLOzone* gl_ozone = factory->GetCurrentGLOzone();
  if (used_by_gl && (!gl_ozone || !gl_ozone->CanImportNativePixmap())) {
    return false;
  }

#if BUILDFLAG(IS_FUCHSIA)
  if (gr_context_type != GrContextType::kVulkan) {
    return false;
  }

  // For now just use OzoneImageBacking for primary plane buffers.
  // TODO(crbug.com/1310026): When Vulkan/GL interop is supported on Fuchsia
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

bool OzoneImageBackingFactory::CanImportNativePixmapToWebGPU() {
#if BUILDFLAG(IS_CHROMEOS)
  // Safe to always return true here, as it's not possible to create a WebGPU
  // adapter that doesn't support importing native pixmaps:
  // https://source.chromium.org/chromium/chromium/src/+/main:gpu/command_buffer/service/webgpu_decoder_impl.cc;drc=daed597d580d450d36578c0cc53b4f72d3b507da;l=1291
  // TODO(crbug.com/1349189): To check it without vk_context_provider.
  return true;
#else
  // Assume that if skia/vulkan vkDevice supports the Vulkan extensions
  // (external_memory_dma_buf, image_drm_format_modifier), then Dawn/WebGPU also
  // support the extensions until there is capability to check the extensions
  // from Dawn vkDevice when they are exposed.
  return CanImportNativePixmapToVulkan();
#endif
}

}  // namespace gpu
