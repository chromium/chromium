// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/shaping/shape_result_run.h"

#include <hb.h>

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

namespace {

ShapeResultRun* CreateTestShapeResultRun(unsigned num_glyphs,
                                         unsigned num_characters) {
  return MakeGarbageCollected<ShapeResultRun>(
      /*font*/ nullptr, hb_direction_t::HB_DIRECTION_LTR,
      CanvasRotationInVertical::kRegular, hb_script_t::HB_SCRIPT_LATIN,
      /*start_index*/ 0, num_glyphs, num_characters);
}

}  // namespace

class ShapeResultRunTest : public testing::Test {};

TEST_F(ShapeResultRunTest, GlyphDataCopyConstructor) {
  ShapeResultRun* run = CreateTestShapeResultRun(2, 2);

  ShapeResultRun* run2 = MakeGarbageCollected<ShapeResultRun>(*run);
  EXPECT_FALSE(run2->glyph_data_.HasNonZeroOffsets());

  run->glyph_data_.SetOffsetAt(0, GlyphOffset(1, 1));
  ShapeResultRun* run3 = MakeGarbageCollected<ShapeResultRun>(*run);
  ASSERT_TRUE(run3->glyph_data_.HasNonZeroOffsets());
  EXPECT_EQ(GlyphOffset(1, 1), run3->glyph_data_.Offsets()[0]);
}

TEST_F(ShapeResultRunTest, GlyphDataCopyFromRange) {
  ShapeResultRun* run = CreateTestShapeResultRun(2, 2);

  ShapeResultRun* run2 = CreateTestShapeResultRun(2, 2);
  run2->glyph_data_.CopyFromRange(GlyphDataRange{*run});
  EXPECT_FALSE(run2->glyph_data_.HasNonZeroOffsets());

  run->glyph_data_.SetOffsetAt(0, GlyphOffset(1, 1));
  ASSERT_TRUE(run->glyph_data_.HasNonZeroOffsets());

  ShapeResultRun* run3 = CreateTestShapeResultRun(2, 2);
  run3->glyph_data_.CopyFromRange(GlyphDataRange{*run});
  ASSERT_TRUE(run3->glyph_data_.HasNonZeroOffsets());
  EXPECT_EQ(GlyphOffset(1, 1), run3->glyph_data_.Offsets()[0]);
}

TEST_F(ShapeResultRunTest, GlyphDataReverse) {
  ShapeResultRun* run = CreateTestShapeResultRun(2, 2);

  run->glyph_data_.Reverse();
  EXPECT_FALSE(run->glyph_data_.HasNonZeroOffsets());

  run->glyph_data_.SetOffsetAt(0, GlyphOffset(1, 1));
  ASSERT_TRUE(run->glyph_data_.HasNonZeroOffsets());
  run->glyph_data_.Reverse();
  EXPECT_EQ(GlyphOffset(), run->glyph_data_.Offsets()[0]);
  EXPECT_EQ(GlyphOffset(1, 1), run->glyph_data_.Offsets()[1]);
}

TEST_F(ShapeResultRunTest, GlyphDataAddOffsetHeightAt) {
  ShapeResultRun* run = CreateTestShapeResultRun(2, 2);

  run->glyph_data_.AddOffsetHeightAt(1, 1.5f);
  ASSERT_TRUE(run->glyph_data_.HasNonZeroOffsets());
  EXPECT_EQ(GlyphOffset(0, 1.5f), run->glyph_data_.Offsets()[1]);

  run->glyph_data_.AddOffsetHeightAt(1, 2.0f);
  ASSERT_TRUE(run->glyph_data_.HasNonZeroOffsets());
  EXPECT_EQ(GlyphOffset(0, 3.5f), run->glyph_data_.Offsets()[1]);
}

TEST_F(ShapeResultRunTest, GlyphDataAddOffsetWidthAt) {
  ShapeResultRun* run = CreateTestShapeResultRun(2, 2);

  run->glyph_data_.AddOffsetWidthAt(1, 1.5f);
  ASSERT_TRUE(run->glyph_data_.HasNonZeroOffsets());
  EXPECT_EQ(GlyphOffset(1.5f, 0), run->glyph_data_.Offsets()[1]);

  run->glyph_data_.AddOffsetWidthAt(1, 2.0f);
  ASSERT_TRUE(run->glyph_data_.HasNonZeroOffsets());
  EXPECT_EQ(GlyphOffset(3.5f, 0), run->glyph_data_.Offsets()[1]);
}

TEST_F(ShapeResultRunTest, GlyphDataSetAt) {
  ShapeResultRun* run = CreateTestShapeResultRun(2, 2);

  run->glyph_data_.SetOffsetAt(0, GlyphOffset());
  // Setting a zero offset should not allocate storage if it wasn't already
  // allocated.
  EXPECT_FALSE(run->glyph_data_.HasNonZeroOffsets());

  run->glyph_data_.SetOffsetAt(1, GlyphOffset(1, 1));
  ASSERT_TRUE(run->glyph_data_.HasNonZeroOffsets());
  EXPECT_EQ(GlyphOffset(1, 1), run->glyph_data_.Offsets()[1]);
}

TEST_F(ShapeResultRunTest, GlyphDataShrink) {
  // Case 1: Shrink when no offsets are allocated.
  ShapeResultRun* run_no_offsets = CreateTestShapeResultRun(3, 3);
  EXPECT_FALSE(run_no_offsets->glyph_data_.HasNonZeroOffsets());
  EXPECT_EQ(3u, run_no_offsets->glyph_data_.size());

  run_no_offsets->glyph_data_.Shrink(2);  // Shrink from 3 to 2
  // Offsets should remain unallocated.
  EXPECT_FALSE(run_no_offsets->glyph_data_.HasNonZeroOffsets());
  EXPECT_EQ(2u, run_no_offsets->glyph_data_.size());  // Data should shrink.

  run_no_offsets->glyph_data_.Shrink(1);  // Shrink from 2 to 1
  EXPECT_FALSE(run_no_offsets->glyph_data_.HasNonZeroOffsets());
  EXPECT_EQ(1u, run_no_offsets->glyph_data_.size());

  // Case 2: Shrink when offsets are allocated.
  ShapeResultRun* run_with_offsets = CreateTestShapeResultRun(3, 3);
  run_with_offsets->glyph_data_.SetOffsetAt(0, GlyphOffset(1, 0));
  run_with_offsets->glyph_data_.SetOffsetAt(1, GlyphOffset(2, 0));
  run_with_offsets->glyph_data_.SetOffsetAt(2, GlyphOffset(3, 0));
  ASSERT_TRUE(run_with_offsets->glyph_data_.HasNonZeroOffsets());
  EXPECT_EQ(3u, run_with_offsets->glyph_data_.size());
  ASSERT_EQ(3u, run_with_offsets->glyph_data_.Offsets().size());

  // Shrink to a smaller size.
  run_with_offsets->glyph_data_.Shrink(2);
  ASSERT_TRUE(run_with_offsets->glyph_data_.HasNonZeroOffsets());
  EXPECT_EQ(2u, run_with_offsets->glyph_data_.size());
  ASSERT_EQ(2u, run_with_offsets->glyph_data_.Offsets().size());
  EXPECT_EQ(GlyphOffset(1, 0), run_with_offsets->glyph_data_.Offsets()[0]);
  EXPECT_EQ(GlyphOffset(2, 0), run_with_offsets->glyph_data_.Offsets()[1]);

  // Shrink further.
  run_with_offsets->glyph_data_.Shrink(1);
  ASSERT_TRUE(run_with_offsets->glyph_data_.HasNonZeroOffsets());
  EXPECT_EQ(1u, run_with_offsets->glyph_data_.size());
  ASSERT_EQ(1u, run_with_offsets->glyph_data_.Offsets().size());
  EXPECT_EQ(GlyphOffset(1, 0), run_with_offsets->glyph_data_.Offsets()[0]);

  // Case 3: Shrink to the same size (no-op for vector sizes).
  ShapeResultRun* run_shrink_same_size = CreateTestShapeResultRun(2, 2);
  run_shrink_same_size->glyph_data_.SetOffsetAt(0, GlyphOffset(5, 0));
  ASSERT_TRUE(run_shrink_same_size->glyph_data_.HasNonZeroOffsets());
  run_shrink_same_size->glyph_data_.Shrink(2);  // Shrink to current size.
  ASSERT_TRUE(run_shrink_same_size->glyph_data_.HasNonZeroOffsets());
  EXPECT_EQ(2u, run_shrink_same_size->glyph_data_.size());
  ASSERT_EQ(2u, run_shrink_same_size->glyph_data_.Offsets().size());
  EXPECT_EQ(GlyphOffset(5, 0), run_shrink_same_size->glyph_data_.Offsets()[0]);
}

}  // namespace blink
