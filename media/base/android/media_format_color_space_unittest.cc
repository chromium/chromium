// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/android/media_format_color_space.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/color_space.h"

namespace media {

TEST(MediaFormatColorSpaceTest, ToGfxColorSpace_Valid) {
  MediaFormatColorSpace mf_cs;
  mf_cs.standard = 1;  // COLOR_STANDARD_BT709
  mf_cs.transfer = 3;  // COLOR_TRANSFER_SDR_VIDEO
  mf_cs.range = 2;     // COLOR_RANGE_LIMITED

  gfx::ColorSpace color_space = mf_cs.ToGfxColorSpace();

  EXPECT_TRUE(color_space.IsValid());
  EXPECT_EQ(color_space.GetPrimaryID(), gfx::ColorSpace::PrimaryID::BT709);
  EXPECT_EQ(color_space.GetTransferID(),
            gfx::ColorSpace::TransferID::SMPTE170M);
  EXPECT_EQ(color_space.GetMatrixID(), gfx::ColorSpace::MatrixID::BT709);
  EXPECT_EQ(color_space.GetRangeID(), gfx::ColorSpace::RangeID::LIMITED);
}

TEST(MediaFormatColorSpaceTest, ToGfxColorSpace_InvalidStandard) {
  MediaFormatColorSpace mf_cs;
  mf_cs.standard = 99;
  mf_cs.transfer = 3;
  mf_cs.range = 2;

  gfx::ColorSpace color_space = mf_cs.ToGfxColorSpace();

  EXPECT_FALSE(color_space.IsValid());
}

TEST(MediaFormatColorSpaceTest, ToGfxColorSpace_InvalidTransfer) {
  MediaFormatColorSpace mf_cs;
  mf_cs.standard = 1;
  mf_cs.transfer = 99;
  mf_cs.range = 2;

  gfx::ColorSpace color_space = mf_cs.ToGfxColorSpace();

  EXPECT_FALSE(color_space.IsValid());
}

TEST(MediaFormatColorSpaceTest, ToGfxColorSpace_InvalidRange) {
  MediaFormatColorSpace mf_cs;
  mf_cs.standard = 1;
  mf_cs.transfer = 3;
  mf_cs.range = 99;

  gfx::ColorSpace color_space = mf_cs.ToGfxColorSpace();

  EXPECT_FALSE(color_space.IsValid());
}

}  // namespace media
