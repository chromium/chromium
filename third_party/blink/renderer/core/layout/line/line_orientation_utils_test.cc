// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/line/line_orientation_utils.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {
namespace {

TEST(LineOrientationUtilsTest, LineOrientationLayoutRectOutsets_Horizontal) {
  LayoutRectOutsets outsets(1, 2, 3, 4);
  EXPECT_EQ(
      LayoutRectOutsets(1, 2, 3, 4),
      LineOrientationLayoutRectOutsets(outsets, WritingMode::kHorizontalTb));
}

TEST(LineOrientationUtilsTest, LineOrientationLayoutRectOutsets_Vertical) {
  LayoutRectOutsets outsets(1, 2, 3, 4);
  EXPECT_EQ(
      LayoutRectOutsets(4, 3, 2, 1),
      LineOrientationLayoutRectOutsets(outsets, WritingMode::kVerticalLr));
  EXPECT_EQ(
      LayoutRectOutsets(4, 3, 2, 1),
      LineOrientationLayoutRectOutsets(outsets, WritingMode::kVerticalRl));
}

TEST(LineOrientationUtilsTest,
     LineOrientationLayoutRectOutsetsWithFlippedLines) {
  LayoutRectOutsets outsets(1, 2, 3, 4);
  EXPECT_EQ(LayoutRectOutsets(1, 2, 3, 4),
            LineOrientationLayoutRectOutsetsWithFlippedLines(
                outsets, WritingMode::kHorizontalTb));
  EXPECT_EQ(LayoutRectOutsets(2, 3, 4, 1),
            LineOrientationLayoutRectOutsetsWithFlippedLines(
                outsets, WritingMode::kVerticalLr));
  EXPECT_EQ(LayoutRectOutsets(4, 3, 2, 1),
            LineOrientationLayoutRectOutsetsWithFlippedLines(
                outsets, WritingMode::kVerticalRl));
}

}  // namespace
}  // namespace blink
