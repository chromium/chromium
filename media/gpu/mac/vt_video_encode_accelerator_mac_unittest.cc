// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/mac/vt_video_encode_accelerator_mac.h"

#include <memory>
#include <vector>

#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "media/base/media_switches.h"
#include "media/base/video_codecs.h"
#include "media/base/video_types.h"
#include "media/media_buildflags.h"
#include "media/video/video_encode_accelerator.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

TEST(VTVideoEncodeAcceleratorTest, CalculatePsnr_PerfectMatch) {
  // MSE == 0 should return the max cap of 128.0 dB.
  EXPECT_DOUBLE_EQ(
      VTVideoEncodeAccelerator::CalculatePsnrForTesting(0.0, PIXEL_FORMAT_I420),
      128.0);
  EXPECT_DOUBLE_EQ(
      VTVideoEncodeAccelerator::CalculatePsnrForTesting(0.0, PIXEL_FORMAT_NV12),
      128.0);
  EXPECT_DOUBLE_EQ(VTVideoEncodeAccelerator::CalculatePsnrForTesting(
                       0.0, PIXEL_FORMAT_P010LE),
                   128.0);
}

TEST(VTVideoEncodeAcceleratorTest, CalculatePsnr_8Bit) {
  // Test PSNR calculations for 8-bit formats (max_value = 255).
  // PSNR = 10 * log10(255^2 / 1) = 48.1308 dB.
  EXPECT_NEAR(
      VTVideoEncodeAccelerator::CalculatePsnrForTesting(1.0, PIXEL_FORMAT_I420),
      48.13, 0.01);
  EXPECT_NEAR(
      VTVideoEncodeAccelerator::CalculatePsnrForTesting(1.0, PIXEL_FORMAT_NV12),
      48.13, 0.01);

  // PSNR = 10 * log10(255^2 / 100) = 28.1308 dB.
  EXPECT_NEAR(VTVideoEncodeAccelerator::CalculatePsnrForTesting(
                  100.0, PIXEL_FORMAT_I420),
              28.13, 0.01);
  EXPECT_NEAR(VTVideoEncodeAccelerator::CalculatePsnrForTesting(
                  100.0, PIXEL_FORMAT_NV12),
              28.13, 0.01);
}

TEST(VTVideoEncodeAcceleratorTest, CalculatePsnr_10Bit) {
  // Test PSNR calculations for 10-bit formats (max_value = 1023).
  // PSNR = 10 * log10(1023^2 / 1) = 60.1975 dB.
  EXPECT_NEAR(VTVideoEncodeAccelerator::CalculatePsnrForTesting(
                  1.0, PIXEL_FORMAT_P010LE),
              60.20, 0.01);

  // PSNR = 10 * log10(1023^2 / 100) = 40.1975 dB.
  EXPECT_NEAR(VTVideoEncodeAccelerator::CalculatePsnrForTesting(
                  100.0, PIXEL_FORMAT_P010LE),
              40.20, 0.01);
}

TEST(VTVideoEncodeAcceleratorTest, CalculatePsnr_CapMaxPSNR) {
  // Extremely small MSE should be capped at 128.0 dB (lossless).
  EXPECT_DOUBLE_EQ(VTVideoEncodeAccelerator::CalculatePsnrForTesting(
                       1e-9, PIXEL_FORMAT_I420),
                   128.0);
  EXPECT_DOUBLE_EQ(VTVideoEncodeAccelerator::CalculatePsnrForTesting(
                       1e-9, PIXEL_FORMAT_NV12),
                   128.0);
  EXPECT_DOUBLE_EQ(VTVideoEncodeAccelerator::CalculatePsnrForTesting(
                       1e-9, PIXEL_FORMAT_P010LE),
                   128.0);
}

TEST(VTVideoEncodeAcceleratorTest, SupportedProfilesAdvertiseGpuFormats) {
  base::test::ScopedFeatureList scoped_feature_list;
#if BUILDFLAG(ENABLE_PLATFORM_HEVC)
  scoped_feature_list.InitWithFeatures(
      {kVTVideoEncodeAcceleratorOpaqueSharedImageEncode,
       kPlatformHEVCEncoderSupport, kPlatformHEVCHbdEncoderSupport},
      {});
#else
  scoped_feature_list.InitAndEnableFeature(
      kVTVideoEncodeAcceleratorOpaqueSharedImageEncode);
#endif
  base::test::TaskEnvironment task_environment;
  std::unique_ptr<VideoEncodeAccelerator> encoder(
      new VTVideoEncodeAccelerator());
  auto profiles = encoder->GetSupportedProfiles();
  ASSERT_FALSE(profiles.empty());

  bool saw_nv12 = false;
  for (const auto& profile : profiles) {
    ASSERT_FALSE(profile.gpu_supported_pixel_formats.empty());
    // HW and SW VT sessions share the same CVPixelBuffer input path.
    EXPECT_TRUE(profile.supports_gpu_shared_images);
    if (profile.profile == HEVCPROFILE_MAIN10) {
      EXPECT_EQ(profile.gpu_supported_pixel_formats,
                std::vector<VideoPixelFormat>({PIXEL_FORMAT_P010LE}));
    } else if (profile.profile == HEVCPROFILE_REXT) {
      ASSERT_EQ(profile.gpu_supported_pixel_formats.size(), 1u);
      const VideoPixelFormat dest = profile.gpu_supported_pixel_formats[0];
      EXPECT_TRUE(dest == PIXEL_FORMAT_NV16 || dest == PIXEL_FORMAT_NV24 ||
                  dest == PIXEL_FORMAT_P210LE || dest == PIXEL_FORMAT_P410LE);
    } else {
      saw_nv12 = true;
      EXPECT_EQ(profile.gpu_supported_pixel_formats,
                std::vector<VideoPixelFormat>({PIXEL_FORMAT_NV12}));
    }
  }
  EXPECT_TRUE(saw_nv12);
}

#if BUILDFLAG(ENABLE_PLATFORM_HEVC) && \
    BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)
TEST(VTVideoEncodeAcceleratorTest, AdvertisesHevcRextOnlyOnAppleSilicon) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {kVTVideoEncodeAcceleratorOpaqueSharedImageEncode,
       kPlatformHEVCEncoderSupport, kPlatformHEVCHbdEncoderSupport},
      {});
  base::test::TaskEnvironment task_environment;
  std::unique_ptr<VideoEncodeAccelerator> encoder(
      new VTVideoEncodeAccelerator());
  auto profiles = encoder->GetSupportedProfiles();

  std::vector<const VideoEncodeAccelerator::SupportedProfile*> rext_hw;
  std::vector<const VideoEncodeAccelerator::SupportedProfile*> rext_sw;
  for (const auto& profile : profiles) {
    if (profile.profile != HEVCPROFILE_REXT) {
      continue;
    }
    if (profile.is_software_codec) {
      rext_sw.push_back(&profile);
    } else {
      rext_hw.push_back(&profile);
    }
  }
  EXPECT_TRUE(rext_sw.empty());
#if defined(ARCH_CPU_X86_FAMILY)
  // Intel VideoToolbox does not get HEVCPROFILE_REXT in the compile-time list.
  EXPECT_TRUE(rext_hw.empty());
#else
  // Apple Silicon compiles RExt into GetSupportedVideoCodecProfiles(), but
  // each combo is only advertised if CanCreateHardwareCompressionSession()
  // succeeds. CQ/swarming Macs often cannot create those HW sessions.
  if (rext_hw.empty()) {
    GTEST_SKIP() << "HEVC RExt hardware encode is not available";
  }
  // Four combos × min-resolution variants × portrait copies. At least the four
  // session formats must appear when hardware encode is present.
  bool saw_nv16 = false, saw_nv24 = false, saw_p210 = false, saw_p410 = false;
  for (const auto* profile : rext_hw) {
    ASSERT_TRUE(profile->chroma_sampling.has_value());
    ASSERT_TRUE(profile->bit_depth.has_value());
    ASSERT_EQ(profile->gpu_supported_pixel_formats.size(), 1u);
    const VideoPixelFormat dest = profile->gpu_supported_pixel_formats[0];
    if (*profile->chroma_sampling == VideoChromaSampling::k422 &&
        *profile->bit_depth == 8) {
      saw_nv16 = true;
      EXPECT_EQ(dest, PIXEL_FORMAT_NV16);
    } else if (*profile->chroma_sampling == VideoChromaSampling::k444 &&
               *profile->bit_depth == 8) {
      saw_nv24 = true;
      EXPECT_EQ(dest, PIXEL_FORMAT_NV24);
    } else if (*profile->chroma_sampling == VideoChromaSampling::k422 &&
               *profile->bit_depth == 10) {
      saw_p210 = true;
      EXPECT_EQ(dest, PIXEL_FORMAT_P210LE);
    } else if (*profile->chroma_sampling == VideoChromaSampling::k444 &&
               *profile->bit_depth == 10) {
      saw_p410 = true;
      EXPECT_EQ(dest, PIXEL_FORMAT_P410LE);
    } else {
      ADD_FAILURE() << "Unexpected RExt chroma/bit-depth combo";
    }
  }
  EXPECT_TRUE(saw_nv16);
  EXPECT_TRUE(saw_nv24);
  EXPECT_TRUE(saw_p210);
  EXPECT_TRUE(saw_p410);
#endif  // defined(ARCH_CPU_X86_FAMILY)
}
#endif  // BUILDFLAG(ENABLE_PLATFORM_HEVC) &&
        // BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)

}  // namespace media
