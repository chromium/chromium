// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "skia/ext/cicp.h"
#include "skia/ext/skcolorspace_primaries.h"
#include "skia/ext/skcolorspace_trfn.h"

namespace skia {

bool CICPGetPrimaries(uint8_t primaries, SkColorSpacePrimaries& sk_primaries) {
  // Rec. ITU-T H.273, Table 2.
  switch (primaries) {
    case 0:
      // Reserved.
      break;
    case 1:
      sk_primaries = SkNamedPrimariesExt::kRec709;
      return true;
    case 2:
      // Unspecified.
      break;
    case 3:
      // Reserved.
      break;
    case 4:
      sk_primaries = SkNamedPrimariesExt::kRec470SystemM;
      return true;
    case 5:
      sk_primaries = SkNamedPrimariesExt::kRec470SystemBG;
      return true;
    case 6:
      sk_primaries = SkNamedPrimariesExt::kRec601;
      return true;
    case 7:
      sk_primaries = SkNamedPrimariesExt::kSMPTE_ST_240;
      return true;
    case 8:
      sk_primaries = SkNamedPrimariesExt::kGenericFilm;
      return true;
    case 9:
      sk_primaries = SkNamedPrimariesExt::kRec2020;
      return true;
    case 10:
      sk_primaries = SkNamedPrimariesExt::kSMPTE_ST_428_1;
      return true;
    case 11:
      sk_primaries = SkNamedPrimariesExt::kSMPTE_RP_431_2;
      return true;
    case 12:
      sk_primaries = SkNamedPrimariesExt::kSMPTE_EG_432_1;
      return true;
    case 22:
      sk_primaries = SkNamedPrimariesExt::kITU_T_H273_Value22;
      return true;
    default:
      // Reserved.
      break;
  }
  sk_primaries = SkNamedPrimariesExt::kInvalid;
  return false;
}

bool CICPGetTransferFn(uint8_t transfer_characteristics,
                       bool prefer_srgb_trfn,
                       skcms_TransferFunction& trfn) {
  // Rec. ITU-T H.273, Table 3.
  switch (transfer_characteristics) {
    case 0:
      // Reserved.
      break;
    case 1:
      trfn = prefer_srgb_trfn ? SkNamedTransferFnExt::kSRGB
                              : SkNamedTransferFnExt::kRec709;
      return true;
    case 2:
      // Unspecified.
      break;
    case 3:
      // Reserved.
      break;
    case 4:
      trfn = SkNamedTransferFnExt::kRec470SystemM;
      return true;
    case 5:
      trfn = SkNamedTransferFnExt::kRec470SystemBG;
      return true;
    case 6:
      trfn = prefer_srgb_trfn ? SkNamedTransferFnExt::kSRGB
                              : SkNamedTransferFnExt::kRec601;
      return true;
    case 7:
      trfn = SkNamedTransferFnExt::kSMPTE_ST_240;
      return true;
    case 8:
      trfn = SkNamedTransferFn::kLinear;
      return true;
    case 9:
      // Logarithmic transfer characteristic (100:1 range).
      break;
    case 10:
      // Logarithmic transfer characteristic (100 * Sqrt( 10 ) : 1 range).
      break;
    case 11:
      trfn = prefer_srgb_trfn ? SkNamedTransferFnExt::kSRGB
                              : SkNamedTransferFnExt::kIEC61966_2_4;
      break;
    case 12:
      // Rec. ITU-R BT.1361-0 extended colour gamut system (historical).
      // Same as kRec709 on positive values, differs on negative values.
      break;
    case 13:
      // IEC 61966-2-1.
      trfn = SkNamedTransferFnExt::kSRGB;
      return true;
    case 14:
      trfn = SkNamedTransferFnExt::kRec2020_10bit;
      return true;
    case 15:
      trfn = SkNamedTransferFnExt::kRec2020_12bit;
      return true;
    case 16:
      trfn = SkNamedTransferFn::kPQ;
      return true;
    case 17:
      trfn = SkNamedTransferFnExt::kSMPTE_ST_428_1;
      return true;
    case 18:
      trfn = SkNamedTransferFn::kHLG;
      return true;
    default:
      // 19-255 Reserved.
      break;
  }

  trfn = SkNamedTransferFnExt::kInvalid;
  return false;
}

sk_sp<SkColorSpace> CICPGetSkColorSpace(uint8_t color_primaries,
                                        uint8_t transfer_characteristics,
                                        uint8_t matrix_coefficients,
                                        uint8_t full_range_flag,
                                        bool prefer_srgb_trfn) {
  if (matrix_coefficients != 0)
    return nullptr;

  if (full_range_flag != 1)
    return nullptr;

  skcms_TransferFunction trfn;
  if (!CICPGetTransferFn(transfer_characteristics, prefer_srgb_trfn, trfn))
    return nullptr;

  SkColorSpacePrimaries primaries;
  if (!CICPGetPrimaries(color_primaries, primaries))
    return nullptr;

  skcms_Matrix3x3 primaries_matrix;
  if (!primaries.toXYZD50(&primaries_matrix))
    return nullptr;

  return SkColorSpace::MakeRGB(trfn, primaries_matrix);
}

bool CICPGetSkYUVColorSpace(uint8_t matrix_coefficients,
                            uint8_t full_range_flag,
                            uint8_t bits_per_color,
                            SkYUVColorSpace& yuv_color_space) {
  // Rec. ITU-T H.273, Table 4.
  switch (matrix_coefficients) {
    case 0:
      // The identity matrix.
      if (full_range_flag) {
        yuv_color_space = kIdentity_SkYUVColorSpace;
        return true;
      }
      break;
    case 1:
      // Rec. ITU-R BT.709-6.
      yuv_color_space = full_range_flag ? kRec709_Full_SkYUVColorSpace
                                        : kRec709_Limited_SkYUVColorSpace;
      return true;
    case 5:
      // Rec. ITU-R BT.470-6 System B, G (historical).
    case 6:
      // Rec. ITU-R BT.601-7.
      yuv_color_space = full_range_flag ? kJPEG_SkYUVColorSpace
                                        : kRec601_Limited_SkYUVColorSpace;
      return true;
    case 9:
      // Rec. ITU-R BT.2020-2 (non-constant luminance)
    case 10:
      // Rec. ITU-R BT.2020-2 (constant luminance)
      switch (bits_per_color) {
        case 8:
          yuv_color_space = full_range_flag
                                ? kBT2020_8bit_Full_SkYUVColorSpace
                                : kBT2020_8bit_Limited_SkYUVColorSpace;
          return true;
        case 10:
          yuv_color_space = full_range_flag
                                ? kBT2020_10bit_Full_SkYUVColorSpace
                                : kBT2020_10bit_Limited_SkYUVColorSpace;
          return true;
        case 12:
          yuv_color_space = full_range_flag
                                ? kBT2020_12bit_Full_SkYUVColorSpace
                                : kBT2020_12bit_Limited_SkYUVColorSpace;
          return true;
        case 16:
          yuv_color_space = full_range_flag
                                ? kBT2020_16bit_Full_SkYUVColorSpace
                                : kBT2020_16bit_Limited_SkYUVColorSpace;
          return true;
        default:
          break;
      }
      break;
    case 2:
      // Unspecified.
    case 3:
      // Reserved.
    case 4:
      // United States Federal Communications Commission.
    case 7:
      // SMPTE ST 240.
    case 8:
      // YCgCo
    case 11:
      // YDZDX
    case 12:
      // Chromaticity-derived non-constant luminance system.
    case 13:
      // Chromaticity-derived constant luminance system.
    case 14:
      // ICTCP
    default:
      // Reserved.
      break;
  }
  yuv_color_space = kIdentity_SkYUVColorSpace;
  return false;
}

}  // namespace skia
