// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/common/shared_image_usage.h"

#include <cstdint>

#include "testing/gtest/include/gtest/gtest.h"

namespace gpu {

TEST(SharedImageUsage, ConstructionFromInitializerList) {
  SharedImageUsageSet usage_set = {SHARED_IMAGE_USAGE_GLES2_READ,
                                   SHARED_IMAGE_USAGE_GLES2_WRITE};
  EXPECT_TRUE(usage_set.Has(SHARED_IMAGE_USAGE_GLES2_READ));
  EXPECT_TRUE(usage_set.Has(SHARED_IMAGE_USAGE_GLES2_WRITE));
}

TEST(SharedImageUsage, FunctionsMemberOperator) {
  SharedImageUsageSet as_usage_set = SHARED_IMAGE_USAGE_GLES2_READ;
  as_usage_set |= SHARED_IMAGE_USAGE_SCANOUT;
  EXPECT_EQ(static_cast<uint32_t>(SHARED_IMAGE_USAGE_GLES2_READ) |
                static_cast<uint32_t>(SHARED_IMAGE_USAGE_SCANOUT),
            static_cast<uint32_t>(as_usage_set));
}

TEST(SharedImageUsage, FunctionsHasSingleElement) {
  SharedImageUsageSet as_usage_set =
      SHARED_IMAGE_USAGE_GLES2_READ | SHARED_IMAGE_USAGE_DISPLAY_READ;
  as_usage_set |= SHARED_IMAGE_USAGE_SCANOUT;

  EXPECT_TRUE(as_usage_set.Has(SHARED_IMAGE_USAGE_SCANOUT));
  EXPECT_TRUE(as_usage_set.Has(SHARED_IMAGE_USAGE_GLES2_READ));
  EXPECT_FALSE(as_usage_set.Has(SHARED_IMAGE_USAGE_WEBGPU_READ));
}

TEST(SharedImageUsage, FunctionsHasAll) {
  SharedImageUsageSet as_usage_set =
      SHARED_IMAGE_USAGE_GLES2_READ | SHARED_IMAGE_USAGE_DISPLAY_READ;
  as_usage_set |= SHARED_IMAGE_USAGE_SCANOUT;

  EXPECT_TRUE(as_usage_set.HasAll(SHARED_IMAGE_USAGE_DISPLAY_READ |
                                  SHARED_IMAGE_USAGE_SCANOUT));
  EXPECT_FALSE(as_usage_set.HasAll(SHARED_IMAGE_USAGE_DISPLAY_READ |
                                   SHARED_IMAGE_USAGE_WEBGPU_READ));
}

TEST(SharedImageUsage, FunctionsHasAny) {
  SharedImageUsageSet as_usage_set =
      SHARED_IMAGE_USAGE_GLES2_READ | SHARED_IMAGE_USAGE_DISPLAY_READ;
  as_usage_set |= SHARED_IMAGE_USAGE_SCANOUT;

  EXPECT_TRUE(as_usage_set.HasAny(SHARED_IMAGE_USAGE_DISPLAY_READ |
                                  SHARED_IMAGE_USAGE_WEBGPU_READ));
  EXPECT_FALSE(as_usage_set.Has(SHARED_IMAGE_USAGE_MIPMAP));
  EXPECT_FALSE(as_usage_set.HasAny(SHARED_IMAGE_USAGE_MIPMAP |
                                   SHARED_IMAGE_USAGE_CPU_WRITE));
}

TEST(SharedImageUsage, FunctionsIntersect) {
  SharedImageUsageSet as_usage_set = SHARED_IMAGE_USAGE_WEBGPU_WRITE |
                                     SHARED_IMAGE_USAGE_RASTER_WRITE |
                                     SHARED_IMAGE_USAGE_HIGH_PERFORMANCE_GPU;
  as_usage_set =
      Intersection(as_usage_set, SHARED_IMAGE_USAGE_WEBGPU_WRITE |
                                     SHARED_IMAGE_USAGE_RAW_DRAW |
                                     SHARED_IMAGE_USAGE_HIGH_PERFORMANCE_GPU);
  EXPECT_EQ(static_cast<uint32_t>(SHARED_IMAGE_USAGE_WEBGPU_WRITE |
                                  SHARED_IMAGE_USAGE_HIGH_PERFORMANCE_GPU),
            static_cast<uint32_t>(as_usage_set));
}

TEST(SharedImageUsage, GlobalOperatorCasting) {
  // Global operators will create a 'SharedImageUsageSet'.
  auto as_usage_set = SHARED_IMAGE_USAGE_GLES2_READ |
                      SHARED_IMAGE_USAGE_CPU_WRITE |
                      SHARED_IMAGE_USAGE_WEBGPU_READ;
  as_usage_set |= as_usage_set | SHARED_IMAGE_USAGE_RAW_DRAW;
  as_usage_set |= SHARED_IMAGE_USAGE_RASTER_WRITE | as_usage_set;
  EXPECT_TRUE(as_usage_set.HasAll(
      SHARED_IMAGE_USAGE_GLES2_READ | SHARED_IMAGE_USAGE_CPU_WRITE |
      SHARED_IMAGE_USAGE_WEBGPU_READ | SHARED_IMAGE_USAGE_RAW_DRAW |
      SHARED_IMAGE_USAGE_RASTER_WRITE));
}

TEST(SharedImageUsage, RemoveAll) {
  SharedImageUsageSet as_usage_set =
      SHARED_IMAGE_USAGE_WEBGPU_WRITE | SHARED_IMAGE_USAGE_RASTER_WRITE |
      SHARED_IMAGE_USAGE_HIGH_PERFORMANCE_GPU | SHARED_IMAGE_USAGE_RAW_DRAW;
  as_usage_set.RemoveAll(SHARED_IMAGE_USAGE_RASTER_WRITE |
                         SHARED_IMAGE_USAGE_RAW_DRAW);
  EXPECT_TRUE(as_usage_set.HasAll(SHARED_IMAGE_USAGE_WEBGPU_WRITE |
                                  SHARED_IMAGE_USAGE_HIGH_PERFORMANCE_GPU));
  EXPECT_FALSE(as_usage_set.HasAny(SHARED_IMAGE_USAGE_RASTER_WRITE |
                                   SHARED_IMAGE_USAGE_RAW_DRAW));
}

TEST(SharedImageUsage, RemoveAllButNotPresent) {
  SharedImageUsageSet as_usage_set =
      SHARED_IMAGE_USAGE_WEBGPU_WRITE | SHARED_IMAGE_USAGE_RASTER_WRITE |
      SHARED_IMAGE_USAGE_HIGH_PERFORMANCE_GPU | SHARED_IMAGE_USAGE_RAW_DRAW;
  // We intentionally remove 'SHARED_IMAGE_USAGE_CPU_WRITE' even though it was
  // never added in above. We are testing that this is allowed and that it will
  // not produce an internal bit flip.
  as_usage_set.RemoveAll(SHARED_IMAGE_USAGE_RASTER_WRITE |
                         SHARED_IMAGE_USAGE_CPU_WRITE);
  EXPECT_TRUE(as_usage_set.HasAll(SHARED_IMAGE_USAGE_WEBGPU_WRITE |
                                  SHARED_IMAGE_USAGE_HIGH_PERFORMANCE_GPU |
                                  SHARED_IMAGE_USAGE_RAW_DRAW));
  EXPECT_FALSE(as_usage_set.HasAny(SHARED_IMAGE_USAGE_RASTER_WRITE |
                                   SHARED_IMAGE_USAGE_CPU_WRITE));
}

TEST(SharedImageUsage, GlobalOperatorSetUnion) {
  // Global operators will create a 'SharedImageUsageSet'.
  SharedImageUsageSet usage_a =
      SHARED_IMAGE_USAGE_CPU_WRITE | SHARED_IMAGE_USAGE_WEBGPU_READ;
  SharedImageUsageSet usage_b =
      SHARED_IMAGE_USAGE_HIGH_PERFORMANCE_GPU | SHARED_IMAGE_USAGE_RASTER_WRITE;
  SharedImageUsageSet usage_set_union = usage_a | usage_b;
  EXPECT_TRUE(usage_set_union.HasAll(SHARED_IMAGE_USAGE_HIGH_PERFORMANCE_GPU |
                                     SHARED_IMAGE_USAGE_RASTER_WRITE |
                                     SHARED_IMAGE_USAGE_CPU_WRITE |
                                     SHARED_IMAGE_USAGE_WEBGPU_READ));
}

TEST(SharedImageUsage, ImplicitCasting) {
  SharedImageUsageSet as_usage_set =
      SHARED_IMAGE_USAGE_CPU_WRITE | SHARED_IMAGE_USAGE_WEBGPU_READ;
  // TODO(crbug.com/343347288): Remove after all usage has been converted to
  // `SharedImageUsageSet`.
  // Temporary exception to allow for existing, non type safe, conversions.
  uint32_t as_uint32_t = as_usage_set;

  EXPECT_EQ(static_cast<uint32_t>(SHARED_IMAGE_USAGE_CPU_WRITE) |
                static_cast<uint32_t>(SHARED_IMAGE_USAGE_WEBGPU_READ),
            as_uint32_t);
}

TEST(SharedImageUsage, EqualityTest) {
  SharedImageUsageSet usage_a =
      SHARED_IMAGE_USAGE_CPU_WRITE | SHARED_IMAGE_USAGE_WEBGPU_READ;
  SharedImageUsageSet usage_b =
      SHARED_IMAGE_USAGE_WEBGPU_READ | SHARED_IMAGE_USAGE_CPU_WRITE;
  SharedImageUsageSet usage_c = SHARED_IMAGE_USAGE_CPU_WRITE;
  SharedImageUsageSet usage_d = SHARED_IMAGE_USAGE_CPU_WRITE |
                                SHARED_IMAGE_USAGE_WEBGPU_READ |
                                SHARED_IMAGE_USAGE_HIGH_PERFORMANCE_GPU;
  EXPECT_TRUE(usage_a == usage_b);
  EXPECT_FALSE(usage_a == usage_c);
  EXPECT_FALSE(usage_a == usage_d);
}

TEST(SharedImageUsage, ExplicitCasting) {
  // Explicit creation from raw bits of a uint32_t.
  SharedImageUsageSet explicit_constructor_set =
      SharedImageUsageSet(uint32_t(0b101));
  EXPECT_TRUE(explicit_constructor_set.HasAll(SHARED_IMAGE_USAGE_GLES2_READ |
                                              SHARED_IMAGE_USAGE_DISPLAY_READ));
  // This cast will intentionally not compile. Leaving this here as an example.
  // SharedImageUsageSet fail_to_compile = uint32_t(0x1);
}

TEST(SharedImageUsage, Combinations) {
  for (uint32_t i = 0; (1 << i) <= LAST_SHARED_IMAGE_USAGE; i++) {
    SharedImageUsageSet set_combine_two = static_cast<SharedImageUsage>(1 << i);
    for (uint32_t j = 0; (1 << j) <= LAST_SHARED_IMAGE_USAGE; j++) {
      set_combine_two.PutAll(static_cast<SharedImageUsage>(1 << j));
      EXPECT_TRUE(
          set_combine_two.HasAll(static_cast<SharedImageUsage>(1 << j) |
                                 static_cast<SharedImageUsage>(1 << i)));
    }
  }
}

}  // namespace gpu
