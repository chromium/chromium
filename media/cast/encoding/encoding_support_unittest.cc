// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/encoding/encoding_support.h"

#include <stdint.h>

#include "base/no_destructor.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "base/cpu.h"  // nogncheck
#endif

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

TEST(EncodingSupportTest, EnablesVp8HardwareEncoderProperly) {
  constexpr bool is_enabled =
#if BUILDFLAG(IS_CHROMEOS)
      false;
#else
      true;
#endif

  EXPECT_EQ(is_enabled, IsHardwareEnabled(CODEC_VIDEO_VP8, GetValidProfiles()));
}

TEST(EncodingSupportTest, EnablesH264HardwareEncoderProperly) {
#if BUILDFLAG(IS_CHROMEOS)
  static const base::NoDestructor<base::CPU> cpuid;
  static const bool is_amd = cpuid->vendor_name() == "AuthenticAMD";
#endif

  static const bool is_enabled =
// On ChromeOS only, disable hardware encoder on AMD chipsets due to
// failure on Chromecast chipsets to decode.
#if BUILDFLAG(IS_CHROMEOS)
      !is_amd;
// The hardware encoder also has major issues on Mac OSX and on Windows.
#elif BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_WIN)
      false;
#else
      true;
#endif

  EXPECT_EQ(is_enabled,
            IsHardwareEnabled(CODEC_VIDEO_H264, GetValidProfiles()));
}

}  // namespace media::cast::encoding_support
