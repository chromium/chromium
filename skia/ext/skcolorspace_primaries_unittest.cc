// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "skia/ext/skcolorspace_primaries.h"

#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace skia {
namespace {

TEST(SkiaUtils, PrimariesD65) {
  constexpr float kEpsilon = 0.0001f;

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
  constexpr float kEpsilon = 0.0001f;

  // ProPhoto (which has a D50 white point)
  const auto pro_photo = SkNamedPrimaries::kProPhotoRGB;

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
  auto pro_photo_rows = base::span(pro_photo_matrix.vals);
  auto d65_rows = base::span(d65_matrix.vals);
  for (size_t i = 0; i < pro_photo_rows.size(); ++i) {
    auto pro_photo_row = base::span(pro_photo_rows[i]);
    auto d65_row = base::span(d65_rows[i]);
    for (size_t j = 0; j < pro_photo_rows.size(); ++j) {
      EXPECT_NEAR(pro_photo_row[j], d65_row[j], kEpsilon);
    }
  }
}

TEST(SkiaUtils, FractionGamutCovered) {
  constexpr float kEpsilon = 0.0001f;
  constexpr float kBigEpsilon = 0.001f;

  const auto srgb = SkNamedPrimariesExt::kSRGB;
  const auto p3 = SkNamedPrimariesExt::kP3;
  const auto rec2020 = SkNamedPrimaries::kRec2020;
  const auto a98 = SkNamedPrimariesExt::kA98RGB;

  // Identity / self-coverage
  EXPECT_NEAR(FractionGamutCovered(srgb, srgb), 1.0f, kEpsilon);
  EXPECT_NEAR(FractionGamutCovered(p3, p3), 1.0f, kEpsilon);
  EXPECT_NEAR(FractionGamutCovered(rec2020, rec2020), 1.0f, kEpsilon);
  EXPECT_NEAR(FractionGamutCovered(a98, a98), 1.0f, kEpsilon);

  // Larger gamut covering smaller gamut entirely.
  // sRGB is 100% covered by P3 and Rec.2020.
  EXPECT_NEAR(FractionGamutCovered(p3, srgb), 1.0f, kEpsilon);
  EXPECT_NEAR(FractionGamutCovered(rec2020, srgb), 1.0f, kEpsilon);

  // Note: Display P3's red primary (0.68, 0.32) is slightly outside Rec.2020's
  // Green-Red edge, so Rec.2020 covers ~99.98% of Display P3.
  EXPECT_NEAR(FractionGamutCovered(rec2020, p3), 0.9998f, kBigEpsilon);

  // Smaller gamut covering a fraction of larger gamut
  // sRGB area = 0.11205, Display P3 area = 0.1520 => fraction = ~0.73717
  EXPECT_NEAR(FractionGamutCovered(srgb, p3), 0.7372f, kBigEpsilon);
  // sRGB area = 0.11205, Rec.2020 area = 0.21187 => fraction = ~0.5289
  EXPECT_NEAR(FractionGamutCovered(srgb, rec2020), 0.5289f, kBigEpsilon);

  // Partial overlap (A98RGB vs P3)
  EXPECT_NEAR(FractionGamutCovered(p3, a98), 0.8825f, kBigEpsilon);
  EXPECT_NEAR(FractionGamutCovered(a98, p3), 0.8776f, kBigEpsilon);

  // Invalid / degenerate gamuts
  EXPECT_EQ(FractionGamutCovered(SkNamedPrimariesExt::kInvalid, srgb), 0.0f);
  EXPECT_EQ(FractionGamutCovered(srgb, SkNamedPrimariesExt::kInvalid), 0.0f);

  constexpr SkColorSpacePrimaries kCollinear = {0.1f, 0.1f, 0.2f,    0.2f,
                                                0.3f, 0.3f, 0.3127f, 0.3290f};
  EXPECT_EQ(FractionGamutCovered(kCollinear, srgb), 0.0f);
  EXPECT_EQ(FractionGamutCovered(srgb, kCollinear), 0.0f);

  // Disjoint gamuts
  constexpr SkColorSpacePrimaries kDisjoint = {0.8f,  0.8f, 0.9f,    0.8f,
                                               0.85f, 0.9f, 0.3127f, 0.3290f};
  EXPECT_EQ(FractionGamutCovered(kDisjoint, srgb), 0.0f);
  EXPECT_EQ(FractionGamutCovered(srgb, kDisjoint), 0.0f);

  // SkColorSpace overloads
  auto srgb_cs = SkColorSpace::MakeSRGB();
  auto p3_cs =
      SkColorSpace::MakeRGB(SkNamedTransferFn::kSRGB, SkNamedGamut::kDisplayP3);

  // Note: SkColorSpace matrix inversion and chromatic adaptation introduces
  // small floating-point roundoff (~1e-4).
  EXPECT_NEAR(FractionGamutCovered(srgb_cs.get(), srgb), 1.0f, kBigEpsilon);
  EXPECT_NEAR(FractionGamutCovered(p3_cs.get(), srgb), 1.0f, kBigEpsilon);
  EXPECT_NEAR(FractionGamutCovered(srgb_cs.get(), p3), 0.7372f, kBigEpsilon);

  EXPECT_EQ(
      FractionGamutCovered(static_cast<const SkColorSpace*>(nullptr), srgb),
      0.0f);
}

}  // namespace
}  // namespace skia
