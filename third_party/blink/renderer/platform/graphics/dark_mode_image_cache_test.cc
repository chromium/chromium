// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/dark_mode_image_cache.h"

#include "cc/paint/color_filter.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/effects/SkHighContrastFilter.h"

namespace blink {

class DarkModeImageCacheTest : public testing::Test {};

TEST_F(DarkModeImageCacheTest, Caching) {
  DarkModeImageCache cache;

  SkHighContrastConfig config;
  config.fInvertStyle = SkHighContrastConfig::InvertStyle::kInvertLightness;
  sk_sp<cc::ColorFilter> filter = cc::ColorFilter::MakeHighContrast(config);

  SkIRect src1 = SkIRect::MakeXYWH(0, 0, 50, 50);
  SkIRect src2 = SkIRect::MakeXYWH(5, 20, 100, 100);
  SkIRect src3 = SkIRect::MakeXYWH(6, -9, 50, 50);

  EXPECT_FALSE(cache.Exists(src1));
  EXPECT_EQ(cache.Get(src1), nullptr);
  cache.Add(src1, filter);
  EXPECT_TRUE(cache.Exists(src1));
  EXPECT_EQ(cache.Get(src1), filter);

  EXPECT_FALSE(cache.Exists(src2));
  EXPECT_EQ(cache.Get(src2), nullptr);
  cache.Add(src2, nullptr);
  EXPECT_TRUE(cache.Exists(src2));
  EXPECT_EQ(cache.Get(src2), nullptr);

  EXPECT_EQ(cache.Size(), 2u);
  cache.Clear();
  EXPECT_EQ(cache.Size(), 0u);

  EXPECT_FALSE(cache.Exists(src1));
  EXPECT_EQ(cache.Get(src1), nullptr);
  EXPECT_FALSE(cache.Exists(src2));
  EXPECT_EQ(cache.Get(src2), nullptr);
  EXPECT_FALSE(cache.Exists(src3));
  EXPECT_EQ(cache.Get(src3), nullptr);
  cache.Add(src3, filter);
  EXPECT_TRUE(cache.Exists(src3));
  EXPECT_EQ(cache.Get(src3), filter);

  EXPECT_EQ(cache.Size(), 1u);
}

}  // namespace blink
