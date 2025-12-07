// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/geometry/path.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/geometry/path_builder.h"

namespace blink {

TEST(PathTest, PointAtEndOfPath) {
  const Path path = PathBuilder()
      .MoveTo(gfx::PointF(70, -48))
      .CubicTo(gfx::PointF(70, -48), gfx::PointF(136, 136),
               gfx::PointF(230, 166))
      .MoveTo(gfx::PointF(230, 166))
      .CubicTo(gfx::PointF(324, 196), gfx::PointF(472, 370),
               gfx::PointF(460, 470))
      .Finalize();

  PointAndTangent point_and_tangent =
      path.PointAndNormalAtLength(path.length());
  EXPECT_EQ(point_and_tangent.point, gfx::PointF(460, 470));
}

}  // namespace blink
