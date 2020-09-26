// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/supported_types.h"

#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_ANDROID)
#include "base/android/build_info.h"
#endif

namespace media {

#if BUILDFLAG(USE_PROPRIETARY_CODECS)
const bool kPropCodecsEnabled = true;
#else
const bool kPropCodecsEnabled = false;
#endif

#if defined(OS_CHROMEOS) && BUILDFLAG(USE_PROPRIETARY_CODECS)
const bool kMpeg4Supported = true;
#else
const bool kMpeg4Supported = false;
#endif

TEST(SupportedTypesTest, IsSupportedVideoTypeBasics) {
  // Default to common 709.
  const VideoColorSpace kColorSpace = VideoColorSpace::REC709();

  // Some codecs do not have a notion of level.
  const int kUnspecifiedLevel = 0;

  // Expect support for baseline configuration of known codecs.
  EXPECT_TRUE(IsSupportedVideoType(
      {kCodecVP8, VP8PROFILE_ANY, kUnspecifiedLevel, kColorSpace}));
  EXPECT_TRUE(IsSupportedVideoType(
      {kCodecVP9, VP9PROFILE_PROFILE0, kUnspecifiedLevel, kColorSpace}));
  EXPECT_TRUE(IsSupportedVideoType({kCodecTheora, VIDEO_CODEC_PROFILE_UNKNOWN,
                                    kUnspecifiedLevel, kColorSpace}));

  // Expect non-support for the following.
  EXPECT_FALSE(
      IsSupportedVideoType({kUnknownVideoCodec, VIDEO_CODEC_PROFILE_UNKNOWN,
                            kUnspecifiedLevel, kColorSpace}));
  EXPECT_FALSE(IsSupportedVideoType({kCodecVC1, VIDEO_CODEC_PROFILE_UNKNOWN,
                                     kUnspecifiedLevel, kColorSpace}));
  EXPECT_FALSE(IsSupportedVideoType({kCodecMPEG2, VIDEO_CODEC_PROFILE_UNKNOWN,
                                     kUnspecifiedLevel, kColorSpace}));
  EXPECT_FALSE(IsSupportedVideoType({kCodecHEVC, VIDEO_CODEC_PROFILE_UNKNOWN,
                                     kUnspecifiedLevel, kColorSpace}));

  // Expect conditional support for the following.
  EXPECT_EQ(
      kPropCodecsEnabled,
      IsSupportedVideoType({kCodecH264, H264PROFILE_BASELINE, 1, kColorSpace}));
  EXPECT_EQ(kMpeg4Supported,
            IsSupportedVideoType({kCodecMPEG4, VIDEO_CODEC_PROFILE_UNKNOWN,
                                  kUnspecifiedLevel, kColorSpace}));
}

TEST(SupportedTypesTest, IsSupportedVideoType_VP9TransferFunctions) {
  size_t num_found = 0;
  // TODO(hubbe): Verify support for HDR codecs when color management enabled.
  const std::set<VideoColorSpace::TransferID> kSupportedTransfers = {
      VideoColorSpace::TransferID::GAMMA22,
      VideoColorSpace::TransferID::UNSPECIFIED,
      VideoColorSpace::TransferID::BT709,
      VideoColorSpace::TransferID::SMPTE170M,
      VideoColorSpace::TransferID::BT2020_10,
      VideoColorSpace::TransferID::BT2020_12,
      VideoColorSpace::TransferID::IEC61966_2_1,
      VideoColorSpace::TransferID::GAMMA28,
      VideoColorSpace::TransferID::SMPTE240M,
      VideoColorSpace::TransferID::LINEAR,
      VideoColorSpace::TransferID::LOG,
      VideoColorSpace::TransferID::LOG_SQRT,
      VideoColorSpace::TransferID::BT1361_ECG,
      VideoColorSpace::TransferID::SMPTEST2084,
      VideoColorSpace::TransferID::IEC61966_2_4,
      VideoColorSpace::TransferID::SMPTEST428_1,
      VideoColorSpace::TransferID::ARIB_STD_B67,
  };

  for (int i = 0; i <= (1 << (8 * sizeof(VideoColorSpace::TransferID))); i++) {
    VideoColorSpace color_space = VideoColorSpace::REC709();
    color_space.transfer = VideoColorSpace::GetTransferID(i);
    bool found = kSupportedTransfers.find(color_space.transfer) !=
                 kSupportedTransfers.end();
    if (found)
      num_found++;
    EXPECT_EQ(found, IsSupportedVideoType(
                         {kCodecVP9, VP9PROFILE_PROFILE0, 1, color_space}));
  }
  EXPECT_EQ(kSupportedTransfers.size(), num_found);
}

TEST(SupportedTypesTest, IsSupportedVideoType_VP9Primaries) {
  size_t num_found = 0;
  // TODO(hubbe): Verify support for HDR codecs when color management enabled.
  const std::set<VideoColorSpace::PrimaryID> kSupportedPrimaries = {
      VideoColorSpace::PrimaryID::BT709,
      VideoColorSpace::PrimaryID::UNSPECIFIED,
      VideoColorSpace::PrimaryID::BT470M,
      VideoColorSpace::PrimaryID::BT470BG,
      VideoColorSpace::PrimaryID::SMPTE170M,
      VideoColorSpace::PrimaryID::SMPTE240M,
      VideoColorSpace::PrimaryID::FILM,
      VideoColorSpace::PrimaryID::BT2020,
      VideoColorSpace::PrimaryID::SMPTEST428_1,
      VideoColorSpace::PrimaryID::SMPTEST431_2,
      VideoColorSpace::PrimaryID::SMPTEST432_1,
  };

  for (int i = 0; i <= (1 << (8 * sizeof(VideoColorSpace::PrimaryID))); i++) {
    VideoColorSpace color_space = VideoColorSpace::REC709();
    color_space.primaries = VideoColorSpace::GetPrimaryID(i);
    bool found = kSupportedPrimaries.find(color_space.primaries) !=
                 kSupportedPrimaries.end();
    if (found)
      num_found++;
    EXPECT_EQ(found, IsSupportedVideoType(
                         {kCodecVP9, VP9PROFILE_PROFILE0, 1, color_space}));
  }
  EXPECT_EQ(kSupportedPrimaries.size(), num_found);
}

TEST(SupportedTypesTest, IsSupportedVideoType_VP9Matrix) {
  size_t num_found = 0;
  // TODO(hubbe): Verify support for HDR codecs when color management enabled.
  const std::set<VideoColorSpace::MatrixID> kSupportedMatrix = {
      VideoColorSpace::MatrixID::BT709,
      VideoColorSpace::MatrixID::UNSPECIFIED,
      VideoColorSpace::MatrixID::BT470BG,
      VideoColorSpace::MatrixID::SMPTE170M,
      VideoColorSpace::MatrixID::BT2020_NCL,
      VideoColorSpace::MatrixID::RGB,
      VideoColorSpace::MatrixID::FCC,
      VideoColorSpace::MatrixID::SMPTE240M,
      VideoColorSpace::MatrixID::YCOCG,
      VideoColorSpace::MatrixID::YDZDX,
      VideoColorSpace::MatrixID::BT2020_CL,
  };

  for (int i = 0; i <= (1 << (8 * sizeof(VideoColorSpace::MatrixID))); i++) {
    VideoColorSpace color_space = VideoColorSpace::REC709();
    color_space.matrix = VideoColorSpace::GetMatrixID(i);
    bool found =
        kSupportedMatrix.find(color_space.matrix) != kSupportedMatrix.end();
    if (found)
      num_found++;
    EXPECT_EQ(found, IsSupportedVideoType(
                         {kCodecVP9, VP9PROFILE_PROFILE0, 1, color_space}));
  }
  EXPECT_EQ(kSupportedMatrix.size(), num_found);
}

TEST(SupportedTypesTest, IsSupportedVideoType_VP9Profiles) {
  // Default to common 709.
  const VideoColorSpace kColorSpace = VideoColorSpace::REC709();

  // Some codecs do not have a notion of level.
  const int kUnspecifiedLevel = 0;

  EXPECT_TRUE(IsSupportedVideoType(
      {kCodecVP9, VP9PROFILE_PROFILE0, kUnspecifiedLevel, kColorSpace}));
  EXPECT_TRUE(IsSupportedVideoType(
      {kCodecVP9, VP9PROFILE_PROFILE1, kUnspecifiedLevel, kColorSpace}));

// VP9 Profile2 are supported on x86, ChromeOS on ARM and Mac/Win on ARM64.
// See third_party/libvpx/BUILD.gn.
#if defined(ARCH_CPU_X86_FAMILY) ||                           \
    (defined(ARCH_CPU_ARM_FAMILY) && defined(OS_CHROMEOS)) || \
    (defined(ARCH_CPU_ARM64) && (defined(OS_MAC) || defined(OS_WIN)))
  EXPECT_TRUE(IsSupportedVideoType(
      {kCodecVP9, VP9PROFILE_PROFILE2, kUnspecifiedLevel, kColorSpace}));
#endif
}

TEST(SupportedTypesTest, IsSupportedAudioTypeWithSpatialRenderingBasics) {
  const bool is_spatial_rendering = true;
  // Dolby Atmos = E-AC3 (Dolby Digital Plus) + spatialRendering. Currently not
  // supported.
  EXPECT_FALSE(IsSupportedAudioType(
      {kCodecEAC3, AudioCodecProfile::kUnknown, is_spatial_rendering}));

  // Expect non-support for codecs with which there is no spatial audio format.
  EXPECT_FALSE(IsSupportedAudioType(
      {kCodecAAC, AudioCodecProfile::kUnknown, is_spatial_rendering}));
  EXPECT_FALSE(IsSupportedAudioType(
      {kCodecMP3, AudioCodecProfile::kUnknown, is_spatial_rendering}));
  EXPECT_FALSE(IsSupportedAudioType(
      {kCodecPCM, AudioCodecProfile::kUnknown, is_spatial_rendering}));
  EXPECT_FALSE(IsSupportedAudioType(
      {kCodecVorbis, AudioCodecProfile::kUnknown, is_spatial_rendering}));
  EXPECT_FALSE(IsSupportedAudioType(
      {kCodecFLAC, AudioCodecProfile::kUnknown, is_spatial_rendering}));
  EXPECT_FALSE(IsSupportedAudioType(
      {kCodecAMR_NB, AudioCodecProfile::kUnknown, is_spatial_rendering}));
  EXPECT_FALSE(IsSupportedAudioType(
      {kCodecAMR_WB, AudioCodecProfile::kUnknown, is_spatial_rendering}));
  EXPECT_FALSE(IsSupportedAudioType(
      {kCodecPCM_MULAW, AudioCodecProfile::kUnknown, is_spatial_rendering}));
  EXPECT_FALSE(IsSupportedAudioType(
      {kCodecGSM_MS, AudioCodecProfile::kUnknown, is_spatial_rendering}));
  EXPECT_FALSE(IsSupportedAudioType(
      {kCodecPCM_S16BE, AudioCodecProfile::kUnknown, is_spatial_rendering}));
  EXPECT_FALSE(IsSupportedAudioType(
      {kCodecPCM_S24BE, AudioCodecProfile::kUnknown, is_spatial_rendering}));
  EXPECT_FALSE(IsSupportedAudioType(
      {kCodecOpus, AudioCodecProfile::kUnknown, is_spatial_rendering}));
  EXPECT_FALSE(IsSupportedAudioType(
      {kCodecPCM_ALAW, AudioCodecProfile::kUnknown, is_spatial_rendering}));
  EXPECT_FALSE(IsSupportedAudioType(
      {kCodecALAC, AudioCodecProfile::kUnknown, is_spatial_rendering}));
  EXPECT_FALSE(IsSupportedAudioType(
      {kCodecAC3, AudioCodecProfile::kUnknown, is_spatial_rendering}));
  EXPECT_FALSE(IsSupportedAudioType(
      {kCodecMpegHAudio, AudioCodecProfile::kUnknown, is_spatial_rendering}));
  EXPECT_FALSE(IsSupportedAudioType(
      {kUnknownAudioCodec, AudioCodecProfile::kUnknown, is_spatial_rendering}));
}

TEST(SupportedTypesTest, XHE_AACSupportedOnAndroidOnly) {
  // TODO(dalecurtis): Update this test if we ever have support elsewhere.
#if defined(OS_ANDROID)
  const bool is_supported =
      kPropCodecsEnabled &&
      base::android::BuildInfo::GetInstance()->sdk_int() >=
          base::android::SDK_VERSION_P;

  EXPECT_EQ(is_supported, IsSupportedAudioType(
                              {kCodecAAC, AudioCodecProfile::kXHE_AAC, false}));
#else
  EXPECT_FALSE(
      IsSupportedAudioType({kCodecAAC, AudioCodecProfile::kXHE_AAC, false}));
#endif
}

TEST(SupportedTypesTest, IsSupportedVideoTypeWithHdrMetadataBasics) {
  // Default to common 709.
  VideoColorSpace color_space = VideoColorSpace::REC709();

  // Some codecs do not have a notion of level.
  const int kUnspecifiedLevel = 0;

  // Expect support for baseline configuration of known codecs.
  EXPECT_TRUE(IsSupportedVideoType(
      {kCodecVP8, VP8PROFILE_ANY, kUnspecifiedLevel, color_space}));
  EXPECT_TRUE(IsSupportedVideoType(
      {kCodecVP9, VP9PROFILE_PROFILE0, kUnspecifiedLevel, color_space}));
  EXPECT_TRUE(IsSupportedVideoType({kCodecTheora, VIDEO_CODEC_PROFILE_UNKNOWN,
                                    kUnspecifiedLevel, color_space}));

  // All combinations of combinations of color gamuts and transfer functions
  // should be supported.
  color_space.primaries = VideoColorSpace::PrimaryID::SMPTEST431_2;
  color_space.transfer = VideoColorSpace::TransferID::SMPTEST2084;
  EXPECT_TRUE(IsSupportedVideoType(
      {kCodecVP8, VP8PROFILE_ANY, kUnspecifiedLevel, color_space}));
  EXPECT_TRUE(IsSupportedVideoType(
      {kCodecVP9, VP9PROFILE_PROFILE0, kUnspecifiedLevel, color_space}));
  EXPECT_TRUE(IsSupportedVideoType({kCodecTheora, VIDEO_CODEC_PROFILE_UNKNOWN,
                                    kUnspecifiedLevel, color_space}));

  color_space.primaries = VideoColorSpace::PrimaryID::BT2020;
  color_space.transfer = VideoColorSpace::TransferID::ARIB_STD_B67;
  EXPECT_TRUE(IsSupportedVideoType(
      {kCodecVP8, VP8PROFILE_ANY, kUnspecifiedLevel, color_space}));
  EXPECT_TRUE(IsSupportedVideoType(
      {kCodecVP9, VP9PROFILE_PROFILE0, kUnspecifiedLevel, color_space}));
  EXPECT_TRUE(IsSupportedVideoType({kCodecTheora, VIDEO_CODEC_PROFILE_UNKNOWN,
                                    kUnspecifiedLevel, color_space}));

  // No HDR metadata types are supported.
  EXPECT_FALSE(
      IsSupportedVideoType({kCodecVP8, VP8PROFILE_ANY, kUnspecifiedLevel,
                            color_space, gl::HdrMetadataType::kSmpteSt2086}));

  EXPECT_FALSE(IsSupportedVideoType({kCodecVP8, VP8PROFILE_ANY,
                                     kUnspecifiedLevel, color_space,
                                     gl::HdrMetadataType::kSmpteSt2094_10}));

  EXPECT_FALSE(IsSupportedVideoType({kCodecVP8, VP8PROFILE_ANY,
                                     kUnspecifiedLevel, color_space,
                                     gl::HdrMetadataType::kSmpteSt2094_40}));
}
}  // namespace media
