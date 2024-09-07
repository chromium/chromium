// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_CONFIG_SKIA_LIMITS_H_
#define GPU_CONFIG_SKIA_LIMITS_H_

#include <stddef.h>

#include "gpu/gpu_export.h"

namespace gpu {

GPU_EXPORT void DetermineGraphiteImageProviderCacheLimits(
    size_t* max_gpu_main_image_provider_cache_bytes,
    size_t* max_viz_compositor_image_provider_cache_bytes);

GPU_EXPORT void DetermineGrCacheLimitsFromAvailableMemory(
    size_t* max_resource_cache_bytes,
    size_t* max_glyph_cache_texture_bytes);

GPU_EXPORT void DefaultGrCacheLimitsForTests(
    size_t* max_resource_cache_bytes,
    size_t* max_glyph_cache_texture_bytes);

}  // namespace gpu

#endif  // GPU_CONFIG_SKIA_LIMITS_H_
