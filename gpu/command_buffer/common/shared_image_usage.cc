// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/common/shared_image_usage.h"

#include "base/check.h"

namespace gpu {

bool IsValidClientUsage(uint32_t usage) {
  constexpr int32_t kClientMax = (LAST_CLIENT_USAGE << 1) - 1;
  return 0 < usage && usage <= kClientMax;
}

std::string CreateLabelForSharedImageUsage(uint32_t usage) {
  if (!usage)
    return "";

  std::string label;

  if (usage & SHARED_IMAGE_USAGE_GLES2) {
    label += "|Gles2";
  }
  if (usage & SHARED_IMAGE_USAGE_GLES2_FRAMEBUFFER_HINT) {
    label += "|Gles2FramebufferHint";
  }
  if (usage & SHARED_IMAGE_USAGE_RASTER) {
    label += "|Raster";
  }
  if (usage & SHARED_IMAGE_USAGE_RAW_DRAW) {
    label += "|RawDraw";
  }
  if (usage & SHARED_IMAGE_USAGE_DISPLAY) {
    label += "|Display";
  }
  if (usage & SHARED_IMAGE_USAGE_SCANOUT) {
    label += "|Scanout";
  }
  if (usage & SHARED_IMAGE_USAGE_OOP_RASTERIZATION) {
    label += "|OopRasterization";
  }
  if (usage & SHARED_IMAGE_USAGE_RGB_EMULATION) {
    label += "|RgbEmulation";
  }
  if (usage & SHARED_IMAGE_USAGE_WEBGPU) {
    label += "|Webgpu";
  }
  if (usage & SHARED_IMAGE_USAGE_PROTECTED) {
    label += "|Protected";
  }
  if (usage & SHARED_IMAGE_USAGE_CONCURRENT_READ_WRITE) {
    label += "|ConcurrentReadWrite";
  }
  if (usage & SHARED_IMAGE_USAGE_VIDEO_DECODE) {
    label += "|VideoDecode";
  }
  if (usage & SHARED_IMAGE_USAGE_WEBGPU_SWAP_CHAIN_TEXTURE) {
    label += "|WebgpuSwapChainTexture";
  }
  if (usage & SHARED_IMAGE_USAGE_MACOS_VIDEO_TOOLBOX) {
    label += "|MacosVideoToolbox";
  }
  if (usage & SHARED_IMAGE_USAGE_MIPMAP) {
    label += "|Mipmap";
  }
  if (usage & SHARED_IMAGE_USAGE_CPU_WRITE) {
    label += "|CpuWrite";
  }
  if (usage & SHARED_IMAGE_USAGE_RAW_DRAW) {
    label += "|RawDraw";
  }
  if (usage & SHARED_IMAGE_USAGE_RASTER_DELEGATED_COMPOSITING) {
    label += "|RasterDelegatedCompositing";
  }
  if (usage & SHARED_IMAGE_USAGE_CPU_UPLOAD) {
    label += "|CpuUpload";
  }

  DCHECK(!label.empty());

  return label;
}

}  // namespace gpu
