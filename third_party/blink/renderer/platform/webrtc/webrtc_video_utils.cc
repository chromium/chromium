// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/webrtc/webrtc_video_utils.h"

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

}  // namespace blink
