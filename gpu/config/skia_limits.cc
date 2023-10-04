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

}  // namespace

size_t DetermineGraphiteImageProviderCacheLimitFromAvailableMemory() {
  // Use the same value as that for the Ganesh resource cache.
  size_t max_resource_cache_bytes;
  size_t dont_care;
  DetermineGrCacheLimitsFromAvailableMemory(&max_resource_cache_bytes,
                                            &dont_care);

  return max_resource_cache_bytes;
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
