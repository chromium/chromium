// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/config/skia_limits.h"

#include <inttypes.h>

#include "base/system/sys_info.h"
#include "build/build_config.h"
#include "components/miracle_parameter/common/public/miracle_parameter.h"

namespace gpu {

namespace {

BASE_FEATURE(kGrCacheLimitsFeature,
             "GrCacheLimitsFeature",
             base::FEATURE_ENABLED_BY_DEFAULT);

MIRACLE_PARAMETER_FOR_INT(GetMaxGaneshResourceCacheBytes,
                          kGrCacheLimitsFeature,
                          "MaxGaneshResourceCacheBytes",
                          96 * 1024 * 1024)

MIRACLE_PARAMETER_FOR_INT(GetMaxDefaultGlyphCacheTextureBytes,
                          kGrCacheLimitsFeature,
                          "MaxDefaultGlyphCacheTextureBytes",
                          2048 * 1024 * 4)

#if !BUILDFLAG(IS_NACL)
// The limit of the bytes allocated toward GPU resources in the GrContext's
// GPU cache.
MIRACLE_PARAMETER_FOR_INT(GetMaxLowEndGaneshResourceCacheBytes,
                          kGrCacheLimitsFeature,
                          "MaxLowEndGaneshResourceCacheBytes",
                          48 * 1024 * 1024)

MIRACLE_PARAMETER_FOR_INT(GetMaxHighEndGaneshResourceCacheBytes,
                          kGrCacheLimitsFeature,
                          "MaxHighEndGaneshResourceCacheBytes",
                          256 * 1024 * 1024)

// Limits for glyph cache textures.
MIRACLE_PARAMETER_FOR_INT(GetMaxLowEndGlyphCacheTextureBytes,
                          kGrCacheLimitsFeature,
                          "MaxLowEndGlyphCacheTextureBytes",
                          1024 * 512 * 4)

// High-end / low-end memory cutoffs.
MIRACLE_PARAMETER_FOR_INT(GetHighEndMemoryThresholdMB,
                          kGrCacheLimitsFeature,
                          "HighEndMemoryThresholdMB",
                          4096)
#endif

// Limits for the Graphite client image provider which is responsible for
// uploading non-GPU backed images (e.g. raster, lazy/generated) to Graphite.
// The limits are smallish since only a small number of images take this path
// instead of being uploaded via the transfer cache.
MIRACLE_PARAMETER_FOR_INT(GetMaxGpuMainGraphiteImageProviderBytes,
                          kGrCacheLimitsFeature,
                          "MaxGpuMainGraphiteImageProviderBytes",
                          16 * 1024 * 1024)

// The limits for the Viz compositor's image provider are even smaller since
// the only time we encounter such images is via reference image filters on
// composited layers which is a pretty uncommon case.
MIRACLE_PARAMETER_FOR_INT(GetMaxVizCompositorGraphiteImageProviderBytes,
                          kGrCacheLimitsFeature,
                          "MaxVizCompositorGraphiteImageProviderBytes",
                          4 * 1024 * 1024)

}  // namespace

void DetermineGraphiteImageProviderCacheLimits(
    size_t* max_gpu_main_image_provider_cache_bytes,
    size_t* max_viz_compositor_image_provider_cache_bytes) {
  *max_gpu_main_image_provider_cache_bytes =
      GetMaxGpuMainGraphiteImageProviderBytes();
  *max_viz_compositor_image_provider_cache_bytes =
      GetMaxVizCompositorGraphiteImageProviderBytes();
}

void DetermineGrCacheLimitsFromAvailableMemory(
    size_t* max_resource_cache_bytes,
    size_t* max_glyph_cache_texture_bytes) {
  // Default limits.
  *max_resource_cache_bytes = GetMaxGaneshResourceCacheBytes();
  *max_glyph_cache_texture_bytes = GetMaxDefaultGlyphCacheTextureBytes();

// We can't call AmountOfPhysicalMemory under NACL, so leave the default.
#if !BUILDFLAG(IS_NACL)
  if (base::SysInfo::IsLowEndDevice()) {
    *max_resource_cache_bytes = GetMaxLowEndGaneshResourceCacheBytes();
    *max_glyph_cache_texture_bytes = GetMaxLowEndGlyphCacheTextureBytes();
  } else if (base::SysInfo::AmountOfPhysicalMemoryMB() >=
             GetHighEndMemoryThresholdMB()) {
    *max_resource_cache_bytes = GetMaxHighEndGaneshResourceCacheBytes();
  }
#endif
}

void DefaultGrCacheLimitsForTests(size_t* max_resource_cache_bytes,
                                  size_t* max_glyph_cache_texture_bytes) {
  constexpr size_t kDefaultGlyphCacheTextureBytes = 2048 * 1024 * 4;
  constexpr size_t kDefaultGaneshResourceCacheBytes = 96 * 1024 * 1024;
  *max_resource_cache_bytes = kDefaultGaneshResourceCacheBytes;
  *max_glyph_cache_texture_bytes = kDefaultGlyphCacheTextureBytes;
}

}  // namespace gpu
