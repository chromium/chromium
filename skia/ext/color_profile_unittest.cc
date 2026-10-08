// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "skia/ext/color_profile.h"

#include "skia/ext/skia_utils_base.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkData.h"
#include "third_party/skia/include/encode/SkICC.h"

namespace skia {

namespace {

sk_sp<ColorProfile> MakeFromICC(const skcms_TransferFunction& trfn,
                                const skcms_Matrix3x3& to_xyzd50) {
  sk_sp<SkData> data = SkWriteICCProfile(trfn, to_xyzd50);
  if (!data) {
    return nullptr;
  }
  return ColorProfile::Make(skia::as_byte_span(*data));
}

}  // namespace

// Display profiles with a D50 white point are used as-is.
TEST(ColorProfileTest, DisplaySkColorSpaceD50) {
  auto profile =
      MakeFromICC(SkNamedTransferFn::kSRGB, SkNamedGamut::kDisplayP3);
  ASSERT_TRUE(profile);
  EXPECT_TRUE(SkColorSpace::Equals(profile->GetSkColorSpace().get(),
                                   profile->GetDisplaySkColorSpace().get()));
  EXPECT_FALSE(profile->GetDisplaySkColorSpace()->isSRGB());
  EXPECT_TRUE(profile->IsDisplaySkColorSpaceExact());
}

// Display profiles without a D50 white point are rejected, and sRGB is used
// instead. The profile is still used as-is by GetSkColorSpace.
// https://crbug.com/565342193
TEST(ColorProfileTest, DisplaySkColorSpaceNonD50) {
  // Scale the rows of the sRGB matrix so that the white point is D65-ish
  // (0.9505, 1.0, 1.089) instead of D50 (0.9642, 1.0, 0.8249).
  skcms_Matrix3x3 m = SkNamedGamut::kSRGB;
  auto scale_row = [](float (&row)[3], float white) {
    const float scale = white / (row[0] + row[1] + row[2]);
    row[0] *= scale;
    row[1] *= scale;
    row[2] *= scale;
  };
  scale_row(m.vals[0], 0.9505f);
  scale_row(m.vals[1], 1.0f);
  scale_row(m.vals[2], 1.089f);
  auto profile = MakeFromICC(SkNamedTransferFn::kSRGB, m);
  ASSERT_TRUE(profile);
  EXPECT_FALSE(profile->GetSkColorSpace()->isSRGB());
  EXPECT_TRUE(profile->IsSkColorSpaceExact());
  EXPECT_TRUE(profile->GetDisplaySkColorSpace()->isSRGB());
  EXPECT_FALSE(profile->IsDisplaySkColorSpaceExact());

  // ColorProfiles created from an SkColorSpace always accept it.
  auto profile_from_cs =
      ColorProfile::Make(SkColorSpace::MakeRGB(SkNamedTransferFn::kSRGB, m));
  ASSERT_TRUE(profile_from_cs);
  EXPECT_FALSE(profile_from_cs->GetDisplaySkColorSpace()->isSRGB());
  EXPECT_TRUE(profile_from_cs->IsDisplaySkColorSpaceExact());
}

}  // namespace skia
