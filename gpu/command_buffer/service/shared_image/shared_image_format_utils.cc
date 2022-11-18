// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image/shared_image_format_utils.h"

#include "base/notreached.h"
#include "components/viz/common/resources/resource_format.h"
#include "components/viz/common/resources/resource_format_utils.h"

namespace gpu {

// TODO (hitawala): Add support for multiplanar formats.

int BitsPerPixel(viz::SharedImageFormat format) {
  return viz::BitsPerPixel(format.resource_format());
}

gfx::BufferFormat ToBufferFormat(viz::SharedImageFormat format) {
  return viz::BufferFormat(format.resource_format());
}

bool GLSupportsFormat(viz::SharedImageFormat format) {
  return viz::GLSupportsFormat(format.resource_format());
}
unsigned int GLDataType(viz::SharedImageFormat format) {
  return viz::GLDataType(format.resource_format());
}
unsigned int GLDataFormat(viz::SharedImageFormat format) {
  return viz::GLDataFormat(format.resource_format());
}
unsigned int GLInternalFormat(viz::SharedImageFormat format) {
  return viz::GLInternalFormat(format.resource_format());
}
unsigned int TextureStorageFormat(viz::SharedImageFormat format,
                                  bool use_angle_rgbx_format) {
  return viz::TextureStorageFormat(format.resource_format(),
                                   use_angle_rgbx_format);
}

#if BUILDFLAG(ENABLE_VULKAN)
bool HasVkFormat(viz::SharedImageFormat format) {
  return viz::HasVkFormat(format.resource_format());
}
VkFormat ToVkFormat(viz::SharedImageFormat format) {
  return viz::ToVkFormat(format.resource_format());
}
#endif

wgpu::TextureFormat ToDawnFormat(viz::SharedImageFormat format) {
  return viz::ToDawnFormat(format.resource_format());
}
WGPUTextureFormat ToWGPUFormat(viz::SharedImageFormat format) {
  return viz::ToWGPUFormat(format.resource_format());
}

}  // namespace gpu
