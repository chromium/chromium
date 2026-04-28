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
  auto video_cs = VideoColorSpace(VideoColorSpace::PrimaryID::BT709,
                                  VideoColorSpace::TransferID::UNSPECIFIED,
                                  VideoColorSpace::MatrixID::UNSPECIFIED,
                                  gfx::ColorSpace::RangeID::INVALID);
  EXPECT_FALSE(video_cs.IsSpecified());
  auto gfx_cs = video_cs.ToGfxColorSpace();
  EXPECT_FALSE(gfx_cs.IsValid());
  auto guessed_cs = video_cs.GuessGfxColorSpace();
  EXPECT_TRUE(guessed_cs.IsValid());
  EXPECT_EQ(guessed_cs, gfx::ColorSpace(gfx::ColorSpace::PrimaryID::BT709,
                                        gfx::ColorSpace::TransferID::BT709,
                                        gfx::ColorSpace::MatrixID::BT709,
                                        gfx::ColorSpace::RangeID::LIMITED));
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

struct RoundTripTestCase {
  const char* name;
  VideoColorSpace::PrimaryID primary;
  VideoColorSpace::TransferID transfer;
  VideoColorSpace::MatrixID matrix;
  gfx::ColorSpace::RangeID range;
};

class VideoColorSpaceRoundTripTest
    : public testing::TestWithParam<RoundTripTestCase> {};

TEST_P(VideoColorSpaceRoundTripTest, RoundTrip) {
  const RoundTripTestCase& tc = GetParam();
  VideoColorSpace original(tc.primary, tc.transfer, tc.matrix, tc.range);
  gfx::ColorSpace gfx_cs = original.ToGfxColorSpace();
  EXPECT_TRUE(gfx_cs.IsValid())
      << "ToGfxColorSpace() returned invalid for: " << original.ToString();
  VideoColorSpace result = VideoColorSpace::FromGfxColorSpace(gfx_cs);
  EXPECT_EQ(original, result)
      << "Round-trip failed. Original: " << original.ToString()
      << " Result: " << result.ToString();
}

INSTANTIATE_TEST_SUITE_P(
    VideoColorSpaceConversion,
    VideoColorSpaceRoundTripTest,
    testing::Values(
        RoundTripTestCase{"BT709_standard", VideoColorSpace::PrimaryID::BT709,
                          VideoColorSpace::TransferID::BT709,
                          VideoColorSpace::MatrixID::BT709,
                          gfx::ColorSpace::RangeID::LIMITED},
        RoundTripTestCase{"SMPTE170M_BT601",
                          VideoColorSpace::PrimaryID::SMPTE170M,
                          VideoColorSpace::TransferID::SMPTE170M,
                          VideoColorSpace::MatrixID::SMPTE170M,
                          gfx::ColorSpace::RangeID::LIMITED},
        RoundTripTestCase{"BT2020_HDR10", VideoColorSpace::PrimaryID::BT2020,
                          VideoColorSpace::TransferID::SMPTEST2084,
                          VideoColorSpace::MatrixID::BT2020_NCL,
                          gfx::ColorSpace::RangeID::LIMITED},
        RoundTripTestCase{"BT2020_HLG", VideoColorSpace::PrimaryID::BT2020,
                          VideoColorSpace::TransferID::ARIB_STD_B67,
                          VideoColorSpace::MatrixID::BT2020_NCL,
                          gfx::ColorSpace::RangeID::LIMITED},
        RoundTripTestCase{"P3_HDR10", VideoColorSpace::PrimaryID::SMPTEST432_1,
                          VideoColorSpace::TransferID::SMPTEST2084,
                          VideoColorSpace::MatrixID::BT2020_NCL,
                          gfx::ColorSpace::RangeID::LIMITED},
        RoundTripTestCase{"JPEG", VideoColorSpace::PrimaryID::BT709,
                          VideoColorSpace::TransferID::IEC61966_2_1,
                          VideoColorSpace::MatrixID::SMPTE170M,
                          gfx::ColorSpace::RangeID::FULL},
        RoundTripTestCase{"BT2020_10bit", VideoColorSpace::PrimaryID::BT2020,
                          VideoColorSpace::TransferID::BT2020_10,
                          VideoColorSpace::MatrixID::BT2020_NCL,
                          gfx::ColorSpace::RangeID::LIMITED},
        // NOTE: BT2020_CL (MatrixID 10) is no longer supported — ToGfxMatrixID
        // maps it to INVALID. See crbug.com/333906350. No round-trip test case.
        RoundTripTestCase{"DCI_cinema_HDR_full",
                          VideoColorSpace::PrimaryID::SMPTEST431_2,
                          VideoColorSpace::TransferID::SMPTEST2084,
                          VideoColorSpace::MatrixID::BT2020_NCL,
                          gfx::ColorSpace::RangeID::FULL},
        RoundTripTestCase{"legacy_NTSC", VideoColorSpace::PrimaryID::BT470M,
                          VideoColorSpace::TransferID::GAMMA22,
                          VideoColorSpace::MatrixID::FCC,
                          gfx::ColorSpace::RangeID::LIMITED},
        RoundTripTestCase{"legacy_PAL", VideoColorSpace::PrimaryID::BT470BG,
                          VideoColorSpace::TransferID::GAMMA28,
                          VideoColorSpace::MatrixID::BT470BG,
                          gfx::ColorSpace::RangeID::LIMITED},
        RoundTripTestCase{"SMPTE240M_early_HDTV",
                          VideoColorSpace::PrimaryID::SMPTE240M,
                          VideoColorSpace::TransferID::SMPTE240M,
                          VideoColorSpace::MatrixID::SMPTE240M,
                          gfx::ColorSpace::RangeID::LIMITED},
        RoundTripTestCase{"FILM_generic", VideoColorSpace::PrimaryID::FILM,
                          VideoColorSpace::TransferID::BT709,
                          VideoColorSpace::MatrixID::BT709,
                          gfx::ColorSpace::RangeID::LIMITED},
        RoundTripTestCase{"BT709_linear", VideoColorSpace::PrimaryID::BT709,
                          VideoColorSpace::TransferID::LINEAR,
                          VideoColorSpace::MatrixID::BT709,
                          gfx::ColorSpace::RangeID::FULL},
        RoundTripTestCase{"BT709_log", VideoColorSpace::PrimaryID::BT709,
                          VideoColorSpace::TransferID::LOG,
                          VideoColorSpace::MatrixID::BT709,
                          gfx::ColorSpace::RangeID::LIMITED},
        RoundTripTestCase{"BT709_log_sqrt", VideoColorSpace::PrimaryID::BT709,
                          VideoColorSpace::TransferID::LOG_SQRT,
                          VideoColorSpace::MatrixID::BT709,
                          gfx::ColorSpace::RangeID::LIMITED},
        RoundTripTestCase{"IEC61966_2_4", VideoColorSpace::PrimaryID::BT709,
                          VideoColorSpace::TransferID::IEC61966_2_4,
                          VideoColorSpace::MatrixID::BT709,
                          gfx::ColorSpace::RangeID::LIMITED},
        RoundTripTestCase{"BT1361_ECG", VideoColorSpace::PrimaryID::BT709,
                          VideoColorSpace::TransferID::BT1361_ECG,
                          VideoColorSpace::MatrixID::BT709,
                          gfx::ColorSpace::RangeID::LIMITED},
        RoundTripTestCase{"YCOCG_PQ", VideoColorSpace::PrimaryID::BT2020,
                          VideoColorSpace::TransferID::SMPTEST2084,
                          VideoColorSpace::MatrixID::YCOCG,
                          gfx::ColorSpace::RangeID::LIMITED},
        RoundTripTestCase{"YDZDX_PQ", VideoColorSpace::PrimaryID::BT2020,
                          VideoColorSpace::TransferID::SMPTEST2084,
                          VideoColorSpace::MatrixID::YDZDX,
                          gfx::ColorSpace::RangeID::LIMITED}),
    [](const testing::TestParamInfo<RoundTripTestCase>& info) {
      return std::string(info.param.name);
    });

TEST(VideoColorSpaceTest, RgbMatrixRoundTrips) {
  VideoColorSpace original(
      VideoColorSpace::PrimaryID::BT709, VideoColorSpace::TransferID::BT709,
      VideoColorSpace::MatrixID::RGB, gfx::ColorSpace::RangeID::FULL);
  gfx::ColorSpace gfx_cs = original.ToGfxColorSpace();
  EXPECT_TRUE(gfx_cs.IsValid());
  EXPECT_EQ(gfx_cs.GetMatrixID(), gfx::ColorSpace::MatrixID::GBR);
  VideoColorSpace result = VideoColorSpace::FromGfxColorSpace(gfx_cs);
  EXPECT_EQ(result.matrix(), VideoColorSpace::MatrixID::RGB);
}

TEST(VideoColorSpaceTest, XyzCinemaRgbMatrixRoundTrips) {
  VideoColorSpace original(VideoColorSpace::PrimaryID::SMPTEST428_1,
                           VideoColorSpace::TransferID::SMPTEST428_1,
                           VideoColorSpace::MatrixID::RGB,
                           gfx::ColorSpace::RangeID::FULL);
  gfx::ColorSpace gfx_cs = original.ToGfxColorSpace();
  EXPECT_TRUE(gfx_cs.IsValid());
  EXPECT_EQ(gfx_cs.GetMatrixID(), gfx::ColorSpace::MatrixID::GBR);
  VideoColorSpace result = VideoColorSpace::FromGfxColorSpace(gfx_cs);
  EXPECT_EQ(result.matrix(), VideoColorSpace::MatrixID::RGB);
}

TEST(VideoColorSpaceTest, GuessOnlyBT2020Primary) {
  VideoColorSpace cs(VideoColorSpace::PrimaryID::BT2020,
                     VideoColorSpace::TransferID::UNSPECIFIED,
                     VideoColorSpace::MatrixID::UNSPECIFIED,
                     gfx::ColorSpace::RangeID::INVALID);
  gfx::ColorSpace guessed = cs.GuessGfxColorSpace();
  EXPECT_TRUE(guessed.IsValid());
  EXPECT_EQ(guessed.GetPrimaryID(), gfx::ColorSpace::PrimaryID::BT2020);
  EXPECT_EQ(guessed.GetTransferID(), gfx::ColorSpace::TransferID::BT2020_10);
  EXPECT_EQ(guessed.GetMatrixID(), gfx::ColorSpace::MatrixID::BT2020_NCL);
  EXPECT_EQ(guessed.GetRangeID(), gfx::ColorSpace::RangeID::DERIVED);
}

TEST(VideoColorSpaceTest, GuessOnlyPQTransfer) {
  VideoColorSpace cs(VideoColorSpace::PrimaryID::UNSPECIFIED,
                     VideoColorSpace::TransferID::SMPTEST2084,
                     VideoColorSpace::MatrixID::UNSPECIFIED,
                     gfx::ColorSpace::RangeID::INVALID);
  gfx::ColorSpace guessed = cs.GuessGfxColorSpace();
  EXPECT_TRUE(guessed.IsValid());
  EXPECT_EQ(guessed.GetTransferID(), gfx::ColorSpace::TransferID::PQ);
  EXPECT_EQ(guessed.GetPrimaryID(), gfx::ColorSpace::PrimaryID::BT2020);
  EXPECT_EQ(guessed.GetMatrixID(), gfx::ColorSpace::MatrixID::BT2020_NCL);
}

TEST(VideoColorSpaceTest, GuessOnlyHLGTransfer) {
  VideoColorSpace cs(VideoColorSpace::PrimaryID::UNSPECIFIED,
                     VideoColorSpace::TransferID::ARIB_STD_B67,
                     VideoColorSpace::MatrixID::UNSPECIFIED,
                     gfx::ColorSpace::RangeID::INVALID);
  gfx::ColorSpace guessed = cs.GuessGfxColorSpace();
  EXPECT_TRUE(guessed.IsValid());
  EXPECT_EQ(guessed.GetTransferID(), gfx::ColorSpace::TransferID::HLG);
  EXPECT_EQ(guessed.GetPrimaryID(), gfx::ColorSpace::PrimaryID::BT2020);
  EXPECT_EQ(guessed.GetMatrixID(), gfx::ColorSpace::MatrixID::BT2020_NCL);
}

TEST(VideoColorSpaceTest, GuessOnlyBT2020NCLMatrix) {
  VideoColorSpace cs(VideoColorSpace::PrimaryID::UNSPECIFIED,
                     VideoColorSpace::TransferID::UNSPECIFIED,
                     VideoColorSpace::MatrixID::BT2020_NCL,
                     gfx::ColorSpace::RangeID::INVALID);
  gfx::ColorSpace guessed = cs.GuessGfxColorSpace();
  EXPECT_TRUE(guessed.IsValid());
  EXPECT_EQ(guessed.GetMatrixID(), gfx::ColorSpace::MatrixID::BT2020_NCL);
  EXPECT_EQ(guessed.GetPrimaryID(), gfx::ColorSpace::PrimaryID::BT2020);
  EXPECT_EQ(guessed.GetTransferID(), gfx::ColorSpace::TransferID::BT2020_10);
  EXPECT_EQ(guessed.GetRangeID(), gfx::ColorSpace::RangeID::DERIVED);
}

TEST(VideoColorSpaceTest, GuessOnlyFullRange) {
  VideoColorSpace cs(VideoColorSpace::PrimaryID::UNSPECIFIED,
                     VideoColorSpace::TransferID::UNSPECIFIED,
                     VideoColorSpace::MatrixID::UNSPECIFIED,
                     gfx::ColorSpace::RangeID::FULL);
  gfx::ColorSpace guessed = cs.GuessGfxColorSpace();
  EXPECT_TRUE(guessed.IsValid());
  EXPECT_EQ(guessed.GetPrimaryID(), gfx::ColorSpace::PrimaryID::BT709);
  EXPECT_EQ(guessed.GetTransferID(), gfx::ColorSpace::TransferID::BT709);
  EXPECT_EQ(guessed.GetMatrixID(), gfx::ColorSpace::MatrixID::BT709);
  EXPECT_EQ(guessed.GetRangeID(), gfx::ColorSpace::RangeID::FULL);
}

TEST(VideoColorSpaceTest, GuessBT2020PrimaryAndPQTransfer) {
  VideoColorSpace cs(VideoColorSpace::PrimaryID::BT2020,
                     VideoColorSpace::TransferID::SMPTEST2084,
                     VideoColorSpace::MatrixID::UNSPECIFIED,
                     gfx::ColorSpace::RangeID::INVALID);
  gfx::ColorSpace guessed = cs.GuessGfxColorSpace();
  EXPECT_TRUE(guessed.IsValid());
  EXPECT_EQ(guessed.GetPrimaryID(), gfx::ColorSpace::PrimaryID::BT2020);
  EXPECT_EQ(guessed.GetTransferID(), gfx::ColorSpace::TransferID::PQ);
  EXPECT_EQ(guessed.GetMatrixID(), gfx::ColorSpace::MatrixID::BT2020_NCL);
  EXPECT_EQ(guessed.GetRangeID(), gfx::ColorSpace::RangeID::DERIVED);
}

TEST(VideoColorSpaceTest, GuessTransferAndMatrixSMPTE170M) {
  VideoColorSpace cs(VideoColorSpace::PrimaryID::UNSPECIFIED,
                     VideoColorSpace::TransferID::SMPTE170M,
                     VideoColorSpace::MatrixID::SMPTE170M,
                     gfx::ColorSpace::RangeID::INVALID);
  gfx::ColorSpace guessed = cs.GuessGfxColorSpace();
  EXPECT_TRUE(guessed.IsValid());
  EXPECT_EQ(guessed.GetPrimaryID(), gfx::ColorSpace::PrimaryID::SMPTE170M);
  EXPECT_EQ(guessed.GetTransferID(), gfx::ColorSpace::TransferID::SMPTE170M);
  EXPECT_EQ(guessed.GetMatrixID(), gfx::ColorSpace::MatrixID::SMPTE170M);
  EXPECT_EQ(guessed.GetRangeID(), gfx::ColorSpace::RangeID::DERIVED);
}

TEST(VideoColorSpaceTest, GuessMissingPrimary) {
  VideoColorSpace cs(VideoColorSpace::PrimaryID::UNSPECIFIED,
                     VideoColorSpace::TransferID::BT709,
                     VideoColorSpace::MatrixID::BT709,
                     gfx::ColorSpace::RangeID::LIMITED);
  gfx::ColorSpace guessed = cs.GuessGfxColorSpace();
  EXPECT_EQ(guessed, gfx::ColorSpace(gfx::ColorSpace::PrimaryID::BT709,
                                     gfx::ColorSpace::TransferID::BT709,
                                     gfx::ColorSpace::MatrixID::BT709,
                                     gfx::ColorSpace::RangeID::LIMITED));
}

TEST(VideoColorSpaceTest, GuessMissingTransfer) {
  VideoColorSpace cs(VideoColorSpace::PrimaryID::BT709,
                     VideoColorSpace::TransferID::UNSPECIFIED,
                     VideoColorSpace::MatrixID::BT709,
                     gfx::ColorSpace::RangeID::LIMITED);
  gfx::ColorSpace guessed = cs.GuessGfxColorSpace();
  EXPECT_EQ(guessed, gfx::ColorSpace(gfx::ColorSpace::PrimaryID::BT709,
                                     gfx::ColorSpace::TransferID::BT709,
                                     gfx::ColorSpace::MatrixID::BT709,
                                     gfx::ColorSpace::RangeID::LIMITED));
}

TEST(VideoColorSpaceTest, GuessMissingMatrix) {
  VideoColorSpace cs(VideoColorSpace::PrimaryID::BT709,
                     VideoColorSpace::TransferID::BT709,
                     VideoColorSpace::MatrixID::UNSPECIFIED,
                     gfx::ColorSpace::RangeID::LIMITED);
  gfx::ColorSpace guessed = cs.GuessGfxColorSpace();
  EXPECT_EQ(guessed, gfx::ColorSpace(gfx::ColorSpace::PrimaryID::BT709,
                                     gfx::ColorSpace::TransferID::BT709,
                                     gfx::ColorSpace::MatrixID::BT709,
                                     gfx::ColorSpace::RangeID::LIMITED));
}

TEST(VideoColorSpaceTest, GuessMissingRange) {
  VideoColorSpace cs(
      VideoColorSpace::PrimaryID::BT709, VideoColorSpace::TransferID::BT709,
      VideoColorSpace::MatrixID::BT709, gfx::ColorSpace::RangeID::INVALID);
  gfx::ColorSpace guessed = cs.GuessGfxColorSpace();
  EXPECT_EQ(guessed, gfx::ColorSpace(gfx::ColorSpace::PrimaryID::BT709,
                                     gfx::ColorSpace::TransferID::BT709,
                                     gfx::ColorSpace::MatrixID::BT709,
                                     gfx::ColorSpace::RangeID::LIMITED));
}

TEST(VideoColorSpaceTest, GuessConflictingBitsHigherWins) {
  VideoColorSpace cs(VideoColorSpace::PrimaryID::BT709,
                     VideoColorSpace::TransferID::UNSPECIFIED,
                     VideoColorSpace::MatrixID::BT470BG,
                     gfx::ColorSpace::RangeID::INVALID);
  gfx::ColorSpace guessed = cs.GuessGfxColorSpace();
  EXPECT_TRUE(guessed.IsValid());
  EXPECT_EQ(guessed.GetPrimaryID(), gfx::ColorSpace::PrimaryID::BT709);
  EXPECT_EQ(guessed.GetTransferID(), gfx::ColorSpace::TransferID::BT709);
  EXPECT_EQ(guessed.GetMatrixID(), gfx::ColorSpace::MatrixID::BT470BG);
}

TEST(VideoColorSpaceTest, WidegamutSdrBt2020PrimaryBt709Transfer) {
  VideoColorSpace cs(
      VideoColorSpace::PrimaryID::BT2020, VideoColorSpace::TransferID::BT709,
      VideoColorSpace::MatrixID::BT709, gfx::ColorSpace::RangeID::LIMITED);
  gfx::ColorSpace gfx_cs = cs.ToGfxColorSpace();
  EXPECT_TRUE(gfx_cs.IsValid());
  EXPECT_EQ(gfx_cs.GetPrimaryID(), gfx::ColorSpace::PrimaryID::BT2020);
  EXPECT_EQ(gfx_cs.GetTransferID(), gfx::ColorSpace::TransferID::BT709);
  EXPECT_EQ(gfx_cs.GetMatrixID(), gfx::ColorSpace::MatrixID::BT709);
}

TEST(VideoColorSpaceTest, RgbMatrixLimitedRange) {
  VideoColorSpace cs(
      VideoColorSpace::PrimaryID::BT709, VideoColorSpace::TransferID::BT709,
      VideoColorSpace::MatrixID::RGB, gfx::ColorSpace::RangeID::LIMITED);
  gfx::ColorSpace gfx_cs = cs.ToGfxColorSpace();
  EXPECT_TRUE(gfx_cs.IsValid());
  EXPECT_EQ(gfx_cs.GetMatrixID(), gfx::ColorSpace::MatrixID::GBR);
  EXPECT_EQ(gfx_cs.GetRangeID(), gfx::ColorSpace::RangeID::LIMITED);
}

TEST(VideoColorSpaceTest, DciPrimariesWithGamma22) {
  VideoColorSpace cs(VideoColorSpace::PrimaryID::SMPTEST431_2,
                     VideoColorSpace::TransferID::GAMMA22,
                     VideoColorSpace::MatrixID::BT709,
                     gfx::ColorSpace::RangeID::LIMITED);
  gfx::ColorSpace gfx_cs = cs.ToGfxColorSpace();
  EXPECT_TRUE(gfx_cs.IsValid());
  EXPECT_EQ(gfx_cs.GetPrimaryID(), gfx::ColorSpace::PrimaryID::SMPTEST431_2);
  EXPECT_EQ(gfx_cs.GetTransferID(), gfx::ColorSpace::TransferID::GAMMA22);
}

TEST(VideoColorSpaceTest, HlgFullRange) {
  VideoColorSpace cs(VideoColorSpace::PrimaryID::BT2020,
                     VideoColorSpace::TransferID::ARIB_STD_B67,
                     VideoColorSpace::MatrixID::BT2020_NCL,
                     gfx::ColorSpace::RangeID::FULL);
  gfx::ColorSpace gfx_cs = cs.ToGfxColorSpace();
  EXPECT_TRUE(gfx_cs.IsValid());
  EXPECT_EQ(gfx_cs.GetTransferID(), gfx::ColorSpace::TransferID::HLG);
  EXPECT_EQ(gfx_cs.GetRangeID(), gfx::ColorSpace::RangeID::FULL);
}

TEST(VideoColorSpaceTest, Bt601PrimaryBt2020Matrix) {
  VideoColorSpace cs(VideoColorSpace::PrimaryID::SMPTE170M,
                     VideoColorSpace::TransferID::SMPTE170M,
                     VideoColorSpace::MatrixID::BT2020_NCL,
                     gfx::ColorSpace::RangeID::LIMITED);
  gfx::ColorSpace gfx_cs = cs.ToGfxColorSpace();
  EXPECT_TRUE(gfx_cs.IsValid());
  EXPECT_EQ(gfx_cs.GetPrimaryID(), gfx::ColorSpace::PrimaryID::SMPTE170M);
  EXPECT_EQ(gfx_cs.GetMatrixID(), gfx::ColorSpace::MatrixID::BT2020_NCL);
}

TEST(VideoColorSpaceTest, LogTransferWideGamut) {
  VideoColorSpace cs(
      VideoColorSpace::PrimaryID::BT2020, VideoColorSpace::TransferID::LOG,
      VideoColorSpace::MatrixID::BT2020_NCL, gfx::ColorSpace::RangeID::LIMITED);
  gfx::ColorSpace gfx_cs = cs.ToGfxColorSpace();
  EXPECT_TRUE(gfx_cs.IsValid());
  EXPECT_EQ(gfx_cs.GetTransferID(), gfx::ColorSpace::TransferID::LOG);
  EXPECT_EQ(gfx_cs.GetPrimaryID(), gfx::ColorSpace::PrimaryID::BT2020);
}

TEST(VideoColorSpaceTest, Ebu3213ePrimaryMapsToValid) {
  VideoColorSpace cs(VideoColorSpace::PrimaryID::EBU_3213_E,
                     VideoColorSpace::TransferID::BT709,
                     VideoColorSpace::MatrixID::BT709,
                     gfx::ColorSpace::RangeID::LIMITED);
  gfx::ColorSpace gfx_cs = cs.ToGfxColorSpace();
  EXPECT_TRUE(gfx_cs.IsValid());
  EXPECT_EQ(gfx::ColorSpace::PrimaryID::EBU_3213_E, gfx_cs.GetPrimaryID());
}

TEST(VideoColorSpaceTest, AllPrimaryIDValuesDoNotCrash) {
  const int kMax = static_cast<int>(VideoColorSpace::PrimaryID::kMaxValue);
  for (int i = 1; i <= kMax; ++i) {
    VideoColorSpace::PrimaryID pid = VideoColorSpace::GetPrimaryID(i);
    if (pid == VideoColorSpace::PrimaryID::INVALID ||
        pid == VideoColorSpace::PrimaryID::UNSPECIFIED) {
      continue;
    }
    VideoColorSpace cs(pid, VideoColorSpace::TransferID::BT709,
                       VideoColorSpace::MatrixID::BT709,
                       gfx::ColorSpace::RangeID::LIMITED);
    (void)cs.ToGfxColorSpace();
    (void)cs.GuessGfxColorSpace();
  }
}

TEST(VideoColorSpaceTest, AllTransferIDValuesProduceValidColorSpace) {
  const int kMax = static_cast<int>(VideoColorSpace::TransferID::kMaxValue);
  for (int i = 1; i <= kMax; ++i) {
    VideoColorSpace::TransferID tid = VideoColorSpace::GetTransferID(i);
    if (tid == VideoColorSpace::TransferID::INVALID ||
        tid == VideoColorSpace::TransferID::UNSPECIFIED) {
      continue;
    }
    VideoColorSpace cs(VideoColorSpace::PrimaryID::BT709, tid,
                       VideoColorSpace::MatrixID::BT709,
                       gfx::ColorSpace::RangeID::LIMITED);
    gfx::ColorSpace gfx_cs = cs.ToGfxColorSpace();
    EXPECT_TRUE(gfx_cs.IsValid()) << "TransferID " << i << " produced invalid";
    (void)cs.GuessGfxColorSpace();
  }
}

TEST(VideoColorSpaceTest, AllMatrixIDValuesProduceValidColorSpace) {
  for (int i = 0; i <= 11; ++i) {
    VideoColorSpace::MatrixID mid = VideoColorSpace::GetMatrixID(i);
    if (mid == VideoColorSpace::MatrixID::INVALID ||
        mid == VideoColorSpace::MatrixID::UNSPECIFIED) {
      continue;
    }
    VideoColorSpace cs(VideoColorSpace::PrimaryID::BT709,
                       VideoColorSpace::TransferID::BT709, mid,
                       gfx::ColorSpace::RangeID::LIMITED);
    gfx::ColorSpace gfx_cs = cs.ToGfxColorSpace();
    EXPECT_TRUE(gfx_cs.IsValid()) << "MatrixID " << i << " produced invalid";
    (void)cs.GuessGfxColorSpace();
  }
}

TEST(VideoColorSpaceTest, BothRangeIDValuesRoundTrip) {
  for (gfx::ColorSpace::RangeID range :
       {gfx::ColorSpace::RangeID::LIMITED, gfx::ColorSpace::RangeID::FULL}) {
    VideoColorSpace cs(VideoColorSpace::PrimaryID::BT709,
                       VideoColorSpace::TransferID::BT709,
                       VideoColorSpace::MatrixID::BT709, range);
    gfx::ColorSpace gfx_cs = cs.ToGfxColorSpace();
    EXPECT_TRUE(gfx_cs.IsValid());
    EXPECT_EQ(gfx_cs.GetRangeID(), range);
    VideoColorSpace result = VideoColorSpace::FromGfxColorSpace(gfx_cs);
    EXPECT_EQ(result.range(), range);
  }
}

TEST(VideoColorSpaceTest, GetPrimaryIDRejectsInvalidValues) {
  EXPECT_EQ(VideoColorSpace::GetPrimaryID(0),
            VideoColorSpace::PrimaryID::INVALID);
  EXPECT_EQ(VideoColorSpace::GetPrimaryID(3),
            VideoColorSpace::PrimaryID::INVALID);
  EXPECT_EQ(VideoColorSpace::GetPrimaryID(13),
            VideoColorSpace::PrimaryID::INVALID);
  EXPECT_EQ(VideoColorSpace::GetPrimaryID(21),
            VideoColorSpace::PrimaryID::INVALID);
  EXPECT_EQ(VideoColorSpace::GetPrimaryID(23),
            VideoColorSpace::PrimaryID::INVALID);
  EXPECT_EQ(VideoColorSpace::GetPrimaryID(-1),
            VideoColorSpace::PrimaryID::INVALID);
}

TEST(VideoColorSpaceTest, GetTransferIDRejectsInvalidValues) {
  EXPECT_EQ(VideoColorSpace::GetTransferID(0),
            VideoColorSpace::TransferID::INVALID);
  EXPECT_EQ(VideoColorSpace::GetTransferID(3),
            VideoColorSpace::TransferID::INVALID);
  EXPECT_EQ(VideoColorSpace::GetTransferID(19),
            VideoColorSpace::TransferID::INVALID);
  EXPECT_EQ(VideoColorSpace::GetTransferID(-1),
            VideoColorSpace::TransferID::INVALID);
}

TEST(VideoColorSpaceTest, GetMatrixIDRejectsInvalidValues) {
  EXPECT_EQ(VideoColorSpace::GetMatrixID(3),
            VideoColorSpace::MatrixID::INVALID);
  EXPECT_EQ(VideoColorSpace::GetMatrixID(12),
            VideoColorSpace::MatrixID::INVALID);
  EXPECT_EQ(VideoColorSpace::GetMatrixID(-1),
            VideoColorSpace::MatrixID::INVALID);
}

TEST(VideoColorSpaceTest, FromGfxCreateSRGB) {
  VideoColorSpace vcs =
      VideoColorSpace::FromGfxColorSpace(gfx::ColorSpace::CreateSRGB());
  EXPECT_EQ(vcs.primaries(), VideoColorSpace::PrimaryID::BT709);
  EXPECT_EQ(vcs.transfer(), VideoColorSpace::TransferID::IEC61966_2_1);
  EXPECT_EQ(vcs.matrix(), VideoColorSpace::MatrixID::RGB);
  EXPECT_EQ(vcs.range(), gfx::ColorSpace::RangeID::FULL);
}

TEST(VideoColorSpaceTest, FromGfxCreateHDR10) {
  VideoColorSpace vcs =
      VideoColorSpace::FromGfxColorSpace(gfx::ColorSpace::CreateHDR10());
  EXPECT_EQ(vcs.primaries(), VideoColorSpace::PrimaryID::BT2020);
  EXPECT_EQ(vcs.transfer(), VideoColorSpace::TransferID::SMPTEST2084);
  EXPECT_EQ(vcs.matrix(), VideoColorSpace::MatrixID::RGB);
  EXPECT_EQ(vcs.range(), gfx::ColorSpace::RangeID::FULL);
}

TEST(VideoColorSpaceTest, FromGfxCreateHLG) {
  VideoColorSpace vcs =
      VideoColorSpace::FromGfxColorSpace(gfx::ColorSpace::CreateHLG());
  EXPECT_EQ(vcs.primaries(), VideoColorSpace::PrimaryID::BT2020);
  EXPECT_EQ(vcs.transfer(), VideoColorSpace::TransferID::ARIB_STD_B67);
  EXPECT_EQ(vcs.matrix(), VideoColorSpace::MatrixID::RGB);
  EXPECT_EQ(vcs.range(), gfx::ColorSpace::RangeID::FULL);
}

TEST(VideoColorSpaceTest, FromGfxGbrMatrixMapsToRgb) {
  gfx::ColorSpace gfx_cs(
      gfx::ColorSpace::PrimaryID::BT709, gfx::ColorSpace::TransferID::BT709,
      gfx::ColorSpace::MatrixID::GBR, gfx::ColorSpace::RangeID::FULL);
  VideoColorSpace vcs = VideoColorSpace::FromGfxColorSpace(gfx_cs);
  EXPECT_EQ(vcs.matrix(), VideoColorSpace::MatrixID::RGB);
}

TEST(VideoColorSpaceTest, FromGfxAdobeRgbPrimaryReturnsInvalid) {
  gfx::ColorSpace gfx_cs(
      gfx::ColorSpace::PrimaryID::ADOBE_RGB, gfx::ColorSpace::TransferID::SRGB,
      gfx::ColorSpace::MatrixID::BT709, gfx::ColorSpace::RangeID::FULL);
  VideoColorSpace vcs = VideoColorSpace::FromGfxColorSpace(gfx_cs);
  EXPECT_EQ(vcs.primaries(), VideoColorSpace::PrimaryID::INVALID);
  EXPECT_EQ(vcs.transfer(), VideoColorSpace::TransferID::IEC61966_2_1);
  EXPECT_EQ(vcs.matrix(), VideoColorSpace::MatrixID::BT709);
}

TEST(VideoColorSpaceTest, FromGfxXyzD50PrimaryReturnsInvalid) {
  gfx::ColorSpace gfx_cs(
      gfx::ColorSpace::PrimaryID::XYZ_D50, gfx::ColorSpace::TransferID::SRGB,
      gfx::ColorSpace::MatrixID::BT709, gfx::ColorSpace::RangeID::FULL);
  VideoColorSpace vcs = VideoColorSpace::FromGfxColorSpace(gfx_cs);
  EXPECT_EQ(vcs.primaries(), VideoColorSpace::PrimaryID::INVALID);
  EXPECT_TRUE(vcs.IsSpecified());
}

TEST(VideoColorSpaceTest, FromGfxSrgbHdrTransferReturnsInvalid) {
  gfx::ColorSpace gfx_cs(
      gfx::ColorSpace::PrimaryID::BT709, gfx::ColorSpace::TransferID::SRGB_HDR,
      gfx::ColorSpace::MatrixID::BT709, gfx::ColorSpace::RangeID::FULL);
  VideoColorSpace vcs = VideoColorSpace::FromGfxColorSpace(gfx_cs);
  EXPECT_EQ(vcs.transfer(), VideoColorSpace::TransferID::INVALID);
}

TEST(VideoColorSpaceTest, FromGfxScrgbLinear80NitsReturnsInvalid) {
  gfx::ColorSpace gfx_cs(gfx::ColorSpace::PrimaryID::BT709,
                         gfx::ColorSpace::TransferID::SCRGB_LINEAR_80_NITS,
                         gfx::ColorSpace::MatrixID::BT709,
                         gfx::ColorSpace::RangeID::FULL);
  VideoColorSpace vcs = VideoColorSpace::FromGfxColorSpace(gfx_cs);
  EXPECT_EQ(vcs.transfer(), VideoColorSpace::TransferID::INVALID);
}

TEST(VideoColorSpaceTest, FromGfxLinearHdrTransferReturnsInvalid) {
  gfx::ColorSpace gfx_cs(gfx::ColorSpace::PrimaryID::BT709,
                         gfx::ColorSpace::TransferID::LINEAR_HDR,
                         gfx::ColorSpace::MatrixID::BT709,
                         gfx::ColorSpace::RangeID::FULL);
  VideoColorSpace vcs = VideoColorSpace::FromGfxColorSpace(gfx_cs);
  EXPECT_EQ(vcs.transfer(), VideoColorSpace::TransferID::INVALID);
}

TEST(VideoColorSpaceTest, FromGfxBt709AppleTransferMapsToBt709) {
  gfx::ColorSpace gfx_cs(gfx::ColorSpace::PrimaryID::BT709,
                         gfx::ColorSpace::TransferID::BT709_APPLE,
                         gfx::ColorSpace::MatrixID::BT709,
                         gfx::ColorSpace::RangeID::LIMITED);
  VideoColorSpace vcs = VideoColorSpace::FromGfxColorSpace(gfx_cs);
  EXPECT_EQ(vcs.transfer(), VideoColorSpace::TransferID::BT709);
}

TEST(VideoColorSpaceTest, FromGfxDefaultConstructedReturnsAllInvalid) {
  gfx::ColorSpace default_gfx;
  VideoColorSpace vcs = VideoColorSpace::FromGfxColorSpace(default_gfx);
  EXPECT_EQ(vcs.primaries(), VideoColorSpace::PrimaryID::INVALID);
  EXPECT_EQ(vcs.transfer(), VideoColorSpace::TransferID::INVALID);
  EXPECT_EQ(vcs.matrix(), VideoColorSpace::MatrixID::INVALID);
  EXPECT_FALSE(vcs.IsSpecified());
}

TEST(VideoColorSpaceTest, Rec709ToGfxMatchesCreateRec709) {
  EXPECT_EQ(VideoColorSpace::REC709().ToGfxColorSpace(),
            gfx::ColorSpace::CreateREC709());
}

TEST(VideoColorSpaceTest, Rec601ToGfxMatchesCreateRec601) {
  EXPECT_EQ(VideoColorSpace::REC601().ToGfxColorSpace(),
            gfx::ColorSpace::CreateREC601());
}

TEST(VideoColorSpaceTest, JpegToGfxMatchesCreateJpeg) {
  EXPECT_EQ(VideoColorSpace::JPEG().ToGfxColorSpace(),
            gfx::ColorSpace::CreateJpeg());
}

TEST(VideoColorSpaceTest, EnumAndIntConstructorsProduceSameResult) {
  VideoColorSpace from_enum(
      VideoColorSpace::PrimaryID::BT709, VideoColorSpace::TransferID::BT709,
      VideoColorSpace::MatrixID::BT709, gfx::ColorSpace::RangeID::LIMITED);
  VideoColorSpace from_int(1, 1, 1, gfx::ColorSpace::RangeID::LIMITED);
  EXPECT_EQ(from_enum, from_int);
}

TEST(VideoColorSpaceTest, EnumAndIntConstructorsEqualBt2020Pq) {
  VideoColorSpace from_enum(VideoColorSpace::PrimaryID::BT2020,
                            VideoColorSpace::TransferID::SMPTEST2084,
                            VideoColorSpace::MatrixID::BT2020_NCL,
                            gfx::ColorSpace::RangeID::LIMITED);
  VideoColorSpace from_int(9, 16, 9, gfx::ColorSpace::RangeID::LIMITED);
  EXPECT_EQ(from_enum, from_int);
}

TEST(VideoColorSpaceTest, Rec709RoundTripEquality) {
  VideoColorSpace original = VideoColorSpace::REC709();
  VideoColorSpace round_tripped =
      VideoColorSpace::FromGfxColorSpace(original.ToGfxColorSpace());
  EXPECT_EQ(original, round_tripped);
}

TEST(VideoColorSpaceTest, DifferentColorSpacesNotEqual) {
  EXPECT_NE(VideoColorSpace::REC709(), VideoColorSpace::REC601());
  EXPECT_NE(VideoColorSpace::REC709(), VideoColorSpace::JPEG());
  EXPECT_NE(VideoColorSpace::REC601(), VideoColorSpace::JPEG());
}

TEST(VideoColorSpaceTest, IsSpecifiedAllValid) {
  VideoColorSpace cs(
      VideoColorSpace::PrimaryID::BT709, VideoColorSpace::TransferID::BT709,
      VideoColorSpace::MatrixID::BT709, gfx::ColorSpace::RangeID::LIMITED);
  EXPECT_TRUE(cs.IsSpecified());
}

TEST(VideoColorSpaceTest, IsSpecifiedInvalidPrimary) {
  VideoColorSpace cs(
      VideoColorSpace::PrimaryID::INVALID, VideoColorSpace::TransferID::BT709,
      VideoColorSpace::MatrixID::BT709, gfx::ColorSpace::RangeID::LIMITED);
  EXPECT_FALSE(cs.IsSpecified());
}

TEST(VideoColorSpaceTest, IsSpecifiedInvalidTransfer) {
  VideoColorSpace cs(
      VideoColorSpace::PrimaryID::BT709, VideoColorSpace::TransferID::INVALID,
      VideoColorSpace::MatrixID::BT709, gfx::ColorSpace::RangeID::LIMITED);
  EXPECT_FALSE(cs.IsSpecified());
}

TEST(VideoColorSpaceTest, IsSpecifiedInvalidMatrixReturnsFalse) {
  VideoColorSpace cs(
      VideoColorSpace::PrimaryID::BT709, VideoColorSpace::TransferID::BT709,
      VideoColorSpace::MatrixID::INVALID, gfx::ColorSpace::RangeID::LIMITED);
  EXPECT_FALSE(cs.IsSpecified());
}

TEST(VideoColorSpaceTest, IsSpecifiedInvalidRangeReturnsFalse) {
  VideoColorSpace cs(
      VideoColorSpace::PrimaryID::BT709, VideoColorSpace::TransferID::BT709,
      VideoColorSpace::MatrixID::BT709, gfx::ColorSpace::RangeID::INVALID);
  EXPECT_FALSE(cs.IsSpecified());
}

TEST(VideoColorSpaceTest, IsSpecifiedUnspecifiedPrimary) {
  VideoColorSpace cs(VideoColorSpace::PrimaryID::UNSPECIFIED,
                     VideoColorSpace::TransferID::BT709,
                     VideoColorSpace::MatrixID::BT709,
                     gfx::ColorSpace::RangeID::LIMITED);
  EXPECT_FALSE(cs.IsSpecified());
}

TEST(VideoColorSpaceTest, IsSpecifiedUnspecifiedTransfer) {
  VideoColorSpace cs(VideoColorSpace::PrimaryID::BT709,
                     VideoColorSpace::TransferID::UNSPECIFIED,
                     VideoColorSpace::MatrixID::BT709,
                     gfx::ColorSpace::RangeID::LIMITED);
  EXPECT_FALSE(cs.IsSpecified());
}

TEST(VideoColorSpaceTest, IsSpecifiedUnspecifiedMatrixReturnsFalse) {
  VideoColorSpace cs(VideoColorSpace::PrimaryID::BT709,
                     VideoColorSpace::TransferID::BT709,
                     VideoColorSpace::MatrixID::UNSPECIFIED,
                     gfx::ColorSpace::RangeID::LIMITED);
  EXPECT_FALSE(cs.IsSpecified());
}

TEST(VideoColorSpaceTest, IsSpecifiedDerivedRange) {
  VideoColorSpace cs(
      VideoColorSpace::PrimaryID::BT709, VideoColorSpace::TransferID::BT709,
      VideoColorSpace::MatrixID::BT709, gfx::ColorSpace::RangeID::DERIVED);
  EXPECT_TRUE(cs.IsSpecified());
}

TEST(VideoColorSpaceTest, IsSpecifiedBothPrimaryAndTransferInvalid) {
  VideoColorSpace cs(
      VideoColorSpace::PrimaryID::INVALID, VideoColorSpace::TransferID::INVALID,
      VideoColorSpace::MatrixID::BT709, gfx::ColorSpace::RangeID::LIMITED);
  EXPECT_FALSE(cs.IsSpecified());
}

TEST(VideoColorSpaceTest, IsSpecifiedDefaultConstructed) {
  VideoColorSpace cs;
  EXPECT_FALSE(cs.IsSpecified());
}

TEST(VideoColorSpaceTest, IsSpecifiedAllUnspecified) {
  VideoColorSpace cs(VideoColorSpace::PrimaryID::UNSPECIFIED,
                     VideoColorSpace::TransferID::UNSPECIFIED,
                     VideoColorSpace::MatrixID::UNSPECIFIED,
                     gfx::ColorSpace::RangeID::INVALID);
  EXPECT_FALSE(cs.IsSpecified());
}

TEST(VideoColorSpaceTest, IsSpecifiedMixedInvalidAndUnspecified) {
  VideoColorSpace cs(VideoColorSpace::PrimaryID::INVALID,
                     VideoColorSpace::TransferID::UNSPECIFIED,
                     VideoColorSpace::MatrixID::BT709,
                     gfx::ColorSpace::RangeID::LIMITED);
  EXPECT_FALSE(cs.IsSpecified());
}

}  // namespace media
