// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/common/shared_image_usage.h"

#include <cstdint>
#include <string>
#include <utility>

#include "base/check.h"

namespace gpu {

const char kExoTextureLabelPrefix[] = "ExoTexture";

bool IsValidClientUsage(SharedImageUsageSet usage) {
  const uint32_t usage_as_int = uint32_t(usage);
  constexpr int32_t kClientMax = (LAST_CLIENT_USAGE << 1) - 1;
  return 0 < usage_as_int && usage_as_int <= kClientMax;
}

bool HasGLES2ReadOrWriteUsage(SharedImageUsageSet usage) {
  return usage.HasAny(SHARED_IMAGE_USAGE_GLES2_READ |
                      SHARED_IMAGE_USAGE_GLES2_WRITE);
}

std::string CreateLabelForSharedImageUsage(SharedImageUsageSet usage) {
  if (usage.empty()) {
    return {};
  }

  const std::pair<SharedImageUsage, const char*> kUsages[] = {
      {SHARED_IMAGE_USAGE_GLES2_READ, "Gles2Read"},
      {SHARED_IMAGE_USAGE_RASTER_READ, "RasterRead"},
      {SHARED_IMAGE_USAGE_DISPLAY_READ, "DisplayRead"},
      {SHARED_IMAGE_USAGE_DISPLAY_WRITE, "DisplayWrite"},
      {SHARED_IMAGE_USAGE_SCANOUT, "Scanout"},
      {SHARED_IMAGE_USAGE_OOP_RASTERIZATION, "OopRasterization"},
      {SHARED_IMAGE_USAGE_WEBGPU_READ, "WebgpuRead"},
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
      {SHARED_IMAGE_USAGE_SCANOUT_DXGI_SWAP_CHAIN, "ScanoutDxgiSwapChain"},
      {SHARED_IMAGE_USAGE_WEBGPU_STORAGE_TEXTURE, "WebgpuStorageTexture"},
      {SHARED_IMAGE_USAGE_GLES2_WRITE, "Gles2Write"},
      {SHARED_IMAGE_USAGE_RASTER_WRITE, "RasterWrite"},
      {SHARED_IMAGE_USAGE_WEBGPU_WRITE, "WebgpuWrite"},
      {SHARED_IMAGE_USAGE_GLES2_FOR_RASTER_ONLY, "GLES2ForRasterOnly"},
      {SHARED_IMAGE_USAGE_RASTER_OVER_GLES2_ONLY, "RasterOverGLES2Only"},
      {SHARED_IMAGE_USAGE_PROTECTED_VIDEO, "ProtectedVideo"},
      {SHARED_IMAGE_USAGE_WEBGPU_SHARED_BUFFER, "WebgpuSharedBuffer"},
  };

  std::string label;
  for (const auto& [value, name] : kUsages) {
    if (!usage.Has(value)) {
      continue;
    }
    if (!label.empty()) {
      label.append("|");
    }
    label.append(name);
  }

  DCHECK(!label.empty());

  return label;
}

}  // namespace gpu
