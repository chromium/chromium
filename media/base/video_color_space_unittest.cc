// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/video_color_space.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/color_transform.h"
#include "ui/gfx/geometry/transform.h"

namespace media {

TEST(VideoColorSpaceTest, InvalidColorSpace) {
  auto video_cs = VideoColorSpace();
  EXPECT_FALSE(video_cs.IsSpecified());
  auto gfx_cs = video_cs.ToGfxColorSpace();
  EXPECT_FALSE(gfx_cs.IsValid());
  auto guessed_cs = video_cs.GuessGfxColorSpace();
  EXPECT_TRUE(guessed_cs.IsValid());
  EXPECT_EQ(guessed_cs, gfx::ColorSpace::CreateREC709());
}

TEST(VideoColorSpaceTest, PartiallyValid) {
  auto video_cs = VideoColorSpace();
  video_cs.primaries = VideoColorSpace::PrimaryID::BT709;
  EXPECT_FALSE(video_cs.IsSpecified());
  auto gfx_cs = video_cs.ToGfxColorSpace();
  EXPECT_FALSE(gfx_cs.IsValid());
  EXPECT_EQ(gfx_cs, gfx::ColorSpace());
  auto guessed_cs = video_cs.GuessGfxColorSpace();
  EXPECT_TRUE(guessed_cs.IsValid());
  EXPECT_EQ(guessed_cs, gfx::ColorSpace(gfx::ColorSpace::PrimaryID::BT709,
                                        gfx::ColorSpace::TransferID::BT709,
                                        gfx::ColorSpace::MatrixID::BT709,
                                        gfx::ColorSpace::RangeID::DERIVED));
}

TEST(VideoColorSpaceTest, UnknownVideoToSRGB) {
  // Invalid video spaces should be BT709.
  VideoColorSpace invalid_video_color_space = VideoColorSpace(
      VideoColorSpace::PrimaryID::INVALID, VideoColorSpace::TransferID::INVALID,
      VideoColorSpace::MatrixID::INVALID, gfx::ColorSpace::RangeID::LIMITED);
  gfx::ColorSpace unknown = invalid_video_color_space.GuessGfxColorSpace();
  gfx::ColorSpace sRGB = gfx::ColorSpace::CreateSRGB();
  std::unique_ptr<gfx::ColorTransform> t(
      gfx::ColorTransform::NewColorTransform(unknown, sRGB));

  gfx::ColorTransform::TriStim tmp(16.0f / 255.0f, 0.5f, 0.5f);
  t->Transform(&tmp, 1);
  EXPECT_NEAR(tmp.x(), 0.0f, 0.001f);
  EXPECT_NEAR(tmp.y(), 0.0f, 0.001f);
  EXPECT_NEAR(tmp.z(), 0.0f, 0.001f);

  tmp = gfx::ColorTransform::TriStim(235.0f / 255.0f, 0.5f, 0.5f);
  t->Transform(&tmp, 1);
  EXPECT_NEAR(tmp.x(), 1.0f, 0.001f);
  EXPECT_NEAR(tmp.y(), 1.0f, 0.001f);
  EXPECT_NEAR(tmp.z(), 1.0f, 0.001f);

  // Test a blue color
  tmp = gfx::ColorTransform::TriStim(128.0f / 255.0f, 240.0f / 255.0f, 0.5f);
  t->Transform(&tmp, 1);
  EXPECT_GT(tmp.z(), tmp.x());
  EXPECT_GT(tmp.z(), tmp.y());
}

}  // namespace media
