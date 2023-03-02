// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/interest_group/ad_display_size.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace blink {

namespace {

const GURL kUrl1("https://origin1.test/url1");
const GURL kUrl2("https://origin1.test/url2");

}  // namespace

TEST(AdSizeTest, OperatorCompare) {
  // AdSizes with different units.
  AdSize ad_size_in_pixels(100, AdSize::LengthUnit::kPixels, 100,
                           AdSize::LengthUnit::kPixels);
  AdSize ad_size_in_screenwidth(100, AdSize::LengthUnit::kScreenWidth, 100,
                                AdSize::LengthUnit::kScreenWidth);
  AdSize ad_size_in_mix_units(100, AdSize::LengthUnit::kPixels, 100,
                              AdSize::LengthUnit::kScreenWidth);

  EXPECT_FALSE(ad_size_in_pixels == ad_size_in_screenwidth);
  EXPECT_TRUE(ad_size_in_pixels != ad_size_in_screenwidth);
  EXPECT_FALSE(ad_size_in_pixels == ad_size_in_mix_units);
  EXPECT_TRUE(ad_size_in_pixels != ad_size_in_mix_units);
  EXPECT_FALSE(ad_size_in_screenwidth == ad_size_in_mix_units);
  EXPECT_TRUE(ad_size_in_screenwidth != ad_size_in_mix_units);

  // AdSizes with different numeric values.
  AdSize ad_size_in_pixels_small(5, AdSize::LengthUnit::kPixels, 5,
                                 AdSize::LengthUnit::kPixels);

  EXPECT_FALSE(ad_size_in_pixels == ad_size_in_pixels_small);
  EXPECT_TRUE(ad_size_in_pixels != ad_size_in_pixels_small);

  // AdSizes with the same numeric values and units.
  AdSize ad_size_in_pixels_clone = ad_size_in_pixels;

  EXPECT_TRUE(ad_size_in_pixels == ad_size_in_pixels_clone);
  EXPECT_FALSE(ad_size_in_pixels != ad_size_in_pixels_clone);
}

}  // namespace blink
