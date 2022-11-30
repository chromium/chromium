// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/video_color_space.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/color_transform.h"
#include "ui/gfx/geometry/transform.h"

namespace media {

class VideoColorSpaceTest : public testing::Test {};

TEST(VideoColorSpaceTest, UnknownVideoToSRGB) {
  // Invalid video spaces should be BT709.
  VideoColorSpace invalid_video_color_space = VideoColorSpace(
      VideoColorSpace::PrimaryID::INVALID, VideoColorSpace::TransferID::INVALID,
      VideoColorSpace::MatrixID::INVALID, gfx::ColorSpace::RangeID::LIMITED);
  gfx::ColorSpace unknown = invalid_video_color_space.ToGfxColorSpace();
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
