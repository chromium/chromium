// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <set>

#include "base/strings/stringprintf.h"
#include "media/base/video_codecs.h"
#include "media/base/video_color_space.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

TEST(ParseVP9CodecId, NewStyleVP9CodecIDs) {
  VideoCodecProfile profile = VIDEO_CODEC_PROFILE_UNKNOWN;
  uint8_t level = 0;
  VideoColorSpace color_space;

  // Old style is not subset of new style.
  EXPECT_FALSE(ParseNewStyleVp9CodecID("vp8", &profile, &level, &color_space));
  EXPECT_FALSE(ParseNewStyleVp9CodecID("vp9", &profile, &level, &color_space));

  // Parsing should fail when first 4 required fields are not provided.
  EXPECT_FALSE(ParseNewStyleVp9CodecID("vp09", &profile, &level, &color_space));
  EXPECT_FALSE(
      ParseNewStyleVp9CodecID("vp09.00", &profile, &level, &color_space));
  EXPECT_FALSE(
      ParseNewStyleVp9CodecID("vp09.00.10", &profile, &level, &color_space));

  // Expect success when all required fields supplied (and valid).
  // TransferID not specified by string, should default to 709.
  EXPECT_TRUE(
      ParseNewStyleVp9CodecID("vp09.00.10.08", &profile, &level, &color_space));
  EXPECT_EQ(VP9PROFILE_PROFILE0, profile);
  EXPECT_EQ(10, level);
  EXPECT_EQ(VideoColorSpace::TransferID::BT709, color_space.transfer);

  // Verify profile's 1 and 2 parse correctly.
  EXPECT_TRUE(
      ParseNewStyleVp9CodecID("vp09.01.10.08", &profile, &level, &color_space));
  EXPECT_EQ(VP9PROFILE_PROFILE1, profile);
  EXPECT_TRUE(
      ParseNewStyleVp9CodecID("vp09.02.10.08", &profile, &level, &color_space));
  EXPECT_EQ(VP9PROFILE_PROFILE2, profile);
  EXPECT_TRUE(
      ParseNewStyleVp9CodecID("vp09.03.10.08", &profile, &level, &color_space));
  EXPECT_EQ(VP9PROFILE_PROFILE3, profile);
  // Profile 4 is not a thing.
  EXPECT_FALSE(
      ParseNewStyleVp9CodecID("vp09.04.10.08", &profile, &level, &color_space));

  // Verify valid levels parse correctly.
  const std::set<int> kValidVp9Levels = {10, 11, 20, 21, 30, 31, 40,
                                         41, 50, 51, 52, 60, 61, 62};
  size_t num_valid_levels = 0;
  for (int i = 0; i < 99; ++i) {
    // Write "i" as the level.
    char codec_string[14];
    snprintf(codec_string, 14, "vp09.00.%02d.08", i);
    if (kValidVp9Levels.find(i) != kValidVp9Levels.end()) {
      EXPECT_TRUE(ParseNewStyleVp9CodecID(codec_string, &profile, &level,
                                          &color_space));
      EXPECT_EQ(VP9PROFILE_PROFILE0, profile);
      EXPECT_EQ(i, level);
      EXPECT_EQ(VideoColorSpace::TransferID::BT709, color_space.transfer);
      num_valid_levels++;
    } else {
      EXPECT_FALSE(ParseNewStyleVp9CodecID(codec_string, &profile, &level,
                                           &color_space));
    }
  }
  EXPECT_EQ(kValidVp9Levels.size(), num_valid_levels);

  // Verify bitdepths. Only 8, 10, 12 are valid.
  EXPECT_TRUE(
      ParseNewStyleVp9CodecID("vp09.02.10.8", &profile, &level, &color_space));
  EXPECT_TRUE(
      ParseNewStyleVp9CodecID("vp09.02.10.10", &profile, &level, &color_space));
  EXPECT_TRUE(
      ParseNewStyleVp9CodecID("vp09.02.10.12", &profile, &level, &color_space));
  EXPECT_FALSE(
      ParseNewStyleVp9CodecID("vp09.02.10.13", &profile, &level, &color_space));

  // Verify chroma subsampling values.
  EXPECT_TRUE(ParseNewStyleVp9CodecID("vp09.02.10.10.00", &profile, &level,
                                      &color_space));
  EXPECT_TRUE(ParseNewStyleVp9CodecID("vp09.02.10.10.01", &profile, &level,
                                      &color_space));
  EXPECT_TRUE(ParseNewStyleVp9CodecID("vp09.02.10.10.02", &profile, &level,
                                      &color_space));
  EXPECT_TRUE(ParseNewStyleVp9CodecID("vp09.02.10.10.03", &profile, &level,
                                      &color_space));
  // Values 4 - 7 are reserved.
  EXPECT_FALSE(ParseNewStyleVp9CodecID("vp09.02.10.10.04", &profile, &level,
                                       &color_space));

  // Verify a few color profiles.
  // BT709
  EXPECT_TRUE(ParseNewStyleVp9CodecID("vp09.02.10.10.00.01", &profile, &level,
                                      &color_space));
  // BT2020
  EXPECT_TRUE(ParseNewStyleVp9CodecID("vp09.02.10.10.00.09", &profile, &level,
                                      &color_space));
  // 0 is invalid.
  EXPECT_FALSE(ParseNewStyleVp9CodecID("vp09.02.10.10.00.00", &profile, &level,
                                       &color_space));
  // 23 - 255 are reserved.
  EXPECT_FALSE(ParseNewStyleVp9CodecID("vp09.02.10.10.00.23", &profile, &level,
                                       &color_space));

  // Verify a few common EOTFs parse correctly.
  EXPECT_TRUE(ParseNewStyleVp9CodecID("vp09.02.10.10.00.01.01", &profile,
                                      &level, &color_space));
  EXPECT_EQ(VideoColorSpace::TransferID::BT709, color_space.transfer);
  EXPECT_TRUE(ParseNewStyleVp9CodecID("vp09.02.10.10.00.01.04", &profile,
                                      &level, &color_space));
  EXPECT_EQ(VideoColorSpace::TransferID::GAMMA22, color_space.transfer);
  EXPECT_TRUE(ParseNewStyleVp9CodecID("vp09.02.10.10.00.01.06", &profile,
                                      &level, &color_space));
  EXPECT_EQ(VideoColorSpace::TransferID::SMPTE170M, color_space.transfer);
  EXPECT_TRUE(ParseNewStyleVp9CodecID("vp09.02.10.10.00.01.14", &profile,
                                      &level, &color_space));
  EXPECT_EQ(VideoColorSpace::TransferID::BT2020_10, color_space.transfer);
  EXPECT_TRUE(ParseNewStyleVp9CodecID("vp09.02.10.12.00.01.15", &profile,
                                      &level, &color_space));
  EXPECT_EQ(VideoColorSpace::TransferID::BT2020_12, color_space.transfer);
  EXPECT_TRUE(ParseNewStyleVp9CodecID("vp09.02.10.10.00.01.13", &profile,
                                      &level, &color_space));
  EXPECT_EQ(VideoColorSpace::TransferID::IEC61966_2_1, color_space.transfer);
  EXPECT_TRUE(ParseNewStyleVp9CodecID("vp09.02.10.10.00.01.16", &profile,
                                      &level, &color_space));
  EXPECT_EQ(VideoColorSpace::TransferID::SMPTEST2084, color_space.transfer);
  // Verify 0 and 3 are reserved EOTF values.
  EXPECT_FALSE(ParseNewStyleVp9CodecID("vp09.02.10.08.00.01.00", &profile,
                                       &level, &color_space));
  EXPECT_FALSE(ParseNewStyleVp9CodecID("vp09.02.10.08.00.01.03", &profile,
                                       &level, &color_space));

  // Verify a few matrix coefficients.
  EXPECT_TRUE(ParseNewStyleVp9CodecID("vp09.02.10.10.00.01.01.00", &profile,
                                      &level, &color_space));
  EXPECT_TRUE(ParseNewStyleVp9CodecID("vp09.02.10.10.00.01.01.01", &profile,
                                      &level, &color_space));
  EXPECT_TRUE(ParseNewStyleVp9CodecID("vp09.02.10.10.00.01.01.10", &profile,
                                      &level, &color_space));
  // Values 12 - 255 reserved.
  EXPECT_FALSE(ParseNewStyleVp9CodecID("vp09.02.10.10.00.01.01.12", &profile,
                                       &level, &color_space));

  // Verify full range flag (boolean 0 or 1).
  EXPECT_TRUE(ParseNewStyleVp9CodecID("vp09.02.10.10.00.01.01.01.00", &profile,
                                      &level, &color_space));
  EXPECT_TRUE(ParseNewStyleVp9CodecID("vp09.02.10.10.00.01.01.01.01", &profile,
                                      &level, &color_space));
  EXPECT_FALSE(ParseNewStyleVp9CodecID("vp09.02.10.10.00.01.01.01.02", &profile,
                                       &level, &color_space));
}

#if BUILDFLAG(ENABLE_AV1_DECODER)
TEST(ParseAv1CodecId, VerifyRequiredValues) {
  VideoCodecProfile profile = VIDEO_CODEC_PROFILE_UNKNOWN;
  uint8_t level = 0;
  VideoColorSpace color_space;

  // Old style is not subset of new style.
  EXPECT_FALSE(ParseAv1CodecId("av1", &profile, &level, &color_space));

  // Parsing should fail when first 4 required fields are not provided.
  EXPECT_FALSE(ParseAv1CodecId("av01", &profile, &level, &color_space));
  EXPECT_FALSE(ParseAv1CodecId("av01.0", &profile, &level, &color_space));
  EXPECT_FALSE(ParseAv1CodecId("av01.0.04M", &profile, &level, &color_space));

  // Expect success when all required fields supplied (and valid).
  // TransferID not specified by string, should default to 709.
  EXPECT_TRUE(ParseAv1CodecId("av01.0.04M.08", &profile, &level, &color_space));
  EXPECT_EQ(AV1PROFILE_PROFILE_MAIN, profile);
  EXPECT_EQ(4, level);
  EXPECT_EQ(VideoColorSpace::TransferID::BT709, color_space.transfer);

  // Verify high and pro profiles parse correctly.
  EXPECT_TRUE(ParseAv1CodecId("av01.1.04M.08", &profile, &level, &color_space));
  EXPECT_EQ(AV1PROFILE_PROFILE_HIGH, profile);
  EXPECT_TRUE(ParseAv1CodecId("av01.2.04M.08", &profile, &level, &color_space));
  EXPECT_EQ(AV1PROFILE_PROFILE_PRO, profile);

  // Leading zeros or negative values are forbidden.
  EXPECT_FALSE(
      ParseAv1CodecId("av01.00.04M.08", &profile, &level, &color_space));
  EXPECT_FALSE(
      ParseAv1CodecId("av01.-0.04M.08", &profile, &level, &color_space));
  EXPECT_FALSE(
      ParseAv1CodecId("av01.-1.04M.08", &profile, &level, &color_space));

  // There are no profile values > 2
  for (int i = 3; i <= 9; ++i) {
    const std::string codec_string = base::StringPrintf("av01.%d.00M.08", i);
    SCOPED_TRACE(codec_string);
    EXPECT_FALSE(ParseAv1CodecId(codec_string, &profile, &level, &color_space));
  }

  // Leading zeros are required for the level.
  EXPECT_FALSE(ParseAv1CodecId("av01.0.4M.08", &profile, &level, &color_space));

  // Negative values are not allowed.
  EXPECT_FALSE(
      ParseAv1CodecId("av01.0.-4M.08", &profile, &level, &color_space));

  // Verify valid levels parse correctly. Valid profiles are 00 -> 31.
  for (int i = 0; i < 99; ++i) {
    const std::string codec_string = base::StringPrintf("av01.0.%02dM.08", i);
    SCOPED_TRACE(codec_string);

    if (i < 32) {
      EXPECT_TRUE(
          ParseAv1CodecId(codec_string, &profile, &level, &color_space));
      EXPECT_EQ(AV1PROFILE_PROFILE_MAIN, profile);
      EXPECT_EQ(i, level);
      EXPECT_EQ(VideoColorSpace::TransferID::BT709, color_space.transfer);
    } else {
      EXPECT_FALSE(
          ParseAv1CodecId(codec_string, &profile, &level, &color_space));
    }
  }

  // Verify tier parses correctly.
  for (char c = '\0'; c <= '\255'; ++c) {
    const std::string codec_string = base::StringPrintf("av01.1.00%c.08", c);
    SCOPED_TRACE(codec_string);

    if (c == 'M' || c == 'H') {
      EXPECT_TRUE(
          ParseAv1CodecId(codec_string, &profile, &level, &color_space));
      EXPECT_EQ(AV1PROFILE_PROFILE_HIGH, profile);
      EXPECT_EQ(0, level);
      EXPECT_EQ(VideoColorSpace::TransferID::BT709, color_space.transfer);
    } else {
      EXPECT_FALSE(
          ParseAv1CodecId(codec_string, &profile, &level, &color_space));
    }
  }

  // Leading zeros are required for the bit depth.
  EXPECT_FALSE(ParseAv1CodecId("av01.0.04M.8", &profile, &level, &color_space));

  // Verify bitdepths. Only 8, 10, 12 are valid.
  for (int i = 0; i < 99; ++i) {
    const std::string codec_string = base::StringPrintf("av01.0.00M.%02d", i);
    SCOPED_TRACE(codec_string);

    if (i == 8 || i == 10 || i == 12) {
      EXPECT_TRUE(
          ParseAv1CodecId(codec_string, &profile, &level, &color_space));
      EXPECT_EQ(AV1PROFILE_PROFILE_MAIN, profile);
      EXPECT_EQ(0, level);
      EXPECT_EQ(VideoColorSpace::TransferID::BT709, color_space.transfer);
    } else {
      EXPECT_FALSE(
          ParseAv1CodecId(codec_string, &profile, &level, &color_space));
    }
  }
}

TEST(ParseAv1CodecId, VerifyOptionalMonochrome) {
  VideoCodecProfile profile = VIDEO_CODEC_PROFILE_UNKNOWN;
  uint8_t level = 0;
  VideoColorSpace color_space;

  // monochrome is either 0, 1 and leading zeros are not allowed.
  EXPECT_FALSE(
      ParseAv1CodecId("av01.0.04M.08.00", &profile, &level, &color_space));
  for (int i = 0; i <= 9; ++i) {
    const std::string codec_string = base::StringPrintf("av01.0.00M.08.%d", i);
    SCOPED_TRACE(codec_string);
    EXPECT_EQ(i < 2,
              ParseAv1CodecId(codec_string, &profile, &level, &color_space));
  }
}

TEST(ParseAv1CodecId, VerifyOptionalSubsampling) {
  VideoCodecProfile profile = VIDEO_CODEC_PROFILE_UNKNOWN;
  uint8_t level = 0;
  VideoColorSpace color_space;

  // chroma subsampling values are {0,1}{0,1}{0,3} with the last value always
  // zero if either of the first two values are zero.
  EXPECT_TRUE(
      ParseAv1CodecId("av01.0.00M.10.0.000", &profile, &level, &color_space));
  EXPECT_TRUE(
      ParseAv1CodecId("av01.0.00M.10.0.100", &profile, &level, &color_space));
  EXPECT_TRUE(
      ParseAv1CodecId("av01.0.00M.10.0.010", &profile, &level, &color_space));
  EXPECT_TRUE(
      ParseAv1CodecId("av01.0.00M.10.0.111", &profile, &level, &color_space));
  EXPECT_TRUE(
      ParseAv1CodecId("av01.0.00M.10.0.112", &profile, &level, &color_space));
  EXPECT_TRUE(
      ParseAv1CodecId("av01.0.00M.10.0.113", &profile, &level, &color_space));

  // Invalid cases.
  EXPECT_FALSE(
      ParseAv1CodecId("av01.0.00M.10.0.101", &profile, &level, &color_space));
  EXPECT_FALSE(
      ParseAv1CodecId("av01.0.00M.10.0.102", &profile, &level, &color_space));
  EXPECT_FALSE(
      ParseAv1CodecId("av01.0.00M.10.0.103", &profile, &level, &color_space));
  EXPECT_FALSE(
      ParseAv1CodecId("av01.0.00M.10.0.011", &profile, &level, &color_space));
  EXPECT_FALSE(
      ParseAv1CodecId("av01.0.00M.10.0.012", &profile, &level, &color_space));
  EXPECT_FALSE(
      ParseAv1CodecId("av01.0.00M.10.0.013", &profile, &level, &color_space));

  // The last-value may be non-zero if the first two values are non-zero.
  EXPECT_TRUE(
      ParseAv1CodecId("av01.0.00M.10.0.110", &profile, &level, &color_space));
  EXPECT_TRUE(
      ParseAv1CodecId("av01.0.00M.10.0.100", &profile, &level, &color_space));
  EXPECT_TRUE(
      ParseAv1CodecId("av01.0.00M.10.0.010", &profile, &level, &color_space));

  for (int i = 2; i <= 9; ++i) {
    for (int j = 2; j <= 9; ++j) {
      for (int k = 4; k <= 9; ++k) {
        const std::string codec_string =
            base::StringPrintf("av01.0.00M.08.0.%d%d%d", i, j, k);
        SCOPED_TRACE(codec_string);
        EXPECT_FALSE(
            ParseAv1CodecId(codec_string, &profile, &level, &color_space));
      }
    }
  }
}

TEST(ParseAv1CodecId, VerifyOptionalColorProperties) {
  VideoCodecProfile profile = VIDEO_CODEC_PROFILE_UNKNOWN;
  uint8_t level = 0;
  VideoColorSpace color_space;

  // Verify a few color properties. This is non-exhaustive since validation is
  // handled by common color space function. Below we validate only portions
  // specific to the AV1 codec string.

  // Leading zeros must be provided.
  EXPECT_FALSE(
      ParseAv1CodecId("av01.0.00M.10.0.000.1", &profile, &level, &color_space));
  // Negative values are not allowed.
  EXPECT_FALSE(ParseAv1CodecId("av01.0.00M.10.0.000.-1", &profile, &level,
                               &color_space));

  // BT709
  EXPECT_TRUE(ParseAv1CodecId("av01.0.00M.10.0.000.01", &profile, &level,
                              &color_space));
  EXPECT_EQ(VideoColorSpace::PrimaryID::BT709, color_space.primaries);
  // BT2020
  EXPECT_TRUE(ParseAv1CodecId("av01.0.00M.10.0.000.09", &profile, &level,
                              &color_space));
  EXPECT_EQ(VideoColorSpace::PrimaryID::BT2020, color_space.primaries);
  // 0 is invalid.
  EXPECT_FALSE(ParseAv1CodecId("av01.0.00M.10.0.000.00", &profile, &level,
                               &color_space));
  EXPECT_EQ(VideoColorSpace::PrimaryID::INVALID, color_space.primaries);
  // 23 - 255 are reserved.
  EXPECT_FALSE(ParseAv1CodecId("av01.0.00M.10.0.000.23", &profile, &level,
                               &color_space));
  EXPECT_EQ(VideoColorSpace::PrimaryID::INVALID, color_space.primaries);

  // Leading zeros must be provided.
  EXPECT_FALSE(ParseAv1CodecId("av01.0.00M.10.0.000.01.1", &profile, &level,
                               &color_space));
  // Negative values are not allowed.
  EXPECT_FALSE(ParseAv1CodecId("av01.0.00M.10.0.000.01.-1", &profile, &level,
                               &color_space));

  // Verify a few common EOTFs parse correctly.
  EXPECT_TRUE(ParseAv1CodecId("av01.0.00M.10.0.000.01.01", &profile, &level,
                              &color_space));
  EXPECT_EQ(VideoColorSpace::TransferID::BT709, color_space.transfer);
  EXPECT_TRUE(ParseAv1CodecId("av01.0.00M.10.0.000.01.04", &profile, &level,
                              &color_space));
  EXPECT_EQ(VideoColorSpace::TransferID::GAMMA22, color_space.transfer);
  EXPECT_TRUE(ParseAv1CodecId("av01.0.00M.10.0.000.01.06", &profile, &level,
                              &color_space));
  EXPECT_EQ(VideoColorSpace::TransferID::SMPTE170M, color_space.transfer);
  EXPECT_TRUE(ParseAv1CodecId("av01.0.00M.10.0.000.01.14", &profile, &level,
                              &color_space));
  EXPECT_EQ(VideoColorSpace::TransferID::BT2020_10, color_space.transfer);
  EXPECT_TRUE(ParseAv1CodecId("av01.0.00M.10.0.000.01.15", &profile, &level,
                              &color_space));
  EXPECT_EQ(VideoColorSpace::TransferID::BT2020_12, color_space.transfer);
  EXPECT_TRUE(ParseAv1CodecId("av01.0.00M.10.0.000.01.13", &profile, &level,
                              &color_space));
  EXPECT_EQ(VideoColorSpace::TransferID::IEC61966_2_1, color_space.transfer);
  EXPECT_TRUE(ParseAv1CodecId("av01.0.00M.10.0.000.01.16", &profile, &level,
                              &color_space));
  EXPECT_EQ(VideoColorSpace::TransferID::SMPTEST2084, color_space.transfer);
  // Verify 0 and 3 are reserved EOTF values.
  EXPECT_FALSE(ParseAv1CodecId("av01.0.00M.10.0.000.01.00", &profile, &level,
                               &color_space));
  EXPECT_FALSE(ParseAv1CodecId("av01.0.00M.10.0.000.01.03", &profile, &level,
                               &color_space));

  // Leading zeros must be provided.
  EXPECT_FALSE(ParseAv1CodecId("av01.0.00M.10.0.000.01.01.1", &profile, &level,
                               &color_space));
  // Negative values are not allowed.
  EXPECT_FALSE(ParseAv1CodecId("av01.0.00M.10.0.000.01.01.-1", &profile, &level,
                               &color_space));

  // Verify a few matrix coefficients.
  EXPECT_TRUE(ParseAv1CodecId("av01.0.00M.10.0.000.01.01.00", &profile, &level,
                              &color_space));
  EXPECT_EQ(VideoColorSpace::MatrixID::RGB, color_space.matrix);
  EXPECT_TRUE(ParseAv1CodecId("av01.0.00M.10.0.000.01.01.01", &profile, &level,
                              &color_space));
  EXPECT_EQ(VideoColorSpace::MatrixID::BT709, color_space.matrix);
  EXPECT_TRUE(ParseAv1CodecId("av01.0.00M.10.0.000.01.01.10", &profile, &level,
                              &color_space));
  EXPECT_EQ(VideoColorSpace::MatrixID::BT2020_CL, color_space.matrix);

  // Values 12 - 255 reserved. Though 12 at least is a valid value we should
  // support in the future. https://crbug.com/854290
  EXPECT_FALSE(ParseAv1CodecId("av01.0.00M.10.0.000.01.01.12", &profile, &level,
                               &color_space));

  // Leading zeros are not allowed.
  EXPECT_FALSE(ParseAv1CodecId("av01.0.00M.10.0.000.01.01.00.00", &profile,
                               &level, &color_space));
  // Negative values are not allowed.
  EXPECT_FALSE(ParseAv1CodecId("av01.0.00M.10.0.000.01.01.00.-1", &profile,
                               &level, &color_space));

  // Verify full range flag (boolean 0 or 1).
  EXPECT_TRUE(ParseAv1CodecId("av01.0.00M.10.0.000.01.01.00.0", &profile,
                              &level, &color_space));
  EXPECT_EQ(gfx::ColorSpace::RangeID::LIMITED, color_space.range);
  EXPECT_TRUE(ParseAv1CodecId("av01.0.00M.10.0.000.01.01.00.1", &profile,
                              &level, &color_space));
  EXPECT_EQ(gfx::ColorSpace::RangeID::FULL, color_space.range);
  EXPECT_FALSE(ParseAv1CodecId("av01.0.00M.10.0.000.01.01.00.2", &profile,
                               &level, &color_space));
}
#endif  // BUILDFLAG(ENABLE_AV1_DECODER)

#if BUILDFLAG(ENABLE_PLATFORM_HEVC)
TEST(ParseHEVCCodecIdTest, InvalidHEVCCodecIds) {
  VideoCodecProfile profile = VIDEO_CODEC_PROFILE_UNKNOWN;
  uint8_t level_idc = 0;

  // Both hev1 and hvc1 should be supported
  EXPECT_TRUE(ParseHEVCCodecId("hev1.1.6.L93.B0", &profile, &level_idc));
  EXPECT_EQ(profile, HEVCPROFILE_MAIN);
  EXPECT_EQ(level_idc, 93);
  EXPECT_TRUE(ParseHEVCCodecId("hvc1.1.6.L93.B0", &profile, &level_idc));
  EXPECT_EQ(profile, HEVCPROFILE_MAIN);
  EXPECT_EQ(level_idc, 93);

  // Check that codec id string with insufficient number of dot-separated
  // elements are rejected. There must be at least 4 elements: hev1/hvc1 prefix,
  // profile, profile_compatibility, tier+level.
  EXPECT_TRUE(ParseHEVCCodecId("hvc1.1.6.L93", &profile, &level_idc));
  EXPECT_EQ(profile, HEVCPROFILE_MAIN);
  EXPECT_EQ(level_idc, 93);
  EXPECT_FALSE(ParseHEVCCodecId("hvc1", &profile, &level_idc));
  EXPECT_FALSE(ParseHEVCCodecId("hev1", &profile, &level_idc));
  EXPECT_FALSE(ParseHEVCCodecId("hvc1.", &profile, &level_idc));
  EXPECT_FALSE(ParseHEVCCodecId("hvc1..", &profile, &level_idc));
  EXPECT_FALSE(ParseHEVCCodecId("hvc1...", &profile, &level_idc));
  EXPECT_FALSE(ParseHEVCCodecId("hvc1....", &profile, &level_idc));
  EXPECT_FALSE(ParseHEVCCodecId("hvc1.1", &profile, &level_idc));
  EXPECT_FALSE(ParseHEVCCodecId("hvc1.1.", &profile, &level_idc));
  EXPECT_FALSE(ParseHEVCCodecId("hvc1.1..", &profile, &level_idc));
  EXPECT_FALSE(ParseHEVCCodecId("hvc1.1...", &profile, &level_idc));
  EXPECT_FALSE(ParseHEVCCodecId("hvc1.1.6", &profile, &level_idc));
  EXPECT_FALSE(ParseHEVCCodecId("hvc1.1.6.", &profile, &level_idc));
  EXPECT_FALSE(ParseHEVCCodecId("hvc1.1.6..", &profile, &level_idc));
  EXPECT_FALSE(ParseHEVCCodecId("hvc1.1.6...", &profile, &level_idc));
  EXPECT_FALSE(ParseHEVCCodecId("hvc1.1..L93", &profile, &level_idc));
  EXPECT_FALSE(ParseHEVCCodecId("hvc1.1..L93.", &profile, &level_idc));
  EXPECT_FALSE(ParseHEVCCodecId("hvc1.1..L93..", &profile, &level_idc));
  EXPECT_FALSE(ParseHEVCCodecId("hvc1..6", &profile, &level_idc));
  EXPECT_FALSE(ParseHEVCCodecId("hvc1..6.", &profile, &level_idc));
  EXPECT_FALSE(ParseHEVCCodecId("hvc1..6..", &profile, &level_idc));
  EXPECT_FALSE(ParseHEVCCodecId("hvc1..6...", &profile, &level_idc));
  EXPECT_FALSE(ParseHEVCCodecId("hvc1..6.L93", &profile, &level_idc));
  EXPECT_FALSE(ParseHEVCCodecId("hvc1..6.L93.", &profile, &level_idc));
  EXPECT_FALSE(ParseHEVCCodecId("hvc1..6.L93..", &profile, &level_idc));

  // Check that codec ids with empty constraint bytes are rejected.
  EXPECT_FALSE(ParseHEVCCodecId("hvc1.1.6.L93.", &profile, &level_idc));
  EXPECT_FALSE(ParseHEVCCodecId("hvc1.1.6.L93..", &profile, &level_idc));
  EXPECT_FALSE(ParseHEVCCodecId("hvc1.1.6.L93...", &profile, &level_idc));
  EXPECT_FALSE(ParseHEVCCodecId("hvc1.1.6.L93....", &profile, &level_idc));
  EXPECT_FALSE(ParseHEVCCodecId("hvc1.1.6.L93.....", &profile, &level_idc));
  EXPECT_FALSE(ParseHEVCCodecId("hvc1.1.6.L93......", &profile, &level_idc));
  EXPECT_FALSE(ParseHEVCCodecId("hvc1.1.6.L93.......", &profile, &level_idc));
  EXPECT_FALSE(ParseHEVCCodecId("hvc1.1.6.L93.......0", &profile, &level_idc));
  EXPECT_FALSE(ParseHEVCCodecId("hvc1.1.6.L93.0.", &profile, &level_idc));
  EXPECT_FALSE(ParseHEVCCodecId("hvc1.1.6.L93.0..", &profile, &level_idc));
  EXPECT_FALSE(ParseHEVCCodecId("hvc1.1.6.L93.0..0", &profile, &level_idc));
  EXPECT_FALSE(
      ParseHEVCCodecId("hvc1.1.6.L93.0..0.0.0.0.0", &profile, &level_idc));
  EXPECT_FALSE(
      ParseHEVCCodecId("hvc1.1.6.L93.0.0.0.0.0.0.", &profile, &level_idc));

  // Different variations of general_profile_space (empty, 'A', 'B', 'C')
  EXPECT_TRUE(ParseHEVCCodecId("hvc1.1.6.L93.B0", &profile, &level_idc));
  EXPECT_TRUE(ParseHEVCCodecId("hvc1.A1.6.L93.B0", &profile, &level_idc));
  EXPECT_TRUE(ParseHEVCCodecId("hvc1.B1.6.L93.B0", &profile, &level_idc));
  EXPECT_TRUE(ParseHEVCCodecId("hvc1.C1.6.L93.B0", &profile, &level_idc));
  EXPECT_FALSE(ParseHEVCCodecId("hvc1.D1.6.L93.B0", &profile, &level_idc));

  // general_profile_idc (the number after the first dot) must be a 5-bit
  // decimal-encoded number (between 0 and 31)
  EXPECT_TRUE(ParseHEVCCodecId("hvc1.0.6.L93.B0", &profile, &level_idc));
  EXPECT_TRUE(ParseHEVCCodecId("hvc1.31.6.L93.B0", &profile, &level_idc));
  EXPECT_TRUE(ParseHEVCCodecId("hvc1.1.6.L93.B0", &profile, &level_idc));
  EXPECT_EQ(profile, HEVCPROFILE_MAIN);
  EXPECT_TRUE(ParseHEVCCodecId("hvc1.2.2.L93.B0", &profile, &level_idc));
  EXPECT_EQ(profile, HEVCPROFILE_MAIN10);
  EXPECT_TRUE(ParseHEVCCodecId("hvc1.3.4.L93.B0", &profile, &level_idc));
  EXPECT_EQ(profile, HEVCPROFILE_MAIN_STILL_PICTURE);
  EXPECT_FALSE(ParseHEVCCodecId("hvc1.-1.6.L93.B0", &profile, &level_idc));
  EXPECT_FALSE(ParseHEVCCodecId("hvc1.32.6.L93.B0", &profile, &level_idc));
  EXPECT_FALSE(ParseHEVCCodecId("hvc1.999.6.L93.B0", &profile, &level_idc));
  EXPECT_FALSE(ParseHEVCCodecId("hvc1.A.6.L93.B0", &profile, &level_idc));
  EXPECT_FALSE(ParseHEVCCodecId("hvc1.1F.6.L93.B0", &profile, &level_idc));

  // general_profile_compatibility_flags is a 32-bit hex number
  EXPECT_TRUE(ParseHEVCCodecId("hvc1.1.0.L93.B0", &profile, &level_idc));
  EXPECT_TRUE(ParseHEVCCodecId("hvc1.1.FF.L93.B0", &profile, &level_idc));
  EXPECT_TRUE(ParseHEVCCodecId("hvc1.1.FFFF.L93.B0", &profile, &level_idc));
  EXPECT_TRUE(ParseHEVCCodecId("hvc1.1.FFFFFFFF.L93.B0", &profile, &level_idc));
  EXPECT_FALSE(
      ParseHEVCCodecId("hvc1.1.100000000.L93.B0", &profile, &level_idc));
  EXPECT_FALSE(
      ParseHEVCCodecId("hvc1.1.FFFFFFFFF.L93.B0", &profile, &level_idc));
  EXPECT_FALSE(ParseHEVCCodecId("hvc1.1.-1.L93.B0", &profile, &level_idc));
  EXPECT_FALSE(ParseHEVCCodecId("hvc1.1.0G.L93.B0", &profile, &level_idc));

  // general_tier_flag is encoded as either character 'L' (general_tier_flag==0)
  // or character 'H' (general_tier_flag==1) in the fourth element of the string
  EXPECT_TRUE(ParseHEVCCodecId("hvc1.1.6.L93.B0", &profile, &level_idc));
  EXPECT_TRUE(ParseHEVCCodecId("hvc1.1.0.H93.B0", &profile, &level_idc));
  EXPECT_FALSE(ParseHEVCCodecId("hvc1.1.0.93.B0", &profile, &level_idc));
  EXPECT_FALSE(ParseHEVCCodecId("hvc1.1.0.A93.B0", &profile, &level_idc));

  // general_level_idc is 8-bit decimal-encoded number after general_tier_flag.
  EXPECT_TRUE(ParseHEVCCodecId("hvc1.1.6.L0.B0", &profile, &level_idc));
  EXPECT_EQ(level_idc, 0);
  EXPECT_TRUE(ParseHEVCCodecId("hvc1.1.6.L1.B0", &profile, &level_idc));
  EXPECT_EQ(level_idc, 1);
  // Level 3.1 (93 == 3.1 * 30)
  EXPECT_TRUE(ParseHEVCCodecId("hvc1.1.6.L93.B0", &profile, &level_idc));
  EXPECT_EQ(level_idc, 93);
  // Level 5 (150 == 5 * 30)
  EXPECT_TRUE(ParseHEVCCodecId("hvc1.1.6.L150.B0", &profile, &level_idc));
  EXPECT_EQ(level_idc, 150);
  EXPECT_TRUE(ParseHEVCCodecId("hvc1.1.6.L255.B0", &profile, &level_idc));
  EXPECT_EQ(level_idc, 255);
  EXPECT_FALSE(ParseHEVCCodecId("hvc1.1.6.L256.B0", &profile, &level_idc));
  EXPECT_FALSE(ParseHEVCCodecId("hvc1.1.6.L999.B0", &profile, &level_idc));
  EXPECT_FALSE(ParseHEVCCodecId("hvc1.1.6.L-1.B0", &profile, &level_idc));

  // The elements after the fourth dot are hex-encoded bytes containing
  // constraint flags (up to 6 bytes), trailing zero bytes may be omitted
  EXPECT_TRUE(
      ParseHEVCCodecId("hvc1.1.6.L0.0.0.0.0.0.0", &profile, &level_idc));
  EXPECT_TRUE(
      ParseHEVCCodecId("hvc1.1.6.L0.00.00.00.00.00.00", &profile, &level_idc));
  EXPECT_TRUE(ParseHEVCCodecId("hvc1.1.6.L0.12", &profile, &level_idc));
  EXPECT_TRUE(ParseHEVCCodecId("hvc1.1.6.L0.12.34.56", &profile, &level_idc));
  EXPECT_TRUE(
      ParseHEVCCodecId("hvc1.1.6.L0.12.34.56.78.9A.BC", &profile, &level_idc));
  EXPECT_TRUE(
      ParseHEVCCodecId("hvc1.1.6.L0.FF.FF.FF.FF.FF.FF", &profile, &level_idc));
  EXPECT_FALSE(ParseHEVCCodecId("hvc1.1.6.L0.FF.FF.FF.FF.FF.FF.0", &profile,
                                &level_idc));
  EXPECT_FALSE(ParseHEVCCodecId("hvc1.1.6.L0.100", &profile, &level_idc));
  EXPECT_FALSE(ParseHEVCCodecId("hvc1.1.6.L0.1FF", &profile, &level_idc));
  EXPECT_FALSE(ParseHEVCCodecId("hvc1.1.6.L0.-1", &profile, &level_idc));
}
#endif

#if BUILDFLAG(ENABLE_PLATFORM_DOLBY_VISION)
TEST(ParseDolbyVisionCodecIdTest, InvalidDolbyVisionCodecIds) {
  VideoCodecProfile profile = VIDEO_CODEC_PROFILE_UNKNOWN;
  uint8_t level_id = 0;

  // Codec dvav/dva1 should only contain profile 0.
  EXPECT_TRUE(ParseDolbyVisionCodecId("dvav.00.07", &profile, &level_id));
  EXPECT_EQ(profile, DOLBYVISION_PROFILE0);
  EXPECT_EQ(level_id, 7);
  EXPECT_TRUE(ParseDolbyVisionCodecId("dva1.00.07", &profile, &level_id));
  EXPECT_EQ(profile, DOLBYVISION_PROFILE0);
  EXPECT_EQ(level_id, 7);
  EXPECT_FALSE(ParseDolbyVisionCodecId("dvav.04.07", &profile, &level_id));
  EXPECT_FALSE(ParseDolbyVisionCodecId("dva1.04.07", &profile, &level_id));
  EXPECT_FALSE(ParseDolbyVisionCodecId("dvav.05.07", &profile, &level_id));
  EXPECT_FALSE(ParseDolbyVisionCodecId("dva1.05.07", &profile, &level_id));
  EXPECT_FALSE(ParseDolbyVisionCodecId("dvav.07.07", &profile, &level_id));
  EXPECT_FALSE(ParseDolbyVisionCodecId("dva1.07.07", &profile, &level_id));

#if BUILDFLAG(ENABLE_PLATFORM_HEVC)
  // Codec dvhe/dvh1 should only contain profile 4, 5, and 7.
  EXPECT_TRUE(ParseDolbyVisionCodecId("dvhe.04.07", &profile, &level_id));
  EXPECT_EQ(profile, DOLBYVISION_PROFILE4);
  EXPECT_EQ(level_id, 7);
  EXPECT_TRUE(ParseDolbyVisionCodecId("dvhe.05.07", &profile, &level_id));
  EXPECT_EQ(profile, DOLBYVISION_PROFILE5);
  EXPECT_EQ(level_id, 7);
  EXPECT_TRUE(ParseDolbyVisionCodecId("dvh1.05.07", &profile, &level_id));
  EXPECT_EQ(profile, DOLBYVISION_PROFILE5);
  EXPECT_EQ(level_id, 7);
  EXPECT_TRUE(ParseDolbyVisionCodecId("dvhe.07.07", &profile, &level_id));
  EXPECT_EQ(profile, DOLBYVISION_PROFILE7);
  EXPECT_EQ(level_id, 7);
  EXPECT_FALSE(ParseDolbyVisionCodecId("dvhe.00.07", &profile, &level_id));
  EXPECT_FALSE(ParseDolbyVisionCodecId("dvh1.00.07", &profile, &level_id));

  // Profiles 1, 2, 3 and 6 are deprecated.
  EXPECT_FALSE(ParseDolbyVisionCodecId("dvav.01.07", &profile, &level_id));
  EXPECT_FALSE(ParseDolbyVisionCodecId("dvhe.02.07", &profile, &level_id));
  EXPECT_FALSE(ParseDolbyVisionCodecId("dvhe.03.07", &profile, &level_id));
  EXPECT_FALSE(ParseDolbyVisionCodecId("dvhe.06.07", &profile, &level_id));

  // Level should be numbers between 1 and 9.
  EXPECT_FALSE(ParseDolbyVisionCodecId("dvhe.04.00", &profile, &level_id));
  EXPECT_FALSE(ParseDolbyVisionCodecId("dvhe.04.10", &profile, &level_id));
  EXPECT_FALSE(ParseDolbyVisionCodecId("dvhe.04.20", &profile, &level_id));
  EXPECT_FALSE(ParseDolbyVisionCodecId("dvhe.04.99", &profile, &level_id));

  // Valid codec string is <FourCC>.<two digits profile>.<two digits level>.
  EXPECT_FALSE(ParseDolbyVisionCodecId("dvhe", &profile, &level_id));
  EXPECT_FALSE(ParseDolbyVisionCodecId("dvhe.", &profile, &level_id));
  EXPECT_FALSE(ParseDolbyVisionCodecId("dvhe..", &profile, &level_id));
  EXPECT_FALSE(ParseDolbyVisionCodecId("dvhe...", &profile, &level_id));
  EXPECT_FALSE(ParseDolbyVisionCodecId("dvhe....", &profile, &level_id));
  EXPECT_FALSE(ParseDolbyVisionCodecId("dvhe.5", &profile, &level_id));
  EXPECT_FALSE(ParseDolbyVisionCodecId("dvhe.5.", &profile, &level_id));
  EXPECT_FALSE(ParseDolbyVisionCodecId("dvhe.5..", &profile, &level_id));
  EXPECT_FALSE(ParseDolbyVisionCodecId("dvhe.5...", &profile, &level_id));
  EXPECT_FALSE(ParseDolbyVisionCodecId("dvhe.5.7", &profile, &level_id));
  EXPECT_FALSE(ParseDolbyVisionCodecId("dvhe.5.7.", &profile, &level_id));
  EXPECT_FALSE(ParseDolbyVisionCodecId("dvhe.5.7..", &profile, &level_id));
  EXPECT_FALSE(ParseDolbyVisionCodecId("dvhe.5.7...", &profile, &level_id));
  EXPECT_FALSE(ParseDolbyVisionCodecId("dvhe..5", &profile, &level_id));
#endif
}
#endif

}  // namespace media
