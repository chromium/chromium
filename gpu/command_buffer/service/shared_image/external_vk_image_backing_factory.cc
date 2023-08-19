// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image/external_vk_image_backing_factory.h"

#include "build/build_config.h"
#include "components/viz/common/gpu/vulkan_context_provider.h"
#include "components/viz/common/resources/shared_image_format.h"
#include "components/viz/common/resources/shared_image_format_utils.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/shared_image/external_vk_image_backing.h"
#include "gpu/command_buffer/service/shared_image/shared_image_format_service_utils.h"
#include "gpu/command_buffer/service/shared_image/shared_image_representation.h"
#include "gpu/vulkan/vulkan_command_buffer.h"
#include "gpu/vulkan/vulkan_command_pool.h"
#include "gpu/vulkan/vulkan_device_queue.h"
#include "gpu/vulkan/vulkan_fence_helper.h"
#include "gpu/vulkan/vulkan_function_pointers.h"
#include "gpu/vulkan/vulkan_implementation.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gfx/gpu_memory_buffer.h"
#include "ui/gl/buildflags.h"

namespace gpu {

namespace {

VkImageUsageFlags GetMaximalImageUsageFlags(
    VkFormatFeatureFlags feature_flags) {
  VkImageUsageFlags usage_flags = 0;
  // The TRANSFER_SRC/DST format features were added in Vulkan 1.1 and their
  // support is required when SAMPLED_IMAGE is supported. In Vulkan 1.0 all
  // formats support these features implicitly. See discussion in
  // https://github.com/KhronosGroup/Vulkan-Docs/issues/1223
  if (feature_flags & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT)
    usage_flags |= VK_IMAGE_USAGE_SAMPLED_BIT |
                   VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                   VK_IMAGE_USAGE_TRANSFER_DST_BIT;

  // VUID-VkImageViewCreateInfo-usage-02652: support for INPUT_ATTACHMENT is
  // implied by both of COLOR_ATTACHNENT and DEPTH_STENCIL_ATTACHMENT
  if (feature_flags & VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT)
    usage_flags |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                   VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;
  if (feature_flags & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)
    usage_flags |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT |
                   VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;

  if (feature_flags & VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT)
    usage_flags |= VK_IMAGE_USAGE_STORAGE_BIT;
  if (feature_flags & VK_FORMAT_FEATURE_TRANSFER_SRC_BIT)
    usage_flags |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
  if (feature_flags & VK_FORMAT_FEATURE_TRANSFER_DST_BIT)
    usage_flags |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;

  return usage_flags;
}

base::flat_map<VkFormat, VkImageUsageFlags> CreateImageUsageCache(
    VkPhysicalDevice vk_physical_device) {
  base::flat_map<VkFormat, VkImageUsageFlags> image_usage_cache;

  auto add_to_cache_if_supported = [&image_usage_cache, &vk_physical_device](
                                       viz::SharedImageFormat format) {
    if (!HasVkFormat(format)) {
      return;
    }
    VkFormat vk_format = ToVkFormat(format);
    DCHECK_NE(vk_format, VK_FORMAT_UNDEFINED);
    VkFormatProperties format_props = {};
    vkGetPhysicalDeviceFormatProperties(vk_physical_device, vk_format,
                                        &format_props);
    image_usage_cache[vk_format] =
        GetMaximalImageUsageFlags(format_props.optimalTilingFeatures);
  };

  for (auto format : viz::SinglePlaneFormat::kAll) {
    add_to_cache_if_supported(format);
  }

  for (auto format : viz::LegacyMultiPlaneFormat::kAll) {
    add_to_cache_if_supported(format);
  }

  return image_usage_cache;
}

}  // namespace

constexpr uint32_t kSupportedUsage =
#if BUILDFLAG(IS_LINUX) && BUILDFLAG(USE_DAWN)
    SHARED_IMAGE_USAGE_WEBGPU | SHARED_IMAGE_USAGE_WEBGPU_SWAP_CHAIN_TEXTURE |
    SHARED_IMAGE_USAGE_WEBGPU_STORAGE_TEXTURE |
#endif
    SHARED_IMAGE_USAGE_GLES2 | SHARED_IMAGE_USAGE_GLES2_FRAMEBUFFER_HINT |
    SHARED_IMAGE_USAGE_DISPLAY_WRITE | SHARED_IMAGE_USAGE_DISPLAY_READ |
    SHARED_IMAGE_USAGE_RASTER | SHARED_IMAGE_USAGE_OOP_RASTERIZATION |
    SHARED_IMAGE_USAGE_SCANOUT | SHARED_IMAGE_USAGE_VIDEO_DECODE |
    SHARED_IMAGE_USAGE_HIGH_PERFORMANCE_GPU | SHARED_IMAGE_USAGE_CPU_UPLOAD |
    SHARED_IMAGE_USAGE_CPU_WRITE;

ExternalVkImageBackingFactory::ExternalVkImageBackingFactory(
    scoped_refptr<SharedContextState> context_state)
    : SharedImageBackingFactory(kSupportedUsage),
      context_state_(std::move(context_state)),
      command_pool_(context_state_->vk_context_provider()
                        ->GetDeviceQueue()
                        ->CreateCommandPool()),
      image_usage_cache_(
          CreateImageUsageCache(context_state_->vk_context_provider()
                                    ->GetDeviceQueue()
                                    ->GetVulkanPhysicalDevice())) {}

ExternalVkImageBackingFactory::~ExternalVkImageBackingFactory() {
  if (command_pool_) {
    context_state_->vk_context_provider()
        ->GetDeviceQueue()
        ->GetFenceHelper()
        ->EnqueueVulkanObjectCleanupForSubmittedWork(std::move(command_pool_));
  }
}

std::unique_ptr<SharedImageBacking>
ExternalVkImageBackingFactory::CreateSharedImage(
    const Mailbox& mailbox,
    viz::SharedImageFormat format,
    SurfaceHandle surface_handle,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
    uint32_t usage,
    std::string debug_label,
    bool is_thread_safe) {
  DCHECK(!is_thread_safe);
  return ExternalVkImageBacking::Create(
      context_state_, command_pool_.get(), mailbox, format, size, color_space,
      surface_origin, alpha_type, usage, image_usage_cache_,
      base::span<const uint8_t>());
}

std::unique_ptr<SharedImageBacking>
ExternalVkImageBackingFactory::CreateSharedImage(
    const Mailbox& mailbox,
    viz::SharedImageFormat format,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
    uint32_t usage,
    std::string debug_label,
    base::span<const uint8_t> pixel_data) {
  return ExternalVkImageBacking::Create(
      context_state_, command_pool_.get(), mailbox, format, size, color_space,
      surface_origin, alpha_type, usage, image_usage_cache_, pixel_data);
}

std::unique_ptr<SharedImageBacking>
ExternalVkImageBackingFactory::CreateSharedImage(
    const Mailbox& mailbox,
    viz::SharedImageFormat format,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
    uint32_t usage,
    std::string debug_label,
    gfx::GpuMemoryBufferHandle handle) {
  CHECK(CanImportGpuMemoryBuffer(handle.type));
  return ExternalVkImageBacking::CreateFromGMB(
      context_state_, command_pool_.get(), mailbox, std::move(handle), format,
      size, color_space, surface_origin, alpha_type, usage);
}

std::unique_ptr<SharedImageBacking>
ExternalVkImageBackingFactory::CreateSharedImage(
    const Mailbox& mailbox,
    gfx::GpuMemoryBufferHandle handle,
    gfx::BufferFormat buffer_format,
    gfx::BufferPlane plane,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
    uint32_t usage,
    std::string debug_label) {
  if (plane != gfx::BufferPlane::DEFAULT) {
    LOG(ERROR) << "Invalid plane";
    return nullptr;
  }
  return CreateSharedImage(mailbox,
                           viz::GetSinglePlaneSharedImageFormat(buffer_format),
                           size, color_space, surface_origin, alpha_type, usage,
                           debug_label, std::move(handle));
}

std::unique_ptr<SharedImageBacking>
ExternalVkImageBackingFactory::CreateSharedImage(
    const Mailbox& mailbox,
    viz::SharedImageFormat format,
    SurfaceHandle surface_handle,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
    uint32_t usage,
    std::string debug_label,
    bool is_thread_safe,
    gfx::BufferUsage buffer_usage) {
  DCHECK(!is_thread_safe);
#if BUILDFLAG(IS_OZONE)
  // Creating the backing with a native pixmap so that it can be CPU mappable.
  return ExternalVkImageBacking::CreateWithPixmap(
      context_state_, command_pool_.get(), mailbox, format, surface_handle,
      size, color_space, surface_origin, alpha_type, usage, buffer_usage);
#else
  // A CPU mappable backing of this type can only be requested for OZONE
  // platforms.
  NOTREACHED();
  return nullptr;
#endif  // BUILDFLAG(IS_OZONE)
}

bool ExternalVkImageBackingFactory::CanImportGpuMemoryBuffer(
    gfx::GpuMemoryBufferType memory_buffer_type) {
  auto* device_queue = context_state_->vk_context_provider()->GetDeviceQueue();
  return context_state_->vk_context_provider()
      ->GetVulkanImplementation()
      ->CanImportGpuMemoryBuffer(device_queue, memory_buffer_type);
}

bool ExternalVkImageBackingFactory::IsSupported(
    uint32_t usage,
    viz::SharedImageFormat format,
    const gfx::Size& size,
    bool thread_safe,
    gfx::GpuMemoryBufferType gmb_type,
    GrContextType gr_context_type,
    base::span<const uint8_t> pixel_data) {
  if (format.is_multi_plane()) {
    if (gmb_type != gfx::EMPTY_BUFFER) {
      return false;
    }

    if (format != viz::MultiPlaneFormat::kNV12 &&
        format != viz::MultiPlaneFormat::kYV12 &&
        format != viz::MultiPlaneFormat::kI420) {
      return false;
    }
  }

  // ALPHA_8 is only used by UI and should never need GL/Vulkan interop.
  // LUMINANCE_8 is only used with GL ES2 contexts and shouldn't be relevant for
  // devices that support Vulkan.
  if (format == viz::SinglePlaneFormat::kALPHA_8 ||
      format == viz::SinglePlaneFormat::kLUMINANCE_8) {
    return false;
  }

#if BUILDFLAG(IS_LINUX)
  if (format.IsLegacyMultiplanar()) {
    // ExternalVkImageBacking doesn't work properly with external sampler
    // multi-planar formats on Linux, see https://crbug.com/1394888.
    return false;
  }
#endif

  if (gmb_type == gfx::EMPTY_BUFFER) {
    if (usage & SHARED_IMAGE_USAGE_CPU_WRITE) {
      // Only CPU writable when the client provides a NativePixmap.
      return false;
    }
  } else {
    if (!CanImportGpuMemoryBuffer(gmb_type)) {
      return false;
    }
  }

  if (thread_safe) {
    LOG(ERROR) << "ExternalVkImageBackingFactory currently do not support "
                  "cross-thread usage.";
    return false;
  }

#if BUILDFLAG(IS_ANDROID)
  // Scanout on Android requires explicit fence synchronization which is only
  // supported by the interop factory.
  if (usage & SHARED_IMAGE_USAGE_SCANOUT) {
    return false;
  }
#endif

  return true;
}

}  // namespace gpu
