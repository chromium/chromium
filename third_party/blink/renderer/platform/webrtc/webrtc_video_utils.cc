// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/webrtc/webrtc_video_utils.h"

#include "base/logging.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/webrtc/api/video_codecs/h264_profile_level_id.h"
#if BUILDFLAG(RTC_USE_H265)
#include "third_party/webrtc/api/video_codecs/h265_profile_tier_level.h"
#endif  // BUILDFLAG(RTC_USE_H265)
#include "third_party/webrtc/api/video_codecs/video_codec.h"
#include "third_party/webrtc/api/video_codecs/vp9_profile.h"

namespace blink {

media::VideoRotation WebRtcToMediaVideoRotation(
    webrtc::VideoRotation rotation) {
  switch (rotation) {
    case webrtc::kVideoRotation_0:
      return media::VIDEO_ROTATION_0;
    case webrtc::kVideoRotation_90:
      return media::VIDEO_ROTATION_90;
    case webrtc::kVideoRotation_180:
      return media::VIDEO_ROTATION_180;
    case webrtc::kVideoRotation_270:
      return media::VIDEO_ROTATION_270;
  }
  return media::VIDEO_ROTATION_0;
}

media::VideoCodec WebRtcToMediaVideoCodec(webrtc::VideoCodecType codec) {
  switch (codec) {
    case webrtc::kVideoCodecAV1:
      return media::VideoCodec::kAV1;
    case webrtc::kVideoCodecVP8:
      return media::VideoCodec::kVP8;
    case webrtc::kVideoCodecVP9:
      return media::VideoCodec::kVP9;
    case webrtc::kVideoCodecH264:
      return media::VideoCodec::kH264;
#if BUILDFLAG(RTC_USE_H265)
    case webrtc::kVideoCodecH265:
      return media::VideoCodec::kHEVC;
#endif  // BUILDFLAG(RTC_USE_H265)
    default:
      return media::VideoCodec::kUnknown;
  }
}

media::VideoCodecProfile WebRtcVideoFormatToMediaVideoCodecProfile(
    const webrtc::SdpVideoFormat& format) {
  const webrtc::VideoCodecType video_codec_type =
      webrtc::PayloadStringToCodecType(format.name);
  switch (video_codec_type) {
    case webrtc::kVideoCodecAV1:
      return media::AV1PROFILE_PROFILE_MAIN;
    case webrtc::kVideoCodecVP8:
      return media::VP8PROFILE_ANY;
    case webrtc::kVideoCodecVP9: {
      const std::optional<webrtc::VP9Profile> vp9_profile =
          webrtc::ParseSdpForVP9Profile(format.parameters);
      // The return value is std::nullopt if the profile-id is specified
      // but its value is invalid.
      if (!vp9_profile) {
        return media::VIDEO_CODEC_PROFILE_UNKNOWN;
      }
      switch (*vp9_profile) {
        case webrtc::VP9Profile::kProfile2:
          return media::VP9PROFILE_PROFILE2;
        case webrtc::VP9Profile::kProfile1:
          return media::VP9PROFILE_PROFILE1;
        case webrtc::VP9Profile::kProfile0:
        default:
          return media::VP9PROFILE_PROFILE0;
      }
    }
    case webrtc::kVideoCodecH264: {
      const std::optional<webrtc::H264ProfileLevelId> h264_profile_level_id =
          webrtc::ParseSdpForH264ProfileLevelId(format.parameters);
      // The return value is std::nullopt if the profile-level-id is specified
      // but its value is invalid.
      if (!h264_profile_level_id) {
        return media::VIDEO_CODEC_PROFILE_UNKNOWN;
      }
      switch (h264_profile_level_id->profile) {
        case webrtc::H264Profile::kProfileMain:
          return media::H264PROFILE_MAIN;
        case webrtc::H264Profile::kProfileConstrainedHigh:
        case webrtc::H264Profile::kProfileHigh:
          return media::H264PROFILE_HIGH;
        case webrtc::H264Profile::kProfileConstrainedBaseline:
        case webrtc::H264Profile::kProfileBaseline:
        default:
          return media::H264PROFILE_BASELINE;
      }
    }
#if BUILDFLAG(RTC_USE_H265)
    case webrtc::kVideoCodecH265: {
      const std::optional<webrtc::H265ProfileTierLevel> h265_ptl =
          webrtc::ParseSdpForH265ProfileTierLevel(format.parameters);
      if (!h265_ptl) {
        return media::VIDEO_CODEC_PROFILE_UNKNOWN;
      }
      switch (h265_ptl->profile) {
        case webrtc::H265Profile::kProfileMain:
          return media::HEVCPROFILE_MAIN;
        case webrtc::H265Profile::kProfileMain10:
          return media::HEVCPROFILE_MAIN10;
        default:
          return media::VIDEO_CODEC_PROFILE_UNKNOWN;
      }
    }
#endif
    default:
      return media::VIDEO_CODEC_PROFILE_UNKNOWN;
  }
}

gfx::ColorSpace WebRtcToGfxColorSpace(const webrtc::ColorSpace& color_space) {
  gfx::ColorSpace::PrimaryID primaries = gfx::ColorSpace::PrimaryID::INVALID;
  switch (color_space.primaries()) {
    case webrtc::ColorSpace::PrimaryID::kBT709:
    case webrtc::ColorSpace::PrimaryID::kUnspecified:
      primaries = gfx::ColorSpace::PrimaryID::BT709;
      break;
    case webrtc::ColorSpace::PrimaryID::kBT470M:
      primaries = gfx::ColorSpace::PrimaryID::BT470M;
      break;
    case webrtc::ColorSpace::PrimaryID::kBT470BG:
      primaries = gfx::ColorSpace::PrimaryID::BT470BG;
      break;
    case webrtc::ColorSpace::PrimaryID::kSMPTE170M:
      primaries = gfx::ColorSpace::PrimaryID::SMPTE170M;
      break;
    case webrtc::ColorSpace::PrimaryID::kSMPTE240M:
      primaries = gfx::ColorSpace::PrimaryID::SMPTE240M;
      break;
    case webrtc::ColorSpace::PrimaryID::kFILM:
      primaries = gfx::ColorSpace::PrimaryID::FILM;
      break;
    case webrtc::ColorSpace::PrimaryID::kBT2020:
      primaries = gfx::ColorSpace::PrimaryID::BT2020;
      break;
    case webrtc::ColorSpace::PrimaryID::kSMPTEST428:
      primaries = gfx::ColorSpace::PrimaryID::SMPTEST428_1;
      break;
    case webrtc::ColorSpace::PrimaryID::kSMPTEST431:
      primaries = gfx::ColorSpace::PrimaryID::SMPTEST431_2;
      break;
    case webrtc::ColorSpace::PrimaryID::kSMPTEST432:
      primaries = gfx::ColorSpace::PrimaryID::P3;
      break;
    case webrtc::ColorSpace::PrimaryID::kJEDECP22:
      primaries = gfx::ColorSpace::PrimaryID::EBU_3213_E;
      break;
    default:
      break;
  }

  gfx::ColorSpace::TransferID transfer = gfx::ColorSpace::TransferID::INVALID;
  switch (color_space.transfer()) {
    case webrtc::ColorSpace::TransferID::kBT709:
    case webrtc::ColorSpace::TransferID::kUnspecified:
      transfer = gfx::ColorSpace::TransferID::BT709;
      break;
    case webrtc::ColorSpace::TransferID::kGAMMA22:
      transfer = gfx::ColorSpace::TransferID::GAMMA22;
      break;
    case webrtc::ColorSpace::TransferID::kGAMMA28:
      transfer = gfx::ColorSpace::TransferID::GAMMA28;
      break;
    case webrtc::ColorSpace::TransferID::kSMPTE170M:
      transfer = gfx::ColorSpace::TransferID::SMPTE170M;
      break;
    case webrtc::ColorSpace::TransferID::kSMPTE240M:
      transfer = gfx::ColorSpace::TransferID::SMPTE240M;
      break;
    case webrtc::ColorSpace::TransferID::kLINEAR:
      transfer = gfx::ColorSpace::TransferID::LINEAR;
      break;
    case webrtc::ColorSpace::TransferID::kLOG:
      transfer = gfx::ColorSpace::TransferID::LOG;
      break;
    case webrtc::ColorSpace::TransferID::kLOG_SQRT:
      transfer = gfx::ColorSpace::TransferID::LOG_SQRT;
      break;
    case webrtc::ColorSpace::TransferID::kIEC61966_2_4:
      transfer = gfx::ColorSpace::TransferID::IEC61966_2_4;
      break;
    case webrtc::ColorSpace::TransferID::kBT1361_ECG:
      transfer = gfx::ColorSpace::TransferID::BT1361_ECG;
      break;
    case webrtc::ColorSpace::TransferID::kIEC61966_2_1:
      transfer = gfx::ColorSpace::TransferID::SRGB;
      break;
    case webrtc::ColorSpace::TransferID::kBT2020_10:
      transfer = gfx::ColorSpace::TransferID::BT2020_10;
      break;
    case webrtc::ColorSpace::TransferID::kBT2020_12:
      transfer = gfx::ColorSpace::TransferID::BT2020_12;
      break;
    case webrtc::ColorSpace::TransferID::kSMPTEST2084:
      transfer = gfx::ColorSpace::TransferID::PQ;
      break;
    case webrtc::ColorSpace::TransferID::kSMPTEST428:
      transfer = gfx::ColorSpace::TransferID::SMPTEST428_1;
      break;
    case webrtc::ColorSpace::TransferID::kARIB_STD_B67:
      transfer = gfx::ColorSpace::TransferID::HLG;
      break;
    default:
      break;
  }

  gfx::ColorSpace::MatrixID matrix = gfx::ColorSpace::MatrixID::INVALID;
  switch (color_space.matrix()) {
    case webrtc::ColorSpace::MatrixID::kRGB:
      matrix = gfx::ColorSpace::MatrixID::RGB;
      break;
    case webrtc::ColorSpace::MatrixID::kBT709:
    case webrtc::ColorSpace::MatrixID::kUnspecified:
      matrix = gfx::ColorSpace::MatrixID::BT709;
      break;
    case webrtc::ColorSpace::MatrixID::kFCC:
      matrix = gfx::ColorSpace::MatrixID::FCC;
      break;
    case webrtc::ColorSpace::MatrixID::kBT470BG:
      matrix = gfx::ColorSpace::MatrixID::BT470BG;
      break;
    case webrtc::ColorSpace::MatrixID::kSMPTE170M:
      matrix = gfx::ColorSpace::MatrixID::SMPTE170M;
      break;
    case webrtc::ColorSpace::MatrixID::kSMPTE240M:
      matrix = gfx::ColorSpace::MatrixID::SMPTE240M;
      break;
    case webrtc::ColorSpace::MatrixID::kYCOCG:
      matrix = gfx::ColorSpace::MatrixID::YCOCG;
      break;
    case webrtc::ColorSpace::MatrixID::kBT2020_NCL:
      matrix = gfx::ColorSpace::MatrixID::BT2020_NCL;
      break;
    case webrtc::ColorSpace::MatrixID::kSMPTE2085:
      matrix = gfx::ColorSpace::MatrixID::YDZDX;
      break;
    default:
      break;
  }

  gfx::ColorSpace::RangeID range = gfx::ColorSpace::RangeID::INVALID;
  switch (color_space.range()) {
    case webrtc::ColorSpace::RangeID::kLimited:
      range = gfx::ColorSpace::RangeID::LIMITED;
      break;
    case webrtc::ColorSpace::RangeID::kFull:
      range = gfx::ColorSpace::RangeID::FULL;
      break;
    default:
      break;
  }

  return gfx::ColorSpace(primaries, transfer, matrix, range);
}

webrtc::ColorSpace GfxToWebRtcColorSpace(const gfx::ColorSpace& color_space) {
  webrtc::ColorSpace::PrimaryID primaries =
      webrtc::ColorSpace::PrimaryID::kUnspecified;
  switch (color_space.GetPrimaryID()) {
    case gfx::ColorSpace::PrimaryID::BT709:
      primaries = webrtc::ColorSpace::PrimaryID::kBT709;
      break;
    case gfx::ColorSpace::PrimaryID::BT470M:
      primaries = webrtc::ColorSpace::PrimaryID::kBT470M;
      break;
    case gfx::ColorSpace::PrimaryID::BT470BG:
      primaries = webrtc::ColorSpace::PrimaryID::kBT470BG;
      break;
    case gfx::ColorSpace::PrimaryID::SMPTE170M:
      primaries = webrtc::ColorSpace::PrimaryID::kSMPTE170M;
      break;
    case gfx::ColorSpace::PrimaryID::SMPTE240M:
      primaries = webrtc::ColorSpace::PrimaryID::kSMPTE240M;
      break;
    case gfx::ColorSpace::PrimaryID::FILM:
      primaries = webrtc::ColorSpace::PrimaryID::kFILM;
      break;
    case gfx::ColorSpace::PrimaryID::BT2020:
      primaries = webrtc::ColorSpace::PrimaryID::kBT2020;
      break;
    case gfx::ColorSpace::PrimaryID::SMPTEST428_1:
      primaries = webrtc::ColorSpace::PrimaryID::kSMPTEST428;
      break;
    case gfx::ColorSpace::PrimaryID::SMPTEST431_2:
      primaries = webrtc::ColorSpace::PrimaryID::kSMPTEST431;
      break;
    case gfx::ColorSpace::PrimaryID::P3:
      primaries = webrtc::ColorSpace::PrimaryID::kSMPTEST432;
      break;
    case gfx::ColorSpace::PrimaryID::EBU_3213_E:
      primaries = webrtc::ColorSpace::PrimaryID::kJEDECP22;
      break;
    default:
      DVLOG(1) << "Unsupported color primaries.";
      break;
  }

  webrtc::ColorSpace::TransferID transfer =
      webrtc::ColorSpace::TransferID::kUnspecified;
  switch (color_space.GetTransferID()) {
    case gfx::ColorSpace::TransferID::BT709:
      transfer = webrtc::ColorSpace::TransferID::kBT709;
      break;
    case gfx::ColorSpace::TransferID::GAMMA22:
      transfer = webrtc::ColorSpace::TransferID::kGAMMA22;
      break;
    case gfx::ColorSpace::TransferID::GAMMA28:
      transfer = webrtc::ColorSpace::TransferID::kGAMMA28;
      break;
    case gfx::ColorSpace::TransferID::SMPTE170M:
      transfer = webrtc::ColorSpace::TransferID::kSMPTE170M;
      break;
    case gfx::ColorSpace::TransferID::SMPTE240M:
      transfer = webrtc::ColorSpace::TransferID::kSMPTE240M;
      break;
    case gfx::ColorSpace::TransferID::LINEAR:
      transfer = webrtc::ColorSpace::TransferID::kLINEAR;
      break;
    case gfx::ColorSpace::TransferID::LOG:
      transfer = webrtc::ColorSpace::TransferID::kLOG;
      break;
    case gfx::ColorSpace::TransferID::LOG_SQRT:
      transfer = webrtc::ColorSpace::TransferID::kLOG_SQRT;
      break;
    case gfx::ColorSpace::TransferID::IEC61966_2_4:
      transfer = webrtc::ColorSpace::TransferID::kIEC61966_2_4;
      break;
    case gfx::ColorSpace::TransferID::BT1361_ECG:
      transfer = webrtc::ColorSpace::TransferID::kBT1361_ECG;
      break;
    case gfx::ColorSpace::TransferID::SRGB:
      transfer = webrtc::ColorSpace::TransferID::kIEC61966_2_1;
      break;
    case gfx::ColorSpace::TransferID::BT2020_10:
      transfer = webrtc::ColorSpace::TransferID::kBT2020_10;
      break;
    case gfx::ColorSpace::TransferID::BT2020_12:
      transfer = webrtc::ColorSpace::TransferID::kBT2020_12;
      break;
    case gfx::ColorSpace::TransferID::PQ:
      transfer = webrtc::ColorSpace::TransferID::kSMPTEST2084;
      break;
    case gfx::ColorSpace::TransferID::SMPTEST428_1:
      transfer = webrtc::ColorSpace::TransferID::kSMPTEST428;
      break;
    case gfx::ColorSpace::TransferID::HLG:
      transfer = webrtc::ColorSpace::TransferID::kARIB_STD_B67;
      break;
    default:
      DVLOG(1) << "Unsupported transfer.";
      break;
  }

  webrtc::ColorSpace::MatrixID matrix =
      webrtc::ColorSpace::MatrixID::kUnspecified;
  switch (color_space.GetMatrixID()) {
    case gfx::ColorSpace::MatrixID::RGB:
      matrix = webrtc::ColorSpace::MatrixID::kRGB;
      break;
    case gfx::ColorSpace::MatrixID::BT709:
      matrix = webrtc::ColorSpace::MatrixID::kBT709;
      break;
    case gfx::ColorSpace::MatrixID::FCC:
      matrix = webrtc::ColorSpace::MatrixID::kFCC;
      break;
    case gfx::ColorSpace::MatrixID::BT470BG:
      matrix = webrtc::ColorSpace::MatrixID::kBT470BG;
      break;
    case gfx::ColorSpace::MatrixID::SMPTE170M:
      matrix = webrtc::ColorSpace::MatrixID::kSMPTE170M;
      break;
    case gfx::ColorSpace::MatrixID::SMPTE240M:
      matrix = webrtc::ColorSpace::MatrixID::kSMPTE240M;
      break;
    case gfx::ColorSpace::MatrixID::YCOCG:
      matrix = webrtc::ColorSpace::MatrixID::kYCOCG;
      break;
    case gfx::ColorSpace::MatrixID::BT2020_NCL:
      matrix = webrtc::ColorSpace::MatrixID::kBT2020_NCL;
      break;
    case gfx::ColorSpace::MatrixID::YDZDX:
      matrix = webrtc::ColorSpace::MatrixID::kSMPTE2085;
      break;
    default:
      DVLOG(1) << "Unsupported color matrix.";
      break;
  }

  webrtc::ColorSpace::RangeID range = webrtc::ColorSpace::RangeID::kInvalid;
  switch (color_space.GetRangeID()) {
    case gfx::ColorSpace::RangeID::LIMITED:
      range = webrtc::ColorSpace::RangeID::kLimited;
      break;
    case gfx::ColorSpace::RangeID::FULL:
      range = webrtc::ColorSpace::RangeID::kFull;
      break;
    case gfx::ColorSpace::RangeID::DERIVED:
      range = webrtc::ColorSpace::RangeID::kDerived;
      break;
    default:
      DVLOG(1) << "Unsupported color range.";
      break;
  }

  return webrtc::ColorSpace(primaries, transfer, matrix, range);
}

}  // namespace blink
