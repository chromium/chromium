// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/svg/svg_path_query.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/svg/svg_path_byte_stream.h"
#include "third_party/blink/renderer/core/svg/svg_path_utilities.h"
#include "third_party/blink/renderer/platform/geometry/float_point.h"

namespace blink {
namespace {

void PointsApproximatelyEqual(const FloatPoint& p1,
                              const FloatPoint& p2,
                              float epsilon) {
  EXPECT_NEAR(p1.x(), p2.x(), epsilon);
  EXPECT_NEAR(p1.y(), p2.y(), epsilon);
}

TEST(SVGPathQueryTest, PointAtLength_ArcDecomposedToMultipleCubics) {
  SVGPathByteStream path_stream;
  ASSERT_EQ(BuildByteStreamFromString("M56.2,66.2a174.8,174.8,0,1,0,276.0,-2.0",
                                      path_stream),
            SVGParseStatus::kNoError);

  const float step = 7.80249691f;
  PointsApproximatelyEqual(SVGPathQuery(path_stream).GetPointAtLength(0),
                           FloatPoint(56.200f, 66.200f), 0.0005f);
  PointsApproximatelyEqual(SVGPathQuery(path_stream).GetPointAtLength(step),
                           FloatPoint(51.594f, 72.497f), 0.0005f);
  PointsApproximatelyEqual(SVGPathQuery(path_stream).GetPointAtLength(2 * step),
                           FloatPoint(47.270f, 78.991f), 0.0005f);
  PointsApproximatelyEqual(SVGPathQuery(path_stream).GetPointAtLength(3 * step),
                           FloatPoint(43.239f, 85.671f), 0.0005f);
}

}  // namespace
}  // namespace blink
