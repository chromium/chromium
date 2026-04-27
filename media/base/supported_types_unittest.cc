// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/supported_types.h"

#include "build/build_config.h"
#include "media/base/media_switches.h"
#include "media/base/media_util.h"
#include "media/mojo/buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"


#if BUILDFLAG(IS_WIN)
#include "base/win/windows_version.h"
#endif

namespace media {

#if BUILDFLAG(USE_PROPRIETARY_CODECS)
const bool kPropCodecsEnabled = true;
#else
const bool kPropCodecsEnabled = false;
#endif

TEST(SupportedTypesTest, IsDecoderSupportedVideoTypeBasics) {
  // Default to common 709.
  const VideoColorSpace kColorSpace = VideoColorSpace::REC709();

  // Some codecs do not have a notion of level.
  const int kUnspecifiedLevel = 0;

  // Expect support for baseline configuration of known codecs.
  EXPECT_TRUE(IsDecoderSupportedVideoType(
      {VideoCodec::kVP8, VP8PROFILE_ANY, kUnspecifiedLevel, kColorSpace}));
  EXPECT_EQ(IsDecoderSupportedVideoType({VideoCodec::kVP9, VP9PROFILE_PROFILE0,
                                         kUnspecifiedLevel, kColorSpace}),
            BUILDFLAG(ENABLE_LIBVPX));
  EXPECT_FALSE(IsDecoderSupportedVideoType({VideoCodec::kTheora,
                                            VIDEO_CODEC_PROFILE_UNKNOWN,
                                            kUnspecifiedLevel, kColorSpace}));

  // Expect non-support for the following.
  EXPECT_FALSE(IsDecoderSupportedVideoType({VideoCodec::kUnknown,
                                            VIDEO_CODEC_PROFILE_UNKNOWN,
                                            kUnspecifiedLevel, kColorSpace}));
  EXPECT_FALSE(IsDecoderSupportedVideoType({VideoCodec::kVC1,
                                            VIDEO_CODEC_PROFILE_UNKNOWN,
                                            kUnspecifiedLevel, kColorSpace}));
  EXPECT_FALSE(IsDecoderSupportedVideoType({VideoCodec::kMPEG2,
                                            VIDEO_CODEC_PROFILE_UNKNOWN,
                                            kUnspecifiedLevel, kColorSpace}));

  // Expect conditional support for the following.
  EXPECT_EQ(kPropCodecsEnabled,
            IsDecoderSupportedVideoType(
                {VideoCodec::kH264, H264PROFILE_BASELINE, 1, kColorSpace}));
  EXPECT_FALSE(IsDecoderSupportedVideoType({VideoCodec::kMPEG4,
                                            VIDEO_CODEC_PROFILE_UNKNOWN,
                                            kUnspecifiedLevel, kColorSpace}));

#if BUILDFLAG(ENABLE_PLATFORM_HEVC) && \
    !BUILDFLAG(PLATFORM_HAS_OPTIONAL_HEVC_DECODE_SUPPORT)
  EXPECT_TRUE(IsDecoderSupportedVideoType({VideoCodec::kHEVC,
                                           VIDEO_CODEC_PROFILE_UNKNOWN,
                                           kUnspecifiedLevel, kColorSpace}));
#else
  EXPECT_FALSE(IsDecoderSupportedVideoType({VideoCodec::kHEVC,
                                            VIDEO_CODEC_PROFILE_UNKNOWN,
                                            kUnspecifiedLevel, kColorSpace}));
#endif
}

#if defined(ENABLE_LIBVPX)
TEST(SupportedTypesTest, IsDecoderSupportedVideoType_VP9TransferFunctions) {
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
    auto color_space = VideoColorSpace(
        VideoColorSpace::PrimaryID::BT709, VideoColorSpace::GetTransferID(i),
        VideoColorSpace::MatrixID::BT709, gfx::ColorSpace::RangeID::LIMITED);
    bool found = kSupportedTransfers.find(color_space.transfer()) !=
                 kSupportedTransfers.end();
    if (found)
      num_found++;
    EXPECT_EQ(found,
              IsDecoderSupportedVideoType(
                  {VideoCodec::kVP9, VP9PROFILE_PROFILE0, 1, color_space}));
  }
  EXPECT_EQ(kSupportedTransfers.size(), num_found);
}

TEST(SupportedTypesTest, IsDecoderSupportedVideoType_VP9Primaries) {
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
    VideoColorSpace color_space = VideoColorSpace(
        VideoColorSpace::GetPrimaryID(i), VideoColorSpace::TransferID::BT709,
        VideoColorSpace::MatrixID::BT709, gfx::ColorSpace::RangeID::LIMITED);
    bool found = kSupportedPrimaries.find(color_space.primaries()) !=
                 kSupportedPrimaries.end();
    if (found)
      num_found++;
    EXPECT_EQ(found,
              IsDecoderSupportedVideoType(
                  {VideoCodec::kVP9, VP9PROFILE_PROFILE0, 1, color_space}));
  }
  EXPECT_EQ(kSupportedPrimaries.size(), num_found);
}

TEST(SupportedTypesTest, IsDecoderSupportedVideoType_VP9Matrix) {
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
      // NOTE: BT2020_CL (10) is no longer supported — GetMatrixID(10) returns
      // INVALID. See crbug.com/333906350.
  };

  for (int i = 0; i <= (1 << (8 * sizeof(VideoColorSpace::MatrixID))); i++) {
    VideoColorSpace color_space = VideoColorSpace(
        VideoColorSpace::PrimaryID::BT709, VideoColorSpace::TransferID::BT709,
        VideoColorSpace::GetMatrixID(i), gfx::ColorSpace::RangeID::LIMITED);
    bool found =
        kSupportedMatrix.find(color_space.matrix()) != kSupportedMatrix.end();
    if (found)
      num_found++;
    EXPECT_EQ(found,
              IsDecoderSupportedVideoType(
                  {VideoCodec::kVP9, VP9PROFILE_PROFILE0, 1, color_space}));
  }
  EXPECT_EQ(kSupportedMatrix.size(), num_found);
}

TEST(SupportedTypesTest, IsDecoderSupportedVideoType_VP9Profiles) {
  // Default to common 709.
  const VideoColorSpace kColorSpace = VideoColorSpace::REC709();

  // Some codecs do not have a notion of level.
  const int kUnspecifiedLevel = 0;

  EXPECT_TRUE(IsDecoderSupportedVideoType(
      {VideoCodec::kVP9, VP9PROFILE_PROFILE0, kUnspecifiedLevel, kColorSpace}));
  EXPECT_TRUE(IsDecoderSupportedVideoType(
      {VideoCodec::kVP9, VP9PROFILE_PROFILE1, kUnspecifiedLevel, kColorSpace}));

// VP9 Profile2 are supported on x86, ChromeOS on ARM and Mac/Win/Linux
// on ARM64. See third_party/libvpx/BUILD.gn.
#if defined(ARCH_CPU_X86_FAMILY) ||                             \
    (defined(ARCH_CPU_ARM_FAMILY) && BUILDFLAG(IS_CHROMEOS)) || \
    (defined(ARCH_CPU_ARM64) &&                                 \
     (BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX)))

  EXPECT_TRUE(IsDecoderSupportedVideoType(
      {VideoCodec::kVP9, VP9PROFILE_PROFILE2, kUnspecifiedLevel, kColorSpace}));
  EXPECT_TRUE(IsDecoderSupportedVideoType(
      {VideoCodec::kVP9, VP9PROFILE_PROFILE3, kUnspecifiedLevel, kColorSpace}));
#endif
}
#endif  // defined(ENABLE_LIBVPX)

TEST(SupportedTypesTest,
     IsDecoderSupportedAudioTypeWithSpatialRenderingBasics) {
  const bool is_spatial_rendering = true;
  // Dolby Atmos = E-AC3 (Dolby Digital Plus) + spatialRendering. Currently not
  // supported.
  EXPECT_FALSE(IsDecoderSupportedAudioType(
      {AudioCodec::kEAC3, AudioCodecProfile::kUnknown, is_spatial_rendering}));

  // Expect non-support for codecs with which there is no spatial audio format.
  EXPECT_FALSE(IsDecoderSupportedAudioType(
      {AudioCodec::kAAC, AudioCodecProfile::kUnknown, is_spatial_rendering}));
  EXPECT_FALSE(IsDecoderSupportedAudioType(
      {AudioCodec::kMP3, AudioCodecProfile::kUnknown, is_spatial_rendering}));
  EXPECT_FALSE(IsDecoderSupportedAudioType(
      {AudioCodec::kPCM, AudioCodecProfile::kUnknown, is_spatial_rendering}));
  EXPECT_FALSE(IsDecoderSupportedAudioType({AudioCodec::kVorbis,
                                            AudioCodecProfile::kUnknown,
                                            is_spatial_rendering}));
  EXPECT_FALSE(IsDecoderSupportedAudioType(
      {AudioCodec::kFLAC, AudioCodecProfile::kUnknown, is_spatial_rendering}));
  EXPECT_FALSE(IsDecoderSupportedAudioType({AudioCodec::kAMR_NB,
                                            AudioCodecProfile::kUnknown,
                                            is_spatial_rendering}));
  EXPECT_FALSE(IsDecoderSupportedAudioType({AudioCodec::kAMR_WB,
                                            AudioCodecProfile::kUnknown,
                                            is_spatial_rendering}));
  EXPECT_FALSE(IsDecoderSupportedAudioType({AudioCodec::kPCM_MULAW,
                                            AudioCodecProfile::kUnknown,
                                            is_spatial_rendering}));
  EXPECT_FALSE(IsDecoderSupportedAudioType({AudioCodec::kGSM_MS,
                                            AudioCodecProfile::kUnknown,
                                            is_spatial_rendering}));
  EXPECT_FALSE(IsDecoderSupportedAudioType({AudioCodec::kPCM_S16BE,
                                            AudioCodecProfile::kUnknown,
                                            is_spatial_rendering}));
  EXPECT_FALSE(IsDecoderSupportedAudioType({AudioCodec::kPCM_S24BE,
                                            AudioCodecProfile::kUnknown,
                                            is_spatial_rendering}));
  EXPECT_FALSE(IsDecoderSupportedAudioType(
      {AudioCodec::kOpus, AudioCodecProfile::kUnknown, is_spatial_rendering}));
  EXPECT_FALSE(IsDecoderSupportedAudioType({AudioCodec::kPCM_ALAW,
                                            AudioCodecProfile::kUnknown,
                                            is_spatial_rendering}));
  EXPECT_FALSE(IsDecoderSupportedAudioType(
      {AudioCodec::kALAC, AudioCodecProfile::kUnknown, is_spatial_rendering}));
  EXPECT_FALSE(IsDecoderSupportedAudioType(
      {AudioCodec::kAC3, AudioCodecProfile::kUnknown, is_spatial_rendering}));
  EXPECT_FALSE(IsDecoderSupportedAudioType({AudioCodec::kMpegHAudio,
                                            AudioCodecProfile::kUnknown,
                                            is_spatial_rendering}));
#if BUILDFLAG(ENABLE_PLATFORM_DTS_AUDIO)
  EXPECT_FALSE(IsDecoderSupportedAudioType(
      {AudioCodec::kDTS, AudioCodecProfile::kUnknown, is_spatial_rendering}));
  EXPECT_FALSE(IsDecoderSupportedAudioType({AudioCodec::kDTSXP2,
                                            AudioCodecProfile::kUnknown,
                                            is_spatial_rendering}));
#endif  // BUILDFLAG(ENABLE_PLATFORM_DTS_AUDIO)
#if BUILDFLAG(ENABLE_PLATFORM_AC4_AUDIO)
  EXPECT_FALSE(IsDecoderSupportedAudioType(
      {AudioCodec::kAC4, AudioCodecProfile::kUnknown, is_spatial_rendering}));
#endif  // BUILDFLAG(ENABLE_PLATFORM_AC4_AUDIO)
  EXPECT_FALSE(IsDecoderSupportedAudioType({AudioCodec::kUnknown,
                                            AudioCodecProfile::kUnknown,
                                            is_spatial_rendering}));
}

TEST(SupportedTypesTest, XHE_AACSupported) {
  AudioType aac{AudioCodec::kAAC, AudioCodecProfile::kXHE_AAC, false};
  EXPECT_EQ(false, IsDecoderSupportedAudioType(aac));

  UpdateDefaultDecoderSupportedAudioTypes({aac});

  EXPECT_EQ(
#if BUILDFLAG(ENABLE_MOJO_AUDIO_DECODER) && \
    (BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN))
      kPropCodecsEnabled,
#else
      false,
#endif  // BUILDFLAG(ENABLE_MOJO_AUDIO_DECODER) && (BUILDFLAG(IS_ANDROID) ||
        // BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN))
      IsDecoderSupportedAudioType(aac));
}

TEST(SupportedTypesTest, IsDecoderSupportedVideoTypeWithHdrMetadataBasics) {
  // Default to common 709.
  VideoColorSpace color_space = VideoColorSpace::REC709();

  // Some codecs do not have a notion of level.
  const int kUnspecifiedLevel = 0;

  // Expect support for baseline configuration of known codecs.
  EXPECT_TRUE(IsDecoderSupportedVideoType(
      {VideoCodec::kVP8, VP8PROFILE_ANY, kUnspecifiedLevel, color_space}));
#if defined(ENABLE_LIBVPX)
  EXPECT_TRUE(IsDecoderSupportedVideoType(
      {VideoCodec::kVP9, VP9PROFILE_PROFILE0, kUnspecifiedLevel, color_space}));
#endif
  EXPECT_FALSE(IsDecoderSupportedVideoType({VideoCodec::kTheora,
                                            VIDEO_CODEC_PROFILE_UNKNOWN,
                                            kUnspecifiedLevel, color_space}));

  // HDR metadata w/o an HDR color space should return false.
  EXPECT_FALSE(IsDecoderSupportedVideoType(
      {VideoCodec::kVP8, VP8PROFILE_ANY, kUnspecifiedLevel, color_space,
       gfx::HdrMetadataType::kSmpteSt2086}));

  // All combinations of combinations of color gamuts and transfer functions
  // should be supported.
  color_space = VideoColorSpace(VideoColorSpace::PrimaryID::SMPTEST431_2,
                                VideoColorSpace::TransferID::SMPTEST2084,
                                VideoColorSpace::MatrixID::BT709,
                                gfx::ColorSpace::RangeID::LIMITED);
  EXPECT_TRUE(IsDecoderSupportedVideoType(
      {VideoCodec::kVP8, VP8PROFILE_ANY, kUnspecifiedLevel, color_space}));
#if defined(ENABLE_LIBVPX)
  EXPECT_TRUE(IsDecoderSupportedVideoType(
      {VideoCodec::kVP9, VP9PROFILE_PROFILE0, kUnspecifiedLevel, color_space}));
#endif
  EXPECT_FALSE(IsDecoderSupportedVideoType({VideoCodec::kTheora,
                                            VIDEO_CODEC_PROFILE_UNKNOWN,
                                            kUnspecifiedLevel, color_space}));
  EXPECT_TRUE(IsDecoderSupportedVideoType(
      {VideoCodec::kVP8, VP8PROFILE_ANY, kUnspecifiedLevel, color_space,
       gfx::HdrMetadataType::kSmpteSt2086}));

  color_space = VideoColorSpace(VideoColorSpace::PrimaryID::BT2020,
                                VideoColorSpace::TransferID::ARIB_STD_B67,
                                VideoColorSpace::MatrixID::BT709,
                                gfx::ColorSpace::RangeID::LIMITED);
  EXPECT_TRUE(IsDecoderSupportedVideoType(
      {VideoCodec::kVP8, VP8PROFILE_ANY, kUnspecifiedLevel, color_space}));
#if defined(ENABLE_LIBVPX)
  EXPECT_TRUE(IsDecoderSupportedVideoType(
      {VideoCodec::kVP9, VP9PROFILE_PROFILE0, kUnspecifiedLevel, color_space}));
#endif
  EXPECT_FALSE(IsDecoderSupportedVideoType({VideoCodec::kTheora,
                                            VIDEO_CODEC_PROFILE_UNKNOWN,
                                            kUnspecifiedLevel, color_space}));
  // HDR10 metadata only works with the PQ transfer.
  EXPECT_FALSE(IsDecoderSupportedVideoType(
      {VideoCodec::kVP8, VP8PROFILE_ANY, kUnspecifiedLevel, color_space,
       gfx::HdrMetadataType::kSmpteSt2086}));

  // ST2094-10 metadata is not supported even if the codec is dolby vision.
  EXPECT_FALSE(IsDecoderSupportedVideoType(
      {VideoCodec::kDolbyVision, DOLBYVISION_PROFILE5, kUnspecifiedLevel,
       color_space, gfx::HdrMetadataType::kSmpteSt2094_10}));

  EXPECT_FALSE(IsDecoderSupportedVideoType(
      {VideoCodec::kVP8, VP8PROFILE_ANY, kUnspecifiedLevel, color_space,
       gfx::HdrMetadataType::kSmpteSt2094_40}));
}

TEST(SupportedTypesTest, IsEncoderSupportedVideoType_H264Profiles) {
  const bool is_h264_supported = IsOpenH264SoftwareEncoderEnabled();

  EXPECT_EQ(
      IsEncoderSupportedVideoType({VideoCodec::kH264, H264PROFILE_BASELINE}),
      is_h264_supported);
  EXPECT_EQ(IsEncoderSupportedVideoType({VideoCodec::kH264, H264PROFILE_MAIN}),
            is_h264_supported);
  EXPECT_EQ(IsEncoderSupportedVideoType({VideoCodec::kH264, H264PROFILE_HIGH}),
            is_h264_supported);
  EXPECT_EQ(
      IsEncoderSupportedVideoType({VideoCodec::kH264, H264PROFILE_EXTENDED}),
      is_h264_supported);
  EXPECT_FALSE(IsEncoderSupportedVideoType(
      {VideoCodec::kH264, H264PROFILE_HIGH10PROFILE}));
  EXPECT_FALSE(IsEncoderSupportedVideoType(
      {VideoCodec::kH264, H264PROFILE_HIGH422PROFILE}));
}

TEST(SupportedTypesTest, IsEncoderSupportedVideoType_VP8Profiles) {
  EXPECT_EQ(IsEncoderSupportedVideoType({VideoCodec::kVP8, VP8PROFILE_ANY}),
            BUILDFLAG(ENABLE_LIBVPX));
}

TEST(SupportedTypesTest, IsEncoderSupportedVideoType_HEVCProfiles) {
  EXPECT_FALSE(
      IsEncoderSupportedVideoType({VideoCodec::kHEVC, HEVCPROFILE_MAIN}));
}

TEST(SupportedTypesTest, IsEncoderSupportedVideoType_VP9Profiles) {
  EXPECT_EQ(
      IsEncoderSupportedVideoType({VideoCodec::kVP9, VP9PROFILE_PROFILE0}),
      BUILDFLAG(ENABLE_LIBVPX));
  EXPECT_EQ(
      IsEncoderSupportedVideoType({VideoCodec::kVP9, VP9PROFILE_PROFILE1}),
      BUILDFLAG(ENABLE_LIBVPX));

// VP9 Profile2 are supported on x86, ChromeOS on ARM and Mac/Win/Linux
// on ARM64. See third_party/libvpx/BUILD.gn.
#if defined(ARCH_CPU_X86_FAMILY) ||                             \
    (defined(ARCH_CPU_ARM_FAMILY) && BUILDFLAG(IS_CHROMEOS)) || \
    (defined(ARCH_CPU_ARM64) &&                                 \
     (BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX)))

  EXPECT_TRUE(
      IsEncoderSupportedVideoType({VideoCodec::kVP9, VP9PROFILE_PROFILE2}));
  EXPECT_TRUE(
      IsEncoderSupportedVideoType({VideoCodec::kVP9, VP9PROFILE_PROFILE3}));
#endif
}

TEST(SupportedTypesTest, IsEncoderSupportedVideoType_AV1Profiles) {
  EXPECT_EQ(
      IsEncoderSupportedVideoType({VideoCodec::kAV1, AV1PROFILE_PROFILE_MAIN}),
      BUILDFLAG(ENABLE_LIBAOM));
  EXPECT_EQ(
      IsEncoderSupportedVideoType({VideoCodec::kAV1, AV1PROFILE_PROFILE_HIGH}),
      BUILDFLAG(ENABLE_LIBAOM));
  EXPECT_FALSE(
      IsEncoderSupportedVideoType({VideoCodec::kAV1, AV1PROFILE_PROFILE_PRO}));
}

TEST(SupportedTypesTest, IsEncoderBuiltInVideoType) {
  // Note that we don't test all the profile since
  // `IsEncoderSupportedVideoType_${*}` tests should already cover this.
  EXPECT_EQ(
      IsEncoderBuiltInVideoType({VideoCodec::kH264, H264PROFILE_BASELINE}),
      IsOpenH264SoftwareEncoderEnabled());
  EXPECT_EQ(
      IsEncoderBuiltInVideoType({VideoCodec::kAV1, AV1PROFILE_PROFILE_MAIN}),
      BUILDFLAG(ENABLE_LIBAOM));
  EXPECT_EQ(IsEncoderBuiltInVideoType({VideoCodec::kVP9, VP9PROFILE_PROFILE0}),
            BUILDFLAG(ENABLE_LIBVPX));
  EXPECT_EQ(IsEncoderBuiltInVideoType({VideoCodec::kVP8, VP8PROFILE_ANY}),
            BUILDFLAG(ENABLE_LIBVPX));

  EXPECT_FALSE(
      IsEncoderBuiltInVideoType({VideoCodec::kHEVC, HEVCPROFILE_MAIN}));
  EXPECT_FALSE(
      IsEncoderBuiltInVideoType({VideoCodec::kAV1, AV1PROFILE_PROFILE_PRO}));
}

TEST(SupportedTypesTest, IsEncoderOptionalVideoType) {
  EXPECT_EQ(
      IsEncoderOptionalVideoType({VideoCodec::kH264, H264PROFILE_BASELINE}),
      !IsOpenH264SoftwareEncoderEnabled());

  EXPECT_EQ(
      IsEncoderOptionalVideoType({VideoCodec::kAV1, AV1PROFILE_PROFILE_MAIN}),
      !BUILDFLAG(ENABLE_LIBAOM));

  EXPECT_EQ(IsEncoderOptionalVideoType({VideoCodec::kHEVC, HEVCPROFILE_MAIN}),
            BUILDFLAG(PLATFORM_HAS_OPTIONAL_HEVC_ENCODE_SUPPORT));

  EXPECT_EQ(IsEncoderOptionalVideoType({VideoCodec::kVP9, VP9PROFILE_PROFILE0}),
            !BUILDFLAG(ENABLE_LIBVPX));
  EXPECT_EQ(IsEncoderOptionalVideoType({VideoCodec::kVP8, VP8PROFILE_ANY}),
            !BUILDFLAG(ENABLE_LIBVPX));
}

TEST(SupportedTypesTest, IsDecoderBuiltInVideoCodec) {
#if BUILDFLAG(USE_PROPRIETARY_CODECS) && BUILDFLAG(ENABLE_FFMPEG_VIDEO_DECODERS)
  EXPECT_TRUE(IsDecoderBuiltInVideoCodec(VideoCodec::kH264));
#else
  EXPECT_FALSE(IsDecoderBuiltInVideoCodec(VideoCodec::kH264));
#endif  // BUILDFLAG(USE_PROPRIETARY_CODECS) &&
        // BUILDFLAG(ENABLE_FFMPEG_VIDEO_DECODERS)

  EXPECT_FALSE(IsDecoderBuiltInVideoCodec(VideoCodec::kTheora));

#if BUILDFLAG(ENABLE_LIBVPX)
  EXPECT_TRUE(IsDecoderBuiltInVideoCodec(VideoCodec::kVP8));
#else
  EXPECT_FALSE(IsDecoderBuiltInVideoCodec(VideoCodec::kVP8));
#endif  // BUILDFLAG(ENABLE_LIBVPX)

#if BUILDFLAG(ENABLE_LIBVPX)
  EXPECT_TRUE(IsDecoderBuiltInVideoCodec(VideoCodec::kVP9));
#else
  EXPECT_FALSE(IsDecoderBuiltInVideoCodec(VideoCodec::kVP9));
#endif  // BUILDFLAG(ENABLE_LIBVPX)

#if BUILDFLAG(ENABLE_AV1_DECODER)
  EXPECT_TRUE(IsDecoderBuiltInVideoCodec(VideoCodec::kAV1));
#else
  EXPECT_FALSE(IsDecoderBuiltInVideoCodec(VideoCodec::kAV1));
#endif  // BUILDFLAG(ENABLE_AV1_DECODER)

  EXPECT_FALSE(IsDecoderBuiltInVideoCodec(VideoCodec::kUnknown));
  EXPECT_FALSE(IsDecoderBuiltInVideoCodec(VideoCodec::kMPEG4));
  EXPECT_FALSE(IsDecoderBuiltInVideoCodec(VideoCodec::kVC1));
  EXPECT_FALSE(IsDecoderBuiltInVideoCodec(VideoCodec::kMPEG2));
  EXPECT_FALSE(IsDecoderBuiltInVideoCodec(VideoCodec::kHEVC));
  EXPECT_FALSE(IsDecoderBuiltInVideoCodec(VideoCodec::kDolbyVision));
}

TEST(SupportedTypesTest, MayHaveAndAllowSelectOSSoftwareEncoder) {
  EXPECT_EQ(MayHaveAndAllowSelectOSSoftwareEncoder(VideoCodec::kHEVC),
            BUILDFLAG(IS_MAC) && BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER));
  EXPECT_EQ(MayHaveAndAllowSelectOSSoftwareEncoder(VideoCodec::kH264),
            (BUILDFLAG(IS_MAC) || BUILDFLAG(IS_ANDROID)) &&
                !IsOpenH264SoftwareEncoderEnabled());
}

TEST(SupportedTypesTest, ColorSpaceSupport_UnusualButValid_VP9) {
  // VP9 calls IsColorSpaceSupported(). All combinations below use valid
  // (non-INVALID) enum values and should be accepted.
  constexpr int kLevel = 0;

  // BT2020 primaries + BT709 transfer (wide-gamut SDR).
  EXPECT_TRUE(IsDecoderSupportedVideoType(
      {VideoCodec::kVP9, VP9PROFILE_PROFILE0, kLevel,
       VideoColorSpace(VideoColorSpace::PrimaryID::BT2020,
                       VideoColorSpace::TransferID::BT709,
                       VideoColorSpace::MatrixID::BT709,
                       gfx::ColorSpace::RangeID::LIMITED)}));

  // SMPTEST432_1 primaries + SMPTEST2084 transfer (DCI-P3 HDR).
  EXPECT_TRUE(IsDecoderSupportedVideoType(
      {VideoCodec::kVP9, VP9PROFILE_PROFILE0, kLevel,
       VideoColorSpace(VideoColorSpace::PrimaryID::SMPTEST432_1,
                       VideoColorSpace::TransferID::SMPTEST2084,
                       VideoColorSpace::MatrixID::BT2020_NCL,
                       gfx::ColorSpace::RangeID::LIMITED)}));

  // SMPTEST431_2 primaries + BT709 transfer (DCI cinema SDR).
  EXPECT_TRUE(IsDecoderSupportedVideoType(
      {VideoCodec::kVP9, VP9PROFILE_PROFILE0, kLevel,
       VideoColorSpace(VideoColorSpace::PrimaryID::SMPTEST431_2,
                       VideoColorSpace::TransferID::BT709,
                       VideoColorSpace::MatrixID::BT709,
                       gfx::ColorSpace::RangeID::LIMITED)}));

  // BT470M primaries + GAMMA22 transfer + FCC matrix (legacy NTSC).
  EXPECT_TRUE(IsDecoderSupportedVideoType(
      {VideoCodec::kVP9, VP9PROFILE_PROFILE0, kLevel,
       VideoColorSpace(VideoColorSpace::PrimaryID::BT470M,
                       VideoColorSpace::TransferID::GAMMA22,
                       VideoColorSpace::MatrixID::FCC,
                       gfx::ColorSpace::RangeID::LIMITED)}));

  // FILM primaries + BT709 transfer.
  EXPECT_TRUE(IsDecoderSupportedVideoType(
      {VideoCodec::kVP9, VP9PROFILE_PROFILE0, kLevel,
       VideoColorSpace(VideoColorSpace::PrimaryID::FILM,
                       VideoColorSpace::TransferID::BT709,
                       VideoColorSpace::MatrixID::BT709,
                       gfx::ColorSpace::RangeID::LIMITED)}));

  // NOTE: BT2020_CL (MatrixID 10) is no longer supported — ToGfxMatrixID
  // maps it to INVALID, causing IsColorSpaceSupported to reject it.
  // See crbug.com/333906350.

  // SMPTEST428_1 primaries + SMPTEST428_1 transfer (XYZ cinema).
  EXPECT_TRUE(IsDecoderSupportedVideoType(
      {VideoCodec::kVP9, VP9PROFILE_PROFILE0, kLevel,
       VideoColorSpace(VideoColorSpace::PrimaryID::SMPTEST428_1,
                       VideoColorSpace::TransferID::SMPTEST428_1,
                       VideoColorSpace::MatrixID::YDZDX,
                       gfx::ColorSpace::RangeID::FULL)}));

  // BT709 primaries + LOG transfer.
  EXPECT_TRUE(IsDecoderSupportedVideoType(
      {VideoCodec::kVP9, VP9PROFILE_PROFILE0, kLevel,
       VideoColorSpace(VideoColorSpace::PrimaryID::BT709,
                       VideoColorSpace::TransferID::LOG,
                       VideoColorSpace::MatrixID::BT709,
                       gfx::ColorSpace::RangeID::LIMITED)}));

  // BT709 primaries + LINEAR transfer.
  EXPECT_TRUE(IsDecoderSupportedVideoType(
      {VideoCodec::kVP9, VP9PROFILE_PROFILE0, kLevel,
       VideoColorSpace(VideoColorSpace::PrimaryID::BT709,
                       VideoColorSpace::TransferID::LINEAR,
                       VideoColorSpace::MatrixID::BT709,
                       gfx::ColorSpace::RangeID::LIMITED)}));

  // RGB matrix with BT709 primaries and transfer.
  EXPECT_TRUE(IsDecoderSupportedVideoType(
      {VideoCodec::kVP9, VP9PROFILE_PROFILE0, kLevel,
       VideoColorSpace(VideoColorSpace::PrimaryID::BT709,
                       VideoColorSpace::TransferID::BT709,
                       VideoColorSpace::MatrixID::RGB,
                       gfx::ColorSpace::RangeID::FULL)}));
}

#if BUILDFLAG(ENABLE_AV1_DECODER)
TEST(SupportedTypesTest, ColorSpaceSupport_UnusualButValid_AV1) {
  constexpr int kLevel = 0;

  // Wide-gamut SDR: BT2020 + BT709.
  EXPECT_TRUE(IsDecoderSupportedVideoType(
      {VideoCodec::kAV1, AV1PROFILE_PROFILE_MAIN, kLevel,
       VideoColorSpace(VideoColorSpace::PrimaryID::BT2020,
                       VideoColorSpace::TransferID::BT709,
                       VideoColorSpace::MatrixID::BT709,
                       gfx::ColorSpace::RangeID::LIMITED)}));

  // DCI-P3 HDR: SMPTEST432_1 + SMPTEST2084.
  EXPECT_TRUE(IsDecoderSupportedVideoType(
      {VideoCodec::kAV1, AV1PROFILE_PROFILE_MAIN, kLevel,
       VideoColorSpace(VideoColorSpace::PrimaryID::SMPTEST432_1,
                       VideoColorSpace::TransferID::SMPTEST2084,
                       VideoColorSpace::MatrixID::BT2020_NCL,
                       gfx::ColorSpace::RangeID::LIMITED)}));

  // NOTE: BT2020_CL no longer supported — see crbug.com/333906350.

  // XYZ cinema: SMPTEST428_1 + SMPTEST428_1.
  EXPECT_TRUE(IsDecoderSupportedVideoType(
      {VideoCodec::kAV1, AV1PROFILE_PROFILE_MAIN, kLevel,
       VideoColorSpace(VideoColorSpace::PrimaryID::SMPTEST428_1,
                       VideoColorSpace::TransferID::SMPTEST428_1,
                       VideoColorSpace::MatrixID::YDZDX,
                       gfx::ColorSpace::RangeID::FULL)}));

  // RGB matrix.
  EXPECT_TRUE(IsDecoderSupportedVideoType(
      {VideoCodec::kAV1, AV1PROFILE_PROFILE_MAIN, kLevel,
       VideoColorSpace(VideoColorSpace::PrimaryID::BT709,
                       VideoColorSpace::TransferID::BT709,
                       VideoColorSpace::MatrixID::RGB,
                       gfx::ColorSpace::RangeID::FULL)}));
}
#endif  // BUILDFLAG(ENABLE_AV1_DECODER)

// H264 does not call IsColorSpaceSupported() — it returns true unconditionally
// (when proprietary codecs are enabled). Verify this behavior, including that
// H264 accepts color spaces that other codecs would reject.
TEST(SupportedTypesTest, ColorSpaceSupport_H264_BypassesColorSpaceCheck) {
  constexpr int kLevel = 1;

  // H264 accepts EBU_3213_E primaries.
  EXPECT_EQ(kPropCodecsEnabled,
            IsDecoderSupportedVideoType(
                {VideoCodec::kH264, H264PROFILE_BASELINE, kLevel,
                 VideoColorSpace(VideoColorSpace::PrimaryID::EBU_3213_E,
                                 VideoColorSpace::TransferID::BT709,
                                 VideoColorSpace::MatrixID::BT709,
                                 gfx::ColorSpace::RangeID::LIMITED)}));

  // H264 accepts all-INVALID color space components.
  EXPECT_EQ(kPropCodecsEnabled,
            IsDecoderSupportedVideoType(
                {VideoCodec::kH264, H264PROFILE_BASELINE, kLevel,
                 VideoColorSpace(VideoColorSpace::PrimaryID::INVALID,
                                 VideoColorSpace::TransferID::INVALID,
                                 VideoColorSpace::MatrixID::INVALID,
                                 gfx::ColorSpace::RangeID::INVALID)}));

  // H264 also accepts valid unusual combinations.
  EXPECT_EQ(kPropCodecsEnabled,
            IsDecoderSupportedVideoType(
                {VideoCodec::kH264, H264PROFILE_BASELINE, kLevel,
                 VideoColorSpace(VideoColorSpace::PrimaryID::BT2020,
                                 VideoColorSpace::TransferID::SMPTEST2084,
                                 VideoColorSpace::MatrixID::BT2020_NCL,
                                 gfx::ColorSpace::RangeID::LIMITED)}));
}

// HEVC calls IsColorSpaceSupported() when HEVC decoding is unconditionally
// enabled, so it rejects invalid color spaces unlike H264.
#if BUILDFLAG(ENABLE_PLATFORM_HEVC) && \
    !BUILDFLAG(PLATFORM_HAS_OPTIONAL_HEVC_DECODE_SUPPORT)
TEST(SupportedTypesTest, ColorSpaceSupport_UnusualButValid_HEVC) {
  constexpr int kLevel = 0;

  // HDR10: BT2020 + PQ.
  EXPECT_TRUE(IsDecoderSupportedVideoType(
      {VideoCodec::kHEVC, HEVCPROFILE_MAIN10, kLevel,
       VideoColorSpace(VideoColorSpace::PrimaryID::BT2020,
                       VideoColorSpace::TransferID::SMPTEST2084,
                       VideoColorSpace::MatrixID::BT2020_NCL,
                       gfx::ColorSpace::RangeID::LIMITED)}));

  // NOTE: BT2020_CL no longer supported — see crbug.com/333906350.

  // HEVC accepts EBU_3213_E (has a valid gfx::ColorSpace mapping).
  EXPECT_TRUE(IsDecoderSupportedVideoType(
      {VideoCodec::kHEVC, HEVCPROFILE_MAIN, kLevel,
       VideoColorSpace(VideoColorSpace::PrimaryID::EBU_3213_E,
                       VideoColorSpace::TransferID::BT709,
                       VideoColorSpace::MatrixID::BT709,
                       gfx::ColorSpace::RangeID::LIMITED)}));
}
#endif  // BUILDFLAG(ENABLE_PLATFORM_HEVC) &&
        // !BUILDFLAG(PLATFORM_HAS_OPTIONAL_HEVC_DECODE_SUPPORT)

TEST(SupportedTypesTest, ColorSpaceBoundary_RejectedPrimaries) {
  constexpr int kLevel = 0;

  // EBU_3213_E is accepted (has a valid gfx::ColorSpace mapping).
  EXPECT_TRUE(IsDecoderSupportedVideoType(
      {VideoCodec::kVP9, VP9PROFILE_PROFILE0, kLevel,
       VideoColorSpace(VideoColorSpace::PrimaryID::EBU_3213_E,
                       VideoColorSpace::TransferID::BT709,
                       VideoColorSpace::MatrixID::BT709,
                       gfx::ColorSpace::RangeID::LIMITED)}));

  // INVALID primary is rejected.
  EXPECT_FALSE(IsDecoderSupportedVideoType(
      {VideoCodec::kVP9, VP9PROFILE_PROFILE0, kLevel,
       VideoColorSpace(VideoColorSpace::PrimaryID::INVALID,
                       VideoColorSpace::TransferID::BT709,
                       VideoColorSpace::MatrixID::BT709,
                       gfx::ColorSpace::RangeID::LIMITED)}));
}

TEST(SupportedTypesTest, ColorSpaceBoundary_RejectedTransfer) {
  constexpr int kLevel = 0;

  // INVALID transfer is the only rejected transfer value.
  EXPECT_FALSE(IsDecoderSupportedVideoType(
      {VideoCodec::kVP9, VP9PROFILE_PROFILE0, kLevel,
       VideoColorSpace(VideoColorSpace::PrimaryID::BT709,
                       VideoColorSpace::TransferID::INVALID,
                       VideoColorSpace::MatrixID::BT709,
                       gfx::ColorSpace::RangeID::LIMITED)}));
}

TEST(SupportedTypesTest, ColorSpaceBoundary_RejectedMatrix) {
  constexpr int kLevel = 0;

  // INVALID matrix is the only rejected matrix value.
  EXPECT_FALSE(IsDecoderSupportedVideoType(
      {VideoCodec::kVP9, VP9PROFILE_PROFILE0, kLevel,
       VideoColorSpace(VideoColorSpace::PrimaryID::BT709,
                       VideoColorSpace::TransferID::BT709,
                       VideoColorSpace::MatrixID::INVALID,
                       gfx::ColorSpace::RangeID::LIMITED)}));
}

TEST(SupportedTypesTest, ColorSpaceBoundary_RejectedRange) {
  constexpr int kLevel = 0;

  // INVALID range is rejected even when primary/transfer/matrix are valid.
  EXPECT_FALSE(IsDecoderSupportedVideoType(
      {VideoCodec::kVP9, VP9PROFILE_PROFILE0, kLevel,
       VideoColorSpace(VideoColorSpace::PrimaryID::BT709,
                       VideoColorSpace::TransferID::BT709,
                       VideoColorSpace::MatrixID::BT709,
                       gfx::ColorSpace::RangeID::INVALID)}));
}

TEST(SupportedTypesTest, ColorSpaceBoundary_AllInvalid) {
  constexpr int kLevel = 0;

  // All INVALID components should be rejected.
  EXPECT_FALSE(IsDecoderSupportedVideoType(
      {VideoCodec::kVP9, VP9PROFILE_PROFILE0, kLevel,
       VideoColorSpace(VideoColorSpace::PrimaryID::INVALID,
                       VideoColorSpace::TransferID::INVALID,
                       VideoColorSpace::MatrixID::INVALID,
                       gfx::ColorSpace::RangeID::INVALID)}));

#if BUILDFLAG(ENABLE_AV1_DECODER)
  // AV1 should also reject all-INVALID.
  EXPECT_FALSE(IsDecoderSupportedVideoType(
      {VideoCodec::kAV1, AV1PROFILE_PROFILE_MAIN, kLevel,
       VideoColorSpace(VideoColorSpace::PrimaryID::INVALID,
                       VideoColorSpace::TransferID::INVALID,
                       VideoColorSpace::MatrixID::INVALID,
                       gfx::ColorSpace::RangeID::INVALID)}));
#endif  // BUILDFLAG(ENABLE_AV1_DECODER)
}

TEST(SupportedTypesTest, ColorSpaceBoundary_MixedSupportedAndUnsupported) {
  constexpr int kLevel = 0;

  // Supported primary + unsupported (INVALID) transfer -> rejected.
  EXPECT_FALSE(IsDecoderSupportedVideoType(
      {VideoCodec::kVP9, VP9PROFILE_PROFILE0, kLevel,
       VideoColorSpace(VideoColorSpace::PrimaryID::BT2020,
                       VideoColorSpace::TransferID::INVALID,
                       VideoColorSpace::MatrixID::BT709,
                       gfx::ColorSpace::RangeID::LIMITED)}));

  // EBU_3213_E primary + supported transfer -> accepted.
  EXPECT_TRUE(IsDecoderSupportedVideoType(
      {VideoCodec::kVP9, VP9PROFILE_PROFILE0, kLevel,
       VideoColorSpace(VideoColorSpace::PrimaryID::EBU_3213_E,
                       VideoColorSpace::TransferID::SMPTEST2084,
                       VideoColorSpace::MatrixID::BT2020_NCL,
                       gfx::ColorSpace::RangeID::LIMITED)}));

  // Supported primary + supported transfer + INVALID matrix -> rejected.
  EXPECT_FALSE(IsDecoderSupportedVideoType(
      {VideoCodec::kVP9, VP9PROFILE_PROFILE0, kLevel,
       VideoColorSpace(VideoColorSpace::PrimaryID::BT709,
                       VideoColorSpace::TransferID::BT709,
                       VideoColorSpace::MatrixID::INVALID,
                       gfx::ColorSpace::RangeID::LIMITED)}));

  // All valid except INVALID range -> rejected.
  EXPECT_FALSE(IsDecoderSupportedVideoType(
      {VideoCodec::kVP9, VP9PROFILE_PROFILE0, kLevel,
       VideoColorSpace(VideoColorSpace::PrimaryID::BT709,
                       VideoColorSpace::TransferID::BT709,
                       VideoColorSpace::MatrixID::BT709,
                       gfx::ColorSpace::RangeID::INVALID)}));
}

TEST(SupportedTypesTest, ColorSpaceBoundary_UnspecifiedComponentsAccepted) {
  constexpr int kLevel = 0;

  // UNSPECIFIED primaries, transfer, and matrix should all be accepted.
  EXPECT_TRUE(IsDecoderSupportedVideoType(
      {VideoCodec::kVP9, VP9PROFILE_PROFILE0, kLevel,
       VideoColorSpace(VideoColorSpace::PrimaryID::UNSPECIFIED,
                       VideoColorSpace::TransferID::UNSPECIFIED,
                       VideoColorSpace::MatrixID::UNSPECIFIED,
                       gfx::ColorSpace::RangeID::LIMITED)}));

  // UNSPECIFIED primary with specified transfer and matrix.
  EXPECT_TRUE(IsDecoderSupportedVideoType(
      {VideoCodec::kVP9, VP9PROFILE_PROFILE0, kLevel,
       VideoColorSpace(VideoColorSpace::PrimaryID::UNSPECIFIED,
                       VideoColorSpace::TransferID::SMPTEST2084,
                       VideoColorSpace::MatrixID::BT2020_NCL,
                       gfx::ColorSpace::RangeID::LIMITED)}));

  // Specified primary with UNSPECIFIED transfer and matrix.
  EXPECT_TRUE(IsDecoderSupportedVideoType(
      {VideoCodec::kVP9, VP9PROFILE_PROFILE0, kLevel,
       VideoColorSpace(VideoColorSpace::PrimaryID::BT2020,
                       VideoColorSpace::TransferID::UNSPECIFIED,
                       VideoColorSpace::MatrixID::UNSPECIFIED,
                       gfx::ColorSpace::RangeID::FULL)}));
}

TEST(SupportedTypesTest, ProfileColorSpace_VP9Profile0WithBT2020) {
  constexpr int kLevel = 0;

  // VP9 Profile 0 should accept BT2020 primaries. The profile check and
  // color space check are independent — Profile 0 is always supported, and
  // BT2020 is a valid primary.
  EXPECT_TRUE(IsDecoderSupportedVideoType(
      {VideoCodec::kVP9, VP9PROFILE_PROFILE0, kLevel,
       VideoColorSpace(VideoColorSpace::PrimaryID::BT2020,
                       VideoColorSpace::TransferID::BT2020_10,
                       VideoColorSpace::MatrixID::BT2020_NCL,
                       gfx::ColorSpace::RangeID::LIMITED)}));

  // Profile 0 with full BT2020 HDR (PQ transfer).
  EXPECT_TRUE(IsDecoderSupportedVideoType(
      {VideoCodec::kVP9, VP9PROFILE_PROFILE0, kLevel,
       VideoColorSpace(VideoColorSpace::PrimaryID::BT2020,
                       VideoColorSpace::TransferID::SMPTEST2084,
                       VideoColorSpace::MatrixID::BT2020_NCL,
                       gfx::ColorSpace::RangeID::LIMITED)}));
}

#if defined(ARCH_CPU_X86_FAMILY) ||                             \
    (defined(ARCH_CPU_ARM_FAMILY) && BUILDFLAG(IS_CHROMEOS)) || \
    (defined(ARCH_CPU_ARM64) && (BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)))
TEST(SupportedTypesTest, ProfileColorSpace_VP9Profile2WithHDR) {
  // VP9 Profile 2 support depends on high bit depth support in libvpx.
  // On architectures where Profile 2 is supported, HDR color spaces
  // should also be accepted.
  constexpr int kLevel = 0;

  // Profile 2 with PQ transfer (HDR10).
  EXPECT_TRUE(IsDecoderSupportedVideoType(
      {VideoCodec::kVP9, VP9PROFILE_PROFILE2, kLevel,
       VideoColorSpace(VideoColorSpace::PrimaryID::BT2020,
                       VideoColorSpace::TransferID::SMPTEST2084,
                       VideoColorSpace::MatrixID::BT2020_NCL,
                       gfx::ColorSpace::RangeID::LIMITED)}));

  // Profile 2 with HLG transfer.
  EXPECT_TRUE(IsDecoderSupportedVideoType(
      {VideoCodec::kVP9, VP9PROFILE_PROFILE2, kLevel,
       VideoColorSpace(VideoColorSpace::PrimaryID::BT2020,
                       VideoColorSpace::TransferID::ARIB_STD_B67,
                       VideoColorSpace::MatrixID::BT2020_NCL,
                       gfx::ColorSpace::RangeID::LIMITED)}));

  // Profile 2 should still reject INVALID color spaces.
  EXPECT_FALSE(IsDecoderSupportedVideoType(
      {VideoCodec::kVP9, VP9PROFILE_PROFILE2, kLevel,
       VideoColorSpace(VideoColorSpace::PrimaryID::INVALID,
                       VideoColorSpace::TransferID::BT709,
                       VideoColorSpace::MatrixID::BT709,
                       gfx::ColorSpace::RangeID::LIMITED)}));
}
#endif

#if BUILDFLAG(ENABLE_AV1_DECODER)
TEST(SupportedTypesTest, ProfileColorSpace_AV1MainWithBT2020PQ) {
  constexpr int kLevel = 0;

  // AV1 Main profile with BT2020 primaries and PQ transfer (HDR10).
  EXPECT_TRUE(IsDecoderSupportedVideoType(
      {VideoCodec::kAV1, AV1PROFILE_PROFILE_MAIN, kLevel,
       VideoColorSpace(VideoColorSpace::PrimaryID::BT2020,
                       VideoColorSpace::TransferID::SMPTEST2084,
                       VideoColorSpace::MatrixID::BT2020_NCL,
                       gfx::ColorSpace::RangeID::LIMITED)}));

  // AV1 Main profile with HLG.
  EXPECT_TRUE(IsDecoderSupportedVideoType(
      {VideoCodec::kAV1, AV1PROFILE_PROFILE_MAIN, kLevel,
       VideoColorSpace(VideoColorSpace::PrimaryID::BT2020,
                       VideoColorSpace::TransferID::ARIB_STD_B67,
                       VideoColorSpace::MatrixID::BT2020_NCL,
                       gfx::ColorSpace::RangeID::LIMITED)}));

  // AV1 should reject invalid color spaces even with valid profile.
  EXPECT_FALSE(IsDecoderSupportedVideoType(
      {VideoCodec::kAV1, AV1PROFILE_PROFILE_MAIN, kLevel,
       VideoColorSpace(VideoColorSpace::PrimaryID::INVALID,
                       VideoColorSpace::TransferID::BT709,
                       VideoColorSpace::MatrixID::BT709,
                       gfx::ColorSpace::RangeID::LIMITED)}));
}
#endif  // BUILDFLAG(ENABLE_AV1_DECODER)

TEST(SupportedTypesTest, ProfileColorSpace_H264High10WithBT2020) {
  constexpr int kLevel = 1;

  // H264 High 10 profile with BT2020 primaries. H264 doesn't check
  // IsColorSpaceSupported, so this is accepted regardless of color space.
  EXPECT_EQ(kPropCodecsEnabled,
            IsDecoderSupportedVideoType(
                {VideoCodec::kH264, H264PROFILE_HIGH10PROFILE, kLevel,
                 VideoColorSpace(VideoColorSpace::PrimaryID::BT2020,
                                 VideoColorSpace::TransferID::SMPTEST2084,
                                 VideoColorSpace::MatrixID::BT2020_NCL,
                                 gfx::ColorSpace::RangeID::LIMITED)}));

  // H264 High 10 even accepts INVALID color spaces.
  EXPECT_EQ(kPropCodecsEnabled,
            IsDecoderSupportedVideoType(
                {VideoCodec::kH264, H264PROFILE_HIGH10PROFILE, kLevel,
                 VideoColorSpace(VideoColorSpace::PrimaryID::INVALID,
                                 VideoColorSpace::TransferID::INVALID,
                                 VideoColorSpace::MatrixID::INVALID,
                                 gfx::ColorSpace::RangeID::INVALID)}));
}

TEST(SupportedTypesTest, HdrMetadata_DolbyVision) {
  // DolbyVision with SmpteSt2086 + PQ transfer should be accepted
  // (IsDecoderSupportedHdrMetadata checks transfer, not codec).
  // Note: DV decoder support itself is platform-dependent, so we only test
  // that HDR metadata validation doesn't independently reject it.
  const VideoColorSpace kPqColorSpace(VideoColorSpace::PrimaryID::BT2020,
                                      VideoColorSpace::TransferID::SMPTEST2084,
                                      VideoColorSpace::MatrixID::BT2020_NCL,
                                      gfx::ColorSpace::RangeID::LIMITED);

  // DV + SmpteSt2094_10 is always rejected (not Dolby Vision RPU metadata).
  EXPECT_FALSE(IsDecoderSupportedVideoType(
      {VideoCodec::kDolbyVision, DOLBYVISION_PROFILE5, 0, kPqColorSpace,
       gfx::HdrMetadataType::kSmpteSt2094_10}));

  // DV + SmpteSt2094_40 is always rejected.
  EXPECT_FALSE(IsDecoderSupportedVideoType(
      {VideoCodec::kDolbyVision, DOLBYVISION_PROFILE5, 0, kPqColorSpace,
       gfx::HdrMetadataType::kSmpteSt2094_40}));

  // DV + SmpteSt2086 with non-PQ transfer is rejected by HDR metadata check.
  const VideoColorSpace kHlgColorSpace(
      VideoColorSpace::PrimaryID::BT2020,
      VideoColorSpace::TransferID::ARIB_STD_B67,
      VideoColorSpace::MatrixID::BT2020_NCL, gfx::ColorSpace::RangeID::LIMITED);
  EXPECT_FALSE(IsDecoderSupportedVideoType(
      {VideoCodec::kDolbyVision, DOLBYVISION_PROFILE5, 0, kHlgColorSpace,
       gfx::HdrMetadataType::kSmpteSt2086}));
}

// Gap 2: H264 + HDR metadata. H264 bypasses color space checks but
// IsDecoderSupportedHdrMetadata still runs before codec dispatch.
TEST(SupportedTypesTest, HdrMetadata_H264) {
  constexpr int kLevel = 1;

  // H264 + SmpteSt2086 + PQ transfer: metadata check passes, H264 returns
  // true unconditionally.
  EXPECT_EQ(kPropCodecsEnabled,
            IsDecoderSupportedVideoType(
                {VideoCodec::kH264, H264PROFILE_BASELINE, kLevel,
                 VideoColorSpace(VideoColorSpace::PrimaryID::BT2020,
                                 VideoColorSpace::TransferID::SMPTEST2084,
                                 VideoColorSpace::MatrixID::BT2020_NCL,
                                 gfx::ColorSpace::RangeID::LIMITED),
                 gfx::HdrMetadataType::kSmpteSt2086}));

  // H264 + SmpteSt2086 + non-PQ transfer: rejected by HDR metadata check
  // (before codec dispatch).
  EXPECT_FALSE(IsDecoderSupportedVideoType(
      {VideoCodec::kH264, H264PROFILE_BASELINE, kLevel,
       VideoColorSpace(VideoColorSpace::PrimaryID::BT2020,
                       VideoColorSpace::TransferID::ARIB_STD_B67,
                       VideoColorSpace::MatrixID::BT2020_NCL,
                       gfx::ColorSpace::RangeID::LIMITED),
       gfx::HdrMetadataType::kSmpteSt2086}));

  // H264 + SmpteSt2094_10: always rejected regardless of codec.
  EXPECT_FALSE(IsDecoderSupportedVideoType(
      {VideoCodec::kH264, H264PROFILE_BASELINE, kLevel,
       VideoColorSpace(VideoColorSpace::PrimaryID::BT2020,
                       VideoColorSpace::TransferID::SMPTEST2084,
                       VideoColorSpace::MatrixID::BT2020_NCL,
                       gfx::ColorSpace::RangeID::LIMITED),
       gfx::HdrMetadataType::kSmpteSt2094_10}));
}

// Gap 3: UNSPECIFIED transfer + HDR metadata. IsHDR() returns false for
// UNSPECIFIED, so SmpteSt2086 should be rejected since transfer != PQ.
TEST(SupportedTypesTest, HdrMetadata_UnspecifiedTransfer) {
  // UNSPECIFIED transfer with SmpteSt2086 metadata → rejected.
  EXPECT_FALSE(IsDecoderSupportedVideoType(
      {VideoCodec::kVP8, VP8PROFILE_ANY, 0,
       VideoColorSpace(VideoColorSpace::PrimaryID::BT2020,
                       VideoColorSpace::TransferID::UNSPECIFIED,
                       VideoColorSpace::MatrixID::BT2020_NCL,
                       gfx::ColorSpace::RangeID::LIMITED),
       gfx::HdrMetadataType::kSmpteSt2086}));

  // UNSPECIFIED transfer without metadata → accepted (VP8 doesn't check
  // color space).
  EXPECT_TRUE(IsDecoderSupportedVideoType(
      {VideoCodec::kVP8, VP8PROFILE_ANY, 0,
       VideoColorSpace(VideoColorSpace::PrimaryID::BT2020,
                       VideoColorSpace::TransferID::UNSPECIFIED,
                       VideoColorSpace::MatrixID::BT2020_NCL,
                       gfx::ColorSpace::RangeID::LIMITED)}));
}

#if BUILDFLAG(ENABLE_PLATFORM_HEVC) && \
    !BUILDFLAG(PLATFORM_HAS_OPTIONAL_HEVC_DECODE_SUPPORT)
TEST(SupportedTypesTest, ColorSpaceSupport_HEVC_AlwaysValidated) {
  // HEVC should reject INVALID color space regardless of platform support
  // configuration.
  EXPECT_FALSE(IsDecoderSupportedVideoType(
      {VideoCodec::kHEVC, HEVCPROFILE_MAIN, 0,
       VideoColorSpace(VideoColorSpace::PrimaryID::INVALID,
                       VideoColorSpace::TransferID::BT709,
                       VideoColorSpace::MatrixID::BT709,
                       gfx::ColorSpace::RangeID::LIMITED)}));

  // HEVC accepts EBU_3213_E (has a valid gfx::ColorSpace mapping).
  EXPECT_TRUE(IsDecoderSupportedVideoType(
      {VideoCodec::kHEVC, HEVCPROFILE_MAIN, 0,
       VideoColorSpace(VideoColorSpace::PrimaryID::EBU_3213_E,
                       VideoColorSpace::TransferID::BT709,
                       VideoColorSpace::MatrixID::BT709,
                       gfx::ColorSpace::RangeID::LIMITED)}));

  // HEVC should reject INVALID range.
  EXPECT_FALSE(IsDecoderSupportedVideoType(
      {VideoCodec::kHEVC, HEVCPROFILE_MAIN, 0,
       VideoColorSpace(VideoColorSpace::PrimaryID::BT709,
                       VideoColorSpace::TransferID::BT709,
                       VideoColorSpace::MatrixID::BT709,
                       gfx::ColorSpace::RangeID::INVALID)}));
}
#endif  // BUILDFLAG(ENABLE_PLATFORM_HEVC) &&
        // !BUILDFLAG(PLATFORM_HAS_OPTIONAL_HEVC_DECODE_SUPPORT)

// Gap 5: VP8 does not call IsColorSpaceSupported — it accepts any color space.
TEST(SupportedTypesTest, ColorSpaceSupport_VP8_NoValidation) {
  // VP8 accepts EBU_3213_E.
  EXPECT_TRUE(IsDecoderSupportedVideoType(
      {VideoCodec::kVP8, VP8PROFILE_ANY, 0,
       VideoColorSpace(VideoColorSpace::PrimaryID::EBU_3213_E,
                       VideoColorSpace::TransferID::BT709,
                       VideoColorSpace::MatrixID::BT709,
                       gfx::ColorSpace::RangeID::LIMITED)}));

  // VP8 accepts all-INVALID color space components.
  EXPECT_TRUE(IsDecoderSupportedVideoType(
      {VideoCodec::kVP8, VP8PROFILE_ANY, 0,
       VideoColorSpace(VideoColorSpace::PrimaryID::INVALID,
                       VideoColorSpace::TransferID::INVALID,
                       VideoColorSpace::MatrixID::INVALID,
                       gfx::ColorSpace::RangeID::INVALID)}));
}

// Gap 6: SmpteSt2086 metadata with various non-PQ HDR-adjacent transfers.
// Only PQ (SMPTEST2084) should be accepted; all others must be rejected.
TEST(SupportedTypesTest, HdrMetadata_SmpteSt2086_RequiresPQ) {
  constexpr int kLevel = 0;

  // PQ transfer + SmpteSt2086 → accepted.
  EXPECT_TRUE(IsDecoderSupportedVideoType(
      {VideoCodec::kVP8, VP8PROFILE_ANY, kLevel,
       VideoColorSpace(VideoColorSpace::PrimaryID::BT2020,
                       VideoColorSpace::TransferID::SMPTEST2084,
                       VideoColorSpace::MatrixID::BT2020_NCL,
                       gfx::ColorSpace::RangeID::LIMITED),
       gfx::HdrMetadataType::kSmpteSt2086}));

  // BT2020_10 transfer + SmpteSt2086 → rejected.
  EXPECT_FALSE(IsDecoderSupportedVideoType(
      {VideoCodec::kVP8, VP8PROFILE_ANY, kLevel,
       VideoColorSpace(VideoColorSpace::PrimaryID::BT2020,
                       VideoColorSpace::TransferID::BT2020_10,
                       VideoColorSpace::MatrixID::BT2020_NCL,
                       gfx::ColorSpace::RangeID::LIMITED),
       gfx::HdrMetadataType::kSmpteSt2086}));

  // BT2020_12 transfer + SmpteSt2086 → rejected.
  EXPECT_FALSE(IsDecoderSupportedVideoType(
      {VideoCodec::kVP8, VP8PROFILE_ANY, kLevel,
       VideoColorSpace(VideoColorSpace::PrimaryID::BT2020,
                       VideoColorSpace::TransferID::BT2020_12,
                       VideoColorSpace::MatrixID::BT2020_NCL,
                       gfx::ColorSpace::RangeID::LIMITED),
       gfx::HdrMetadataType::kSmpteSt2086}));

  // LINEAR transfer + SmpteSt2086 → rejected.
  EXPECT_FALSE(IsDecoderSupportedVideoType(
      {VideoCodec::kVP8, VP8PROFILE_ANY, kLevel,
       VideoColorSpace(VideoColorSpace::PrimaryID::BT709,
                       VideoColorSpace::TransferID::LINEAR,
                       VideoColorSpace::MatrixID::BT709,
                       gfx::ColorSpace::RangeID::LIMITED),
       gfx::HdrMetadataType::kSmpteSt2086}));

  // BT709 transfer + SmpteSt2086 → rejected.
  EXPECT_FALSE(IsDecoderSupportedVideoType(
      {VideoCodec::kVP8, VP8PROFILE_ANY, kLevel,
       VideoColorSpace(VideoColorSpace::PrimaryID::BT709,
                       VideoColorSpace::TransferID::BT709,
                       VideoColorSpace::MatrixID::BT709,
                       gfx::ColorSpace::RangeID::LIMITED),
       gfx::HdrMetadataType::kSmpteSt2086}));

  // HLG (ARIB_STD_B67) transfer + SmpteSt2086 → rejected.
  EXPECT_FALSE(IsDecoderSupportedVideoType(
      {VideoCodec::kVP8, VP8PROFILE_ANY, kLevel,
       VideoColorSpace(VideoColorSpace::PrimaryID::BT2020,
                       VideoColorSpace::TransferID::ARIB_STD_B67,
                       VideoColorSpace::MatrixID::BT2020_NCL,
                       gfx::ColorSpace::RangeID::LIMITED),
       gfx::HdrMetadataType::kSmpteSt2086}));
}

}  // namespace media
