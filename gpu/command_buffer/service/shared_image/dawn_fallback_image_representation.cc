// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image/dawn_fallback_image_representation.h"

#include <dawn/native/DawnNative.h>

#include "base/bits.h"
#include "base/synchronization/atomic_flag.h"
#include "base/threading/platform_thread.h"
#include "components/viz/common/resources/shared_image_format_utils.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/shared_image/dawn_copy_strategy.h"
#include "gpu/command_buffer/service/shared_image/shared_image_format_service_utils.h"
#include "gpu/config/gpu_finch_features.h"
#include "third_party/skia/include/core/SkColorSpace.h"
#include "ui/gfx/geometry/skia_conversions.h"

namespace gpu {

DawnFallbackImageRepresentation::DawnFallbackImageRepresentation(
    SharedImageManager* manager,
    SharedImageBacking* backing,
    MemoryTypeTracker* tracker,
    wgpu::Device device,
    wgpu::TextureFormat wgpu_format,
    std::vector<wgpu::TextureFormat> view_formats)
    : DawnImageRepresentation(manager, backing, tracker),
      device_(device),
      wgpu_format_(wgpu_format),
      view_formats_(std::move(view_formats)) {}

DawnFallbackImageRepresentation::~DawnFallbackImageRepresentation() = default;

wgpu::Texture DawnFallbackImageRepresentation::BeginAccess(
    wgpu::TextureUsage wgpu_texture_usage,
    wgpu::TextureUsage internal_usage) {
  const std::string debug_label = "DawnFallbackSharedImageRep(" +
                                  CreateLabelForSharedImageUsage(usage()) + ")";

  wgpu::TextureDescriptor texture_descriptor;
  texture_descriptor.label = debug_label.c_str();
  texture_descriptor.format = wgpu_format_;
  texture_descriptor.usage = wgpu_texture_usage;

  texture_descriptor.dimension = wgpu::TextureDimension::e2D;
  texture_descriptor.size = {static_cast<uint32_t>(size().width()),
                             static_cast<uint32_t>(size().height()), 1};
  texture_descriptor.mipLevelCount = 1;
  texture_descriptor.sampleCount = 1;
  texture_descriptor.viewFormatCount = view_formats_.size();
  texture_descriptor.viewFormats = view_formats_.data();

  // Note: The texture must be internally copyable as this class itself uses the
  // texture as the dest and source of copies for readback from and upload to
  // the backing respectively.
  wgpu::DawnTextureInternalUsageDescriptor internalDesc;
  internalDesc.internalUsage = internal_usage | wgpu::TextureUsage::CopySrc |
                               wgpu::TextureUsage::CopyDst;

  texture_descriptor.nextInChain = &internalDesc;

  texture_ = device_.CreateTexture(&texture_descriptor);

  // Copy data from the image's backing to the texture. We only do it if the
  // image is marked as cleared/initialized.
  if (IsCleared() && !DawnCopyStrategy::CopyFromBackingToTexture(
                         backing(), texture_, device_)) {
    texture_ = nullptr;
  }

  return texture_;
}

void DawnFallbackImageRepresentation::EndAccess() {
  if (!texture_) {
    return;
  }

  // Upload the texture's content to the backing. Only do it if the texture is
  // initialized.
  if (dawn::native::IsTextureSubresourceInitialized(
          texture_.Get(), /*baseMipLevel=*/0, /*levelCount=*/1,
          /*baseArrayLayer=*/0,
          /*layerCount=*/1) &&
      DawnCopyStrategy::CopyFromTextureToBacking(texture_, backing(),
                                                 device_)) {
    SetCleared();
  }

  // All further operations on the textures are errors (they would be racy
  // with other backings).
  texture_.Destroy();

  texture_ = nullptr;
}

}  // namespace gpu
