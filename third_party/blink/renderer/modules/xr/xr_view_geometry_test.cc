// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/xr/xr_view_geometry.h"

#include <cmath>
#include <limits>

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {
namespace {

constexpr float kPiOverFour = 0.7853981633974483f;
constexpr float kPiOverTwo = 1.5707963267948966f;
constexpr float kNearDepth = 0.25f;

void ExpectMatrix(const gfx::Transform& actual,
                  const gfx::Transform& expected) {
  for (int row = 0; row < 4; ++row) {
    for (int column = 0; column < 4; ++column) {
      EXPECT_TRUE(std::isfinite(actual.rc(row, column)));
      EXPECT_FLOAT_EQ(actual.rc(row, column), expected.rc(row, column));
    }
  }
}

TEST(XRViewGeometryTest, FoVWebGLInfiniteFar) {
  XRViewGeometry geometry(XRGraphicsBinding::Api::kWebGL);
  geometry.UpdateProjectionMatrixFromFoV(
      kPiOverFour, kPiOverFour, kPiOverFour, kPiOverFour, kNearDepth,
      std::numeric_limits<float>::infinity());

  ExpectMatrix(geometry.ProjectionMatrix(),
               gfx::Transform::ColMajor(1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f,
                                        0.0f, 0.0f, 0.0f, 0.0f, -1.0f, -1.0f,
                                        0.0f, 0.0f, -0.5f, 0.0f));
}

TEST(XRViewGeometryTest, FoVWebGPUInfiniteFar) {
  XRViewGeometry geometry(XRGraphicsBinding::Api::kWebGPU);
  geometry.UpdateProjectionMatrixFromFoV(
      kPiOverFour, kPiOverFour, kPiOverFour, kPiOverFour, kNearDepth,
      std::numeric_limits<float>::infinity());

  ExpectMatrix(geometry.ProjectionMatrix(),
               gfx::Transform::ColMajor(1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f,
                                        0.0f, 0.0f, 0.0f, 0.0f, -1.0f, -1.0f,
                                        0.0f, 0.0f, -0.25f, 0.0f));
}

TEST(XRViewGeometryTest, AspectWebGLInfiniteFar) {
  XRViewGeometry geometry(XRGraphicsBinding::Api::kWebGL);
  geometry.UpdateProjectionMatrixFromAspect(
      kPiOverTwo, 1.0f, kNearDepth, std::numeric_limits<float>::infinity());

  ExpectMatrix(geometry.ProjectionMatrix(),
               gfx::Transform::ColMajor(1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f,
                                        0.0f, 0.0f, 0.0f, 0.0f, -1.0f, -1.0f,
                                        0.0f, 0.0f, -0.5f, 0.0f));
}

TEST(XRViewGeometryTest, AspectWebGPUInfiniteFar) {
  XRViewGeometry geometry(XRGraphicsBinding::Api::kWebGPU);
  geometry.UpdateProjectionMatrixFromAspect(
      kPiOverTwo, 1.0f, kNearDepth, std::numeric_limits<float>::infinity());

  ExpectMatrix(geometry.ProjectionMatrix(),
               gfx::Transform::ColMajor(1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f,
                                        0.0f, 0.0f, 0.0f, 0.0f, -1.0f, -1.0f,
                                        0.0f, 0.0f, -0.25f, 0.0f));
}

TEST(XRViewGeometryTest, FoVWebGLFiniteFar) {
  XRViewGeometry geometry(XRGraphicsBinding::Api::kWebGL);
  geometry.UpdateProjectionMatrixFromFoV(kPiOverFour, kPiOverFour, kPiOverFour,
                                         kPiOverFour, kNearDepth, 1.0f);

  ExpectMatrix(geometry.ProjectionMatrix(),
               gfx::Transform::ColMajor(1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f,
                                        0.0f, 0.0f, 0.0f, 0.0f, -5.0f / 3.0f,
                                        -1.0f, 0.0f, 0.0f, -2.0f / 3.0f, 0.0f));
}

}  // namespace
}  // namespace blink
