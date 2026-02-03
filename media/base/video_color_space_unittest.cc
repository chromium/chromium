// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/video_color_space.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/color_space.h"
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

TEST(VideoColorSpaceTest, UnknownVideo) {
  // Invalid video spaces should be BT709.
  VideoColorSpace invalid_video_color_space = VideoColorSpace(
      VideoColorSpace::PrimaryID::INVALID, VideoColorSpace::TransferID::INVALID,
      VideoColorSpace::MatrixID::INVALID, gfx::ColorSpace::RangeID::LIMITED);
  gfx::ColorSpace unknown = invalid_video_color_space.GuessGfxColorSpace();
  EXPECT_EQ(unknown, gfx::ColorSpace(gfx::ColorSpace::PrimaryID::BT709,
                                     gfx::ColorSpace::TransferID::BT709,
                                     gfx::ColorSpace::MatrixID::BT709,
                                     gfx::ColorSpace::RangeID::LIMITED));
}

TEST(VideoColorSpaceTest, IsHDR) {
  VideoColorSpace video_cs = VideoColorSpace::REC709();
  EXPECT_FALSE(video_cs.IsHDR());

  VideoColorSpace video_cs_smpte2084 = VideoColorSpace(
      VideoColorSpace::PrimaryID::BT2020,
      VideoColorSpace::TransferID::SMPTEST2084,
      VideoColorSpace::MatrixID::BT2020_NCL, gfx::ColorSpace::RangeID::LIMITED);
  EXPECT_TRUE(video_cs_smpte2084.IsHDR());

  VideoColorSpace video_cs_arib_std_b67 = VideoColorSpace(
      VideoColorSpace::PrimaryID::BT2020,
      VideoColorSpace::TransferID::ARIB_STD_B67,
      VideoColorSpace::MatrixID::BT2020_NCL, gfx::ColorSpace::RangeID::LIMITED);
  EXPECT_TRUE(video_cs_arib_std_b67.IsHDR());

  // BT.2020 with SDR transfer functions is not HDR.
  VideoColorSpace video_cs_bt2020_sdr = VideoColorSpace(
      VideoColorSpace::PrimaryID::BT2020,
      VideoColorSpace::TransferID::BT2020_10,
      VideoColorSpace::MatrixID::BT2020_NCL, gfx::ColorSpace::RangeID::LIMITED);
  EXPECT_FALSE(video_cs_bt2020_sdr.IsHDR());
}

TEST(VideoColorSpaceTest, GuessBT2020FromPrimaries) {
  VideoColorSpace video_cs =
      VideoColorSpace(VideoColorSpace::PrimaryID::BT2020,
                      VideoColorSpace::TransferID::UNSPECIFIED,
                      VideoColorSpace::MatrixID::UNSPECIFIED,
                      gfx::ColorSpace::RangeID::LIMITED);

  auto guessed_cs = video_cs.GuessGfxColorSpace();
  EXPECT_TRUE(guessed_cs.IsValid());
  EXPECT_EQ(guessed_cs, gfx::ColorSpace(gfx::ColorSpace::PrimaryID::BT2020,
                                        gfx::ColorSpace::TransferID::BT2020_10,
                                        gfx::ColorSpace::MatrixID::BT2020_NCL,
                                        gfx::ColorSpace::RangeID::LIMITED));
}

TEST(VideoColorSpaceTest, GuessBT2020FromPQTransfer) {
  // PQ transfer function strongly indicates BT2020 (used in HDR10).
  VideoColorSpace video_cs =
      VideoColorSpace(VideoColorSpace::PrimaryID::UNSPECIFIED,
                      VideoColorSpace::TransferID::SMPTEST2084,
                      VideoColorSpace::MatrixID::UNSPECIFIED,
                      gfx::ColorSpace::RangeID::LIMITED);

  auto guessed_cs = video_cs.GuessGfxColorSpace();
  EXPECT_TRUE(guessed_cs.IsValid());
  EXPECT_EQ(guessed_cs, gfx::ColorSpace(gfx::ColorSpace::PrimaryID::BT2020,
                                        gfx::ColorSpace::TransferID::PQ,
                                        gfx::ColorSpace::MatrixID::BT2020_NCL,
                                        gfx::ColorSpace::RangeID::LIMITED));
}

TEST(VideoColorSpaceTest, GuessBT2020FromHLGTransfer) {
  // HLG transfer function strongly indicates BT2020 (used in HLG HDR).
  VideoColorSpace video_cs =
      VideoColorSpace(VideoColorSpace::PrimaryID::UNSPECIFIED,
                      VideoColorSpace::TransferID::ARIB_STD_B67,
                      VideoColorSpace::MatrixID::UNSPECIFIED,
                      gfx::ColorSpace::RangeID::LIMITED);

  auto guessed_cs = video_cs.GuessGfxColorSpace();
  EXPECT_TRUE(guessed_cs.IsValid());
  EXPECT_EQ(guessed_cs, gfx::ColorSpace(gfx::ColorSpace::PrimaryID::BT2020,
                                        gfx::ColorSpace::TransferID::HLG,
                                        gfx::ColorSpace::MatrixID::BT2020_NCL,
                                        gfx::ColorSpace::RangeID::LIMITED));
}

TEST(VideoColorSpaceTest, GuessBT2020FromBT2020_10Transfer) {
  VideoColorSpace video_cs =
      VideoColorSpace(VideoColorSpace::PrimaryID::UNSPECIFIED,
                      VideoColorSpace::TransferID::BT2020_10,
                      VideoColorSpace::MatrixID::UNSPECIFIED,
                      gfx::ColorSpace::RangeID::LIMITED);

  auto guessed_cs = video_cs.GuessGfxColorSpace();
  EXPECT_TRUE(guessed_cs.IsValid());
  EXPECT_EQ(guessed_cs, gfx::ColorSpace(gfx::ColorSpace::PrimaryID::BT2020,
                                        gfx::ColorSpace::TransferID::BT2020_10,
                                        gfx::ColorSpace::MatrixID::BT2020_NCL,
                                        gfx::ColorSpace::RangeID::LIMITED));
}

TEST(VideoColorSpaceTest, GuessBT2020FromBT2020_12Transfer) {
  VideoColorSpace video_cs =
      VideoColorSpace(VideoColorSpace::PrimaryID::UNSPECIFIED,
                      VideoColorSpace::TransferID::BT2020_12,
                      VideoColorSpace::MatrixID::UNSPECIFIED,
                      gfx::ColorSpace::RangeID::LIMITED);

  auto guessed_cs = video_cs.GuessGfxColorSpace();
  EXPECT_TRUE(guessed_cs.IsValid());
  EXPECT_EQ(guessed_cs, gfx::ColorSpace(gfx::ColorSpace::PrimaryID::BT2020,
                                        gfx::ColorSpace::TransferID::BT2020_12,
                                        gfx::ColorSpace::MatrixID::BT2020_NCL,
                                        gfx::ColorSpace::RangeID::LIMITED));
}

TEST(VideoColorSpaceTest, GuessBT2020FromMatrix) {
  VideoColorSpace video_cs = VideoColorSpace(
      VideoColorSpace::PrimaryID::UNSPECIFIED,
      VideoColorSpace::TransferID::UNSPECIFIED,
      VideoColorSpace::MatrixID::BT2020_NCL, gfx::ColorSpace::RangeID::LIMITED);

  auto guessed_cs = video_cs.GuessGfxColorSpace();
  EXPECT_TRUE(guessed_cs.IsValid());
  EXPECT_EQ(guessed_cs, gfx::ColorSpace(gfx::ColorSpace::PrimaryID::BT2020,
                                        gfx::ColorSpace::TransferID::BT2020_10,
                                        gfx::ColorSpace::MatrixID::BT2020_NCL,
                                        gfx::ColorSpace::RangeID::LIMITED));
}

TEST(VideoColorSpaceTest, GuessBT709FromPartialHints) {
  // When only BT.709 hints are present, should guess BT.709 (not BT.2020).
  VideoColorSpace video_cs =
      VideoColorSpace(VideoColorSpace::PrimaryID::BT709,
                      VideoColorSpace::TransferID::UNSPECIFIED,
                      VideoColorSpace::MatrixID::UNSPECIFIED,
                      gfx::ColorSpace::RangeID::LIMITED);

  auto guessed_cs = video_cs.GuessGfxColorSpace();
  EXPECT_TRUE(guessed_cs.IsValid());
  EXPECT_EQ(guessed_cs, gfx::ColorSpace(gfx::ColorSpace::PrimaryID::BT709,
                                        gfx::ColorSpace::TransferID::BT709,
                                        gfx::ColorSpace::MatrixID::BT709,
                                        gfx::ColorSpace::RangeID::LIMITED));
}

}  // namespace media
