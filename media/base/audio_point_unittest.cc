// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cmath>

#include "media/base/audio_point.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {
namespace {

TEST(PointTest, PointsToString) {
  std::vector<Point> points = {Point(1, 0, 0.01f), Point(0, 2, 0.02f)};
  EXPECT_EQ("1,0,0.01, 0,2,0.02", PointsToString(points));

  EXPECT_EQ("", PointsToString(std::vector<Point>()));
}

}  // namespace
}  // namespace media
