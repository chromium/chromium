// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/supported_types.h"

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "media/base/media_switches.h"
#include "media/mojo/buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/build_info.h"
#endif

#if BUILDFLAG(IS_WIN)
#include "base/win/windows_version.h"
#endif

namespace media {

#if BUILDFLAG(USE_PROPRIETARY_CODECS)
const bool kPropCodecsEnabled = true;
#else
const bool kPropCodecsEnabled = false;
#endif

TEST(SupportedTypesTest, IsSupportedVideoTypeBasics) {
  // Default to common 709.
  const VideoColorSpace kColorSpace = VideoColorSpace::REC709();

  // Some codecs do not have a notion of level.
  const int kUnspecifiedLevel = 0;

  // Expect support for baseline configuration of known codecs.
  EXPECT_TRUE(IsSupportedVideoType(
      {VideoCodec::kVP8, VP8PROFILE_ANY, kUnspecifiedLevel, kColorSpace}));
  EXPECT_TRUE(IsSupportedVideoType(
      {VideoCodec::kVP9, VP9PROFILE_PROFILE0, kUnspecifiedLevel, kColorSpace}));
  EXPECT_FALSE(
      IsSupportedVideoType({VideoCodec::kTheora, VIDEO_CODEC_PROFILE_UNKNOWN,
                            kUnspecifiedLevel, kColorSpace}));

  // Expect non-support for the following.
  EXPECT_FALSE(
      IsSupportedVideoType({VideoCodec::kUnknown, VIDEO_CODEC_PROFILE_UNKNOWN,
                            kUnspecifiedLevel, kColorSpace}));
  EXPECT_FALSE(
      IsSupportedVideoType({VideoCodec::kVC1, VIDEO_CODEC_PROFILE_UNKNOWN,
                            kUnspecifiedLevel, kColorSpace}));
  EXPECT_FALSE(
      IsSupportedVideoType({VideoCodec::kMPEG2, VIDEO_CODEC_PROFILE_UNKNOWN,
                            kUnspecifiedLevel, kColorSpace}));

  // Expect conditional support for the following.
  EXPECT_EQ(kPropCodecsEnabled,
            IsSupportedVideoType(
                {VideoCodec::kH264, H264PROFILE_BASELINE, 1, kColorSpace}));
  EXPECT_FALSE(
      IsSupportedVideoType({VideoCodec::kMPEG4, VIDEO_CODEC_PROFILE_UNKNOWN,
                            kUnspecifiedLevel, kColorSpace}));

#if BUILDFLAG(ENABLE_PLATFORM_HEVC) && \
    !BUILDFLAG(PLATFORM_HAS_OPTIONAL_HEVC_SUPPORT)
  EXPECT_TRUE(
      IsSupportedVideoType({VideoCodec::kHEVC, VIDEO_CODEC_PROFILE_UNKNOWN,
                            kUnspecifiedLevel, kColorSpace}));
#else
  EXPECT_FALSE(
      IsSupportedVideoType({VideoCodec::kHEVC, VIDEO_CODEC_PROFILE_UNKNOWN,
                            kUnspecifiedLevel, kColorSpace}));
#endif
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
    EXPECT_EQ(found,
              IsSupportedVideoType(
                  {VideoCodec::kVP9, VP9PROFILE_PROFILE0, 1, color_space}));
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
      VideoColorSpace::PrimaryID::EBU_3213_E,
  };

  for (int i = 0; i <= (1 << (8 * sizeof(VideoColorSpace::PrimaryID))); i++) {
    VideoColorSpace color_space = VideoColorSpace::REC709();
    color_space.primaries = VideoColorSpace::GetPrimaryID(i);
    bool found = kSupportedPrimaries.find(color_space.primaries) !=
                 kSupportedPrimaries.end();
    if (found)
      num_found++;
    EXPECT_EQ(found,
              IsSupportedVideoType(
                  {VideoCodec::kVP9, VP9PROFILE_PROFILE0, 1, color_space}));
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
    EXPECT_EQ(found,
              IsSupportedVideoType(
                  {VideoCodec::kVP9, VP9PROFILE_PROFILE0, 1, color_space}));
  }
  EXPECT_EQ(kSupportedMatrix.size(), num_found);
}

TEST(SupportedTypesTest, IsSupportedVideoType_VP9Profiles) {
  // Default to common 709.
  const VideoColorSpace kColorSpace = VideoColorSpace::REC709();

  // Some codecs do not have a notion of level.
  const int kUnspecifiedLevel = 0;

  EXPECT_TRUE(IsSupportedVideoType(
      {VideoCodec::kVP9, VP9PROFILE_PROFILE0, kUnspecifiedLevel, kColorSpace}));
  EXPECT_TRUE(IsSupportedVideoType(
      {VideoCodec::kVP9, VP9PROFILE_PROFILE1, kUnspecifiedLevel, kColorSpace}));

// VP9 Profile2 are supported on x86, ChromeOS on ARM and Mac/Win on ARM64.
// See third_party/libvpx/BUILD.gn.
#if defined(ARCH_CPU_X86_FAMILY) ||                                 \
    (defined(ARCH_CPU_ARM_FAMILY) && BUILDFLAG(IS_CHROMEOS_ASH)) || \
    (defined(ARCH_CPU_ARM64) && (BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)))
  EXPECT_TRUE(IsSupportedVideoType(
      {VideoCodec::kVP9, VP9PROFILE_PROFILE2, kUnspecifiedLevel, kColorSpace}));
#endif
}

TEST(SupportedTypesTest, IsSupportedAudioTypeWithSpatialRenderingBasics) {
  const bool is_spatial_rendering = true;
  // Dolby Atmos = E-AC3 (Dolby Digital Plus) + spatialRendering. Currently not
  // supported.
  EXPECT_FALSE(IsSupportedAudioType(
      {AudioCodec::kEAC3, AudioCodecProfile::kUnknown, is_spatial_rendering}));

  // Expect non-support for codecs with which there is no spatial audio format.
  EXPECT_FALSE(IsSupportedAudioType(
      {AudioCodec::kAAC, AudioCodecProfile::kUnknown, is_spatial_rendering}));
  EXPECT_FALSE(IsSupportedAudioType(
      {AudioCodec::kMP3, AudioCodecProfile::kUnknown, is_spatial_rendering}));
  EXPECT_FALSE(IsSupportedAudioType(
      {AudioCodec::kPCM, AudioCodecProfile::kUnknown, is_spatial_rendering}));
  EXPECT_FALSE(
      IsSupportedAudioType({AudioCodec::kVorbis, AudioCodecProfile::kUnknown,
                            is_spatial_rendering}));
  EXPECT_FALSE(IsSupportedAudioType(
      {AudioCodec::kFLAC, AudioCodecProfile::kUnknown, is_spatial_rendering}));
  EXPECT_FALSE(
      IsSupportedAudioType({AudioCodec::kAMR_NB, AudioCodecProfile::kUnknown,
                            is_spatial_rendering}));
  EXPECT_FALSE(
      IsSupportedAudioType({AudioCodec::kAMR_WB, AudioCodecProfile::kUnknown,
                            is_spatial_rendering}));
  EXPECT_FALSE(
      IsSupportedAudioType({AudioCodec::kPCM_MULAW, AudioCodecProfile::kUnknown,
                            is_spatial_rendering}));
  EXPECT_FALSE(
      IsSupportedAudioType({AudioCodec::kGSM_MS, AudioCodecProfile::kUnknown,
                            is_spatial_rendering}));
  EXPECT_FALSE(
      IsSupportedAudioType({AudioCodec::kPCM_S16BE, AudioCodecProfile::kUnknown,
                            is_spatial_rendering}));
  EXPECT_FALSE(
      IsSupportedAudioType({AudioCodec::kPCM_S24BE, AudioCodecProfile::kUnknown,
                            is_spatial_rendering}));
  EXPECT_FALSE(IsSupportedAudioType(
      {AudioCodec::kOpus, AudioCodecProfile::kUnknown, is_spatial_rendering}));
  EXPECT_FALSE(
      IsSupportedAudioType({AudioCodec::kPCM_ALAW, AudioCodecProfile::kUnknown,
                            is_spatial_rendering}));
  EXPECT_FALSE(IsSupportedAudioType(
      {AudioCodec::kALAC, AudioCodecProfile::kUnknown, is_spatial_rendering}));
  EXPECT_FALSE(IsSupportedAudioType(
      {AudioCodec::kAC3, AudioCodecProfile::kUnknown, is_spatial_rendering}));
  EXPECT_FALSE(IsSupportedAudioType({AudioCodec::kMpegHAudio,
                                     AudioCodecProfile::kUnknown,
                                     is_spatial_rendering}));
#if BUILDFLAG(ENABLE_PLATFORM_DTS_AUDIO)
  EXPECT_FALSE(IsSupportedAudioType(
      {AudioCodec::kDTS, AudioCodecProfile::kUnknown, is_spatial_rendering}));
  EXPECT_FALSE(
      IsSupportedAudioType({AudioCodec::kDTSXP2, AudioCodecProfile::kUnknown,
                            is_spatial_rendering}));
#endif  // BUILDFLAG(ENABLE_PLATFORM_DTS_AUDIO)
#if BUILDFLAG(ENABLE_PLATFORM_AC4_AUDIO)
  EXPECT_FALSE(IsSupportedAudioType(
      {AudioCodec::kAC4, AudioCodecProfile::kUnknown, is_spatial_rendering}));
#endif  // BUILDFLAG(ENABLE_PLATFORM_AC4_AUDIO)
  EXPECT_FALSE(
      IsSupportedAudioType({AudioCodec::kUnknown, AudioCodecProfile::kUnknown,
                            is_spatial_rendering}));
}

TEST(SupportedTypesTest, XHE_AACSupported) {
  AudioType aac{AudioCodec::kAAC, AudioCodecProfile::kXHE_AAC, false};
  EXPECT_EQ(false, IsSupportedAudioType(aac));

  UpdateDefaultSupportedAudioTypes({aac});

  EXPECT_EQ(
#if BUILDFLAG(ENABLE_MOJO_AUDIO_DECODER) && \
    (BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN))
      kPropCodecsEnabled,
#else
      false,
#endif  // BUILDFLAG(ENABLE_MOJO_AUDIO_DECODER) && (BUILDFLAG(IS_ANDROID) ||
        // BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN))
      IsSupportedAudioType(aac));
}

TEST(SupportedTypesTest, IsSupportedVideoTypeWithHdrMetadataBasics) {
  // Default to common 709.
  VideoColorSpace color_space = VideoColorSpace::REC709();

  // Some codecs do not have a notion of level.
  const int kUnspecifiedLevel = 0;

  // Expect support for baseline configuration of known codecs.
  EXPECT_TRUE(IsSupportedVideoType(
      {VideoCodec::kVP8, VP8PROFILE_ANY, kUnspecifiedLevel, color_space}));
  EXPECT_TRUE(IsSupportedVideoType(
      {VideoCodec::kVP9, VP9PROFILE_PROFILE0, kUnspecifiedLevel, color_space}));
  EXPECT_FALSE(
      IsSupportedVideoType({VideoCodec::kTheora, VIDEO_CODEC_PROFILE_UNKNOWN,
                            kUnspecifiedLevel, color_space}));

  // HDR metadata w/o an HDR color space should return false.
  EXPECT_FALSE(
      IsSupportedVideoType({VideoCodec::kVP8, VP8PROFILE_ANY, kUnspecifiedLevel,
                            color_space, gfx::HdrMetadataType::kSmpteSt2086}));

  // All combinations of combinations of color gamuts and transfer functions
  // should be supported.
  color_space.primaries = VideoColorSpace::PrimaryID::SMPTEST431_2;
  color_space.transfer = VideoColorSpace::TransferID::SMPTEST2084;
  EXPECT_TRUE(IsSupportedVideoType(
      {VideoCodec::kVP8, VP8PROFILE_ANY, kUnspecifiedLevel, color_space}));
  EXPECT_TRUE(IsSupportedVideoType(
      {VideoCodec::kVP9, VP9PROFILE_PROFILE0, kUnspecifiedLevel, color_space}));
  EXPECT_FALSE(
      IsSupportedVideoType({VideoCodec::kTheora, VIDEO_CODEC_PROFILE_UNKNOWN,
                            kUnspecifiedLevel, color_space}));
  EXPECT_TRUE(
      IsSupportedVideoType({VideoCodec::kVP8, VP8PROFILE_ANY, kUnspecifiedLevel,
                            color_space, gfx::HdrMetadataType::kSmpteSt2086}));

  color_space.primaries = VideoColorSpace::PrimaryID::BT2020;
  color_space.transfer = VideoColorSpace::TransferID::ARIB_STD_B67;
  EXPECT_TRUE(IsSupportedVideoType(
      {VideoCodec::kVP8, VP8PROFILE_ANY, kUnspecifiedLevel, color_space}));
  EXPECT_TRUE(IsSupportedVideoType(
      {VideoCodec::kVP9, VP9PROFILE_PROFILE0, kUnspecifiedLevel, color_space}));
  EXPECT_FALSE(
      IsSupportedVideoType({VideoCodec::kTheora, VIDEO_CODEC_PROFILE_UNKNOWN,
                            kUnspecifiedLevel, color_space}));
  // HDR10 metadata only works with the PQ transfer.
  EXPECT_FALSE(
      IsSupportedVideoType({VideoCodec::kVP8, VP8PROFILE_ANY, kUnspecifiedLevel,
                            color_space, gfx::HdrMetadataType::kSmpteSt2086}));

  // ST2094-10 metadata is not supported even if the codec is dolby vision.
  EXPECT_FALSE(IsSupportedVideoType(
      {VideoCodec::kDolbyVision, DOLBYVISION_PROFILE5, kUnspecifiedLevel,
       color_space, gfx::HdrMetadataType::kSmpteSt2094_10}));

  EXPECT_FALSE(IsSupportedVideoType({VideoCodec::kVP8, VP8PROFILE_ANY,
                                     kUnspecifiedLevel, color_space,
                                     gfx::HdrMetadataType::kSmpteSt2094_40}));
}

TEST(SupportedTypesTest, IsBuiltInVideoCodec) {
#if BUILDFLAG(USE_PROPRIETARY_CODECS) && BUILDFLAG(ENABLE_FFMPEG_VIDEO_DECODERS)
  EXPECT_EQ(base::FeatureList::IsEnabled(kBuiltInH264Decoder),
            IsBuiltInVideoCodec(VideoCodec::kH264));
#else
  EXPECT_FALSE(IsBuiltInVideoCodec(VideoCodec::kH264));
#endif  // BUILDFLAG(USE_PROPRIETARY_CODECS) &&
        // BUILDFLAG(ENABLE_FFMPEG_VIDEO_DECODERS)

  EXPECT_FALSE(IsBuiltInVideoCodec(VideoCodec::kTheora));

#if BUILDFLAG(ENABLE_LIBVPX)
  EXPECT_TRUE(IsBuiltInVideoCodec(VideoCodec::kVP8));
#else
  EXPECT_FALSE(IsBuiltInVideoCodec(VideoCodec::kVP8));
#endif  // BUILDFLAG(ENABLE_LIBVPX)

#if BUILDFLAG(ENABLE_LIBVPX)
  EXPECT_TRUE(IsBuiltInVideoCodec(VideoCodec::kVP9));
#else
  EXPECT_FALSE(IsBuiltInVideoCodec(VideoCodec::kVP9));
#endif  // BUILDFLAG(ENABLE_LIBVPX)

#if BUILDFLAG(ENABLE_AV1_DECODER)
  EXPECT_TRUE(IsBuiltInVideoCodec(VideoCodec::kAV1));
#else
  EXPECT_FALSE(IsBuiltInVideoCodec(VideoCodec::kAV1));
#endif  // BUILDFLAG(ENABLE_AV1_DECODER)

  EXPECT_FALSE(IsBuiltInVideoCodec(VideoCodec::kUnknown));
  EXPECT_FALSE(IsBuiltInVideoCodec(VideoCodec::kMPEG4));
  EXPECT_FALSE(IsBuiltInVideoCodec(VideoCodec::kVC1));
  EXPECT_FALSE(IsBuiltInVideoCodec(VideoCodec::kMPEG2));
  EXPECT_FALSE(IsBuiltInVideoCodec(VideoCodec::kHEVC));
  EXPECT_FALSE(IsBuiltInVideoCodec(VideoCodec::kDolbyVision));
}
}  // namespace media
