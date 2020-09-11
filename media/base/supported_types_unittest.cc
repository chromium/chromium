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
  const media::VideoColorSpace kColorSpace = media::VideoColorSpace::REC709();

  // Some codecs do not have a notion of level.
  const int kUnspecifiedLevel = 0;

  // Expect support for baseline configuration of known codecs.
  EXPECT_TRUE(IsSupportedVideoType({media::kCodecVP8, media::VP8PROFILE_ANY,
                                    kUnspecifiedLevel, kColorSpace}));
  EXPECT_TRUE(
      IsSupportedVideoType({media::kCodecVP9, media::VP9PROFILE_PROFILE0,
                            kUnspecifiedLevel, kColorSpace}));
  EXPECT_TRUE(IsSupportedVideoType({media::kCodecTheora,
                                    media::VIDEO_CODEC_PROFILE_UNKNOWN,
                                    kUnspecifiedLevel, kColorSpace}));

  // Expect non-support for the following.
  EXPECT_FALSE(IsSupportedVideoType({media::kUnknownVideoCodec,
                                     media::VIDEO_CODEC_PROFILE_UNKNOWN,
                                     kUnspecifiedLevel, kColorSpace}));
  EXPECT_FALSE(IsSupportedVideoType({media::kCodecVC1,
                                     media::VIDEO_CODEC_PROFILE_UNKNOWN,
                                     kUnspecifiedLevel, kColorSpace}));
  EXPECT_FALSE(IsSupportedVideoType({media::kCodecMPEG2,
                                     media::VIDEO_CODEC_PROFILE_UNKNOWN,
                                     kUnspecifiedLevel, kColorSpace}));
  EXPECT_FALSE(IsSupportedVideoType({media::kCodecHEVC,
                                     media::VIDEO_CODEC_PROFILE_UNKNOWN,
                                     kUnspecifiedLevel, kColorSpace}));

  // Expect conditional support for the following.
  EXPECT_EQ(
      kPropCodecsEnabled,
      IsSupportedVideoType(
          {media::kCodecH264, media::H264PROFILE_BASELINE, 1, kColorSpace}));
  EXPECT_EQ(kMpeg4Supported,
            IsSupportedVideoType({media::kCodecMPEG4,
                                  media::VIDEO_CODEC_PROFILE_UNKNOWN,
                                  kUnspecifiedLevel, kColorSpace}));
}

TEST(SupportedTypesTest, IsSupportedVideoType_VP9TransferFunctions) {
  size_t num_found = 0;
  // TODO(hubbe): Verify support for HDR codecs when color management enabled.
  const std::set<media::VideoColorSpace::TransferID> kSupportedTransfers = {
      media::VideoColorSpace::TransferID::GAMMA22,
      media::VideoColorSpace::TransferID::UNSPECIFIED,
      media::VideoColorSpace::TransferID::BT709,
      media::VideoColorSpace::TransferID::SMPTE170M,
      media::VideoColorSpace::TransferID::BT2020_10,
      media::VideoColorSpace::TransferID::BT2020_12,
      media::VideoColorSpace::TransferID::IEC61966_2_1,
      media::VideoColorSpace::TransferID::GAMMA28,
      media::VideoColorSpace::TransferID::SMPTE240M,
      media::VideoColorSpace::TransferID::LINEAR,
      media::VideoColorSpace::TransferID::LOG,
      media::VideoColorSpace::TransferID::LOG_SQRT,
      media::VideoColorSpace::TransferID::BT1361_ECG,
      media::VideoColorSpace::TransferID::SMPTEST2084,
      media::VideoColorSpace::TransferID::IEC61966_2_4,
      media::VideoColorSpace::TransferID::SMPTEST428_1,
      media::VideoColorSpace::TransferID::ARIB_STD_B67,
  };

  for (int i = 0; i <= (1 << (8 * sizeof(media::VideoColorSpace::TransferID)));
       i++) {
    media::VideoColorSpace color_space = media::VideoColorSpace::REC709();
    color_space.transfer = media::VideoColorSpace::GetTransferID(i);
    bool found = kSupportedTransfers.find(color_space.transfer) !=
                 kSupportedTransfers.end();
    if (found)
      num_found++;
    EXPECT_EQ(found, IsSupportedVideoType({media::kCodecVP9,
                                           media::VP9PROFILE_PROFILE0, 1,
                                           color_space}));
  }
  EXPECT_EQ(kSupportedTransfers.size(), num_found);
}

TEST(SupportedTypesTest, IsSupportedVideoType_VP9Primaries) {
  size_t num_found = 0;
  // TODO(hubbe): Verify support for HDR codecs when color management enabled.
  const std::set<media::VideoColorSpace::PrimaryID> kSupportedPrimaries = {
      media::VideoColorSpace::PrimaryID::BT709,
      media::VideoColorSpace::PrimaryID::UNSPECIFIED,
      media::VideoColorSpace::PrimaryID::BT470M,
      media::VideoColorSpace::PrimaryID::BT470BG,
      media::VideoColorSpace::PrimaryID::SMPTE170M,
      media::VideoColorSpace::PrimaryID::SMPTE240M,
      media::VideoColorSpace::PrimaryID::FILM,
      media::VideoColorSpace::PrimaryID::BT2020,
      media::VideoColorSpace::PrimaryID::SMPTEST428_1,
      media::VideoColorSpace::PrimaryID::SMPTEST431_2,
      media::VideoColorSpace::PrimaryID::SMPTEST432_1,
  };

  for (int i = 0; i <= (1 << (8 * sizeof(media::VideoColorSpace::PrimaryID)));
       i++) {
    media::VideoColorSpace color_space = media::VideoColorSpace::REC709();
    color_space.primaries = media::VideoColorSpace::GetPrimaryID(i);
    bool found = kSupportedPrimaries.find(color_space.primaries) !=
                 kSupportedPrimaries.end();
    if (found)
      num_found++;
    EXPECT_EQ(found, IsSupportedVideoType({media::kCodecVP9,
                                           media::VP9PROFILE_PROFILE0, 1,
                                           color_space}));
  }
  EXPECT_EQ(kSupportedPrimaries.size(), num_found);
}

TEST(SupportedTypesTest, IsSupportedVideoType_VP9Matrix) {
  size_t num_found = 0;
  // TODO(hubbe): Verify support for HDR codecs when color management enabled.
  const std::set<media::VideoColorSpace::MatrixID> kSupportedMatrix = {
      media::VideoColorSpace::MatrixID::BT709,
      media::VideoColorSpace::MatrixID::UNSPECIFIED,
      media::VideoColorSpace::MatrixID::BT470BG,
      media::VideoColorSpace::MatrixID::SMPTE170M,
      media::VideoColorSpace::MatrixID::BT2020_NCL,
      media::VideoColorSpace::MatrixID::RGB,
      media::VideoColorSpace::MatrixID::FCC,
      media::VideoColorSpace::MatrixID::SMPTE240M,
      media::VideoColorSpace::MatrixID::YCOCG,
      media::VideoColorSpace::MatrixID::YDZDX,
      media::VideoColorSpace::MatrixID::BT2020_CL,
  };

  for (int i = 0; i <= (1 << (8 * sizeof(media::VideoColorSpace::MatrixID)));
       i++) {
    media::VideoColorSpace color_space = media::VideoColorSpace::REC709();
    color_space.matrix = media::VideoColorSpace::GetMatrixID(i);
    bool found =
        kSupportedMatrix.find(color_space.matrix) != kSupportedMatrix.end();
    if (found)
      num_found++;
    EXPECT_EQ(found, IsSupportedVideoType({media::kCodecVP9,
                                           media::VP9PROFILE_PROFILE0, 1,
                                           color_space}));
  }
  EXPECT_EQ(kSupportedMatrix.size(), num_found);
}

TEST(SupportedTypesTest, IsSupportedAudioTypeWithSpatialRenderingBasics) {
  const bool is_spatial_rendering = true;
  // Dolby Atmos = E-AC3 (Dolby Digital Plus) + spatialRendering. Currently not
  // supported.
  EXPECT_FALSE(IsSupportedAudioType(
      {media::kCodecEAC3, AudioCodecProfile::kUnknown, is_spatial_rendering}));

  // Expect non-support for codecs with which there is no spatial audio format.
  EXPECT_FALSE(IsSupportedAudioType(
      {media::kCodecAAC, AudioCodecProfile::kUnknown, is_spatial_rendering}));
  EXPECT_FALSE(IsSupportedAudioType(
      {media::kCodecMP3, AudioCodecProfile::kUnknown, is_spatial_rendering}));
  EXPECT_FALSE(IsSupportedAudioType(
      {media::kCodecPCM, AudioCodecProfile::kUnknown, is_spatial_rendering}));
  EXPECT_FALSE(
      IsSupportedAudioType({media::kCodecVorbis, AudioCodecProfile::kUnknown,
                            is_spatial_rendering}));
  EXPECT_FALSE(IsSupportedAudioType(
      {media::kCodecFLAC, AudioCodecProfile::kUnknown, is_spatial_rendering}));
  EXPECT_FALSE(
      IsSupportedAudioType({media::kCodecAMR_NB, AudioCodecProfile::kUnknown,
                            is_spatial_rendering}));
  EXPECT_FALSE(
      IsSupportedAudioType({media::kCodecAMR_WB, AudioCodecProfile::kUnknown,
                            is_spatial_rendering}));
  EXPECT_FALSE(
      IsSupportedAudioType({media::kCodecPCM_MULAW, AudioCodecProfile::kUnknown,
                            is_spatial_rendering}));
  EXPECT_FALSE(
      IsSupportedAudioType({media::kCodecGSM_MS, AudioCodecProfile::kUnknown,
                            is_spatial_rendering}));
  EXPECT_FALSE(
      IsSupportedAudioType({media::kCodecPCM_S16BE, AudioCodecProfile::kUnknown,
                            is_spatial_rendering}));
  EXPECT_FALSE(
      IsSupportedAudioType({media::kCodecPCM_S24BE, AudioCodecProfile::kUnknown,
                            is_spatial_rendering}));
  EXPECT_FALSE(IsSupportedAudioType(
      {media::kCodecOpus, AudioCodecProfile::kUnknown, is_spatial_rendering}));
  EXPECT_FALSE(
      IsSupportedAudioType({media::kCodecPCM_ALAW, AudioCodecProfile::kUnknown,
                            is_spatial_rendering}));
  EXPECT_FALSE(IsSupportedAudioType(
      {media::kCodecALAC, AudioCodecProfile::kUnknown, is_spatial_rendering}));
  EXPECT_FALSE(IsSupportedAudioType(
      {media::kCodecAC3, AudioCodecProfile::kUnknown, is_spatial_rendering}));
  EXPECT_FALSE(IsSupportedAudioType({media::kCodecMpegHAudio,
                                     AudioCodecProfile::kUnknown,
                                     is_spatial_rendering}));
  EXPECT_FALSE(IsSupportedAudioType({media::kUnknownAudioCodec,
                                     AudioCodecProfile::kUnknown,
                                     is_spatial_rendering}));
}

TEST(SupportedTypesTest, XHE_AACSupportedOnAndroidOnly) {
  // TODO(dalecurtis): Update this test if we ever have support elsewhere.
#if defined(OS_ANDROID)
  const bool is_supported =
      kPropCodecsEnabled &&
      base::android::BuildInfo::GetInstance()->sdk_int() >=
          base::android::SDK_VERSION_P;

  EXPECT_EQ(is_supported,
            IsSupportedAudioType(
                {media::kCodecAAC, AudioCodecProfile::kXHE_AAC, false}));
#else
  EXPECT_FALSE(IsSupportedAudioType(
      {media::kCodecAAC, AudioCodecProfile::kXHE_AAC, false}));
#endif
}

TEST(SupportedTypesTest, IsSupportedVideoTypeWithHdrMetadataBasics) {
  // Default to common 709.
  media::VideoColorSpace color_space = media::VideoColorSpace::REC709();

  // Some codecs do not have a notion of level.
  const int kUnspecifiedLevel = 0;

  // Expect support for baseline configuration of known codecs.
  EXPECT_TRUE(IsSupportedVideoType({media::kCodecVP8, media::VP8PROFILE_ANY,
                                    kUnspecifiedLevel, color_space}));
  EXPECT_TRUE(
      IsSupportedVideoType({media::kCodecVP9, media::VP9PROFILE_PROFILE0,
                            kUnspecifiedLevel, color_space}));
  EXPECT_TRUE(IsSupportedVideoType({media::kCodecTheora,
                                    media::VIDEO_CODEC_PROFILE_UNKNOWN,
                                    kUnspecifiedLevel, color_space}));

  // All combinations of combinations of color gamuts and transfer functions
  // should be supported.
  color_space.primaries = media::VideoColorSpace::PrimaryID::SMPTEST431_2;
  color_space.transfer = media::VideoColorSpace::TransferID::SMPTEST2084;
  EXPECT_TRUE(IsSupportedVideoType({media::kCodecVP8, media::VP8PROFILE_ANY,
                                    kUnspecifiedLevel, color_space}));
  EXPECT_TRUE(
      IsSupportedVideoType({media::kCodecVP9, media::VP9PROFILE_PROFILE0,
                            kUnspecifiedLevel, color_space}));
  EXPECT_TRUE(IsSupportedVideoType({media::kCodecTheora,
                                    media::VIDEO_CODEC_PROFILE_UNKNOWN,
                                    kUnspecifiedLevel, color_space}));

  color_space.primaries = media::VideoColorSpace::PrimaryID::BT2020;
  color_space.transfer = media::VideoColorSpace::TransferID::ARIB_STD_B67;
  EXPECT_TRUE(IsSupportedVideoType({media::kCodecVP8, media::VP8PROFILE_ANY,
                                    kUnspecifiedLevel, color_space}));
  EXPECT_TRUE(
      IsSupportedVideoType({media::kCodecVP9, media::VP9PROFILE_PROFILE0,
                            kUnspecifiedLevel, color_space}));
  EXPECT_TRUE(IsSupportedVideoType({media::kCodecTheora,
                                    media::VIDEO_CODEC_PROFILE_UNKNOWN,
                                    kUnspecifiedLevel, color_space}));

  // No HDR metadata types are supported.
  EXPECT_FALSE(IsSupportedVideoType({media::kCodecVP8, media::VP8PROFILE_ANY,
                                     kUnspecifiedLevel, color_space,
                                     gl::HdrMetadataType::kSmpteSt2086}));

  EXPECT_FALSE(IsSupportedVideoType({media::kCodecVP8, media::VP8PROFILE_ANY,
                                     kUnspecifiedLevel, color_space,
                                     gl::HdrMetadataType::kSmpteSt2094_10}));

  EXPECT_FALSE(IsSupportedVideoType({media::kCodecVP8, media::VP8PROFILE_ANY,
                                     kUnspecifiedLevel, color_space,
                                     gl::HdrMetadataType::kSmpteSt2094_40}));
}
}  // namespace media
