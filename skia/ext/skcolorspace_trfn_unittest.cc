// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "skia/ext/skcolorspace_trfn.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace skia {
namespace {

constexpr float kEpsilon = 0.001;

TEST(SkiaUtils, ScaleTrfn) {
  skcms_TransferFunction x = SkNamedTransferFnExt::kIEC61966_2_1;
  float alpha = 3.f;

  // Ensure that we round-trip successfully.
  skcms_TransferFunction y = ScaleTransferFunction(x, alpha);
  float alpha_recovered = 0.f;
  EXPECT_TRUE(IsScaledTransferFunction(x, y, &alpha_recovered));
  EXPECT_NEAR(alpha, alpha_recovered, kEpsilon);

  // Make sure that we actually scale the function correctly;
  for (float t = 0.0; t <= 1.f; t += 1.f / 256.f) {
    float x_of_t = skcms_TransferFunction_eval(&x, t);
    float y_of_t = skcms_TransferFunction_eval(&y, t);
    EXPECT_NEAR(y_of_t, alpha * x_of_t, kEpsilon);
  }

  // Ensure that all parameters are checked.
  skcms_TransferFunction z;
  z = y;
  z.b *= 1.1;
  EXPECT_FALSE(IsScaledTransferFunction(x, z, &alpha_recovered));
  z = y;
  z.b *= 1.1;
  EXPECT_FALSE(IsScaledTransferFunction(x, z, &alpha_recovered));
  z = y;
  z.c *= 1.1;
  EXPECT_FALSE(IsScaledTransferFunction(x, z, &alpha_recovered));
  z = y;
  z.d += 0.04;
  EXPECT_FALSE(IsScaledTransferFunction(x, z, &alpha_recovered));
  z = y;
  z.e += 0.1;
  EXPECT_FALSE(IsScaledTransferFunction(x, z, &alpha_recovered));
  z = y;
  z.f += 0.04;
  EXPECT_FALSE(IsScaledTransferFunction(x, z, &alpha_recovered));
  z = y;
  z.g += 1.f;
  EXPECT_FALSE(IsScaledTransferFunction(x, z, &alpha_recovered));
}

}  // namespace
}  // namespace skia
