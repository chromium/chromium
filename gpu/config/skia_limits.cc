// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/config/skia_limits.h"

#include <inttypes.h>

#include "base/metrics/field_trial_params.h"
#include "base/system/sys_info.h"
#include "build/build_config.h"

namespace gpu {

namespace {

BASE_FEATURE(kGrCacheLimitsFeature,
             "GrCacheLimitsFeature",
             base::FEATURE_ENABLED_BY_DEFAULT);

constexpr base::FeatureParam<int> kMaxGaneshResourceCacheBytes(
    &kGrCacheLimitsFeature,
    "MaxGaneshResourceCacheBytes",
    96 * 1024 * 1024);
constexpr base::FeatureParam<int> kMaxDefaultGlyphCacheTextureBytes(
    &kGrCacheLimitsFeature,
    "MaxDefaultGlyphCacheTextureBytes",
    2048 * 1024 * 4);

#if !BUILDFLAG(IS_NACL)
// The limit of the bytes allocated toward GPU resources in the GrContext's
// GPU cache.
constexpr base::FeatureParam<int> kMaxLowEndGaneshResourceCacheBytes(
    &kGrCacheLimitsFeature,
    "MaxLowEndGaneshResourceCacheBytes",
    48 * 1024 * 1024);
constexpr base::FeatureParam<int> kMaxHighEndGaneshResourceCacheBytes(
    &kGrCacheLimitsFeature,
    "MaxHighEndGaneshResourceCacheBytes",
    256 * 1024 * 1024);
// Limits for glyph cache textures.
constexpr base::FeatureParam<int> kMaxLowEndGlyphCacheTextureBytes(
    &kGrCacheLimitsFeature,
    "MaxLowEndGlyphCacheTextureBytes",
    1024 * 512 * 4);
// High-end / low-end memory cutoffs.
constexpr base::FeatureParam<int> kHighEndMemoryThresholdMB(
    &kGrCacheLimitsFeature,
    "HighEndMemoryThresholdMB",
    4096);
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
  *max_resource_cache_bytes = kMaxGaneshResourceCacheBytes.Get();
  *max_glyph_cache_texture_bytes = kMaxDefaultGlyphCacheTextureBytes.Get();

// We can't call AmountOfPhysicalMemory under NACL, so leave the default.
#if !BUILDFLAG(IS_NACL)
  if (base::SysInfo::IsLowEndDevice()) {
    *max_resource_cache_bytes = kMaxLowEndGaneshResourceCacheBytes.Get();
    *max_glyph_cache_texture_bytes = kMaxLowEndGlyphCacheTextureBytes.Get();
  } else if (base::SysInfo::AmountOfPhysicalMemoryMB() >=
             kHighEndMemoryThresholdMB.Get()) {
    *max_resource_cache_bytes = kMaxHighEndGaneshResourceCacheBytes.Get();
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
