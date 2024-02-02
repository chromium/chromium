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

  // Copied constructed.
  AdSize ad_size_in_pixels_clone = ad_size_in_pixels;

  EXPECT_TRUE(ad_size_in_pixels == ad_size_in_pixels_clone);
  EXPECT_FALSE(ad_size_in_pixels != ad_size_in_pixels_clone);

  // Copy assignment.
  ad_size_in_pixels_clone = ad_size_in_pixels;

  EXPECT_TRUE(ad_size_in_pixels == ad_size_in_pixels_clone);
  EXPECT_FALSE(ad_size_in_pixels != ad_size_in_pixels_clone);
}

TEST(AdDescriptorTest, Constructor) {
  AdDescriptor default_constructed;
  EXPECT_EQ(default_constructed.url, GURL());
  EXPECT_EQ(default_constructed.size, std::nullopt);

  // The constructor should construct AdSize as std::nullopt if only url is
  // provided.
  AdDescriptor constructed_with_url(kUrl1);

  EXPECT_EQ(constructed_with_url.url, kUrl1);
  EXPECT_EQ(constructed_with_url.size, std::nullopt);

  AdDescriptor constructed_with_url_ond_nullopt(kUrl1, std::nullopt);

  EXPECT_EQ(constructed_with_url_ond_nullopt.url, kUrl1);
  EXPECT_EQ(constructed_with_url_ond_nullopt.size, std::nullopt);

  AdSize ad_size(100, AdSize::LengthUnit::kPixels, 50,
                 AdSize::LengthUnit::kScreenWidth);
  AdDescriptor constructed_with_url_ond_size(kUrl1, ad_size);

  EXPECT_EQ(constructed_with_url_ond_size.url, kUrl1);
  EXPECT_EQ(constructed_with_url_ond_size.size, ad_size);
}

TEST(AdDescriptorTest, OperatorCompare) {
  // AdDescriptors with different urls.
  AdDescriptor ad_descriptor_without_size(kUrl1);
  AdDescriptor different_ad_descriptor_without_size(kUrl2);

  EXPECT_FALSE(ad_descriptor_without_size ==
               different_ad_descriptor_without_size);
  EXPECT_TRUE(ad_descriptor_without_size !=
              different_ad_descriptor_without_size);

  AdDescriptor ad_descriptor_in_pixels(
      kUrl1, AdSize(100, AdSize::LengthUnit::kPixels, 100,
                    AdSize::LengthUnit::kPixels));

  EXPECT_FALSE(ad_descriptor_without_size == ad_descriptor_in_pixels);
  EXPECT_TRUE(ad_descriptor_without_size != ad_descriptor_in_pixels);

  AdDescriptor ad_descriptor_screenwidth(
      kUrl1, AdSize(100, AdSize::LengthUnit::kScreenWidth, 100,
                    AdSize::LengthUnit::kScreenWidth));

  EXPECT_FALSE(ad_descriptor_in_pixels == ad_descriptor_screenwidth);
  EXPECT_TRUE(ad_descriptor_in_pixels != ad_descriptor_screenwidth);

  // Copy constructed.
  AdDescriptor ad_descriptor_in_pixels_clone = ad_descriptor_in_pixels;

  EXPECT_TRUE(ad_descriptor_in_pixels == ad_descriptor_in_pixels_clone);
  EXPECT_FALSE(ad_descriptor_in_pixels != ad_descriptor_in_pixels_clone);

  // Copy assignment.
  ad_descriptor_in_pixels_clone = ad_descriptor_in_pixels;

  EXPECT_TRUE(ad_descriptor_in_pixels == ad_descriptor_in_pixels_clone);
  EXPECT_FALSE(ad_descriptor_in_pixels != ad_descriptor_in_pixels_clone);
}

}  // namespace blink
