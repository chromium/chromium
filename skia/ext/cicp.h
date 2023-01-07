// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SKIA_EXT_CICP_H_
#define SKIA_EXT_CICP_H_

#include <stdint.h>

#include "third_party/skia/include/core/SkColorSpace.h"
#include "third_party/skia/include/core/SkImageInfo.h"

namespace skia {

// Convert from a CICP primary value listed in Rec. ITU-T H.273, Table 2 to an
// SkColorSpacePrimaries. Return true if `primaries` is valid. All valid values
// are supported.
SK_API bool CICPGetPrimaries(uint8_t primaries,
                             SkColorSpacePrimaries& sk_primaries);

// Convert from a CICP transfer value listed in Rec. ITU-T H.273, Table 3 to an
// skcms_TransferFunction. Return true if `transfer_characteristics` is valid
// and can be represented using an skcms_TransferFunction (several valid values
// cannot). If `prefer_srgb_trfn` is set to true, then use the sRGB transfer
// function for all Rec709-like content.
SK_API bool CICPGetTransferFn(uint8_t transfer_characteristics,
                              bool prefer_srgb_trfn,
                              skcms_TransferFunction& sk_trfn);

// Return the SkColorSpace resulting from the CICPGetPrimaries and
// CICPGetTransferFn. This function does not populate an SkYUVColorSpace, so
// return nullptr if `matrix_coefficients` is not the identity or
// `full_range_flag` is not full range.
SK_API sk_sp<SkColorSpace> CICPGetSkColorSpace(uint8_t color_primaries,
                                               uint8_t transfer_characteristics,
                                               uint8_t matrix_coefficients,
                                               uint8_t full_range_flag,
                                               bool prefer_srgb_trfn);

// Convert from a CICP matrix value listed in Rec. ITU-T H.273, Table 4 to an
// SkYUVColorSpace. The result depends on full or limited range as well as
// the number of bits per color. Return true if the combination of
// `matrix_coefficients`, `full_range_flag`, and `bits_per_color` is valid and
// supported (several valid combinations are not supported).
SK_API bool CICPGetSkYUVColorSpace(uint8_t matrix_coefficients,
                                   uint8_t full_range_flag,
                                   uint8_t bits_per_color,
                                   SkYUVColorSpace& yuv_color_space);

}  // namespace skia

#endif  // SKIA_EXT_CICP_H_
