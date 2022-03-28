// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image_video.h"

#include "base/android/scoped_hardware_buffer_fence_sync.h"
#include "components/viz/common/gpu/vulkan_context_provider.h"
#include "components/viz/common/resources/resource_sizes.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/abstract_texture.h"
#include "gpu/command_buffer/service/abstract_texture_impl.h"
#include "gpu/command_buffer/service/mailbox_manager.h"
#include "gpu/command_buffer/service/ref_counted_lock.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/shared_image_video_image_reader.h"
#include "gpu/command_buffer/service/shared_image_video_surface_texture.h"
#include "gpu/command_buffer/service/texture_owner.h"
#include "gpu/config/gpu_finch_features.h"
#include "gpu/vulkan/vulkan_device_queue.h"
#include "gpu/vulkan/vulkan_implementation.h"

namespace gpu {

SharedImageVideo::SharedImageVideo(const Mailbox& mailbox,
                                   const gfx::Size& size,
                                   const gfx::ColorSpace color_space,
                                   GrSurfaceOrigin surface_origin,
                                   SkAlphaType alpha_type,
                                   bool is_thread_safe)
    : SharedImageBackingAndroid(
          mailbox,
          viz::RGBA_8888,
          size,
          color_space,
          surface_origin,
          alpha_type,
          (SHARED_IMAGE_USAGE_DISPLAY | SHARED_IMAGE_USAGE_GLES2),
          viz::ResourceSizes::UncheckedSizeInBytes<size_t>(size,
                                                           viz::RGBA_8888),
          is_thread_safe,
          base::ScopedFD()) {}

SharedImageVideo::~SharedImageVideo() {}

// Static.
std::unique_ptr<SharedImageVideo> SharedImageVideo::Create(
    const Mailbox& mailbox,
    const gfx::Size& size,
    const gfx::ColorSpace color_space,
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
    scoped_refptr<StreamTextureSharedImageInterface> stream_texture_sii,
    scoped_refptr<SharedContextState> context_state,
    scoped_refptr<RefCountedLock> drdc_lock) {
  if (features::IsAImageReaderEnabled()) {
    return std::make_unique<SharedImageVideoImageReader>(
        mailbox, size, color_space, surface_origin, alpha_type,
        std::move(stream_texture_sii), std::move(context_state),
        std::move(drdc_lock));
  } else {
    DCHECK(!drdc_lock);
    return std::make_unique<SharedImageVideoSurfaceTexture>(
        mailbox, size, color_space, surface_origin, alpha_type,
        std::move(stream_texture_sii), std::move(context_state));
  }
}

// Static.
absl::optional<VulkanYCbCrInfo> SharedImageVideo::GetYcbcrInfo(
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

std::unique_ptr<gles2::AbstractTexture> SharedImageVideo::GenAbstractTexture(
    const bool passthrough) {
  std::unique_ptr<gles2::AbstractTexture> texture;
  if (passthrough) {
    texture = std::make_unique<gles2::AbstractTextureImplPassthrough>(
        GL_TEXTURE_EXTERNAL_OES, GL_RGBA, size().width(), size().height(), 1, 0,
        GL_RGBA, GL_UNSIGNED_BYTE);
  } else {
    texture = std::make_unique<gles2::AbstractTextureImpl>(
        GL_TEXTURE_EXTERNAL_OES, GL_RGBA, size().width(), size().height(), 1, 0,
        GL_RGBA, GL_UNSIGNED_BYTE);
  }
  return texture;
}

gfx::Rect SharedImageVideo::ClearedRect() const {
  // SharedImageVideo objects are always created from pre-initialized textures
  // provided by the media decoder. Always treat these as cleared (return the
  // full rectangle).
  return gfx::Rect(size());
}

void SharedImageVideo::SetClearedRect(const gfx::Rect& cleared_rect) {}

void SharedImageVideo::Update(std::unique_ptr<gfx::GpuFence> in_fence) {
  DCHECK(!in_fence);
}

bool SharedImageVideo::ProduceLegacyMailbox(MailboxManager* mailbox_manager) {
  // Android does not use legacy mailbox anymore. Hence marking this as
  // NOTREACHED() now. Once all platform stops using legacy mailbox, this
  // method can be removed.
  NOTREACHED();
  return false;
}

}  // namespace gpu
