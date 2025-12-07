// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/svg/svg_path_query.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/svg/svg_path_byte_stream.h"
#include "third_party/blink/renderer/core/svg/svg_path_byte_stream_builder.h"
#include "third_party/blink/renderer/core/svg/svg_path_utilities.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/test/geometry_util.h"

namespace blink {
namespace {

TEST(SVGPathQueryTest, PointAtLength_ArcDecomposedToMultipleCubics) {
  test::TaskEnvironment task_environment;
  SVGPathByteStreamBuilder builder;
  ASSERT_EQ(BuildByteStreamFromString("M56.2,66.2a174.8,174.8,0,1,0,276.0,-2.0",
                                      builder),
            SVGParseStatus::kNoError);
  SVGPathByteStream path_stream = builder.CopyByteStream();

  constexpr float kStep = 7.80249691f;
  constexpr float kTolerance = 0.0005f;
  EXPECT_POINTF_NEAR(SVGPathQuery(path_stream).GetPointAtLength(0),
                     gfx::PointF(56.200f, 66.200f), kTolerance);
  EXPECT_POINTF_NEAR(SVGPathQuery(path_stream).GetPointAtLength(kStep),
                     gfx::PointF(51.594f, 72.497f), kTolerance);
  EXPECT_POINTF_NEAR(SVGPathQuery(path_stream).GetPointAtLength(2 * kStep),
                     gfx::PointF(47.270f, 78.991f), kTolerance);
  EXPECT_POINTF_NEAR(SVGPathQuery(path_stream).GetPointAtLength(3 * kStep),
                     gfx::PointF(43.239f, 85.671f), kTolerance);
}

}  // namespace
}  // namespace blink
