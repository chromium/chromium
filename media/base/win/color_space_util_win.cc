// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/win/color_space_util_win.h"

#include <initguid.h>  // NOLINT(build/include_order)
#include <mfapi.h>     // NOLINT(build/include_order)

#include "base/logging.h"
#include "media/base/video_color_space.h"

namespace media {

namespace {

gfx::ColorSpace::PrimaryID MFPrimaryToColorSpace(uint32_t mf_primary) {
  switch (mf_primary) {
    case MFVideoPrimaries_BT709:
      return gfx::ColorSpace::PrimaryID::BT709;
    case MFVideoPrimaries_BT470_2_SysM:
      return gfx::ColorSpace::PrimaryID::BT470M;
    case MFVideoPrimaries_BT470_2_SysBG:
      return gfx::ColorSpace::PrimaryID::BT470BG;
    case MFVideoPrimaries_SMPTE170M:
      return gfx::ColorSpace::PrimaryID::SMPTE170M;
    case MFVideoPrimaries_SMPTE240M:
      return gfx::ColorSpace::PrimaryID::SMPTE240M;
    case MFVideoPrimaries_BT2020:
      return gfx::ColorSpace::PrimaryID::BT2020;
    case MFVideoPrimaries_XYZ:
      return gfx::ColorSpace::PrimaryID::XYZ_D50;
    case MFVideoPrimaries_DCI_P3:
      return gfx::ColorSpace::PrimaryID::P3;
    default:
      return gfx::ColorSpace::PrimaryID::INVALID;
  }
}

gfx::ColorSpace::TransferID MFTransferToColorSpace(uint32_t mf_transfer) {
  switch (mf_transfer) {
    case MFVideoTransFunc_18:
      return gfx::ColorSpace::TransferID::GAMMA18;
    case MFVideoTransFunc_22:
      return gfx::ColorSpace::TransferID::GAMMA22;
    case MFVideoTransFunc_709:
      return gfx::ColorSpace::TransferID::BT709;
    case MFVideoTransFunc_240M:
      return gfx::ColorSpace::TransferID::SMPTE240M;
    case MFVideoTransFunc_sRGB:
      return gfx::ColorSpace::TransferID::SRGB;
    case MFVideoTransFunc_28:
      return gfx::ColorSpace::TransferID::GAMMA28;
    case MFVideoTransFunc_2020:
      return gfx::ColorSpace::TransferID::BT2020_10;
    case MFVideoTransFunc_2084:
      return gfx::ColorSpace::TransferID::PQ;
    default:
      return gfx::ColorSpace::TransferID::INVALID;
  }
}

gfx::ColorSpace::MatrixID MFMatrixToColorSpace(uint32_t mf_matrix) {
  switch (mf_matrix) {
    case MFVideoTransferMatrix_BT709:
      return gfx::ColorSpace::MatrixID::BT709;
    case MFVideoTransferMatrix_BT601:
      return gfx::ColorSpace::MatrixID::SMPTE170M;
    case MFVideoTransferMatrix_SMPTE240M:
      return gfx::ColorSpace::MatrixID::SMPTE240M;
    case MFVideoTransferMatrix_BT2020_10:
      return gfx::ColorSpace::MatrixID::BT2020_NCL;
    case MFVideoTransferMatrix_BT2020_12:
      return gfx::ColorSpace::MatrixID::BT2020_CL;
    default:
      return gfx::ColorSpace::MatrixID::INVALID;
  }
}

gfx::ColorSpace::RangeID MFRangeToColorSpace(uint32_t mf_range) {
  switch (mf_range) {
    case MFNominalRange_0_255:
      return gfx::ColorSpace::RangeID::FULL;
    case MFNominalRange_16_235:
      return gfx::ColorSpace::RangeID::LIMITED;
    case MFNominalRange_48_208:
      return gfx::ColorSpace::RangeID::LIMITED;
    case MFNominalRange_64_127:
      return gfx::ColorSpace::RangeID::LIMITED;
    default:
      return gfx::ColorSpace::RangeID::INVALID;
  }
}

}  // anonymous namespace

gfx::ColorSpace GetMediaTypeColorSpace(IMFMediaType* media_type) {
  uint32_t mf_primary, mf_transfer, mf_matrix, mf_range;

  HRESULT hr = media_type->GetUINT32(MF_MT_VIDEO_PRIMARIES, &mf_primary);
  if (FAILED(hr))
    mf_primary = MFVideoPrimaries_Unknown;

  hr = media_type->GetUINT32(MF_MT_TRANSFER_FUNCTION, &mf_transfer);
  if (FAILED(hr))
    mf_transfer = MFVideoTransFunc_Unknown;

  hr = media_type->GetUINT32(MF_MT_YUV_MATRIX, &mf_matrix);
  if (FAILED(hr))
    mf_matrix = MFVideoTransferMatrix_Unknown;

  hr = media_type->GetUINT32(MF_MT_VIDEO_NOMINAL_RANGE, &mf_range);
  if (FAILED(hr))
    mf_range = MFNominalRange_Unknown;

  gfx::ColorSpace::PrimaryID primary = MFPrimaryToColorSpace(mf_primary);
  gfx::ColorSpace::TransferID transfer = MFTransferToColorSpace(mf_transfer);
  gfx::ColorSpace::MatrixID matrix = MFMatrixToColorSpace(mf_matrix);
  gfx::ColorSpace::RangeID range = MFRangeToColorSpace(mf_range);

  // Convert to VideoColorSpace and back to fill in any gaps.
  VideoColorSpace guesser = VideoColorSpace::FromGfxColorSpace(
      gfx::ColorSpace(primary, transfer, matrix, range));
  return guesser.GuessGfxColorSpace();
}

}  // namespace media