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
  EXPECT_TRUE(IsHardwareEnabled(VideoCodec::kVP8, GetValidProfiles()));
}

TEST(EncodingSupportTest, DenyListedHardwareEncoderNotOffered) {
  EXPECT_TRUE(IsHardwareEnabled(VideoCodec::kVP8, GetValidProfiles()));
  DenyListHardwareCodec(VideoCodec::kVP8);
  EXPECT_FALSE(IsHardwareEnabled(VideoCodec::kVP8, GetValidProfiles()));
}

TEST(EncodingSupportTest, EnablesH264HardwareEncoderProperly) {
#if BUILDFLAG(IS_MAC)
  base::test::ScopedFeatureList feature(kCastStreamingMacHardwareH264);
  EXPECT_TRUE(IsHardwareEnabled(VideoCodec::kH264, GetValidProfiles()));

  feature.Reset();
  feature.InitAndDisableFeature(kCastStreamingMacHardwareH264);
  EXPECT_FALSE(IsHardwareEnabled(VideoCodec::kH264, GetValidProfiles()));
#elif BUILDFLAG(IS_WIN)
  base::test::ScopedFeatureList feature(kCastStreamingWinHardwareH264);
  EXPECT_TRUE(IsHardwareEnabled(VideoCodec::kH264, GetValidProfiles()));

  feature.Reset();
  feature.InitAndDisableFeature(kCastStreamingWinHardwareH264);
  EXPECT_FALSE(IsHardwareEnabled(VideoCodec::kH264, GetValidProfiles()));
#else
  EXPECT_EQ(true, IsHardwareEnabled(VideoCodec::kH264, GetValidProfiles()));
#endif
}

}  // namespace media::cast::encoding_support
