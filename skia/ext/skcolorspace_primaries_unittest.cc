// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "skia/ext/skcolorspace_primaries.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace skia {
namespace {

constexpr float kEpsilon = 0.0001;

TEST(SkiaUtils, PrimariesD65) {
  // DCI P3 (D65)
  const auto p3 = SkNamedPrimariesExt::kP3;

  skcms_Matrix3x3 matrix;
  EXPECT_TRUE(p3.toXYZD50(&matrix));
  const auto primaries_from_matrix = GetD65PrimariesFromToXYZD50Matrix(matrix);

  // The retrieved primaries from the matrix should be the same as the original
  // primaries, because the original primaries had a D65 white point.
  EXPECT_NEAR(p3.fRX, primaries_from_matrix.fRX, kEpsilon);
  EXPECT_NEAR(p3.fRY, primaries_from_matrix.fRY, kEpsilon);
  EXPECT_NEAR(p3.fGX, primaries_from_matrix.fGX, kEpsilon);
  EXPECT_NEAR(p3.fGY, primaries_from_matrix.fGY, kEpsilon);
  EXPECT_NEAR(p3.fBX, primaries_from_matrix.fBX, kEpsilon);
  EXPECT_NEAR(p3.fBY, primaries_from_matrix.fBY, kEpsilon);
  EXPECT_NEAR(p3.fWX, primaries_from_matrix.fWX, kEpsilon);
  EXPECT_NEAR(p3.fWY, primaries_from_matrix.fWY, kEpsilon);
}

TEST(SkiaUtils, PrimariesD50) {
  // ProPhoto (which has a D50 white point)
  const auto pro_photo = SkNamedPrimariesExt::kProPhotoRGB;

  // Convert primaries to a matrix.
  skcms_Matrix3x3 pro_photo_matrix;
  EXPECT_TRUE(pro_photo.toXYZD50(&pro_photo_matrix));

  // The convert the matrix back to primaries with a D65 white point.
  const auto d65 = GetD65PrimariesFromToXYZD50Matrix(pro_photo_matrix);

  // And then convert the D65 primaries to a matrix.
  skcms_Matrix3x3 d65_matrix;
  EXPECT_TRUE(d65.toXYZD50(&d65_matrix));

  // The two matrices should be the same, but the primaries will not be.
  EXPECT_FALSE(pro_photo == d65);
  for (size_t i = 0; i < 3; ++i) {
    for (size_t j = 0; j < 3; ++j) {
      EXPECT_NEAR(pro_photo_matrix.vals[i][j], d65_matrix.vals[i][j], kEpsilon);
    }
  }
}

}  // namespace
}  // namespace skia
