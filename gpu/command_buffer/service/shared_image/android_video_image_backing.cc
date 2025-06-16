// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image/android_video_image_backing.h"

#include <dawn/webgpu.h>

#include "base/android/android_image_reader_compat.h"
#include "base/android/scoped_hardware_buffer_fence_sync.h"
#include "components/viz/common/gpu/vulkan_context_provider.h"
#include "components/viz/common/resources/resource_sizes.h"
#include "components/viz/common/resources/shared_image_format.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/abstract_texture_android.h"
#include "gpu/command_buffer/service/dawn_context_provider.h"
#include "gpu/command_buffer/service/ref_counted_lock.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/shared_image/video_image_reader_image_backing.h"
#include "gpu/command_buffer/service/shared_image/video_surface_texture_image_backing.h"
#include "gpu/command_buffer/service/texture_owner.h"
#include "gpu/vulkan/vulkan_device_queue.h"
#include "gpu/vulkan/vulkan_implementation.h"
#include "ui/gfx/gpu_fence.h"

namespace gpu {

AndroidVideoImageBacking::AndroidVideoImageBacking(
    const Mailbox& mailbox,
    const gfx::Size& size,
    const gfx::ColorSpace color_space,
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
    std::string debug_label,
    bool is_thread_safe)
    : AndroidImageBacking(
          mailbox,
          viz::SinglePlaneFormat::kRGBA_8888,
          size,
          color_space,
          surface_origin,
          alpha_type,
          // This SI will be used to back a VideoFrame. As such, it
          // will potentially be sent to the display compositor and read by the
          // GL interface for WebGL.
          // TODO: crbug.com/354856448 - add a parameter to the constructor that
          // allows to specify whether SCANOUT is needed.
          {SHARED_IMAGE_USAGE_DISPLAY_READ, SHARED_IMAGE_USAGE_GLES2_READ,
           SHARED_IMAGE_USAGE_SCANOUT},
          std::move(debug_label),
          viz::SinglePlaneFormat::kRGBA_8888.EstimatedSizeInBytes(size),
          is_thread_safe,
          base::ScopedFD()) {}

AndroidVideoImageBacking::~AndroidVideoImageBacking() {}

// Static.
std::unique_ptr<AndroidVideoImageBacking> AndroidVideoImageBacking::Create(
    const Mailbox& mailbox,
    const gfx::Size& size,
    const gfx::ColorSpace color_space,
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
    std::string debug_label,
    scoped_refptr<StreamTextureSharedImageInterface> stream_texture_sii,
    scoped_refptr<SharedContextState> context_state,
    scoped_refptr<RefCountedLock> drdc_lock) {
  if (base::android::EnableAndroidImageReader()) {
    return std::make_unique<VideoImageReaderImageBacking>(
        mailbox, size, color_space, surface_origin, alpha_type,
        std::move(debug_label), std::move(stream_texture_sii),
        std::move(context_state), std::move(drdc_lock));
  } else {
    DCHECK(!drdc_lock);
    return std::make_unique<VideoSurfaceTextureImageBacking>(
        mailbox, size, color_space, surface_origin, alpha_type,
        std::move(debug_label), std::move(stream_texture_sii),
        std::move(context_state));
  }
}

// Static.
std::optional<VulkanYCbCrInfo> AndroidVideoImageBacking::GetYcbcrInfo(
    TextureOwner* texture_owner,
    viz::VulkanContextProvider* vulkan_context_provider,
    DawnContextProvider* dawn_context_provider) {
  if (!vulkan_context_provider && !dawn_context_provider) {
    return std::nullopt;
  }

  // Get AHardwareBuffer from the latest frame.
  auto scoped_hardware_buffer = texture_owner->GetAHardwareBuffer();
  if (!scoped_hardware_buffer)
    return std::nullopt;

  DCHECK(scoped_hardware_buffer->buffer());

  if (vulkan_context_provider) {
    CHECK(!dawn_context_provider);

    VulkanImplementation* vk_implementation =
        vulkan_context_provider->GetVulkanImplementation();
    VkDevice vk_device =
        vulkan_context_provider->GetDeviceQueue()->GetVulkanDevice();

    VulkanYCbCrInfo ycbcr_info;
    if (!vk_implementation->GetSamplerYcbcrConversionInfo(
            vk_device, scoped_hardware_buffer->TakeBuffer(), &ycbcr_info)) {
      LOG(ERROR) << "Failed to get the ycbcr info.";
      return std::nullopt;
    }
    return std::optional<VulkanYCbCrInfo>(ycbcr_info);
  }

#if BUILDFLAG(SKIA_USE_DAWN)
  // Get the YCbCr info from Dawn.

  auto device = dawn_context_provider->GetDevice();

  wgpu::AHardwareBufferProperties ahb_properties;
  if (!device.GetAHardwareBufferProperties(scoped_hardware_buffer->buffer(),
                                           &ahb_properties)) {
    LOG(ERROR) << "Failed to get the ycbcr info.";
    return std::nullopt;
  }

  // Populate the Chrome-side YCbCr info from the Dawn info.
  auto ycbcr_info = ahb_properties.yCbCrInfo;

  if (!ycbcr_info.externalFormat) {
    LOG(ERROR) << "Failed to get the ycbcr info.";
    return std::nullopt;
  }

  // NOTE: VulkanYCbCrInfo requires that the format be UNDEFINED if the external
  // format is non-zero.
  auto vk_format = VK_FORMAT_UNDEFINED;

  // NOTE: Dawn does not explicitly reflect `formatFeatures`, but
  // VulkanYCbCrInfo requires that `format_features` be non-zero when the
  // external format is non-zero. The below bit must always be set by the Vulkan
  // spec when YCbCr sampling is used, so it can safely be set here to satisfy
  // this constraint.
  uint32_t format_features = VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT;

  // Pass along the bit of whether linear filtering is supported. Viz will
  // extract this information and use it to construct `vkChromaFilter` when
  // passing YCbCr info to Skia via DawnTextureInfo.
  if (ycbcr_info.vkChromaFilter == wgpu::FilterMode::Linear) {
    format_features |=
        VK_FORMAT_FEATURE_SAMPLED_IMAGE_YCBCR_CONVERSION_LINEAR_FILTER_BIT;
  }

  return VulkanYCbCrInfo(vk_format, ycbcr_info.externalFormat,
                         ycbcr_info.vkYCbCrModel, ycbcr_info.vkYCbCrRange,
                         ycbcr_info.vkXChromaOffset, ycbcr_info.vkYChromaOffset,
                         format_features);
#else
  return std::nullopt;
#endif
}

std::unique_ptr<AbstractTextureAndroid>
AndroidVideoImageBacking::GenAbstractTexture(const bool passthrough) {
  if (passthrough) {
    return AbstractTextureAndroid::CreateForPassthrough(size());
  } else {
    return AbstractTextureAndroid::CreateForValidating(size());
  }
}

SharedImageBackingType AndroidVideoImageBacking::GetType() const {
  return SharedImageBackingType::kVideo;
}

gfx::Rect AndroidVideoImageBacking::ClearedRect() const {
  // AndroidVideoImageBacking objects are always created from pre-initialized
  // textures provided by the media decoder. Always treat these as cleared
  // (return the full rectangle).
  return gfx::Rect(size());
}

void AndroidVideoImageBacking::SetClearedRect(const gfx::Rect& cleared_rect) {}

void AndroidVideoImageBacking::Update(std::unique_ptr<gfx::GpuFence> in_fence) {
  DCHECK(!in_fence);
}

size_t AndroidVideoImageBacking::GetEstimatedSizeForMemoryDump() const {
  // None of these images own memory directly, so we report 0. The real memory
  // will be reported by `TextureOwner`s.
  return 0;
}

}  // namespace gpu
