// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/line_relative_rect.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/layout/geometry/physical_rect.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

namespace blink {

class LineRelativeRectTest : public testing::Test {};

TEST(LineRelativeRectTest, EnclosingRect) {
  test::TaskEnvironment task_environment;
  gfx::RectF r(1000, 10000, 10, 100);
  LineRelativeRect lor = LineRelativeRect::EnclosingRect(r);
  EXPECT_EQ(lor.offset.line_left, 1000) << "offset X";
  EXPECT_EQ(lor.offset.line_over, 10000) << "offset Y";
  EXPECT_EQ(lor.size.inline_size, 10) << "inline size";
  EXPECT_EQ(lor.size.block_size, 100) << "block size";

  // All values are clamped to 1/64, enclosing the rect.
  gfx::RectF r2(1000.005625, 10000.005625, 10.005625, 100.005625);
  LineRelativeRect lor2 = LineRelativeRect::EnclosingRect(r2);
  EXPECT_EQ(lor2.offset.line_left, 1000) << "offset X clamped to 0";
  EXPECT_EQ(lor2.offset.line_over, 10000) << "offset Y clamped to 0";
  EXPECT_EQ(lor2.size.inline_size, LayoutUnit(10.015625))
      << "inline size clamped to 20 and 1/64";
  EXPECT_EQ(lor2.size.block_size, LayoutUnit(100.015625))
      << "block size clamped to 30 and 1/64";
}

TEST(LineRelativeRectTest, CreateFromLineBox) {
  test::TaskEnvironment task_environment;
  PhysicalRect r(1000, 10000, 10, 100);
  LineRelativeRect lor = LineRelativeRect::CreateFromLineBox(r, true);
  EXPECT_EQ(lor.offset.line_left, 1000) << "offset X, no rotation";
  EXPECT_EQ(lor.offset.line_over, 10000) << "offset Y, no rotation";
  EXPECT_EQ(lor.size.inline_size, 10) << "inline size, no rotation";
  EXPECT_EQ(lor.size.block_size, 100) << "block size, no rotation";

  LineRelativeRect lor_veritcal = LineRelativeRect::CreateFromLineBox(r, false);
  EXPECT_EQ(lor_veritcal.offset.line_left, 1000) << "offset X, with rotation";
  EXPECT_EQ(lor_veritcal.offset.line_over, 10000) << "offset Y, with rotation";
  EXPECT_EQ(lor_veritcal.size.inline_size, 100) << "inline size, with rotation";
  EXPECT_EQ(lor_veritcal.size.block_size, 10) << "block size, with rotation";
}

TEST(LineRelativeRectTest, ComputeRelativeToPhysicalTransformAtOrigin) {
  test::TaskEnvironment task_environment;
  LineRelativeRect r_origin = {{LayoutUnit(), LayoutUnit()},
                               {LayoutUnit(20), LayoutUnit(30)}};

  WritingMode writing_mode = WritingMode::kHorizontalTb;
  std::optional<AffineTransform> rotation =
      r_origin.ComputeRelativeToPhysicalTransform(writing_mode);
  EXPECT_EQ(rotation, AffineTransform());

  writing_mode = WritingMode::kVerticalRl;
  rotation = r_origin.ComputeRelativeToPhysicalTransform(writing_mode);
  EXPECT_EQ(rotation, AffineTransform(0, 1, -1, 0, 30, 0)) << "kVerticalRl";

  writing_mode = WritingMode::kSidewaysLr;
  rotation = r_origin.ComputeRelativeToPhysicalTransform(writing_mode);
  EXPECT_EQ(rotation, AffineTransform(0, -1, 1, 0, 0, 20)) << "kSidewaysLr";
}

TEST(LineRelativeRectTest, ComputeRelativeToPhysicalTransformNotAtOrigin) {
  test::TaskEnvironment task_environment;
  LineRelativeRect r_origin = {{LayoutUnit(1000), LayoutUnit(10000)},
                               {LayoutUnit(10), LayoutUnit(100)}};

  WritingMode writing_mode = WritingMode::kHorizontalTb;
  std::optional<AffineTransform> rotation =
      r_origin.ComputeRelativeToPhysicalTransform(writing_mode);
  EXPECT_EQ(rotation, AffineTransform());

  writing_mode = WritingMode::kVerticalRl;
  rotation = r_origin.ComputeRelativeToPhysicalTransform(writing_mode);
  EXPECT_EQ(rotation, AffineTransform(0, 1, -1, 0, 11100, 9000))
      << "kVerticalRl";

  writing_mode = WritingMode::kSidewaysLr;
  rotation = r_origin.ComputeRelativeToPhysicalTransform(writing_mode);
  EXPECT_EQ(rotation, AffineTransform(0, -1, 1, 0, -9000, 11010))
      << "kSidewaysLr";
}

TEST(LineRelativeRectTest, Create_kHorizontalTB) {
  test::TaskEnvironment task_environment;
  PhysicalRect r(1000, 10000, 10, 100);

  const WritingMode writing_mode = WritingMode::kHorizontalTb;
  const bool is_horizontal = IsHorizontalWritingMode(writing_mode);

  const LineRelativeRect rotated_box =
      LineRelativeRect::CreateFromLineBox(r, is_horizontal);
  std::optional<AffineTransform> rotation =
      rotated_box.ComputeRelativeToPhysicalTransform(writing_mode);

  EXPECT_EQ(rotation, AffineTransform());

  // First half of original box r
  PhysicalRect highlight(1000, 10000, 5, 100);
  LineRelativeRect rotated = LineRelativeRect::Create(highlight, rotation);
  EXPECT_EQ(rotated.offset.line_left, 1000) << "first half x, no rotation";
  EXPECT_EQ(rotated.offset.line_over, 10000) << "first half y, no rotation";
  EXPECT_EQ(rotated.size.inline_size, 5)
      << "first half inline_size, no rotation";
  EXPECT_EQ(rotated.size.block_size, 100)
      << "first half block_size, no rotation";

  // Second half of original box r
  PhysicalRect highlight2(1005, 10000, 5, 100);
  LineRelativeRect rotated2 = LineRelativeRect::Create(highlight2, rotation);
  EXPECT_EQ(rotated2.offset.line_left, 1005) << "second half x, no rotation";
  EXPECT_EQ(rotated2.offset.line_over, 10000) << "second half y, no rotation";
  EXPECT_EQ(rotated2.size.inline_size, 5)
      << "second half inline_size, no rotation";
  EXPECT_EQ(rotated2.size.block_size, 100)
      << "second half block_size, no rotation";
}

TEST(LineRelativeRectTest, Create_kSidewaysLr) {
  test::TaskEnvironment task_environment;
  PhysicalRect r(1000, 10000, 10, 100);

  const WritingMode writing_mode = WritingMode::kSidewaysLr;
  const bool is_horizontal = IsHorizontalWritingMode(writing_mode);
  EXPECT_FALSE(is_horizontal);
  const LineRelativeRect rotated_box =
      LineRelativeRect::CreateFromLineBox(r, is_horizontal);
  std::optional<AffineTransform> rotation =
      rotated_box.ComputeRelativeToPhysicalTransform(writing_mode);

  // AffineTransform ("translation(-9000,11100), scale(1,1), angle(-90deg),
  // skewxy(0)")
  EXPECT_EQ(rotation, AffineTransform(0, -1, 1, 0, -9000, 11100));

  // Top half of original box r
  PhysicalRect highlight(1000, 10000, 10, 50);
  LineRelativeRect rotated = LineRelativeRect::Create(highlight, rotation);
  EXPECT_EQ(rotated.offset.line_left, 1050) << "Top half, x";
  EXPECT_EQ(rotated.offset.line_over, 10000) << "Top half, y";
  EXPECT_EQ(rotated.size.inline_size, 50) << "Top half, inline_size";
  EXPECT_EQ(rotated.size.block_size, 10) << "Top half, block_size";

  // Bottom half of original box r
  PhysicalRect highlight2(1000, 10050, 10, 50);
  LineRelativeRect rotated2 = LineRelativeRect::Create(highlight2, rotation);
  EXPECT_EQ(rotated2.offset.line_left, 1000) << "Bottom half, x";
  EXPECT_EQ(rotated2.offset.line_over, 10000) << "Bottom half, y";
  EXPECT_EQ(rotated2.size.inline_size, 50) << "Bottom half, inline_size";
  EXPECT_EQ(rotated2.size.block_size, 10) << "Bottom half, block_size";

  // The whole thing.
  PhysicalRect highlight3(1000, 10000, 10, 100);
  LineRelativeRect rotated3 = LineRelativeRect::Create(highlight3, rotation);
  EXPECT_EQ(rotated3.offset.line_left, 1000) << "whole box, x";
  EXPECT_EQ(rotated3.offset.line_over, 10000) << "whole box, y";
  EXPECT_EQ(rotated3.size.inline_size, 100) << "whole box, inline_size";
  EXPECT_EQ(rotated3.size.block_size, 10) << "whole box, block_size";
}

TEST(LineRelativeRectTest, Create_kVerticalRl) {
  test::TaskEnvironment task_environment;
  PhysicalRect r(1000, 10000, 10, 100);

  const WritingMode writing_mode = WritingMode::kVerticalRl;
  const bool is_horizontal = IsHorizontalWritingMode(writing_mode);
  EXPECT_FALSE(is_horizontal);
  const LineRelativeRect rotated_box =
      LineRelativeRect::CreateFromLineBox(r, is_horizontal);
  std::optional<AffineTransform> rotation =
      rotated_box.ComputeRelativeToPhysicalTransform(writing_mode);

  // AffineTransform ("translation(11010,9000), scale(1,1), angle(90deg),
  // skewxy(0)")
  EXPECT_EQ(rotation, AffineTransform(0, 1, -1, 0, 11010, 9000));

  // Top half of original box r
  PhysicalRect highlight(1000, 10000, 10, 50);
  LineRelativeRect rotated = LineRelativeRect::Create(highlight, rotation);
  EXPECT_EQ(rotated.offset.line_left, 1000) << "top half, x";
  EXPECT_EQ(rotated.offset.line_over, 10000) << "top half, y";
  EXPECT_EQ(rotated.size.inline_size, 50) << "top half, inline_size";
  EXPECT_EQ(rotated.size.block_size, 10) << "top half, block_size";

  // Bottom half of original box r
  PhysicalRect highlight2(1000, 10050, 10, 50);
  LineRelativeRect rotated2 = LineRelativeRect::Create(highlight2, rotation);
  EXPECT_EQ(rotated2.offset.line_left, 1050) << "bottom half, x";
  EXPECT_EQ(rotated2.offset.line_over, 10000) << "bottom half, y";
  EXPECT_EQ(rotated2.size.inline_size, 50) << "bottom half, inline_size";
  EXPECT_EQ(rotated2.size.block_size, 10) << "bottom half, block_size";
}

TEST(LineRelativeRectTest, EnclosingLineRelativeRect) {
  test::TaskEnvironment task_environment;

  // Nothing should change
  LineRelativeRect rect_1 = {{LayoutUnit(10), LayoutUnit(0)},
                             {LayoutUnit(20), LayoutUnit(30)}};
  LineRelativeRect snapped_1 = rect_1.EnclosingLineRelativeRect();
  EXPECT_EQ(snapped_1.offset.line_left, 10);
  EXPECT_EQ(snapped_1.offset.line_over, 0);
  EXPECT_EQ(snapped_1.size.inline_size, 20);
  EXPECT_EQ(snapped_1.size.block_size, 30);

  // Size needs to increase size by 1 pixel, version a.
  LineRelativeRect rect_2 = {{LayoutUnit(10.25), LayoutUnit(0.25)},
                             {LayoutUnit(20.5), LayoutUnit(30.5)}};
  LineRelativeRect snapped_2 = rect_2.EnclosingLineRelativeRect();
  EXPECT_EQ(snapped_2.offset.line_left, 10);
  EXPECT_EQ(snapped_2.offset.line_over, 0);
  EXPECT_EQ(snapped_2.size.inline_size, 21);
  EXPECT_EQ(snapped_2.size.block_size, 31);

  // Size needs to increase size by 1 pixel, version b.
  LineRelativeRect rect_3 = {{LayoutUnit(10.75), LayoutUnit(0.75)},
                             {LayoutUnit(20.25), LayoutUnit(30.25)}};
  LineRelativeRect snapped_3 = rect_3.EnclosingLineRelativeRect();
  EXPECT_EQ(snapped_3.offset.line_left, 10);
  EXPECT_EQ(snapped_3.offset.line_over, 0);
  EXPECT_EQ(snapped_3.size.inline_size, 21);
  EXPECT_EQ(snapped_3.size.block_size, 31);

  // Size needs to increase size by more than 1 pixel.
  LineRelativeRect rect_4 = {{LayoutUnit(10.75), LayoutUnit(0.75)},
                             {LayoutUnit(20.5), LayoutUnit(30.5)}};
  LineRelativeRect snapped_4 = rect_4.EnclosingLineRelativeRect();
  EXPECT_EQ(snapped_4.offset.line_left, 10);
  EXPECT_EQ(snapped_4.offset.line_over, 0);
  EXPECT_EQ(snapped_4.size.inline_size, 22);
  EXPECT_EQ(snapped_4.size.block_size, 32);
}

TEST(LineRelativeRectTest, Inflate) {
  test::TaskEnvironment task_environment;

  // Nothing should change
  LineRelativeRect rect_1 = {{LayoutUnit(10), LayoutUnit(0)},
                             {LayoutUnit(20), LayoutUnit(30)}};
  rect_1.Inflate(LayoutUnit(1));
  EXPECT_EQ(rect_1.offset.line_left, 9);
  EXPECT_EQ(rect_1.offset.line_over, -1);
  EXPECT_EQ(rect_1.size.inline_size, 22);
  EXPECT_EQ(rect_1.size.block_size, 32);
}

TEST(LineRelativeRectTest, Unite) {
  test::TaskEnvironment task_environment;

  // Nothing should change
  LineRelativeRect rect_1 = {{LayoutUnit(10), LayoutUnit(0)},
                             {LayoutUnit(20), LayoutUnit(40)}};
  LineRelativeRect rect_2 = {{LayoutUnit(0), LayoutUnit(10)},
                             {LayoutUnit(40), LayoutUnit(20)}};
  LineRelativeRect rect_1a = rect_1;
  LineRelativeRect rect_2a = rect_2;

  rect_1.Unite(rect_2a);
  EXPECT_EQ(rect_1.offset.line_left, 0);
  EXPECT_EQ(rect_1.offset.line_over, 0);
  EXPECT_EQ(rect_1.size.inline_size, 40);
  EXPECT_EQ(rect_1.size.block_size, 40);

  rect_2.Unite(rect_1a);
  EXPECT_EQ(rect_2.offset.line_left, 0);
  EXPECT_EQ(rect_2.offset.line_over, 0);
  EXPECT_EQ(rect_2.size.inline_size, 40);
  EXPECT_EQ(rect_2.size.block_size, 40);
}

}  // namespace blink
