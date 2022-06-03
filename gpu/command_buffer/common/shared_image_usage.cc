// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/common/shared_image_usage.h"

#include "base/check.h"

namespace gpu {

std::string CreateLabelForSharedImageUsage(uint32_t usage) {
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

  DCHECK(!label.empty());

  return label;
}

}  // namespace gpu
