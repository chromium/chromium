// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/encoding/encoding_support.h"

#include <stdint.h>

#include "base/no_destructor.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "media/base/media_switches.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media::cast::encoding_support {
namespace {

std::vector<media::VideoEncodeAccelerator::SupportedProfile>
GetValidProfiles() {
  static const base::NoDestructor<
      std::vector<media::VideoEncodeAccelerator::SupportedProfile>>
      kValidProfiles({
          VideoEncodeAccelerator::SupportedProfile(media::VP8PROFILE_MIN,
                                                   gfx::Size(1920, 1080)),
          VideoEncodeAccelerator::SupportedProfile(media::H264PROFILE_MIN,
                                                   gfx::Size(1920, 1080)),
      });

  return *kValidProfiles;
}

}  // namespace

TEST(EncodingSupportTest, EnablesVp8HardwareEncoderAlways) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(kCastStreamingVp8);
  EXPECT_TRUE(IsHardwareEnabled(VideoCodec::kVP8, GetValidProfiles(),
                                gfx::Size(1920, 1080), 30.0));
}

TEST(EncodingSupportTest, DenyListedHardwareEncoderNotOffered) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(kCastStreamingVp8);
  EXPECT_TRUE(IsHardwareEnabled(VideoCodec::kVP8, GetValidProfiles(),
                                gfx::Size(1920, 1080), 30.0));
  EXPECT_FALSE(IsHardwareDenyListed(VideoCodec::kVP8));
  DenyListHardwareCodec(VideoCodec::kVP8);
  EXPECT_FALSE(IsHardwareEnabled(VideoCodec::kVP8, GetValidProfiles(),
                                 gfx::Size(1920, 1080), 30.0));
  EXPECT_TRUE(IsHardwareDenyListed(VideoCodec::kVP8));
}

TEST(EncodingSupportTest, EnablesH264HardwareEncoderProperly) {
#if BUILDFLAG(IS_MAC)
  base::test::ScopedFeatureList feature(kCastStreamingMacHardwareH264);
  EXPECT_TRUE(IsHardwareEnabled(VideoCodec::kH264, GetValidProfiles(),
                                gfx::Size(1920, 1080), 30.0));

  feature.Reset();
  feature.InitAndDisableFeature(kCastStreamingMacHardwareH264);
  EXPECT_FALSE(IsHardwareEnabled(VideoCodec::kH264, GetValidProfiles(),
                                 gfx::Size(1920, 1080), 30.0));
#elif BUILDFLAG(IS_WIN)
  base::test::ScopedFeatureList feature(kCastStreamingWinHardwareH264);
  EXPECT_TRUE(IsHardwareEnabled(VideoCodec::kH264, GetValidProfiles(),
                                gfx::Size(1920, 1080), 30.0));

  feature.Reset();
  feature.InitAndDisableFeature(kCastStreamingWinHardwareH264);
  EXPECT_FALSE(IsHardwareEnabled(VideoCodec::kH264, GetValidProfiles(),
                                 gfx::Size(1920, 1080), 30.0));
#else
  EXPECT_EQ(true, IsHardwareEnabled(VideoCodec::kH264, GetValidProfiles(),
                                    gfx::Size(1920, 1080), 30.0));
#endif
}

TEST(EncodingSupportTest, GetCodecParameterString) {
  const gfx::Size k1080p{1920, 1080};

  EXPECT_EQ(GetCodecParameterString(VideoCodec::kVP8, k1080p, 30.0), "");
  EXPECT_EQ(GetCodecParameterString(VideoCodec::kUnknown, k1080p, 30.0), "");

  // 1080p30 tests.
  EXPECT_EQ(GetCodecParameterString(VideoCodec::kVP9, k1080p, 30.0),
            "vp09.00.40.08");
  EXPECT_EQ(GetCodecParameterString(VideoCodec::kHEVC, k1080p, 30.0),
            "hev1.1.6.L120.B0");
  EXPECT_EQ(GetCodecParameterString(VideoCodec::kAV1, k1080p, 30.0),
            "av01.0.08M.08");
  EXPECT_EQ(GetCodecParameterString(VideoCodec::kH264, k1080p, 30.0),
            "avc1.4d0028");

  // 1080p60 tests.
  EXPECT_EQ(GetCodecParameterString(VideoCodec::kVP9, k1080p, 60.0),
            "vp09.00.41.08");
  EXPECT_EQ(GetCodecParameterString(VideoCodec::kHEVC, k1080p, 60.0),
            "hev1.1.6.L123.B0");
  EXPECT_EQ(GetCodecParameterString(VideoCodec::kAV1, k1080p, 60.0),
            "av01.0.09M.08");
  EXPECT_EQ(GetCodecParameterString(VideoCodec::kH264, k1080p, 60.0),
            "avc1.4d002a");
}

}  // namespace media::cast::encoding_support
