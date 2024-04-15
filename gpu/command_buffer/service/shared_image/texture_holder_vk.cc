// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image/texture_holder_vk.h"

#include "base/check.h"
#include "gpu/command_buffer/service/skia_utils.h"
#include "gpu/vulkan/vulkan_image.h"
#include "third_party/skia/include/gpu/ganesh/vk/GrVkBackendSurface.h"

namespace gpu {

TextureHolderVk::TextureHolderVk(std::unique_ptr<VulkanImage> image,
                                 const viz::SharedImageFormat& si_format,
                                 const gfx::ColorSpace& color_space)
    : vulkan_image(std::move(image)) {
  gfx::Size size = vulkan_image->size();
  GrVkImageInfo vk_image_info =
      CreateGrVkImageInfo(vulkan_image.get(), si_format, color_space);
  backend_texture =
      GrBackendTextures::MakeVk(size.width(), size.height(), vk_image_info);
  promise_texture = GrPromiseImageTexture::Make(backend_texture);
}

TextureHolderVk::TextureHolderVk(TextureHolderVk&& other) = default;
TextureHolderVk& TextureHolderVk::operator=(TextureHolderVk&& other) = default;
TextureHolderVk::~TextureHolderVk() = default;

GrVkImageInfo TextureHolderVk::GetGrVkImageInfo() const {
  GrVkImageInfo info;
  bool result = GrBackendTextures::GetVkImageInfo(backend_texture, &info);
  CHECK(result);
  return info;
}

}  // namespace gpu
