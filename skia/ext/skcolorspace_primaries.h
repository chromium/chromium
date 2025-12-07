// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SKIA_EXT_SKCOLORSPACE_PRIMARIES_H_
#define SKIA_EXT_SKCOLORSPACE_PRIMARIES_H_

#include "third_party/skia/include/core/SkColorSpace.h"

#include <string>

// TODO(https://crbug.com/skia/13721): Add these operators to Skia source.
#if !defined(SKIA_COLOR_SPACE_PRIMARIES_OPERATOR_EQUAL)
SK_API bool operator==(const SkColorSpacePrimaries& a,
                       const SkColorSpacePrimaries& b);

SK_API bool operator!=(const SkColorSpacePrimaries& a,
                       const SkColorSpacePrimaries& b);
#endif

namespace skia {

// Display SkColorSpacePrimaries as a string.
SK_API std::string SkColorSpacePrimariesToString(
    const SkColorSpacePrimaries& primaries);

// Given a matrix that transforms to XYZD50, compute the primaries with a D65
// white point that would produce this matrix.
SK_API SkColorSpacePrimaries
GetD65PrimariesFromToXYZD50Matrix(const skcms_Matrix3x3& m);

}  // namespace skia

namespace SkNamedPrimariesExt {

////////////////////////////////////////////////////////////////////////////////
// CSS Color Level 4 predefined and xyz color spaces.

// 'srgb'
static constexpr SkColorSpacePrimaries kSRGB = SkNamedPrimaries::kRec709;

// 'display-p3' (and also 'p3' as a color gamut).
static constexpr SkColorSpacePrimaries kP3 = SkNamedPrimaries::kSMPTE_EG_432_1;

// 'a98-rgb'
static constexpr SkColorSpacePrimaries kA98RGB = {
    0.64f, 0.33f, 0.21f, 0.71f, 0.15f, 0.06f, 0.3127f, 0.3290f};

// 'rec2020' (as both a predefined color space and color gamut).
// The value kRec2020 is already defined above.

// 'xyzd50'
static constexpr SkColorSpacePrimaries kXYZD50 = {
    1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.34567f, 0.35850f};

// 'xyz' and 'xyzd65'
static constexpr SkColorSpacePrimaries kXYZD65 = {1.0f, 0.0f, 0.0f,    1.0f,
                                                  0.0f, 0.0f, 0.3127f, 0.3290f};

////////////////////////////////////////////////////////////////////////////////
// Additional helper color primaries.

// Invalid primaries, initialized to zero.
static constexpr SkColorSpacePrimaries kInvalid = {0};

// The GenericRGB space on macOS.
static constexpr SkColorSpacePrimaries kAppleGenericRGB = {
    0.63002f, 0.34000f, 0.29505f, 0.60498f,
    0.15501f, 0.07701f, 0.3127f,  0.3290f};

// Primaries where the colors are rotated and the gamut is huge. Good for
// testing.
static constexpr SkColorSpacePrimaries kWideGamutColorSpin = {
    0.01f, 0.98f, 0.01f, 0.01f, 0.98f, 0.01f, 0.3127f, 0.3290f};

}  // namespace SkNamedPrimariesExt

#endif  // SKIA_EXT_SKCOLORSPACE_PRIMARIES_H_
