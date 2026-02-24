// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/geometry/box_edge.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

TEST(BoxEdgeTest, Construction) {
  BoxEdge segment(LayoutUnit(10), LayoutUnit(40));
  EXPECT_EQ(LayoutUnit(10), segment.offset);
  EXPECT_EQ(LayoutUnit(40), segment.size);
}

TEST(BoxEdgeTest, End) {
  BoxEdge segment(LayoutUnit(10), LayoutUnit(40));
  EXPECT_EQ(LayoutUnit(50), segment.End());
}

TEST(BoxEdgeTest, IsEmpty) {
  EXPECT_TRUE(BoxEdge(LayoutUnit(10), LayoutUnit(0)).IsEmpty());
  EXPECT_TRUE(BoxEdge(LayoutUnit(50), LayoutUnit(-40)).IsEmpty());
  EXPECT_FALSE(BoxEdge(LayoutUnit(10), LayoutUnit(40)).IsEmpty());
}

TEST(BoxEdgeTest, Move) {
  BoxEdge segment(LayoutUnit(10), LayoutUnit(40));
  segment.Move(LayoutUnit(10));
  EXPECT_EQ(LayoutUnit(20), segment.offset);
  EXPECT_EQ(LayoutUnit(40), segment.size);

  segment.Move(LayoutUnit(-15));
  EXPECT_EQ(LayoutUnit(5), segment.offset);
  EXPECT_EQ(LayoutUnit(40), segment.size);
}

TEST(BoxEdgeTest, Equality) {
  BoxEdge segment1(LayoutUnit(10), LayoutUnit(40));
  BoxEdge segment2(LayoutUnit(10), LayoutUnit(40));
  BoxEdge segment3(LayoutUnit(20), LayoutUnit(40));
  EXPECT_EQ(segment1, segment2);
  EXPECT_NE(segment1, segment3);
}

TEST(BoxEdgeTest, MathOperators) {
  BoxEdge segment(LayoutUnit(10), LayoutUnit(40));
  EXPECT_EQ(BoxEdge(LayoutUnit(20), LayoutUnit(40)), segment + LayoutUnit(10));
  EXPECT_EQ(BoxEdge(LayoutUnit(0), LayoutUnit(40)), segment - LayoutUnit(10));

  segment += LayoutUnit(10);
  EXPECT_EQ(LayoutUnit(20), segment.offset);

  segment -= LayoutUnit(15);
  EXPECT_EQ(LayoutUnit(5), segment.offset);
}

}  // namespace blink
