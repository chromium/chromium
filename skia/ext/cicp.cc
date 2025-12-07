// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "skia/ext/cicp.h"

namespace skia {

sk_sp<SkColorSpace> CICPGetSkColorSpace(uint8_t color_primaries,
                                        uint8_t transfer_characteristics,
                                        uint8_t matrix_coefficients,
                                        uint8_t full_range_flag,
                                        bool prefer_srgb_trfn) {
  if (matrix_coefficients != 0) {
    return nullptr;
  }

  // TODO(https://crbug.com/40229816): Implement this if needed.
  if (full_range_flag != 1) {
    return nullptr;
  }

  auto transfer_id =
      static_cast<SkNamedTransferFn::CicpId>(transfer_characteristics);
  if (prefer_srgb_trfn) {
    switch (transfer_id) {
      case SkNamedTransferFn::CicpId::kRec709:
      case SkNamedTransferFn::CicpId::kRec601:
      case SkNamedTransferFn::CicpId::kIEC61966_2_4:
        transfer_id = SkNamedTransferFn::CicpId::kSRGB;
        break;
      default:
        break;
    };
  }

  auto primaries_id = static_cast<SkNamedPrimaries::CicpId>(color_primaries);
  return SkColorSpace::MakeCICP(primaries_id, transfer_id);
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
