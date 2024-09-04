// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_TEXTURE_HOLDER_VK_H_
#define GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_TEXTURE_HOLDER_VK_H_

#include <memory>

#include "third_party/skia/include/gpu/ganesh/GrBackendSurface.h"
#include "third_party/skia/include/gpu/ganesh/vk/GrVkTypes.h"
#include "third_party/skia/include/private/chromium/GrPromiseImageTexture.h"

namespace gfx {
class ColorSpace;
}  // namespace gfx

namespace viz {
class SharedImageFormat;
}  // namespace viz

namespace gpu {

class VulkanImage;

// Holds VulkanImage + skia representations of it.
struct TextureHolderVk {
  explicit TextureHolderVk(std::unique_ptr<VulkanImage> image,
                           const viz::SharedImageFormat& si_format,
                           const gfx::ColorSpace& color_space);
  TextureHolderVk(TextureHolderVk&& other);
  TextureHolderVk& operator=(TextureHolderVk&& other);
  ~TextureHolderVk();

  GrVkImageInfo GetGrVkImageInfo() const;

  std::unique_ptr<VulkanImage> vulkan_image;
  GrBackendTexture backend_texture;
  sk_sp<GrPromiseImageTexture> promise_texture;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_TEXTURE_HOLDER_VK_H_
