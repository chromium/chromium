// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/service_utils.h"

#include "base/memory/memory_pressure_listener.h"
#include "base/memory_coordinator/utils.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "gpu/config/gpu_finch_features.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gpu {
namespace {

constexpr size_t kMaxCacheSize = 1024u;

TEST(ServiceUtilsTest, UpdateShaderCacheSizeOnMemoryLimit_NonAggressive) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(features::kAggressiveShaderCacheLimits);

  // None (100%)
  EXPECT_EQ(UpdateShaderCacheSizeOnMemoryLimit(kMaxCacheSize, 100),
            kMaxCacheSize);

  // Moderate (50%) -> 25% of max size
  EXPECT_EQ(UpdateShaderCacheSizeOnMemoryLimit(kMaxCacheSize, 50),
            kMaxCacheSize / 4);

  // Critical (0%) -> 0
  EXPECT_EQ(UpdateShaderCacheSizeOnMemoryLimit(kMaxCacheSize, 0), 0u);

  // Intermediate (75%) -> 56.25% of max size (quadratically)
  // 1024 * (0.75 * 0.75) = 1024 * 0.5625 = 576
  EXPECT_EQ(UpdateShaderCacheSizeOnMemoryLimit(kMaxCacheSize, 75), 576u);
}

TEST(ServiceUtilsTest, UpdateShaderCacheSizeOnMemoryLimit_Aggressive) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kAggressiveShaderCacheLimits);

#if BUILDFLAG(IS_ANDROID)
  // Android ignores all pressure.
  EXPECT_EQ(UpdateShaderCacheSizeOnMemoryLimit(kMaxCacheSize, 100),
            kMaxCacheSize);
  EXPECT_EQ(UpdateShaderCacheSizeOnMemoryLimit(kMaxCacheSize, 50),
            kMaxCacheSize);
  EXPECT_EQ(UpdateShaderCacheSizeOnMemoryLimit(kMaxCacheSize, 0),
            kMaxCacheSize);
#else
  // Non-Android desktop:
  // Ignore limits above Moderate (50%).
  EXPECT_EQ(UpdateShaderCacheSizeOnMemoryLimit(kMaxCacheSize, 100),
            kMaxCacheSize);
  EXPECT_EQ(UpdateShaderCacheSizeOnMemoryLimit(kMaxCacheSize, 75),
            kMaxCacheSize);
  EXPECT_EQ(UpdateShaderCacheSizeOnMemoryLimit(kMaxCacheSize, 50),
            kMaxCacheSize);

  // Interpolate between Moderate (50%) and Critical (0%).
  // 25% -> t = (50 - 25)/50 = 0.5. lerp(1.0, 0.25, 0.5) = 0.625.
  // 1024 * 0.625 = 640.
  EXPECT_EQ(UpdateShaderCacheSizeOnMemoryLimit(kMaxCacheSize, 25), 640u);

  // Critical (0%) -> 25% of max size
  EXPECT_EQ(UpdateShaderCacheSizeOnMemoryLimit(kMaxCacheSize, 0),
            kMaxCacheSize / 4);
#endif
}

TEST(ServiceUtilsTest, UpdateShaderCacheSizeOnMemoryPressure) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(features::kAggressiveShaderCacheLimits);

  EXPECT_EQ(UpdateShaderCacheSizeOnMemoryPressure(
                kMaxCacheSize, base::MEMORY_PRESSURE_LEVEL_NONE),
            kMaxCacheSize);
  EXPECT_EQ(UpdateShaderCacheSizeOnMemoryPressure(
                kMaxCacheSize, base::MEMORY_PRESSURE_LEVEL_MODERATE),
            kMaxCacheSize / 4);
  EXPECT_EQ(UpdateShaderCacheSizeOnMemoryPressure(
                kMaxCacheSize, base::MEMORY_PRESSURE_LEVEL_CRITICAL),
            0u);
}

}  // namespace
}  // namespace gpu
