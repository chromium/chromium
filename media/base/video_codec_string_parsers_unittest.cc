// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/containers/flat_set.h"
#include "base/strings/stringprintf.h"
#include "media/base/video_codec_string_parsers.h"
#include "media/base/video_color_space.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

TEST(ParseVP9CodecId, NewStyleVP9CodecIDs) {
  // Old style is not subset of new style.
  EXPECT_FALSE(ParseNewStyleVp9CodecID("vp8"));
  EXPECT_FALSE(ParseNewStyleVp9CodecID("vp9"));

  // Parsing should fail when first 4 required fields are not provided.
  EXPECT_FALSE(ParseNewStyleVp9CodecID("vp09"));
  EXPECT_FALSE(ParseNewStyleVp9CodecID("vp09.00"));
  EXPECT_FALSE(ParseNewStyleVp9CodecID("vp09.00.10"));

  // Expect success when all required fields supplied (and valid).
  // TransferID not specified by string, should default to 709.
  {
    auto result = ParseNewStyleVp9CodecID("vp09.00.10.08");
    ASSERT_TRUE(result);
    EXPECT_EQ(VideoCodec::kVP9, result->codec);
    EXPECT_EQ(VP9PROFILE_PROFILE0, result->profile);
    EXPECT_EQ(10u, result->level);
    EXPECT_EQ(VideoColorSpace::TransferID::BT709, result->color_space.transfer);
    EXPECT_EQ(VideoChromaSampling::k420, result->subsampling);
    EXPECT_EQ(8u, result->bit_depth);
  }

  // Verify profile's 1, 2, and 3 parse correctly.
  {
    auto result = ParseNewStyleVp9CodecID("vp09.01.10.08");
    ASSERT_TRUE(result);
    EXPECT_EQ(VP9PROFILE_PROFILE1, result->profile);
    EXPECT_EQ(8u, result->bit_depth);
  }

  {
    auto result = ParseNewStyleVp9CodecID("vp09.02.10.10");
    ASSERT_TRUE(result);
    EXPECT_EQ(VP9PROFILE_PROFILE2, result->profile);
    EXPECT_EQ(10u, result->bit_depth);
  }

  {
    auto result = ParseNewStyleVp9CodecID("vp09.03.10.12");
    ASSERT_TRUE(result);
    EXPECT_EQ(VP9PROFILE_PROFILE3, result->profile);
    EXPECT_EQ(12u, result->bit_depth);
  }

  // Profile 4 is not a thing.
  EXPECT_FALSE(ParseNewStyleVp9CodecID("vp09.04.10.08"));

  // Verify valid levels parse correctly.
  const base::flat_set<uint32_t> kValidVp9Levels = {10, 11, 20, 21, 30, 31, 40,
                                                    41, 50, 51, 52, 60, 61, 62};
  size_t num_valid_levels = 0;
  for (uint32_t i = 0; i < 99; ++i) {
    // Write "i" as the level.
    auto codec_string = base::StringPrintf("vp09.00.%02d.08", i);
    if (kValidVp9Levels.find(i) != kValidVp9Levels.end()) {
      auto result = ParseNewStyleVp9CodecID(codec_string);
      ASSERT_TRUE(result);
      EXPECT_EQ(VP9PROFILE_PROFILE0, result->profile);
      EXPECT_EQ(i, result->level);
      EXPECT_EQ(VideoColorSpace::TransferID::BT709,
                result->color_space.transfer);
      num_valid_levels++;
    } else {
      EXPECT_FALSE(ParseNewStyleVp9CodecID(codec_string));
    }
  }
  EXPECT_EQ(kValidVp9Levels.size(), num_valid_levels);

  // Verify bitdepths. Only 8, 10, 12 are valid.
  EXPECT_TRUE(ParseNewStyleVp9CodecID("vp09.02.10.8"));
  EXPECT_TRUE(ParseNewStyleVp9CodecID("vp09.02.10.10"));
  EXPECT_TRUE(ParseNewStyleVp9CodecID("vp09.02.10.12"));
  EXPECT_FALSE(ParseNewStyleVp9CodecID("vp09.02.10.13"));

  // Verify chroma subsampling values.
  {
    auto result = ParseNewStyleVp9CodecID("vp09.02.10.10.00");
    ASSERT_TRUE(result);
    EXPECT_EQ(VideoChromaSampling::k420, result->subsampling);
  }
  {
    auto result = ParseNewStyleVp9CodecID("vp09.02.10.10.01");
    ASSERT_TRUE(result);
    EXPECT_EQ(VideoChromaSampling::k420, result->subsampling);
  }
  {
    auto result = ParseNewStyleVp9CodecID("vp09.01.10.10");
    ASSERT_TRUE(result);
    EXPECT_EQ(VideoChromaSampling::k422, result->subsampling);
  }
  {
    auto result = ParseNewStyleVp9CodecID("vp09.03.10.10");
    ASSERT_TRUE(result);
    EXPECT_EQ(VideoChromaSampling::k422, result->subsampling);
  }
  {
    auto result = ParseNewStyleVp9CodecID("vp09.01.10.10.02");
    ASSERT_TRUE(result);
    EXPECT_EQ(VideoChromaSampling::k422, result->subsampling);
  }
  {
    auto result = ParseNewStyleVp9CodecID("vp09.03.10.10.02");
    ASSERT_TRUE(result);
    EXPECT_EQ(VideoChromaSampling::k422, result->subsampling);
  }
  {
    auto result = ParseNewStyleVp9CodecID("vp09.01.10.10.03");
    ASSERT_TRUE(result);
    EXPECT_EQ(VideoChromaSampling::k444, result->subsampling);
  }
  {
    auto result = ParseNewStyleVp9CodecID("vp09.03.10.10.03");
    ASSERT_TRUE(result);
    EXPECT_EQ(VideoChromaSampling::k444, result->subsampling);
  }

  // Values 4 - 7 are reserved.
  EXPECT_FALSE(ParseNewStyleVp9CodecID("vp09.02.10.10.04"));

  // Test invalid profile + sampling combinations. These are invalid but due to
  // in the wild usage will just return the default subsampling value.
  {
    auto result = ParseNewStyleVp9CodecID("vp09.00.10.10.02");
    ASSERT_TRUE(result);
    EXPECT_EQ(VideoChromaSampling::k420, result->subsampling);
  }
  {
    auto result = ParseNewStyleVp9CodecID("vp09.00.10.10.02");
    ASSERT_TRUE(result);
    EXPECT_EQ(VideoChromaSampling::k420, result->subsampling);
  }
  {
    auto result = ParseNewStyleVp9CodecID("vp09.02.10.10.02");
    ASSERT_TRUE(result);
    EXPECT_EQ(VideoChromaSampling::k420, result->subsampling);
  }
  {
    auto result = ParseNewStyleVp9CodecID("vp09.02.10.10.02");
    ASSERT_TRUE(result);
    EXPECT_EQ(VideoChromaSampling::k420, result->subsampling);
  }
  {
    auto result = ParseNewStyleVp9CodecID("vp09.02.10.10.03");
    ASSERT_TRUE(result);
    EXPECT_EQ(VideoChromaSampling::k420, result->subsampling);
  }
  {
    auto result = ParseNewStyleVp9CodecID("vp09.02.10.10.03");
    ASSERT_TRUE(result);
    EXPECT_EQ(VideoChromaSampling::k420, result->subsampling);
  }

  // Verify a few color profiles.
  // BT709
  EXPECT_TRUE(ParseNewStyleVp9CodecID("vp09.02.10.10.00.01"));
  // BT2020
  EXPECT_TRUE(ParseNewStyleVp9CodecID("vp09.02.10.10.00.09"));
  // 0 is invalid.
  EXPECT_FALSE(ParseNewStyleVp9CodecID("vp09.02.10.10.00.00"));
  // 23 - 255 are reserved.
  EXPECT_FALSE(ParseNewStyleVp9CodecID("vp09.02.10.10.00.23"));

  // Verify a few common EOTFs parse correctly.
  for (int eotf : {1, 4, 6, 14, 15, 13, 16}) {
    auto codec_string = base::StringPrintf("vp09.02.10.10.00.01.%02d", eotf);
    auto result = ParseNewStyleVp9CodecID(codec_string);
    ASSERT_TRUE(result) << "eotf=" << eotf;
    EXPECT_EQ(static_cast<VideoColorSpace::TransferID>(eotf),
              result->color_space.transfer);
  }

  // Verify 0 and 3 are reserved EOTF values.
  EXPECT_FALSE(ParseNewStyleVp9CodecID("vp09.02.10.08.00.01.00"));
  EXPECT_FALSE(ParseNewStyleVp9CodecID("vp09.02.10.08.00.01.03"));

  // Verify a few matrix coefficients.
  EXPECT_TRUE(ParseNewStyleVp9CodecID("vp09.02.10.10.00.01.01.00"));
  EXPECT_TRUE(ParseNewStyleVp9CodecID("vp09.02.10.10.00.01.01.01"));
  EXPECT_TRUE(ParseNewStyleVp9CodecID("vp09.02.10.10.00.01.01.10"));
  // Values 12 - 255 reserved.
  EXPECT_FALSE(ParseNewStyleVp9CodecID("vp09.02.10.10.00.01.01.12"));

  // Verify full range flag (boolean 0 or 1).
  EXPECT_TRUE(ParseNewStyleVp9CodecID("vp09.02.10.10.00.01.01.01.00"));
  EXPECT_TRUE(ParseNewStyleVp9CodecID("vp09.02.10.10.00.01.01.01.01"));
  EXPECT_FALSE(ParseNewStyleVp9CodecID("vp09.02.10.10.00.01.01.01.02"));
}

TEST(ParseAv1CodecId, VerifyRequiredValues) {
  // Old style is not subset of new style.
  EXPECT_FALSE(ParseAv1CodecId("av1"));

  // Parsing should fail when first 4 required fields are not provided.
  EXPECT_FALSE(ParseAv1CodecId("av01"));
  EXPECT_FALSE(ParseAv1CodecId("av01.0"));
  EXPECT_FALSE(ParseAv1CodecId("av01.0.04M"));

  // Expect success when all required fields supplied (and valid).
  // TransferID not specified by string, should default to 709.
  {
    auto result = ParseAv1CodecId("av01.0.04M.08");
    ASSERT_TRUE(result);
    EXPECT_EQ(VideoCodec::kAV1, result->codec);
    EXPECT_EQ(AV1PROFILE_PROFILE_MAIN, result->profile);
    EXPECT_EQ(4u, result->level);
    EXPECT_EQ(VideoColorSpace::TransferID::BT709, result->color_space.transfer);
    EXPECT_EQ(VideoChromaSampling::k420, result->subsampling);
    EXPECT_EQ(8u, result->bit_depth);
  }

  // Verify high and pro profiles parse correctly.
  {
    auto result = ParseAv1CodecId("av01.1.04M.10");
    ASSERT_TRUE(result);
    EXPECT_EQ(AV1PROFILE_PROFILE_HIGH, result->profile);
    EXPECT_EQ(10u, result->bit_depth);
    EXPECT_EQ(VideoChromaSampling::k444, result->subsampling);
  }

  {
    auto result = ParseAv1CodecId("av01.2.04M.12");
    ASSERT_TRUE(result);
    EXPECT_EQ(AV1PROFILE_PROFILE_PRO, result->profile);
    EXPECT_EQ(12u, result->bit_depth);
  }
  // Leading zeros or negative values are forbidden.
  EXPECT_FALSE(ParseAv1CodecId("av01.00.04M.08"));
  EXPECT_FALSE(ParseAv1CodecId("av01.-0.04M.08"));
  EXPECT_FALSE(ParseAv1CodecId("av01.-1.04M.08"));

  // There are no profile values > 2
  for (int i = 3; i <= 9; ++i) {
    const std::string codec_string = base::StringPrintf("av01.%d.00M.08", i);
    SCOPED_TRACE(codec_string);
    EXPECT_FALSE(ParseAv1CodecId(codec_string));
  }

  // Leading zeros are required for the level.
  EXPECT_FALSE(ParseAv1CodecId("av01.0.4M.08"));

  // Negative values are not allowed.
  EXPECT_FALSE(ParseAv1CodecId("av01.0.-4M.08"));

  // Verify valid levels parse correctly. Valid profiles are 00 -> 31.
  for (uint32_t i = 0; i < 99; ++i) {
    const std::string codec_string = base::StringPrintf("av01.0.%02dM.08", i);
    SCOPED_TRACE(codec_string);

    if (i < 32) {
      auto result = ParseAv1CodecId(codec_string);
      ASSERT_TRUE(result);
      EXPECT_EQ(AV1PROFILE_PROFILE_MAIN, result->profile);
      EXPECT_EQ(i, result->level);
      EXPECT_EQ(VideoColorSpace::TransferID::BT709,
                result->color_space.transfer);
    } else {
      EXPECT_FALSE(ParseAv1CodecId(codec_string));
    }
  }

  // Verify tier parses correctly.
  for (char c = '\0'; c <= '\255'; ++c) {
    const std::string codec_string = base::StringPrintf("av01.1.00%c.08", c);
    SCOPED_TRACE(codec_string);

    if (c == 'M' || c == 'H') {
      auto result = ParseAv1CodecId(codec_string);
      ASSERT_TRUE(result);
      EXPECT_EQ(AV1PROFILE_PROFILE_HIGH, result->profile);
      EXPECT_EQ(0u, result->level);
      EXPECT_EQ(VideoColorSpace::TransferID::BT709,
                result->color_space.transfer);
    } else {
      EXPECT_FALSE(ParseAv1CodecId(codec_string));
    }
  }

  // Leading zeros are required for the bit depth.
  EXPECT_FALSE(ParseAv1CodecId("av01.0.04M.8"));

  // Verify bitdepths. Only 8, 10, 12 are valid.
  for (int i = 0; i < 99; ++i) {
    const std::string codec_string = base::StringPrintf("av01.0.00M.%02d", i);
    SCOPED_TRACE(codec_string);

    if (i == 8 || i == 10 || i == 12) {
      auto result = ParseAv1CodecId(codec_string);
      ASSERT_TRUE(result);
      EXPECT_EQ(AV1PROFILE_PROFILE_MAIN, result->profile);
      EXPECT_EQ(0u, result->level);
      EXPECT_EQ(VideoColorSpace::TransferID::BT709,
                result->color_space.transfer);
    } else {
      EXPECT_FALSE(ParseAv1CodecId(codec_string));
    }
  }
}

TEST(ParseAv1CodecId, VerifyOptionalMonochrome) {
  // monochrome is either 0, 1 and leading zeros are not allowed.
  EXPECT_FALSE(ParseAv1CodecId("av01.0.04M.08.00"));

  // monochrome is not allowed with high profile.
  EXPECT_FALSE(ParseAv1CodecId("av01.1.04M.08.1"));

  for (int i = 0; i <= 9; ++i) {
    const std::string codec_string = base::StringPrintf("av01.0.00M.08.%d", i);
    SCOPED_TRACE(codec_string);
    if (i < 2) {
      auto result = ParseAv1CodecId(codec_string);
      ASSERT_TRUE(result);
      EXPECT_EQ(result->subsampling,
                i == 0 ? VideoChromaSampling::k420 : VideoChromaSampling::k400);
    } else {
      EXPECT_FALSE(ParseAv1CodecId(codec_string));
    }
  }
}

TEST(ParseAv1CodecId, VerifyOptionalSubsampling) {
  // chroma subsampling values are {0,1}{0,1}{0,3} with the last value always
  // zero if either of the first two values are zero.
  {
    auto result = ParseAv1CodecId("av01.1.00M.10.0.000");
    ASSERT_TRUE(result);
    EXPECT_EQ(result->subsampling, VideoChromaSampling::k444);
  }
  {
    auto result = ParseAv1CodecId("av01.2.00M.10.0.000");
    ASSERT_TRUE(result);
    EXPECT_EQ(result->subsampling, VideoChromaSampling::k444);
  }
  {
    auto result = ParseAv1CodecId("av01.2.00M.10.0.100");
    ASSERT_TRUE(result);
    EXPECT_EQ(result->subsampling, VideoChromaSampling::k422);
  }
  {
    auto result = ParseAv1CodecId("av01.0.00M.10.0.010");
    ASSERT_TRUE(result);
    EXPECT_EQ(result->subsampling, VideoChromaSampling::k420);
  }
  {
    auto result = ParseAv1CodecId("av01.0.00M.10.0.111");
    ASSERT_TRUE(result);
    EXPECT_EQ(result->subsampling, VideoChromaSampling::k420);
  }
  {
    auto result = ParseAv1CodecId("av01.0.00M.10.0.112");
    ASSERT_TRUE(result);
    EXPECT_EQ(result->subsampling, VideoChromaSampling::k420);
  }
  {
    auto result = ParseAv1CodecId("av01.0.00M.10.0.113");
    ASSERT_TRUE(result);
    EXPECT_EQ(result->subsampling, VideoChromaSampling::k420);
  }

  // Invalid cases.
  EXPECT_FALSE(ParseAv1CodecId("av01.0.00M.10.0.101"));
  EXPECT_FALSE(ParseAv1CodecId("av01.0.00M.10.0.102"));
  EXPECT_FALSE(ParseAv1CodecId("av01.0.00M.10.0.103"));
  EXPECT_FALSE(ParseAv1CodecId("av01.0.00M.10.0.011"));
  EXPECT_FALSE(ParseAv1CodecId("av01.0.00M.10.0.012"));
  EXPECT_FALSE(ParseAv1CodecId("av01.0.00M.10.0.013"));

  // These are invalid but due to in the wild usage will just return the
  // default subsampling value.
  {
    auto result = ParseAv1CodecId("av01.0.00M.10.0.100");
    ASSERT_TRUE(result);
    EXPECT_EQ(result->subsampling, VideoChromaSampling::k420);
  }
  {
    auto result = ParseAv1CodecId("av01.1.00M.10.0.100");
    ASSERT_TRUE(result);
    EXPECT_EQ(result->subsampling, VideoChromaSampling::k444);
  }
  {
    auto result = ParseAv1CodecId("av01.0.00M.10.0.000");
    ASSERT_TRUE(result);
    EXPECT_EQ(result->subsampling, VideoChromaSampling::k420);
  }

  // The last-value may be non-zero if the first two values are non-zero.
  {
    auto result = ParseAv1CodecId("av01.0.00M.10.0.110");
    ASSERT_TRUE(result);
    EXPECT_EQ(result->subsampling, VideoChromaSampling::k420);
  }
  {
    auto result = ParseAv1CodecId("av01.2.00M.10.0.100");
    ASSERT_TRUE(result);
    EXPECT_EQ(result->subsampling, VideoChromaSampling::k422);
  }
  {
    auto result = ParseAv1CodecId("av01.0.00M.10.0.010");
    ASSERT_TRUE(result);
    EXPECT_EQ(result->subsampling, VideoChromaSampling::k420);
  }

  for (int i = 2; i <= 9; ++i) {
    for (int j = 2; j <= 9; ++j) {
      for (int k = 4; k <= 9; ++k) {
        const std::string codec_string =
            base::StringPrintf("av01.0.00M.08.0.%d%d%d", i, j, k);
        SCOPED_TRACE(codec_string);
        EXPECT_FALSE(ParseAv1CodecId(codec_string));
      }
    }
  }
}

TEST(ParseAv1CodecId, VerifyOptionalColorProperties) {
  // Verify a few color properties. This is non-exhaustive since validation is
  // handled by common color space function. Below we validate only portions
  // specific to the AV1 codec string.

  // Leading zeros must be provided.
  EXPECT_FALSE(ParseAv1CodecId("av01.0.00M.10.0.000.1"));
  // Negative values are not allowed.
  EXPECT_FALSE(ParseAv1CodecId("av01.0.00M.10.0.000.-1"));

  // BT709
  {
    auto result = ParseAv1CodecId("av01.0.00M.10.0.110.01");
    ASSERT_TRUE(result);
    EXPECT_EQ(VideoColorSpace::PrimaryID::BT709, result->color_space.primaries);
  }
  // BT2020
  {
    auto result = ParseAv1CodecId("av01.0.00M.10.0.110.09");
    ASSERT_TRUE(result);
    EXPECT_EQ(VideoColorSpace::PrimaryID::BT2020,
              result->color_space.primaries);
  }
  // 0 is invalid.
  EXPECT_FALSE(ParseAv1CodecId("av01.0.00M.10.0.110.00"));
  // 23 - 255 are reserved.
  EXPECT_FALSE(ParseAv1CodecId("av01.0.00M.10.0.110.23"));

  // Leading zeros must be provided.
  EXPECT_FALSE(ParseAv1CodecId("av01.0.00M.10.0.110.01.1"));
  // Negative values are not allowed.
  EXPECT_FALSE(ParseAv1CodecId("av01.0.00M.10.0.110.01.-1"));

  // Verify a few common EOTFs parse correctly.
  for (int eotf : {1, 4, 6, 14, 15, 13, 16}) {
    auto codec_string = base::StringPrintf("av01.0.00M.10.0.110.01.%02d", eotf);
    auto result = ParseAv1CodecId(codec_string);
    ASSERT_TRUE(result) << "eotf=" << eotf;
    EXPECT_EQ(static_cast<VideoColorSpace::TransferID>(eotf),
              result->color_space.transfer);
  }

  // Verify 0 and 3 are reserved EOTF values.
  EXPECT_FALSE(ParseAv1CodecId("av01.0.00M.10.0.110.01.00"));
  EXPECT_FALSE(ParseAv1CodecId("av01.0.00M.10.0.110.01.03"));

  // Leading zeros must be provided.
  EXPECT_FALSE(ParseAv1CodecId("av01.0.00M.10.0.110.01.01.1"));
  // Negative values are not allowed.
  EXPECT_FALSE(ParseAv1CodecId("av01.0.00M.10.0.110.01.01.-1"));

  // Verify a few matrix coefficients.
  {
    auto result = ParseAv1CodecId("av01.0.00M.10.0.110.01.01.00");
    ASSERT_TRUE(result);
    EXPECT_EQ(VideoColorSpace::MatrixID::RGB, result->color_space.matrix);
  }
  {
    auto result = ParseAv1CodecId("av01.0.00M.10.0.110.01.01.01");
    ASSERT_TRUE(result);
    EXPECT_EQ(VideoColorSpace::MatrixID::BT709, result->color_space.matrix);
  }
  {
    auto result = ParseAv1CodecId("av01.0.00M.10.0.110.01.01.10");
    ASSERT_TRUE(result);
    EXPECT_EQ(VideoColorSpace::MatrixID::BT2020_CL, result->color_space.matrix);
  }

  // Values 12 - 255 reserved. Though 12 at least is a valid value we should
  // support in the future. https://crbug.com/854290
  EXPECT_FALSE(ParseAv1CodecId("av01.0.00M.10.0.110.01.01.12"));

  // Leading zeros are not allowed.
  EXPECT_FALSE(ParseAv1CodecId("av01.0.00M.10.0.110.01.01.00.00"));
  // Negative values are not allowed.
  EXPECT_FALSE(ParseAv1CodecId("av01.0.00M.10.0.110.01.01.00.-1"));

  // Verify full range flag (boolean 0 or 1).
  {
    auto result = ParseAv1CodecId("av01.0.00M.10.0.110.01.01.00.0");
    ASSERT_TRUE(result);
    EXPECT_EQ(gfx::ColorSpace::RangeID::LIMITED, result->color_space.range);
  }
  {
    auto result = ParseAv1CodecId("av01.0.00M.10.0.110.01.01.00.1");
    ASSERT_TRUE(result);
    EXPECT_EQ(gfx::ColorSpace::RangeID::FULL, result->color_space.range);
  }
  EXPECT_FALSE(ParseAv1CodecId("av01.0.00M.10.0.110.01.01.00.2"));
}

TEST(ParseHEVCCodecIdTest, InvalidHEVCCodecIds) {
  // Both hev1 and hvc1 should be supported
  {
    auto result = ParseHEVCCodecId("hev1.1.6.L93.B0");
    ASSERT_TRUE(result);
    EXPECT_EQ(VideoCodec::kHEVC, result->codec);
    EXPECT_EQ(HEVCPROFILE_MAIN, result->profile);
    EXPECT_EQ(93u, result->level);
  }
  {
    auto result = ParseHEVCCodecId("hvc1.1.6.L93.B0");
    ASSERT_TRUE(result);
    EXPECT_EQ(HEVCPROFILE_MAIN, result->profile);
    EXPECT_EQ(93u, result->level);
  }

  // Check that codec id string with insufficient number of dot-separated
  // elements are rejected. There must be at least 4 elements: hev1/hvc1 prefix,
  // profile, profile_compatibility, tier+level.
  {
    auto result = ParseHEVCCodecId("hev1.1.6.L93");
    ASSERT_TRUE(result);
    EXPECT_EQ(HEVCPROFILE_MAIN, result->profile);
    EXPECT_EQ(93u, result->level);
  }
  EXPECT_FALSE(ParseHEVCCodecId("hvc1"));
  EXPECT_FALSE(ParseHEVCCodecId("hev1"));
  EXPECT_FALSE(ParseHEVCCodecId("hvc1."));
  EXPECT_FALSE(ParseHEVCCodecId("hvc1.."));
  EXPECT_FALSE(ParseHEVCCodecId("hvc1..."));
  EXPECT_FALSE(ParseHEVCCodecId("hvc1...."));
  EXPECT_FALSE(ParseHEVCCodecId("hvc1.1"));
  EXPECT_FALSE(ParseHEVCCodecId("hvc1.1."));
  EXPECT_FALSE(ParseHEVCCodecId("hvc1.1.."));
  EXPECT_FALSE(ParseHEVCCodecId("hvc1.1..."));
  EXPECT_FALSE(ParseHEVCCodecId("hvc1.1.6"));
  EXPECT_FALSE(ParseHEVCCodecId("hvc1.1.6."));
  EXPECT_FALSE(ParseHEVCCodecId("hvc1.1.6.."));
  EXPECT_FALSE(ParseHEVCCodecId("hvc1.1.6..."));
  EXPECT_FALSE(ParseHEVCCodecId("hvc1.1..L93"));
  EXPECT_FALSE(ParseHEVCCodecId("hvc1.1..L93."));
  EXPECT_FALSE(ParseHEVCCodecId("hvc1.1..L93.."));
  EXPECT_FALSE(ParseHEVCCodecId("hvc1..6"));
  EXPECT_FALSE(ParseHEVCCodecId("hvc1..6."));
  EXPECT_FALSE(ParseHEVCCodecId("hvc1..6.."));
  EXPECT_FALSE(ParseHEVCCodecId("hvc1..6..."));
  EXPECT_FALSE(ParseHEVCCodecId("hvc1..6.L93"));
  EXPECT_FALSE(ParseHEVCCodecId("hvc1..6.L93."));
  EXPECT_FALSE(ParseHEVCCodecId("hvc1..6.L93.."));

  // Check that codec ids with empty constraint bytes are rejected.
  EXPECT_FALSE(ParseHEVCCodecId("hvc1.1.6.L93."));
  EXPECT_FALSE(ParseHEVCCodecId("hvc1.1.6.L93.."));
  EXPECT_FALSE(ParseHEVCCodecId("hvc1.1.6.L93..."));
  EXPECT_FALSE(ParseHEVCCodecId("hvc1.1.6.L93...."));
  EXPECT_FALSE(ParseHEVCCodecId("hvc1.1.6.L93....."));
  EXPECT_FALSE(ParseHEVCCodecId("hvc1.1.6.L93......"));
  EXPECT_FALSE(ParseHEVCCodecId("hvc1.1.6.L93......."));
  EXPECT_FALSE(ParseHEVCCodecId("hvc1.1.6.L93.......0"));
  EXPECT_FALSE(ParseHEVCCodecId("hvc1.1.6.L93.0."));
  EXPECT_FALSE(ParseHEVCCodecId("hvc1.1.6.L93.0.."));
  EXPECT_FALSE(ParseHEVCCodecId("hvc1.1.6.L93.0..0"));
  EXPECT_FALSE(ParseHEVCCodecId("hvc1.1.6.L93.0..0.0.0.0.0"));
  EXPECT_FALSE(ParseHEVCCodecId("hvc1.1.6.L93.0.0.0.0.0.0."));

  // Different variations of general_profile_space (empty, 'A', 'B', 'C')
  EXPECT_TRUE(ParseHEVCCodecId("hvc1.1.6.L93.B0"));
  EXPECT_TRUE(ParseHEVCCodecId("hvc1.A1.6.L93.B0"));
  EXPECT_TRUE(ParseHEVCCodecId("hvc1.B1.6.L93.B0"));
  EXPECT_TRUE(ParseHEVCCodecId("hvc1.C1.6.L93.B0"));
  EXPECT_FALSE(ParseHEVCCodecId("hvc1.D1.6.L93.B0"));

  // general_profile_idc (the number after the first dot) must be a 5-bit
  // decimal-encoded number (between 0 and 31)
  EXPECT_TRUE(ParseHEVCCodecId("hvc1.0.6.L93.B0"));
  EXPECT_TRUE(ParseHEVCCodecId("hvc1.31.6.L93.B0"));

  // Spec A.3.2
  // When general_profile_compatibility_flag[1] is equal to 1,
  // general_profile_compatibility_flag[2] should be equal to 1 as well.
  for (const auto* codec_id :
       {"hvc1.1.6.L93.B0", "hvc1.1.0.L93.B0", "hvc1.0.6.L93.B0"}) {
    auto result = ParseHEVCCodecId(codec_id);
    ASSERT_TRUE(result);
    EXPECT_EQ(HEVCPROFILE_MAIN, result->profile);
  }

  // Spec A.3.3
  for (const auto* codec_id :
       {"hvc1.2.4.L93.B0", "hvc1.2.0.L93.B0", "hvc1.0.4.L93.B0"}) {
    auto result = ParseHEVCCodecId(codec_id);
    ASSERT_TRUE(result);
    EXPECT_EQ(HEVCPROFILE_MAIN10, result->profile);
  }

  // Spec A.3.4
  // When general_profile_compatibility_flag[3] is equal to 1,
  // general_profile_compatibility_flag[1] and
  // general_profile_compatibility_flag[2] should be equal to 1 as well.
  for (const auto* codec_id :
       {"hvc1.3.E.L93.B0", "hvc1.0.E.L93.B0", "hvc1.3.0.L93.B0"}) {
    auto result = ParseHEVCCodecId(codec_id);
    ASSERT_TRUE(result);
    EXPECT_EQ(HEVCPROFILE_MAIN_STILL_PICTURE, result->profile);
  }

  // Spec A.3.5
  for (const auto* codec_id :
       {"hvc1.4.10.L93.B0", "hvc1.4.0.L93.B0", "hvc1.0.10.L93.B0"}) {
    auto result = ParseHEVCCodecId(codec_id);
    ASSERT_TRUE(result);
    EXPECT_EQ(HEVCPROFILE_REXT, result->profile);
  }

  // Spec A.3.6
  for (const auto* codec_id :
       {"hvc1.5.20.L93.B0", "hvc1.5.0.L93.B0", "hvc1.0.20.L93.B0"}) {
    auto result = ParseHEVCCodecId(codec_id);
    ASSERT_TRUE(result);
    EXPECT_EQ(HEVCPROFILE_HIGH_THROUGHPUT, result->profile);
  }

  // Spec G.11.1.1
  for (const auto* codec_id :
       {"hvc1.6.40.L93.B0", "hvc1.6.0.L93.B0", "hvc1.0.40.L93.B0"}) {
    auto result = ParseHEVCCodecId(codec_id);
    ASSERT_TRUE(result);
    EXPECT_EQ(HEVCPROFILE_MULTIVIEW_MAIN, result->profile);
  }

  // Spec H.11.1.1
  for (const auto* codec_id :
       {"hvc1.7.80.L93.B0", "hvc1.7.0.L93.B0", "hvc1.0.80.L93.B0"}) {
    auto result = ParseHEVCCodecId(codec_id);
    ASSERT_TRUE(result);
    EXPECT_EQ(HEVCPROFILE_SCALABLE_MAIN, result->profile);
  }

  // Spec I.11.1.1
  for (const auto* codec_id :
       {"hvc1.8.100.L93.B0", "hvc1.8.0.L93.B0", "hvc1.0.100.L93.B0"}) {
    auto result = ParseHEVCCodecId(codec_id);
    ASSERT_TRUE(result);
    EXPECT_EQ(HEVCPROFILE_3D_MAIN, result->profile);
  }

  // Spec A.3.7
  for (const auto* codec_id :
       {"hvc1.9.200.L93.B0", "hvc1.9.0.L93.B0", "hvc1.0.200.L93.B0"}) {
    auto result = ParseHEVCCodecId(codec_id);
    ASSERT_TRUE(result);
    EXPECT_EQ(HEVCPROFILE_SCREEN_EXTENDED, result->profile);
  }

  // Spec H.11.1.2
  for (const auto* codec_id :
       {"hvc1.10.400.L93.B0", "hvc1.10.0.L93.B0", "hvc1.0.400.L93.B0"}) {
    auto result = ParseHEVCCodecId(codec_id);
    ASSERT_TRUE(result);
    EXPECT_EQ(HEVCPROFILE_SCALABLE_REXT, result->profile);
  }

  // Spec A.3.8
  for (const auto* codec_id :
       {"hvc1.11.800.L93.B0", "hvc1.11.0.L93.B0", "hvc1.0.800.L93.B0"}) {
    auto result = ParseHEVCCodecId(codec_id);
    ASSERT_TRUE(result);
    EXPECT_EQ(HEVCPROFILE_HIGH_THROUGHPUT_SCREEN_EXTENDED, result->profile);
  }

  // Unmatched general_profile_idc and general_profile_compatibility_flags
  EXPECT_FALSE(ParseHEVCCodecId("hvc1.12.1000.L93.B0"));
  EXPECT_FALSE(ParseHEVCCodecId("hvc1.12.0.L93.B0"));
  EXPECT_FALSE(ParseHEVCCodecId("hvc1.0.1000.L93.B0"));

  EXPECT_FALSE(ParseHEVCCodecId("hvc1.-1.6.L93.B0"));
  EXPECT_FALSE(ParseHEVCCodecId("hvc1.32.6.L93.B0"));
  EXPECT_FALSE(ParseHEVCCodecId("hvc1.999.6.L93.B0"));
  EXPECT_FALSE(ParseHEVCCodecId("hvc1.A.6.L93.B0"));
  EXPECT_FALSE(ParseHEVCCodecId("hvc1.1F.6.L93.B0"));

  // general_profile_compatibility_flags is a 32-bit hex number
  EXPECT_TRUE(ParseHEVCCodecId("hvc1.1.0.L93.B0"));
  EXPECT_TRUE(ParseHEVCCodecId("hvc1.1.FF.L93.B0"));
  EXPECT_TRUE(ParseHEVCCodecId("hvc1.1.FFFF.L93.B0"));
  EXPECT_TRUE(ParseHEVCCodecId("hvc1.1.FFFFFFFF.L93.B0"));
  EXPECT_FALSE(ParseHEVCCodecId("hvc1.1.100000000.L93.B0"));
  EXPECT_FALSE(ParseHEVCCodecId("hvc1.1.FFFFFFFFF.L93.B0"));
  EXPECT_FALSE(ParseHEVCCodecId("hvc1.1.-1.L93.B0"));
  EXPECT_FALSE(ParseHEVCCodecId("hvc1.1.0G.L93.B0"));

  // general_tier_flag is encoded as either character 'L' (general_tier_flag==0)
  // or character 'H' (general_tier_flag==1) in the fourth element of the string
  EXPECT_TRUE(ParseHEVCCodecId("hvc1.1.6.L93.B0"));
  EXPECT_TRUE(ParseHEVCCodecId("hvc1.1.0.H93.B0"));
  EXPECT_FALSE(ParseHEVCCodecId("hvc1.1.0.93.B0"));
  EXPECT_FALSE(ParseHEVCCodecId("hvc1.1.0.A93.B0"));

  // general_level_idc is 8-bit decimal-encoded number after general_tier_flag.
  {
    auto result = ParseHEVCCodecId("hvc1.1.6.L0.B0");
    ASSERT_TRUE(result);
    EXPECT_EQ(0u, result->level);
  }
  {
    auto result = ParseHEVCCodecId("hvc1.1.6.L1.B0");
    ASSERT_TRUE(result);
    EXPECT_EQ(1u, result->level);
  }
  // Level 3.1 (93 == 3.1 * 30)
  {
    auto result = ParseHEVCCodecId("hvc1.1.6.L93.B0");
    ASSERT_TRUE(result);
    EXPECT_EQ(93u, result->level);
  }
  // Level 5 (150 == 5 * 30)
  {
    auto result = ParseHEVCCodecId("hvc1.1.6.L150.B0");
    ASSERT_TRUE(result);
    EXPECT_EQ(150u, result->level);
  }
  {
    auto result = ParseHEVCCodecId("hvc1.1.6.L255.B0");
    ASSERT_TRUE(result);
    EXPECT_EQ(255u, result->level);
  }
  EXPECT_FALSE(ParseHEVCCodecId("hvc1.1.6.L256.B0"));
  EXPECT_FALSE(ParseHEVCCodecId("hvc1.1.6.L999.B0"));
  EXPECT_FALSE(ParseHEVCCodecId("hvc1.1.6.L-1.B0"));

  // The elements after the fourth dot are hex-encoded bytes containing
  // constraint flags (up to 6 bytes), trailing zero bytes may be omitted
  EXPECT_TRUE(ParseHEVCCodecId("hvc1.1.6.L0.0.0.0.0.0.0"));
  EXPECT_TRUE(ParseHEVCCodecId("hvc1.1.6.L0.00.00.00.00.00.00"));
  EXPECT_TRUE(ParseHEVCCodecId("hvc1.1.6.L0.12"));
  EXPECT_TRUE(ParseHEVCCodecId("hvc1.1.6.L0.12.34.56"));
  EXPECT_TRUE(ParseHEVCCodecId("hvc1.1.6.L0.12.34.56.78.9A.BC"));
  EXPECT_TRUE(ParseHEVCCodecId("hvc1.1.6.L0.FF.FF.FF.FF.FF.FF"));
  EXPECT_FALSE(ParseHEVCCodecId("hvc1.1.6.L0.FF.FF.FF.FF.FF.FF.0"));
  EXPECT_FALSE(ParseHEVCCodecId("hvc1.1.6.L0.100"));
  EXPECT_FALSE(ParseHEVCCodecId("hvc1.1.6.L0.1FF"));
  EXPECT_FALSE(ParseHEVCCodecId("hvc1.1.6.L0.-1"));
}

TEST(ParseVVCCodecIdTest, InvalidVVCCodecIds) {
  // Both vvc1 and vvi1 should be supported
  {
    auto result = ParseVVCCodecId("vvc1.1.L51.CQA.O1+3");
    ASSERT_TRUE(result);
    EXPECT_EQ(VVCPROFILE_MAIN10, result->profile);
    EXPECT_EQ(51u, result->level);
  }
  {
    auto result = ParseVVCCodecId("vvi1.2.L83.CQA.S25+YA.O2+3");
    ASSERT_TRUE(result);
    EXPECT_EQ(VVCPROFILE_MAIN12, result->profile);
    EXPECT_EQ(83u, result->level);
  }

  // Check that codec id string with insufficient number of dot-separated
  // elements are rejected. There must be at least 4 elements: vvc1/vvi1 prefix,
  // profile, level, constraints.
  {
    auto result = ParseVVCCodecId("vvc1.1.L51.CQA");
    ASSERT_TRUE(result);
    EXPECT_EQ(VVCPROFILE_MAIN10, result->profile);
    EXPECT_EQ(51u, result->level);
  }
  EXPECT_FALSE(ParseVVCCodecId("vvc1"));
  EXPECT_FALSE(ParseVVCCodecId("vvi1"));
  EXPECT_FALSE(ParseVVCCodecId("vvc1."));
  EXPECT_FALSE(ParseVVCCodecId("vvc1.."));
  EXPECT_FALSE(ParseVVCCodecId("vvc1..."));
  EXPECT_FALSE(ParseVVCCodecId("vvc1...."));
  EXPECT_FALSE(ParseVVCCodecId("vvc1.1"));
  EXPECT_FALSE(ParseVVCCodecId("vvc1.1."));
  EXPECT_FALSE(ParseVVCCodecId("vvc1.1.."));
  EXPECT_FALSE(ParseVVCCodecId("vvc1.1..."));

  // Check that codec ids with invalid trailing bytes are rejected.
  EXPECT_FALSE(ParseVVCCodecId("vvc1.1.L83."));
  EXPECT_FALSE(ParseVVCCodecId("vvc1.1.L83.."));
  EXPECT_FALSE(ParseVVCCodecId("vvc1.1.L83..."));
  EXPECT_FALSE(ParseVVCCodecId("vvc1.1.L83...."));
  EXPECT_FALSE(ParseVVCCodecId("vvc1.1.L83....."));
  EXPECT_FALSE(ParseVVCCodecId("vvc1.1.L83......"));
  EXPECT_FALSE(ParseVVCCodecId("vvc1.1.L83......."));
  EXPECT_FALSE(ParseVVCCodecId("vvc1.1.L83.......0"));
  EXPECT_FALSE(ParseVVCCodecId("vvc1.1.L83.0."));
  EXPECT_FALSE(ParseVVCCodecId("vvc1.1.L83.0.."));
  EXPECT_FALSE(ParseVVCCodecId("vvc1.1.L83.0..0"));

  // general_profile_idc (the number after the first dot) must be a 5-bit
  // decimal-encoded number (between 1 and 99)
  EXPECT_TRUE(ParseVVCCodecId("vvc1.1.L83.CQA"));
  EXPECT_TRUE(ParseVVCCodecId("vvc1.99.L83.CQA"));
  EXPECT_FALSE(ParseVVCCodecId("vvc1.100.L83.CQA"));
  EXPECT_FALSE(ParseVVCCodecId("vvc1.0.L83.CQA"));

  // general_tier_flag is encoded as either character 'L' (general_tier_flag==0)
  // or character 'H' (general_tier_flag==1) in the 3rd element of the string
  EXPECT_TRUE(ParseVVCCodecId("vvc1.1.L83.CQA"));
  EXPECT_TRUE(ParseVVCCodecId("vvc1.1.H83.CQA"));
  EXPECT_FALSE(ParseVVCCodecId("vvc1.1.83.CQA"));
  EXPECT_FALSE(ParseVVCCodecId("vvc1.1.A83.CQA"));

  // general_level_idc is 8-bit decimal-encoded number after general_tier_flag.
  {
    auto result = ParseVVCCodecId("vvc1.1.L0.CQA");
    ASSERT_TRUE(result);
    EXPECT_EQ(0u, result->level);
  }
  {
    auto result = ParseVVCCodecId("vvc1.1.L1.CQA");
    ASSERT_TRUE(result);
    EXPECT_EQ(1u, result->level);
  }
  // Level 3.1 (51 == 3 * 16 + 1 * 3)
  {
    auto result = ParseVVCCodecId("vvc1.1.L51.CQA");
    ASSERT_TRUE(result);
    EXPECT_EQ(51u, result->level);
  }
  // Level 6.2 (102 == 6 * 16 + 2 * 3)
  {
    auto result = ParseVVCCodecId("vvc1.1.L102.CYA");
    ASSERT_TRUE(result);
    EXPECT_EQ(102u, result->level);
  }
  {
    auto result = ParseVVCCodecId("vvc1.1.L255.CYA");
    ASSERT_TRUE(result);
    EXPECT_EQ(255u, result->level);
  }
  EXPECT_FALSE(ParseVVCCodecId("vvc1.1.L256.CYA"));
  EXPECT_FALSE(ParseVVCCodecId("vvc1.1.L999.CQA"));
  EXPECT_FALSE(ParseVVCCodecId("vvc1.1.L-1.CQA"));

  // constraints string
  EXPECT_FALSE(ParseVVCCodecId("vvc1.100.L83.C."));
  EXPECT_FALSE(ParseVVCCodecId("vvc1.100.L83.2C"));

  // general_sub_profile_idc placement.
  EXPECT_TRUE(ParseVVCCodecId("vvc1.1.L0.CQA.SF1.O0+3"));
  EXPECT_TRUE(ParseVVCCodecId("vvc1.1.L0.CQA.SF1"));
  EXPECT_TRUE(ParseVVCCodecId("vvc1.1.L0.SF1"));
  EXPECT_FALSE(ParseVVCCodecId("vvc1.1.L0.CQA.SF1."));
  EXPECT_TRUE(ParseVVCCodecId("vvc1.1.L0.CQA.SF1+AB.O0+3"));
  EXPECT_TRUE(ParseVVCCodecId("vvc1.1.L0.CQA.SF1+AB+2B.O0+3"));
  EXPECT_FALSE(ParseVVCCodecId("vvc1.1.L0.SF1.CQA.O0+3"));
  EXPECT_FALSE(ParseVVCCodecId("vvc1.1.L0.CQA.O0+3.SF1"));
  EXPECT_FALSE(ParseVVCCodecId("vvc1.1.L0.CQA.O0+3.S"));
  EXPECT_FALSE(ParseVVCCodecId("vvc1.1.L0.CQA.O0+3.S."));

  // OlsIdx & MaxTid
  EXPECT_TRUE(ParseVVCCodecId("vvc1.1.L0.CQA.O0+3"));
  EXPECT_TRUE(ParseVVCCodecId("vvc1.1.L0.CQA.O1"));
  // When MaxTid does not exist, "+" should not be present.
  EXPECT_FALSE(ParseVVCCodecId("vvc1.1.L0.CQA.O1+"));
  EXPECT_FALSE(ParseVVCCodecId("vvc1.1.L0.CQA.O"));
  EXPECT_TRUE(ParseVVCCodecId("vvc1.1.L0.CQA.O+3"));

  EXPECT_FALSE(ParseVVCCodecId("vvc1.1.L83.100"));
  EXPECT_FALSE(ParseVVCCodecId("vvc1.1.L83.1FF"));
  EXPECT_FALSE(ParseVVCCodecId("vvc1.1.L83.-1"));
  EXPECT_FALSE(ParseVVCCodecId("vvc1.1.L0.CQA.SF1.O0+3.100"));
}

TEST(ParseDolbyVisionCodecIdTest, InvalidDolbyVisionCodecIds) {
  // Codec dvav/dva1 should only contain profile 0.
  {
    auto result = ParseDolbyVisionCodecId("dvav.00.07");
    ASSERT_TRUE(result);
    EXPECT_EQ(VideoCodec::kDolbyVision, result->codec);
    EXPECT_EQ(DOLBYVISION_PROFILE0, result->profile);
    EXPECT_EQ(7u, result->level);
  }
  {
    auto result = ParseDolbyVisionCodecId("dva1.00.07");
    ASSERT_TRUE(result);
    EXPECT_EQ(DOLBYVISION_PROFILE0, result->profile);
    EXPECT_EQ(7u, result->level);
  }
  EXPECT_FALSE(ParseDolbyVisionCodecId("dvav.04.07"));
  EXPECT_FALSE(ParseDolbyVisionCodecId("dva1.04.07"));
  EXPECT_FALSE(ParseDolbyVisionCodecId("dvav.05.07"));
  EXPECT_FALSE(ParseDolbyVisionCodecId("dva1.05.07"));
  EXPECT_FALSE(ParseDolbyVisionCodecId("dvav.07.07"));
  EXPECT_FALSE(ParseDolbyVisionCodecId("dva1.07.07"));

  // Codec dvhe/dvh1 should only contain profile 5, and 7.
  {
    auto result = ParseDolbyVisionCodecId("dvhe.05.07");
    ASSERT_TRUE(result);
    EXPECT_EQ(DOLBYVISION_PROFILE5, result->profile);
    EXPECT_EQ(7u, result->level);
  }
  {
    auto result = ParseDolbyVisionCodecId("dvh1.05.07");
    ASSERT_TRUE(result);
    EXPECT_EQ(DOLBYVISION_PROFILE5, result->profile);
    EXPECT_EQ(7u, result->level);
  }
  {
    auto result = ParseDolbyVisionCodecId("dvhe.07.07");
    ASSERT_TRUE(result);
    EXPECT_EQ(DOLBYVISION_PROFILE7, result->profile);
    EXPECT_EQ(7u, result->level);
  }
  EXPECT_FALSE(ParseDolbyVisionCodecId("dvhe.00.07"));
  EXPECT_FALSE(ParseDolbyVisionCodecId("dvh1.00.07"));

  // Profiles 1, 2, 3, 4 and 6 are deprecated.
  EXPECT_FALSE(ParseDolbyVisionCodecId("dvav.01.07"));
  EXPECT_FALSE(ParseDolbyVisionCodecId("dvhe.02.07"));
  EXPECT_FALSE(ParseDolbyVisionCodecId("dvhe.03.07"));
  EXPECT_FALSE(ParseDolbyVisionCodecId("dvhe.04.07"));
  EXPECT_FALSE(ParseDolbyVisionCodecId("dvhe.06.07"));

  // Level should be two digit number and in the range [01, 13].
  EXPECT_FALSE(ParseDolbyVisionCodecId("dvhe.05.00"));

  for (uint32_t level : {1, 9, 10, 13}) {
    auto codec_id = base::StringPrintf("dvhe.05.%02d", level);
    auto result = ParseDolbyVisionCodecId(codec_id);
    ASSERT_TRUE(result);
    EXPECT_EQ(DOLBYVISION_PROFILE5, result->profile);
    EXPECT_EQ(level, result->level);
  }

  EXPECT_FALSE(ParseDolbyVisionCodecId("dvhe.05.14"));
  EXPECT_FALSE(ParseDolbyVisionCodecId("dvhe.05.20"));
  EXPECT_FALSE(ParseDolbyVisionCodecId("dvhe.05.99"));

  // Valid codec string is <FourCC>.<two digits profile>.<two digits level>.
  EXPECT_FALSE(ParseDolbyVisionCodecId("dvhe"));
  EXPECT_FALSE(ParseDolbyVisionCodecId("dvhe."));
  EXPECT_FALSE(ParseDolbyVisionCodecId("dvhe.."));
  EXPECT_FALSE(ParseDolbyVisionCodecId("dvhe..."));
  EXPECT_FALSE(ParseDolbyVisionCodecId("dvhe...."));
  EXPECT_FALSE(ParseDolbyVisionCodecId("dvhe.5"));
  EXPECT_FALSE(ParseDolbyVisionCodecId("dvhe.5."));
  EXPECT_FALSE(ParseDolbyVisionCodecId("dvhe.5.."));
  EXPECT_FALSE(ParseDolbyVisionCodecId("dvhe.5..."));
  EXPECT_FALSE(ParseDolbyVisionCodecId("dvhe.5.7"));
  EXPECT_FALSE(ParseDolbyVisionCodecId("dvhe.5.7."));
  EXPECT_FALSE(ParseDolbyVisionCodecId("dvhe.5.7.."));
  EXPECT_FALSE(ParseDolbyVisionCodecId("dvhe.5.7..."));
  EXPECT_FALSE(ParseDolbyVisionCodecId("dvhe.05.7"));
  EXPECT_FALSE(ParseDolbyVisionCodecId("dvhe.05.007"));
  EXPECT_FALSE(ParseDolbyVisionCodecId("dvhe..5"));
}

}  // namespace media
