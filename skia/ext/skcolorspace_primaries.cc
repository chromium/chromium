// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "skia/ext/skcolorspace_primaries.h"

#include <iomanip>
#include <sstream>

bool operator==(const SkColorSpacePrimaries& a,
                const SkColorSpacePrimaries& b) {
  return a.fRX == b.fRX && a.fRY == b.fRY && a.fGX == b.fGX && a.fGY == b.fGY &&
         a.fBX == b.fBX && a.fBY == b.fBY && a.fWX == b.fWX && a.fWY == b.fWY;
}

bool operator!=(const SkColorSpacePrimaries& a,
                const SkColorSpacePrimaries& b) {
  return !(a == b);
}

namespace skia {

std::string SkColorSpacePrimariesToString(
    const SkColorSpacePrimaries& primaries) {
  if (primaries == SkNamedPrimariesExt::kInvalid)
    return "invalid";

  std::stringstream ss;
  ss << std::fixed << std::setprecision(4);
  ss << "{";
  if (primaries == SkNamedPrimariesExt::kSRGB)
    ss << "name:'srgb', ";
  else if (primaries == SkNamedPrimariesExt::kP3)
    ss << "name:'p3', ";
  else if (primaries == SkNamedPrimariesExt::kRec2020)
    ss << "name:'rec2020', ";
  ss << "r:[" << primaries.fRX << ", " << primaries.fRY << "], ";
  ss << "g:[" << primaries.fGX << ", " << primaries.fGY << "], ";
  ss << "b:[" << primaries.fBX << ", " << primaries.fRY << "], ";
  ss << "w:[" << primaries.fWX << ", " << primaries.fWY << "]";
  ss << "}";
  return ss.str();
}

SkColorSpacePrimaries GetD65PrimariesFromToXYZD50Matrix(
    const skcms_Matrix3x3& m_d50) {
  constexpr float kD65_X = 0.3127f;
  constexpr float kD65_Y = 0.3290f;
  skcms_Matrix3x3 adapt_d65_to_d50;
  skcms_AdaptToXYZD50(kD65_X, kD65_Y, &adapt_d65_to_d50);

  skcms_Matrix3x3 adapt_d50_to_d65;
  skcms_Matrix3x3_invert(&adapt_d65_to_d50, &adapt_d50_to_d65);

  const skcms_Matrix3x3 m = skcms_Matrix3x3_concat(&adapt_d50_to_d65, &m_d50);
  const float sum_R = m.vals[0][0] + m.vals[1][0] + m.vals[2][0];
  const float sum_G = m.vals[0][1] + m.vals[1][1] + m.vals[2][1];
  const float sum_B = m.vals[0][2] + m.vals[1][2] + m.vals[2][2];
  SkColorSpacePrimaries primaries;
  primaries.fRX = m.vals[0][0] / sum_R;
  primaries.fRY = m.vals[1][0] / sum_R;
  primaries.fGX = m.vals[0][1] / sum_G;
  primaries.fGY = m.vals[1][1] / sum_G;
  primaries.fBX = m.vals[0][2] / sum_B;
  primaries.fBY = m.vals[1][2] / sum_B;
  primaries.fWX = kD65_X;
  primaries.fWY = kD65_Y;
  return primaries;
}

}  // namespace skia
