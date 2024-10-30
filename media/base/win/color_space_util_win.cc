// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/win/color_space_util_win.h"

#include <initguid.h>

#include <mfapi.h>

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
    case MFVideoPrimaries_SMPTE_C:
      return gfx::ColorSpace::PrimaryID::SMPTE170M;
    case MFVideoPrimaries_SMPTE240M:
      return gfx::ColorSpace::PrimaryID::SMPTE240M;
    case MFVideoPrimaries_EBU3213:
      return gfx::ColorSpace::PrimaryID::EBU_3213_E;
    case MFVideoPrimaries_BT2020:
      return gfx::ColorSpace::PrimaryID::BT2020;
    case MFVideoPrimaries_XYZ:
      return gfx::ColorSpace::PrimaryID::SMPTEST428_1;
    case MFVideoPrimaries_DCI_P3:
      return gfx::ColorSpace::PrimaryID::SMPTEST431_2;
    default:
      return gfx::ColorSpace::PrimaryID::INVALID;
  }
}

MFVideoPrimaries ColorSpaceToMFPrimary(gfx::ColorSpace::PrimaryID color_space) {
  switch (color_space) {
    case gfx::ColorSpace::PrimaryID::BT709:
      return MFVideoPrimaries_BT709;
    case gfx::ColorSpace::PrimaryID::BT470M:
      return MFVideoPrimaries_BT470_2_SysM;
    case gfx::ColorSpace::PrimaryID::BT470BG:
      return MFVideoPrimaries_BT470_2_SysBG;
    case gfx::ColorSpace::PrimaryID::SMPTE170M:
      return MFVideoPrimaries_SMPTE170M;
    case gfx::ColorSpace::PrimaryID::SMPTE240M:
      return MFVideoPrimaries_SMPTE240M;
    case gfx::ColorSpace::PrimaryID::EBU_3213_E:
      return MFVideoPrimaries_EBU3213;
    case gfx::ColorSpace::PrimaryID::BT2020:
      return MFVideoPrimaries_BT2020;
    case gfx::ColorSpace::PrimaryID::SMPTEST428_1:
      return MFVideoPrimaries_XYZ;
    case gfx::ColorSpace::PrimaryID::SMPTEST431_2:
      return MFVideoPrimaries_DCI_P3;
    default:
      return MFVideoPrimaries_Unknown;
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
    case MFVideoTransFunc_HLG:
      return gfx::ColorSpace::TransferID::HLG;
    default:
      return gfx::ColorSpace::TransferID::INVALID;
  }
}

MFVideoTransferFunction ColorSpaceToMFTransfer(
    gfx::ColorSpace::TransferID color_space) {
  switch (color_space) {
    case gfx::ColorSpace::TransferID::GAMMA18:
      return MFVideoTransFunc_18;
    case gfx::ColorSpace::TransferID::GAMMA22:
      return MFVideoTransFunc_22;
    case gfx::ColorSpace::TransferID::BT709:
      return MFVideoTransFunc_709;
    case gfx::ColorSpace::TransferID::SMPTE240M:
      return MFVideoTransFunc_240M;
    case gfx::ColorSpace::TransferID::SRGB:
      return MFVideoTransFunc_sRGB;
    case gfx::ColorSpace::TransferID::GAMMA28:
      return MFVideoTransFunc_28;
    case gfx::ColorSpace::TransferID::BT2020_10:
    case gfx::ColorSpace::TransferID::BT2020_12:
      return MFVideoTransFunc_2020;
    case gfx::ColorSpace::TransferID::PQ:
      return MFVideoTransFunc_2084;
    case gfx::ColorSpace::TransferID::HLG:
      return MFVideoTransFunc_HLG;
    default:
      return MFVideoTransFunc_Unknown;
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
    case MFVideoTransferMatrix_BT2020_12:
      return gfx::ColorSpace::MatrixID::BT2020_NCL;
    default:
      return gfx::ColorSpace::MatrixID::INVALID;
  }
}

MFVideoTransferMatrix ColorSpaceToMFMatrix(
    gfx::ColorSpace::MatrixID color_space) {
  switch (color_space) {
    case gfx::ColorSpace::MatrixID::BT709:
      return MFVideoTransferMatrix_BT709;
    case gfx::ColorSpace::MatrixID::SMPTE170M:
      return MFVideoTransferMatrix_BT601;
    case gfx::ColorSpace::MatrixID::SMPTE240M:
      return MFVideoTransferMatrix_SMPTE240M;
    case gfx::ColorSpace::MatrixID::BT2020_NCL:
      return MFVideoTransferMatrix_BT2020_10;
    default:
      return MFVideoTransferMatrix_Unknown;
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

MFNominalRange ColorSpaceToMFRange(gfx::ColorSpace::RangeID color_space) {
  switch (color_space) {
    case gfx::ColorSpace::RangeID::FULL:
      return MFNominalRange_0_255;
    case gfx::ColorSpace::RangeID::DERIVED:
    case gfx::ColorSpace::RangeID::LIMITED:
      return MFNominalRange_16_235;
    default:
      return MFNominalRange_Unknown;
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

void GetMediaTypeColorValues(const gfx::ColorSpace& color_space,
                             MFVideoPrimaries* out_primaries,
                             MFVideoTransferFunction* out_transfer,
                             MFVideoTransferMatrix* out_matrix,
                             MFNominalRange* out_range) {
  *out_primaries = ColorSpaceToMFPrimary(color_space.GetPrimaryID());
  *out_transfer = ColorSpaceToMFTransfer(color_space.GetTransferID());
  *out_matrix = ColorSpaceToMFMatrix(color_space.GetMatrixID());
  *out_range = ColorSpaceToMFRange(color_space.GetRangeID());
}

}  // namespace media