// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/common/shared_image_usage.h"

#include <string>
#include <utility>

#include "base/check.h"

namespace gpu {

bool IsValidClientUsage(uint32_t usage) {
  constexpr int32_t kClientMax = (LAST_CLIENT_USAGE << 1) - 1;
  return 0 < usage && usage <= kClientMax;
}

std::string CreateLabelForSharedImageUsage(uint32_t usage) {
  if (!usage)
    return {};

  const std::pair<SharedImageUsage, const char*> kUsages[] = {
      {SHARED_IMAGE_USAGE_GLES2, "Gles2"},
      {SHARED_IMAGE_USAGE_GLES2_FRAMEBUFFER_HINT, "Gles2FramebufferHint"},
      {SHARED_IMAGE_USAGE_RASTER, "Raster"},
      {SHARED_IMAGE_USAGE_DISPLAY_READ, "DisplayRead"},
      {SHARED_IMAGE_USAGE_DISPLAY_WRITE, "DisplayWrite"},
      {SHARED_IMAGE_USAGE_SCANOUT, "Scanout"},
      {SHARED_IMAGE_USAGE_OOP_RASTERIZATION, "OopRasterization"},
      {SHARED_IMAGE_USAGE_WEBGPU, "Webgpu"},
      {SHARED_IMAGE_USAGE_PROTECTED, "Protected"},
      {SHARED_IMAGE_USAGE_CONCURRENT_READ_WRITE, "ConcurrentReadWrite"},
      {SHARED_IMAGE_USAGE_VIDEO_DECODE, "VideoDecode"},
      {SHARED_IMAGE_USAGE_WEBGPU_SWAP_CHAIN_TEXTURE, "WebgpuSwapChainTexture"},
      {SHARED_IMAGE_USAGE_MACOS_VIDEO_TOOLBOX, "MacosVideoToolbox"},
      {SHARED_IMAGE_USAGE_MIPMAP, "Mipmap"},
      {SHARED_IMAGE_USAGE_CPU_WRITE, "CpuWrite"},
      {SHARED_IMAGE_USAGE_RAW_DRAW, "RawDraw"},
      {SHARED_IMAGE_USAGE_RASTER_DELEGATED_COMPOSITING,
       "RasterDelegatedCompositing"},
      {SHARED_IMAGE_USAGE_HIGH_PERFORMANCE_GPU, "HighPerformanceGpu"},
      {SHARED_IMAGE_USAGE_CPU_UPLOAD, "CpuUpload"},
      {SHARED_IMAGE_USAGE_SCANOUT_DCOMP_SURFACE, "ScanoutDCompSurface"},
      {SHARED_IMAGE_USAGE_WEBGPU_STORAGE_TEXTURE, "WebgpuStorageTexture"},
  };

  std::string label;
  for (const auto& [value, name] : kUsages) {
    if ((value & usage) != value)
      continue;
    if (!label.empty())
      label.append("|");
    label.append(name);
  }

  DCHECK(!label.empty());

  return label;
}

}  // namespace gpu
