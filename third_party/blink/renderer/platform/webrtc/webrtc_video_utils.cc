// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/webrtc/webrtc_video_utils.h"

#include "third_party/webrtc/api/video_codecs/h264_profile_level_id.h"
#include "third_party/webrtc/api/video_codecs/video_codec.h"
#include "third_party/webrtc/api/video_codecs/vp9_profile.h"

namespace blink {
namespace {
struct ScalabilityModeSpatialLayers {
  const char* name;
  int spatial_layers;
  int dependent_spatial_layers;
};

constexpr ScalabilityModeSpatialLayers kSvcSpatialLayers[] = {
    // See https://www.w3.org/TR/webrtc-svc/ for a list of scalability modes.
    {"L1T2", 1, 1},           {"L1T3", 1, 1},
    {"L2T1", 2, 2},           {"L2T2", 2, 2},
    {"L2T3", 2, 2},           {"L2T1h", 2, 2},
    {"L2T2h", 2, 2},          {"L2T3h", 2, 2},
    {"S2T1", 2, 1},           {"S2T2", 2, 1},
    {"S2T3", 2, 1},           {"S2T1h", 2, 1},
    {"S2T2h", 2, 1},          {"S2T3h", 2, 1},
    {"L3T1", 3, 3},           {"L3T2", 3, 3},
    {"L3T3", 3, 3},           {"S3T1", 3, 1},
    {"S3T2", 3, 1},           {"S3T3", 3, 1},
    {"S3T1h", 3, 1},          {"S3T2h", 3, 1},
    {"S3T3h", 3, 1},          {"L2T2_KEY", 2, 2},
    {"L2T2_KEY_SHIFT", 2, 2}, {"L2T3_KEY", 2, 2},
    {"L2T3_KEY_SHIFT", 2, 2}, {"L3T2_KEY", 3, 3},
    {"L3T2_KEY_SHIFT", 3, 3}, {"L3T3_KEY", 3, 3}};

}  // namespace

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
      const absl::optional<webrtc::VP9Profile> vp9_profile =
          webrtc::ParseSdpForVP9Profile(format.parameters);
      // The return value is absl::nullopt if the profile-id is specified
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
      const absl::optional<webrtc::H264ProfileLevelId> h264_profile_level_id =
          webrtc::ParseSdpForH264ProfileLevelId(format.parameters);
      // The return value is absl::nullopt if the profile-level-id is specified
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
    default:
      return media::VIDEO_CODEC_PROFILE_UNKNOWN;
  }
}

media::VideoColorSpace WebRtcToMediaVideoColorSpace(
    const webrtc::ColorSpace& color_space) {
  media::VideoColorSpace::PrimaryID primaries =
      media::VideoColorSpace::PrimaryID::INVALID;
  switch (color_space.primaries()) {
    case webrtc::ColorSpace::PrimaryID::kBT709:
      primaries = media::VideoColorSpace::PrimaryID::BT709;
      break;
    case webrtc::ColorSpace::PrimaryID::kBT470M:
      primaries = media::VideoColorSpace::PrimaryID::BT470M;
      break;
    case webrtc::ColorSpace::PrimaryID::kBT470BG:
      primaries = media::VideoColorSpace::PrimaryID::BT470BG;
      break;
    case webrtc::ColorSpace::PrimaryID::kSMPTE170M:
      primaries = media::VideoColorSpace::PrimaryID::SMPTE170M;
      break;
    case webrtc::ColorSpace::PrimaryID::kSMPTE240M:
      primaries = media::VideoColorSpace::PrimaryID::SMPTE240M;
      break;
    case webrtc::ColorSpace::PrimaryID::kFILM:
      primaries = media::VideoColorSpace::PrimaryID::FILM;
      break;
    case webrtc::ColorSpace::PrimaryID::kBT2020:
      primaries = media::VideoColorSpace::PrimaryID::BT2020;
      break;
    case webrtc::ColorSpace::PrimaryID::kSMPTEST428:
      primaries = media::VideoColorSpace::PrimaryID::SMPTEST428_1;
      break;
    case webrtc::ColorSpace::PrimaryID::kSMPTEST431:
      primaries = media::VideoColorSpace::PrimaryID::SMPTEST431_2;
      break;
    case webrtc::ColorSpace::PrimaryID::kSMPTEST432:
      primaries = media::VideoColorSpace::PrimaryID::SMPTEST432_1;
      break;
    case webrtc::ColorSpace::PrimaryID::kJEDECP22:
      primaries = media::VideoColorSpace::PrimaryID::EBU_3213_E;
      break;
    default:
      break;
  }

  media::VideoColorSpace::TransferID transfer =
      media::VideoColorSpace::TransferID::INVALID;
  switch (color_space.transfer()) {
    case webrtc::ColorSpace::TransferID::kBT709:
      transfer = media::VideoColorSpace::TransferID::BT709;
      break;
    case webrtc::ColorSpace::TransferID::kGAMMA22:
      transfer = media::VideoColorSpace::TransferID::GAMMA22;
      break;
    case webrtc::ColorSpace::TransferID::kGAMMA28:
      transfer = media::VideoColorSpace::TransferID::GAMMA28;
      break;
    case webrtc::ColorSpace::TransferID::kSMPTE170M:
      transfer = media::VideoColorSpace::TransferID::SMPTE170M;
      break;
    case webrtc::ColorSpace::TransferID::kSMPTE240M:
      transfer = media::VideoColorSpace::TransferID::SMPTE240M;
      break;
    case webrtc::ColorSpace::TransferID::kLINEAR:
      transfer = media::VideoColorSpace::TransferID::LINEAR;
      break;
    case webrtc::ColorSpace::TransferID::kLOG:
      transfer = media::VideoColorSpace::TransferID::LOG;
      break;
    case webrtc::ColorSpace::TransferID::kLOG_SQRT:
      transfer = media::VideoColorSpace::TransferID::LOG_SQRT;
      break;
    case webrtc::ColorSpace::TransferID::kIEC61966_2_4:
      transfer = media::VideoColorSpace::TransferID::IEC61966_2_4;
      break;
    case webrtc::ColorSpace::TransferID::kBT1361_ECG:
      transfer = media::VideoColorSpace::TransferID::BT1361_ECG;
      break;
    case webrtc::ColorSpace::TransferID::kIEC61966_2_1:
      transfer = media::VideoColorSpace::TransferID::IEC61966_2_1;
      break;
    case webrtc::ColorSpace::TransferID::kBT2020_10:
      transfer = media::VideoColorSpace::TransferID::BT2020_10;
      break;
    case webrtc::ColorSpace::TransferID::kBT2020_12:
      transfer = media::VideoColorSpace::TransferID::BT2020_12;
      break;
    case webrtc::ColorSpace::TransferID::kSMPTEST2084:
      transfer = media::VideoColorSpace::TransferID::SMPTEST2084;
      break;
    case webrtc::ColorSpace::TransferID::kSMPTEST428:
      transfer = media::VideoColorSpace::TransferID::SMPTEST428_1;
      break;
    case webrtc::ColorSpace::TransferID::kARIB_STD_B67:
      transfer = media::VideoColorSpace::TransferID::ARIB_STD_B67;
      break;
    default:
      break;
  }

  media::VideoColorSpace::MatrixID matrix =
      media::VideoColorSpace::MatrixID::INVALID;
  switch (color_space.matrix()) {
    case webrtc::ColorSpace::MatrixID::kRGB:
      matrix = media::VideoColorSpace::MatrixID::RGB;
      break;
    case webrtc::ColorSpace::MatrixID::kBT709:
      matrix = media::VideoColorSpace::MatrixID::BT709;
      break;
    case webrtc::ColorSpace::MatrixID::kFCC:
      matrix = media::VideoColorSpace::MatrixID::FCC;
      break;
    case webrtc::ColorSpace::MatrixID::kBT470BG:
      matrix = media::VideoColorSpace::MatrixID::BT470BG;
      break;
    case webrtc::ColorSpace::MatrixID::kSMPTE170M:
      matrix = media::VideoColorSpace::MatrixID::SMPTE170M;
      break;
    case webrtc::ColorSpace::MatrixID::kSMPTE240M:
      matrix = media::VideoColorSpace::MatrixID::SMPTE240M;
      break;
    case webrtc::ColorSpace::MatrixID::kYCOCG:
      matrix = media::VideoColorSpace::MatrixID::YCOCG;
      break;
    case webrtc::ColorSpace::MatrixID::kBT2020_NCL:
      matrix = media::VideoColorSpace::MatrixID::BT2020_NCL;
      break;
    case webrtc::ColorSpace::MatrixID::kBT2020_CL:
      matrix = media::VideoColorSpace::MatrixID::BT2020_CL;
      break;
    case webrtc::ColorSpace::MatrixID::kSMPTE2085:
      matrix = media::VideoColorSpace::MatrixID::YDZDX;
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

  return media::VideoColorSpace(primaries, transfer, matrix, range);
}

absl::optional<int> WebRtcScalabilityModeSpatialLayers(
    const std::string& scalability_mode) {
  for (const auto& entry : kSvcSpatialLayers) {
    if (entry.name == scalability_mode) {
      return entry.spatial_layers;
    }
  }
  return absl::nullopt;
}

absl::optional<int> WebRtcScalabilityModeDependentSpatialLayers(
    const std::string& scalability_mode) {
  for (const auto& entry : kSvcSpatialLayers) {
    if (entry.name == scalability_mode) {
      return entry.dependent_spatial_layers;
    }
  }
  return absl::nullopt;
}
}  // namespace blink
