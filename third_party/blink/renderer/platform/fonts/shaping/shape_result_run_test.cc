// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/shaping/shape_result_run.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class ShapeResultRunTest : public testing::Test {};

TEST_F(ShapeResultRunTest, CopyConstructor) {
  GlyphOffsetArray offsets;

  GlyphOffsetArray offsets2(offsets);
  EXPECT_FALSE(offsets2.HasStorage());

  offsets.SetAt(0, GlyphOffset(1, 1), 2);
  GlyphOffsetArray offsets3(offsets);
  ASSERT_TRUE(offsets3.HasStorage());
  EXPECT_EQ(GlyphOffset(1, 1), offsets3.GetStorage()[0]);
}

TEST_F(ShapeResultRunTest, CopyFromRange) {
  ShapeResultRun* run = MakeGarbageCollected<ShapeResultRun>(
      nullptr, HB_DIRECTION_LTR, CanvasRotationInVertical::kRegular,
      HB_SCRIPT_COMMON, 0, 2, 2);

  GlyphOffsetArray offsets2;
  offsets2.CopyFromRange(GlyphDataRange{*run});
  EXPECT_FALSE(offsets2.HasStorage());

  run->glyph_data_.SetOffsetAt(0, GlyphOffset(1, 1));
  ASSERT_TRUE(run->glyph_data_.HasNonZeroOffsets());

  GlyphOffsetArray offsets3;
  offsets3.CopyFromRange(GlyphDataRange{*run});
  ASSERT_TRUE(offsets3.HasStorage());
  EXPECT_EQ(GlyphOffset(1, 1), offsets3.GetStorage()[0]);
}

TEST_F(ShapeResultRunTest, GlyphOffsetArrayReverse) {
  GlyphOffsetArray offsets;

  offsets.Reverse();
  EXPECT_FALSE(offsets.HasStorage());

  offsets.SetAt(0, GlyphOffset(1, 1), 2);
  ASSERT_TRUE(offsets.HasStorage());
  offsets.Reverse();
  EXPECT_EQ(GlyphOffset(), offsets.GetStorage()[0]);
  EXPECT_EQ(GlyphOffset(1, 1), UNSAFE_TODO(offsets.GetStorage()[1]));
}

TEST_F(ShapeResultRunTest, GlyphOffsetArraySetAddOffsetHeightAt) {
  GlyphOffsetArray offsets;

  offsets.AddHeightAt(1, 1.5f, 2);
  ASSERT_TRUE(offsets.HasStorage());
  EXPECT_EQ(GlyphOffset(0, 1.5f), UNSAFE_TODO(offsets.GetStorage()[1]));

  offsets.AddHeightAt(1, 2.0f, 2);
  ASSERT_TRUE(offsets.HasStorage());
  EXPECT_EQ(GlyphOffset(0, 3.5f), UNSAFE_TODO(offsets.GetStorage()[1]));
}

TEST_F(ShapeResultRunTest, GlyphOffsetArraySetAddOffsetWidthAt) {
  GlyphOffsetArray offsets;

  offsets.AddWidthAt(1, 1.5f, 2);
  ASSERT_TRUE(offsets.HasStorage());
  EXPECT_EQ(GlyphOffset(1.5f, 0), UNSAFE_TODO(offsets.GetStorage()[1]));

  offsets.AddWidthAt(1, 2.0f, 2);
  ASSERT_TRUE(offsets.HasStorage());
  EXPECT_EQ(GlyphOffset(3.5f, 0), UNSAFE_TODO(offsets.GetStorage()[1]));
}

TEST_F(ShapeResultRunTest, GlyphOffsetArraySetAt) {
  GlyphOffsetArray offsets;

  offsets.SetAt(0, GlyphOffset(), 2);
  EXPECT_FALSE(offsets.HasStorage());

  offsets.SetAt(1, GlyphOffset(1, 1), 2);
  EXPECT_TRUE(offsets.HasStorage());
}

TEST_F(ShapeResultRunTest, GlyphOffsetArrayShrink) {
  GlyphOffsetArray offsets;

  offsets.Shrink(2);
  EXPECT_FALSE(offsets.HasStorage());

  offsets.SetAt(0, GlyphOffset(1, 1), 2);
  ASSERT_TRUE(offsets.HasStorage());

  offsets.Shrink(1);
  ASSERT_TRUE(offsets.HasStorage());
  EXPECT_EQ(GlyphOffset(1, 1), offsets.GetStorage()[0]);
}

}  // namespace blink
