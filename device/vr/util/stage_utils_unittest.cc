// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/util/stage_utils.h"

#include "ui/gfx/geometry/point3_f.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/gfx/geometry/vector3d_f.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace {
constexpr size_t kExpectedBoundsSize = 4;
}

namespace device {

TEST(StageUtils, Basic) {
  auto bounds = vr_utils::GetStageBoundsFromSize(2.0, 2.0);

  // The Expected bounds (without transform) are:
  //  (1, 0, -1)
  //  (1, 0, 1)
  //  (-1, 0, 1)
  //  (-1, 0, -1)
  // This represents a box 2x2 box, centered around 0.
  // Note that the choice of which point starts is (unfortunately) an
  // implementation detail.  The order however, should always be
  // counter-clockwise.
  std::vector<gfx::Point3F> expected_bounds = {
      gfx::Point3F(1, 0, -1), gfx::Point3F(1, 0, 1), gfx::Point3F(-1, 0, 1),
      gfx::Point3F(-1, 0, -1)};

  ASSERT_EQ(bounds.size(), kExpectedBoundsSize);
  for (size_t i = 0; i < kExpectedBoundsSize; i++) {
    EXPECT_FLOAT_EQ(bounds[i].x(), expected_bounds[i].x());
    EXPECT_FLOAT_EQ(bounds[i].y(), expected_bounds[i].y());
    EXPECT_FLOAT_EQ(bounds[i].z(), expected_bounds[i].z());
  }
}

TEST(StageUtils, InvalidBounds) {
  // clang-format off
  std::vector<std::pair<float, float>> test_data{
    { 0.0,   0.0 },
    { 0.0,   1.0 },
    { 1.0,   0.0 },
    { 1.0,  -1.0 },
    { -1.0,  1.0 },
    {-1.0,  -1.0 }
  };
  // clang-format on

  // This avoids any compiler issues with 0 not being deduced to the right type.
  constexpr size_t kEmptyBoundsSize = 0;
  for (const auto& data : test_data) {
    auto bounds = vr_utils::GetStageBoundsFromSize(data.first, data.second);
    ASSERT_EQ(bounds.size(), kEmptyBoundsSize);
  }
}

}  // namespace device
