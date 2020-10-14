// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/raster_dark_mode_filter_impl.h"

#include "base/check_op.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/graphics/dark_mode_filter.h"

// These tests just test end to end calls for RasterDarkModeFilterImpl. For
// detailed tests check dark mode module tests.
namespace blink {
namespace {

TEST(RasterDarkModeFilterImplTest, AnalyzeShouldApplyToImageAPI) {
  DarkModeSettings settings;
  settings.image_policy = DarkModeImagePolicy::kFilterSmart;
  RasterDarkModeFilterImpl filter(settings);
  EXPECT_EQ(filter.AnalyzeShouldApplyToImage(SkIRect::MakeWH(500, 500),
                                             SkIRect::MakeWH(500, 500)),
            cc::RasterDarkModeFilter::Result::kDoNotApplyFilter);
}

TEST(RasterDarkModeFilterImplTest, ApplyToImageAPI) {
  DarkModeSettings settings;
  settings.image_policy = DarkModeImagePolicy::kFilterSmart;
  RasterDarkModeFilterImpl filter(settings);
  SkPixmap pixmap;
  EXPECT_EQ(filter.ApplyToImage(pixmap, SkIRect::MakeWH(50, 50),
                                SkIRect::MakeWH(50, 50)),
            nullptr);
}

TEST(RasterDarkModeFilterImplTest, GetImageFilterAPI) {
  DarkModeSettings settings;
  settings.image_policy = DarkModeImagePolicy::kFilterAll;
  RasterDarkModeFilterImpl filter(settings);
  EXPECT_NE(filter.GetImageFilter(), nullptr);
}

}  // namespace
}  // namespace blink
