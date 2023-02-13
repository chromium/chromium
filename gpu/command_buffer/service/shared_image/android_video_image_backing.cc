// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image/android_video_image_backing.h"

#include "base/android/scoped_hardware_buffer_fence_sync.h"
#include "components/viz/common/gpu/vulkan_context_provider.h"
#include "components/viz/common/resources/resource_sizes.h"
#include "components/viz/common/resources/shared_image_format.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/abstract_texture_android.h"
#include "gpu/command_buffer/service/ref_counted_lock.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/shared_image/video_image_reader_image_backing.h"
#include "gpu/command_buffer/service/shared_image/video_surface_texture_image_backing.h"
#include "gpu/command_buffer/service/texture_owner.h"
#include "gpu/config/gpu_finch_features.h"
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
    bool is_thread_safe)
    : AndroidImageBacking(
          mailbox,
          viz::SinglePlaneFormat::kRGBA_8888,
          size,
          color_space,
          surface_origin,
          alpha_type,
          (SHARED_IMAGE_USAGE_DISPLAY_READ | SHARED_IMAGE_USAGE_GLES2),
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
    scoped_refptr<StreamTextureSharedImageInterface> stream_texture_sii,
    scoped_refptr<SharedContextState> context_state,
    scoped_refptr<RefCountedLock> drdc_lock) {
  if (features::IsAImageReaderEnabled()) {
    return std::make_unique<VideoImageReaderImageBacking>(
        mailbox, size, color_space, surface_origin, alpha_type,
        std::move(stream_texture_sii), std::move(context_state),
        std::move(drdc_lock));
  } else {
    DCHECK(!drdc_lock);
    return std::make_unique<VideoSurfaceTextureImageBacking>(
        mailbox, size, color_space, surface_origin, alpha_type,
        std::move(stream_texture_sii), std::move(context_state));
  }
}

// Static.
absl::optional<VulkanYCbCrInfo> AndroidVideoImageBacking::GetYcbcrInfo(
    TextureOwner* texture_owner,
    viz::VulkanContextProvider* vulkan_context_provider) {
  if (!vulkan_context_provider)
    return absl::nullopt;

  // Get AHardwareBuffer from the latest frame.
  auto scoped_hardware_buffer = texture_owner->GetAHardwareBuffer();
  if (!scoped_hardware_buffer)
    return absl::nullopt;

  DCHECK(scoped_hardware_buffer->buffer());
  VulkanImplementation* vk_implementation =
      vulkan_context_provider->GetVulkanImplementation();
  VkDevice vk_device =
      vulkan_context_provider->GetDeviceQueue()->GetVulkanDevice();

  VulkanYCbCrInfo ycbcr_info;
  if (!vk_implementation->GetSamplerYcbcrConversionInfo(
          vk_device, scoped_hardware_buffer->TakeBuffer(), &ycbcr_info)) {
    LOG(ERROR) << "Failed to get the ycbcr info.";
    return absl::nullopt;
  }
  return absl::optional<VulkanYCbCrInfo>(ycbcr_info);
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

}  // namespace gpu
