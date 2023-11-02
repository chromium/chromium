// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/path.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

TEST(PathTest, PointAtEndOfPath) {
  Path path;
  path.MoveTo(gfx::PointF(70, -48));
  path.AddBezierCurveTo(gfx::PointF(70, -48), gfx::PointF(136, 136),
                        gfx::PointF(230, 166));
  path.MoveTo(gfx::PointF(230, 166));
  path.AddBezierCurveTo(gfx::PointF(324, 196), gfx::PointF(472, 370),
                        gfx::PointF(460, 470));

  PointAndTangent point_and_tangent =
      path.PointAndNormalAtLength(path.length());
  EXPECT_EQ(point_and_tangent.point, gfx::PointF(460, 470));
}

}  // namespace blink
