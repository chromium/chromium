// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
  if (primaries == kSkColorSpacePrimariesZero)
    return "invalid";

  std::stringstream ss;
  ss << std::fixed << std::setprecision(4);
  ss << "{";
  if (primaries == kSkColorSpacePrimariesSRGB)
    ss << "name:'srgb', ";
  else if (primaries == kSkColorSpacePrimariesP3)
    ss << "name:'p3', ";
  else if (primaries == kSkColorSpacePrimariesRec2020)
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

SkColorSpacePrimaries kSkColorSpacePrimariesZero = {0};

SkColorSpacePrimaries kSkColorSpacePrimariesSRGB = {
    0.640f, 0.330f, 0.300f, 0.600f, 0.150f, 0.060f, 0.3127f, 0.3290f,
};

SkColorSpacePrimaries kSkColorSpacePrimariesP3 = {
    0.680f, 0.320f, 0.265f, 0.690f, 0.150f, 0.060f, 0.3127f, 0.3290f,
};

SkColorSpacePrimaries kSkColorSpacePrimariesRec2020 = {
    0.708f, 0.292f, 0.170f, 0.797f, 0.131f, 0.046f, 0.3127f, 0.3290f,
};

SkColorSpacePrimaries kSkColorSpacePrimariesProPhotoD50 = {
    0.7347f, 0.2653f, 0.1596f, 0.8404f, 0.0366f, 0.0001f, 0.34567f, 0.35850f,
};

SkColorSpacePrimaries kSkColorSpacePrimariesWideGamutColorSpin = {
    0.01f, 0.98f, 0.01f, 0.01f, 0.98f, 0.01f, 0.3127f, 0.3290f,
};

}  // namespace skia
