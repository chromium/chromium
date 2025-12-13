// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/config/skia_limits.h"

#include <inttypes.h>

#include "base/system/sys_info.h"
#include "build/build_config.h"

namespace gpu {

namespace {

BASE_FEATURE(kGrCacheLimitsFeature, base::FEATURE_ENABLED_BY_DEFAULT);

// Limits for the Graphite client image provider which is responsible for
// uploading non-GPU backed images (e.g. raster, lazy/generated) to Graphite.
// The limits are smallish since only a small number of images take this path
// instead of being uploaded via the transfer cache.
BASE_FEATURE_PARAM(size_t, kMaxGpuMainGraphiteImageProviderBytes,
                    &kGrCacheLimitsFeature,
                    "MaxGpuMainGraphiteImageProviderBytes",
                    16 * 1024 * 1024);

// The limits for the Viz compositor's image provider are even smaller since
// the only time we encounter such images is via reference image filters on
// composited layers which is a pretty uncommon case.
BASE_FEATURE_PARAM(size_t, kMaxVizCompositorGraphiteImageProviderBytes,
                    &kGrCacheLimitsFeature,
                    "MaxVizCompositorGraphiteImageProviderBytes",
                    4 * 1024 * 1024);

}  // namespace

void DetermineGraphiteImageProviderCacheLimits(
    size_t* max_gpu_main_image_provider_cache_bytes,
    size_t* max_viz_compositor_image_provider_cache_bytes) {
  *max_gpu_main_image_provider_cache_bytes =
      kMaxGpuMainGraphiteImageProviderBytes.Get();
  *max_viz_compositor_image_provider_cache_bytes =
      kMaxVizCompositorGraphiteImageProviderBytes.Get();
}

void DetermineGrCacheLimitsFromAvailableMemory(
    size_t* max_resource_cache_bytes,
    size_t* max_glyph_cache_texture_bytes) {
  constexpr size_t kMaxGaneshResourceCacheBytes = 96 * 1024 * 1024;
  constexpr size_t kMaxDefaultGlyphCacheTextureBytes = 2048 * 1024 * 4;
  // Default limits.
  *max_resource_cache_bytes = kMaxGaneshResourceCacheBytes;
  *max_glyph_cache_texture_bytes = kMaxDefaultGlyphCacheTextureBytes;

  // The limit of the bytes allocated toward GPU resources in the GrContext's
  // GPU cache.
  constexpr size_t kMaxLowEndGaneshResourceCacheBytes = 48 * 1024 * 1024;
  constexpr size_t kMaxHighEndGaneshResourceCacheBytes = 256 * 1024 * 1024;
  // Limits for glyph cache textures.
  constexpr size_t kMaxLowEndGlyphCacheTextureBytes = 1024 * 512 * 4;
  // High-end / low-end memory cutoffs.
  constexpr int64_t kHighEndMemoryThresholdInMB = 4096;
  if (base::SysInfo::IsLowEndDevice()) {
    *max_resource_cache_bytes = kMaxLowEndGaneshResourceCacheBytes;
    *max_glyph_cache_texture_bytes = kMaxLowEndGlyphCacheTextureBytes;
  } else if (base::SysInfo::AmountOfPhysicalMemory().InMiB() >=
             kHighEndMemoryThresholdInMB) {
    *max_resource_cache_bytes = kMaxHighEndGaneshResourceCacheBytes;
  }
}

void DefaultGrCacheLimitsForTests(size_t* max_resource_cache_bytes,
                                  size_t* max_glyph_cache_texture_bytes) {
  constexpr size_t kDefaultGlyphCacheTextureBytes = 2048 * 1024 * 4;
  constexpr size_t kDefaultGaneshResourceCacheBytes = 96 * 1024 * 1024;
  *max_resource_cache_bytes = kDefaultGaneshResourceCacheBytes;
  *max_glyph_cache_texture_bytes = kDefaultGlyphCacheTextureBytes;
}

}  // namespace gpu
