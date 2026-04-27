// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/android/media_format_color_space.h"

#include "media/base/video_color_space.h"

namespace media {

namespace {

// Android MediaFormat.COLOR_STANDARD_* values.
constexpr int kColorStandardBT709 = 1;
constexpr int kColorStandardBT601PAL = 2;
constexpr int kColorStandardBT601NTSC = 4;
constexpr int kColorStandardBT2020 = 6;

// Android MediaFormat.COLOR_TRANSFER_* values.
constexpr int kColorTransferLinear = 1;
constexpr int kColorTransferSDRVideo = 3;
constexpr int kColorTransferST2084 = 6;
constexpr int kColorTransferHLG = 7;

// Android MediaFormat.COLOR_RANGE_* values.
constexpr int kColorRangeFull = 1;
constexpr int kColorRangeLimited = 2;

}  // namespace

MediaFormatColorSpace::MediaFormatColorSpace(
    const VideoColorSpace& color_space) {
  switch (color_space.primaries()) {
    case VideoColorSpace::PrimaryID::BT709:
      standard = kColorStandardBT709;
      break;
    case VideoColorSpace::PrimaryID::BT470M:
    case VideoColorSpace::PrimaryID::BT470BG:
    case VideoColorSpace::PrimaryID::SMPTE170M:
    case VideoColorSpace::PrimaryID::SMPTE240M:
      standard = kColorStandardBT601NTSC;
      break;
    case VideoColorSpace::PrimaryID::BT2020:
      standard = kColorStandardBT2020;
      break;
    default:
      break;
  }

  switch (color_space.transfer()) {
    case VideoColorSpace::TransferID::BT709:
    case VideoColorSpace::TransferID::SMPTE170M:
    case VideoColorSpace::TransferID::SMPTE240M:
      transfer = kColorTransferSDRVideo;
      break;
    case VideoColorSpace::TransferID::LINEAR:
      transfer = kColorTransferLinear;
      break;
    case VideoColorSpace::TransferID::SMPTEST2084:
      transfer = kColorTransferST2084;
      break;
    case VideoColorSpace::TransferID::ARIB_STD_B67:
      transfer = kColorTransferHLG;
      break;
    default:
      break;
  }

  switch (color_space.range()) {
    case gfx::ColorSpace::RangeID::LIMITED:
      range = kColorRangeLimited;
      break;
    case gfx::ColorSpace::RangeID::FULL:
      range = kColorRangeFull;
      break;
    default:
      break;
  }
}

// static
MediaFormatColorSpace MediaFormatColorSpace::MakeRec709() {
  MediaFormatColorSpace result;
  result.standard = kColorStandardBT709;
  result.transfer = kColorTransferSDRVideo;
  result.range = kColorRangeLimited;
  return result;
}

// static
MediaFormatColorSpace MediaFormatColorSpace::MakeHdr10() {
  MediaFormatColorSpace result;
  result.standard = kColorStandardBT2020;
  result.transfer = kColorTransferST2084;
  result.range = kColorRangeLimited;
  return result;
}

gfx::ColorSpace MediaFormatColorSpace::ToGfxColorSpace() const {
  auto primary_id = gfx::ColorSpace::PrimaryID::INVALID;
  auto matrix_id = gfx::ColorSpace::MatrixID::INVALID;
  switch (standard) {
    case kColorStandardBT709:
      primary_id = gfx::ColorSpace::PrimaryID::BT709;
      matrix_id = gfx::ColorSpace::MatrixID::BT709;
      break;
    case kColorStandardBT601PAL:
      primary_id = gfx::ColorSpace::PrimaryID::BT470BG;
      matrix_id = gfx::ColorSpace::MatrixID::SMPTE170M;
      break;
    case kColorStandardBT601NTSC:
      primary_id = gfx::ColorSpace::PrimaryID::SMPTE170M;
      matrix_id = gfx::ColorSpace::MatrixID::SMPTE170M;
      break;
    case kColorStandardBT2020:
      primary_id = gfx::ColorSpace::PrimaryID::BT2020;
      matrix_id = gfx::ColorSpace::MatrixID::BT2020_NCL;
      break;
    default:
      return gfx::ColorSpace();
  }

  auto transfer_id = gfx::ColorSpace::TransferID::INVALID;
  switch (transfer) {
    case kColorTransferLinear:
      transfer_id = gfx::ColorSpace::TransferID::LINEAR_HDR;
      break;
    case kColorTransferSDRVideo:
      transfer_id = gfx::ColorSpace::TransferID::SMPTE170M;
      break;
    case kColorTransferST2084:
      transfer_id = gfx::ColorSpace::TransferID::PQ;
      break;
    case kColorTransferHLG:
      transfer_id = gfx::ColorSpace::TransferID::HLG;
      break;
    default:
      return gfx::ColorSpace();
  }

  auto range_id = gfx::ColorSpace::RangeID::INVALID;
  switch (range) {
    case kColorRangeFull:
      range_id = gfx::ColorSpace::RangeID::FULL;
      break;
    case kColorRangeLimited:
      range_id = gfx::ColorSpace::RangeID::LIMITED;
      break;
    default:
      return gfx::ColorSpace();
  }

  return gfx::ColorSpace(primary_id, transfer_id, matrix_id, range_id);
}

}  // namespace media
