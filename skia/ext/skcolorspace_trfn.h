// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SKIA_EXT_SKCOLORSPACE_TRFN_H_
#define SKIA_EXT_SKCOLORSPACE_TRFN_H_

#include "third_party/skia/include/core/SkColorSpace.h"

namespace skia {

// Returns a transfer function that is equal to `alpha` * `x`.
skcms_TransferFunction SK_API
ScaleTransferFunction(const skcms_TransferFunction& f, float alpha);

// Returns true if `y` = `alpha` * `x`, and computes and stores alpha if `alpha`
// is non-nullptr. Returns false is `x` is the zero function or `alpha` is zero.
bool SK_API IsScaledTransferFunction(const skcms_TransferFunction& x,
                                     const skcms_TransferFunction& y,
                                     float* alpha);

}  // namespace skia

namespace SkNamedTransferFnExt {

////////////////////////////////////////////////////////////////////////////////
// Color primaries defined by ITU-T H.273, table 3. Names are given by the first
// specification referenced in the value's row.

// Rec. ITU-R BT.709-6, value 1.
static constexpr skcms_TransferFunction kRec709 = {2.222222222222f,
                                                   0.909672415686f,
                                                   0.090327584314f,
                                                   0.222222222222f,
                                                   0.081242858299f,
                                                   0.f,
                                                   0.f};

// Rec. ITU-R BT.470-6 System M (historical) assumed display gamma 2.2, value 4.
static constexpr skcms_TransferFunction kRec470SystemM = {2.2f, 1.f};

// Rec. ITU-R BT.470-6 System B, G (historical) assumed display gamma 2.8,
// value 5.
static constexpr skcms_TransferFunction kRec470SystemBG = {2.8f, 1.f};

// Rec. ITU-R BT.601-7, same as kRec709, value 6.
static constexpr skcms_TransferFunction kRec601 = kRec709;

// SMPTE ST 240, value 7.
static constexpr skcms_TransferFunction kSMPTE_ST_240 = {2.222222222222f,
                                                         0.899626676224f,
                                                         0.100373323776f,
                                                         0.25f,
                                                         0.091286342118f,
                                                         0.f,
                                                         0.f};

// IEC 61966-2-4, value 11, same as kRec709 (but is explicitly extended).
static constexpr skcms_TransferFunction kIEC61966_2_4 = kRec709;

// IEC 61966-2-1 sRGB, value 13. This is almost equal to
// SkNamedTransferFnExt::kSRGB. The differences are rounding errors that
// cause test failures (and should be unified).
static constexpr skcms_TransferFunction kIEC61966_2_1 = {
    2.4f, 0.947867345704f, 0.052132654296f, 0.077399380805f, 0.040449937172f};

// Rec. ITU-R BT.2020-2 (10-bit system), value 14.
static constexpr skcms_TransferFunction kRec2020_10bit = kRec709;

// Rec. ITU-R BT.2020-2 (12-bit system), value 15.
static constexpr skcms_TransferFunction kRec2020_12bit = kRec709;

// SMPTE ST 428-1, value 17.
static constexpr skcms_TransferFunction kSMPTE_ST_428_1 = {2.6f,
                                                           1.034080527699f};

////////////////////////////////////////////////////////////////////////////////
// CSS Color Level 4 predefined color spaces.

// 'srgb', 'display-p3'
static constexpr skcms_TransferFunction kSRGB = kIEC61966_2_1;

// 'a98-rgb'
static constexpr skcms_TransferFunction kA98RGB = {2.2f, 1.};

// 'prophoto-rgb'
static constexpr skcms_TransferFunction kProPhotoRGB = {1.8f, 1.};

// 'rec2020' uses the same transfer function as kRec709.
static constexpr skcms_TransferFunction kRec2020 = kRec709;

////////////////////////////////////////////////////////////////////////////////
// Additional helper transfer functions.

// Invalid primaries, initialized to zero.
static constexpr skcms_TransferFunction kInvalid = {0};

// The interpretation of kRec709 that is produced by accelerated video decode
// on macOS.
static constexpr skcms_TransferFunction kRec709Apple = {1.961f, 1.};

// If the sRGB transfer function is f(x), then this transfer function is
// f(x * 1023 / 510). This function gives 510 values to SDR content, and can
// reach a maximum brightnes of 4.99x SDR brightness.
static constexpr skcms_TransferFunction kSRGBExtended1023Over510 = {
    SkNamedTransferFnExt::kSRGB.g,
    SkNamedTransferFnExt::kSRGB.a * 1023 / 510,
    SkNamedTransferFnExt::kSRGB.b,
    SkNamedTransferFnExt::kSRGB.c * 1023 / 510,
    SkNamedTransferFnExt::kSRGB.d * 1023 / 510,
    SkNamedTransferFnExt::kSRGB.e,
    SkNamedTransferFnExt::kSRGB.f};

}  // namespace SkNamedTransferFnExt

#endif  // SKIA_EXT_SKCOLORSPACE_TRFN_H_
