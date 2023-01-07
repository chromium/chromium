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

}  // namespace skia

namespace SkNamedPrimariesExt {

////////////////////////////////////////////////////////////////////////////////
// Color primaries defined by ITU-T H.273, table 2. Names are given by the first
// specification referenced in the value's row.

// Rec. ITU-R BT.709-6, value 1.
static constexpr SkColorSpacePrimaries kRec709 = {
    0.64f, 0.33f, 0.3f, 0.6f, 0.15f, 0.06f, 0.3127f, 0.329f};

// Rec. ITU-R BT.470-6 System M (historical), value 4.
static constexpr SkColorSpacePrimaries kRec470SystemM = {
    0.67f, 0.33f, 0.21f, 0.71f, 0.14f, 0.08f, 0.31f, 0.316f};

// Rec. ITU-R BT.470-6 System B, G (historical), value 5.
static constexpr SkColorSpacePrimaries kRec470SystemBG = {
    0.64f, 0.33f, 0.29f, 0.60f, 0.15f, 0.06f, 0.3127f, 0.3290f};

// Rec. ITU-R BT.601-7 525, value 6.
static constexpr SkColorSpacePrimaries kRec601 = {
    0.630f, 0.340f, 0.310f, 0.595f, 0.155f, 0.070f, 0.3127f, 0.3290f};

// SMPTE ST 240, value 7 (functionally the same as value 6).
static constexpr SkColorSpacePrimaries kSMPTE_ST_240 = kRec601;

// Generic film (colour filters using Illuminant C), value 8.
static constexpr SkColorSpacePrimaries kGenericFilm = {
    0.681f, 0.319f, 0.243f, 0.692f, 0.145f, 0.049f, 0.310f, 0.316f};

// Rec. ITU-R BT.2020-2, value 9.
static constexpr SkColorSpacePrimaries kRec2020{
    0.708f, 0.292f, 0.170f, 0.797f, 0.131f, 0.046f, 0.3127f, 0.3290f};

// SMPTE ST 428-1, value 10.
static constexpr SkColorSpacePrimaries kSMPTE_ST_428_1 = {
    1.f, 0.f, 0.f, 1.f, 0.f, 0.f, 1.f / 3.f, 1.f / 3.f};

// SMPTE RP 431-2, value 11.
static constexpr SkColorSpacePrimaries kSMPTE_RP_431_2 = {
    0.680f, 0.320f, 0.265f, 0.690f, 0.150f, 0.060f, 0.314f, 0.351f};

// SMPTE EG 432-1, value 12.
static constexpr SkColorSpacePrimaries kSMPTE_EG_432_1 = {
    0.680f, 0.320f, 0.265f, 0.690f, 0.150f, 0.060f, 0.3127f, 0.3290f};

// No corresponding industry specification identified, value 22.
// This is sometimes referred to as EBU 3213-E, but that document doesn't
// specify these values.
static constexpr SkColorSpacePrimaries kITU_T_H273_Value22 = {
    0.630f, 0.340f, 0.295f, 0.605f, 0.155f, 0.077f, 0.3127f, 0.3290f};

////////////////////////////////////////////////////////////////////////////////
// CSS Color Level 4 predefined and xyz color spaces.

// 'srgb'
static constexpr SkColorSpacePrimaries kSRGB = kRec709;

// 'display-p3' (and also 'p3' as a color gamut).
static constexpr SkColorSpacePrimaries kP3 = kSMPTE_EG_432_1;

// 'a98-rgb'
static constexpr SkColorSpacePrimaries kA98RGB = {
    0.64f, 0.33f, 0.21f, 0.71f, 0.15f, 0.06f, 0.3127f, 0.3290f};

// 'prophoto-rgb'
static constexpr SkColorSpacePrimaries kProPhotoRGB = {
    0.7347f, 0.2653f, 0.1596f, 0.8404f, 0.0366f, 0.0001f, 0.34567f, 0.35850f};

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
