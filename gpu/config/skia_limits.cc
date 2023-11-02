// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/config/skia_limits.h"

#include <inttypes.h>

#include "base/system/sys_info.h"
#include "build/build_config.h"

namespace gpu {

void DetermineGrCacheLimitsFromAvailableMemory(
    size_t* max_resource_cache_bytes,
    size_t* max_glyph_cache_texture_bytes) {
  // Default limits.
  constexpr size_t kMaxGaneshResourceCacheBytes = 96 * 1024 * 1024;
  constexpr size_t kMaxDefaultGlyphCacheTextureBytes = 2048 * 1024 * 4;

  *max_resource_cache_bytes = kMaxGaneshResourceCacheBytes;
  *max_glyph_cache_texture_bytes = kMaxDefaultGlyphCacheTextureBytes;

// We can't call AmountOfPhysicalMemory under NACL, so leave the default.
#if !BUILDFLAG(IS_NACL)
  // The limit of the bytes allocated toward GPU resources in the GrContext's
  // GPU cache.
  constexpr size_t kMaxLowEndGaneshResourceCacheBytes = 48 * 1024 * 1024;
  constexpr size_t kMaxHighEndGaneshResourceCacheBytes = 256 * 1024 * 1024;
  // Limits for glyph cache textures.
  constexpr size_t kMaxLowEndGlyphCacheTextureBytes = 1024 * 512 * 4;
  // High-end / low-end memory cutoffs.
  constexpr uint64_t kHighEndMemoryThreshold = 4096ULL * 1024 * 1024;

  if (base::SysInfo::IsLowEndDevice()) {
    *max_resource_cache_bytes = kMaxLowEndGaneshResourceCacheBytes;
    *max_glyph_cache_texture_bytes = kMaxLowEndGlyphCacheTextureBytes;
  } else if (base::SysInfo::AmountOfPhysicalMemory() >=
             kHighEndMemoryThreshold) {
    *max_resource_cache_bytes = kMaxHighEndGaneshResourceCacheBytes;
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
