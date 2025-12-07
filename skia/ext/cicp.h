// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SKIA_EXT_CICP_H_
#define SKIA_EXT_CICP_H_

#include <stdint.h>

#include "third_party/skia/include/core/SkColorSpace.h"
#include "third_party/skia/include/core/SkImageInfo.h"

namespace skia {

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
