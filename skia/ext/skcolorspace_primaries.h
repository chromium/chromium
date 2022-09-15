// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SKIA_EXT_SKCOLORSPACE_PRIMARIES_H_
#define SKIA_EXT_SKCOLORSPACE_PRIMARIES_H_

#include "third_party/skia/include/core/SkColorSpace.h"

#include <string>

// TODO(https://crbug.com/skia/13721): Add these operators to Skia source.
SK_API bool operator==(const SkColorSpacePrimaries& a,
                       const SkColorSpacePrimaries& b);

SK_API bool operator!=(const SkColorSpacePrimaries& a,
                       const SkColorSpacePrimaries& b);

namespace skia {

// Display SkColorSpacePrimaries as a string.
SK_API std::string SkColorSpacePrimariesToString(
    const SkColorSpacePrimaries& primaries);

// Given a matrix that transforms to XYZD50, compute the primaries with a D65
// white point that would produce this matrix.
SK_API SkColorSpacePrimaries
GetD65PrimariesFromToXYZD50Matrix(const skcms_Matrix3x3& m);

// Primaries initialized to zero (an invalid value).
extern SK_API SkColorSpacePrimaries kSkColorSpacePrimariesZero;

// The sRGB or BT709 primaries.
extern SK_API SkColorSpacePrimaries kSkColorSpacePrimariesSRGB;

// P3 primaries.
extern SK_API SkColorSpacePrimaries kSkColorSpacePrimariesP3;

// Rec2020 primaries.
extern SK_API SkColorSpacePrimaries kSkColorSpacePrimariesRec2020;

// ProPhoto primaries (this has a D50 white point).
extern SK_API SkColorSpacePrimaries kSkColorSpacePrimariesProPhotoD50;

// Primaries where the colors are rotated and the gamut is huge. Good for
// testing.
extern SK_API SkColorSpacePrimaries kSkColorSpacePrimariesWideGamutColorSpin;

}  // namespace skia

#endif  // SKIA_EXT_SKCOLORSPACE_PRIMARIES_H_
