// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/video_aspect_ratio.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace media {

namespace {

constexpr gfx::Rect kRect_4_3(0, 0, 4, 3);
constexpr gfx::Rect kRect_16_9(0, 0, 16, 9);

}  // namespace

TEST(VideoAspectRatioTest, DefaultConstruction) {
  VideoAspectRatio aspect_ratio;
  EXPECT_FALSE(aspect_ratio.IsValid());
  EXPECT_EQ(aspect_ratio.GetNaturalSize(kRect_16_9), gfx::Size(16, 9));
}

TEST(VideoAspectRatioTest, FromNaturalSize) {
  VideoAspectRatio aspect_ratio;

  aspect_ratio = VideoAspectRatio(kRect_16_9, gfx::Size(16, 9));
  EXPECT_TRUE(aspect_ratio.IsValid());
  EXPECT_EQ(aspect_ratio.GetNaturalSize(kRect_16_9), gfx::Size(16, 9));

  aspect_ratio = VideoAspectRatio(kRect_4_3, gfx::Size(16, 9));
  EXPECT_TRUE(aspect_ratio.IsValid());
  EXPECT_EQ(aspect_ratio.GetNaturalSize(kRect_4_3), gfx::Size(5, 3));

  aspect_ratio = VideoAspectRatio(kRect_16_9, gfx::Size(4, 3));
  EXPECT_TRUE(aspect_ratio.IsValid());
  EXPECT_EQ(aspect_ratio.GetNaturalSize(kRect_16_9), gfx::Size(16, 12));
}

TEST(VideoAspectRatioTest, Pixel) {
  VideoAspectRatio aspect_ratio;

  aspect_ratio = VideoAspectRatio::PAR(1, 1);
  EXPECT_TRUE(aspect_ratio.IsValid());
  EXPECT_EQ(aspect_ratio.GetNaturalSize(kRect_16_9), gfx::Size(16, 9));

  aspect_ratio = VideoAspectRatio::PAR(1, 2);
  EXPECT_TRUE(aspect_ratio.IsValid());
  EXPECT_EQ(aspect_ratio.GetNaturalSize(kRect_16_9), gfx::Size(16, 18));

  aspect_ratio = VideoAspectRatio::PAR(2, 1);
  EXPECT_TRUE(aspect_ratio.IsValid());
  EXPECT_EQ(aspect_ratio.GetNaturalSize(kRect_16_9), gfx::Size(32, 9));
}

TEST(VideoAspectRatioTest, Display) {
  VideoAspectRatio aspect_ratio;

  aspect_ratio = VideoAspectRatio::DAR(1, 1);
  EXPECT_TRUE(aspect_ratio.IsValid());
  EXPECT_EQ(aspect_ratio.GetNaturalSize(kRect_16_9), gfx::Size(16, 16));

  aspect_ratio = VideoAspectRatio::DAR(1, 2);
  EXPECT_TRUE(aspect_ratio.IsValid());
  EXPECT_EQ(aspect_ratio.GetNaturalSize(kRect_16_9), gfx::Size(16, 32));

  aspect_ratio = VideoAspectRatio::DAR(2, 1);
  EXPECT_TRUE(aspect_ratio.IsValid());
  EXPECT_EQ(aspect_ratio.GetNaturalSize(kRect_16_9), gfx::Size(18, 9));
}

}  // namespace media
