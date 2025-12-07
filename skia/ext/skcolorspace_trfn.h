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
// CSS Color Level 4 predefined color spaces.

// 'srgb', 'display-p3'
static constexpr skcms_TransferFunction kSRGB =
    SkNamedTransferFn::kIEC61966_2_1;

// 'rec2020' uses the same transfer function as kRec709.
static constexpr skcms_TransferFunction kRec2020 = SkNamedTransferFn::kRec709;

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
