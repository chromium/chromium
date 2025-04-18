// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/platform/fonts/shaping/shape_result_run.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class ShapeResultRunTest : public testing::Test {};

TEST_F(ShapeResultRunTest, CopyConstructor) {
  GlyphOffsetArray offsets(2);

  GlyphOffsetArray offsets2(offsets);
  EXPECT_FALSE(offsets2.HasStorage());

  offsets.SetAt(0, GlyphOffset(1, 1));
  GlyphOffsetArray offsets3(offsets);
  ASSERT_TRUE(offsets3.HasStorage());
  EXPECT_EQ(GlyphOffset(1, 1), offsets3.GetStorage()[0]);
}

TEST_F(ShapeResultRunTest, CopyFromRange) {
  GlyphOffsetArray offsets(2);
  HarfBuzzRunGlyphData glyhp_data[2];

  GlyphOffsetArray offsets2(2);
  offsets2.CopyFromRange({&glyhp_data[0], &glyhp_data[2], nullptr});
  EXPECT_FALSE(offsets2.HasStorage());

  offsets.SetAt(0, GlyphOffset(1, 1));
  ASSERT_TRUE(offsets.HasStorage());

  GlyphOffsetArray offsets3(2);
  offsets3.CopyFromRange(
      {&glyhp_data[0], &glyhp_data[2], offsets.GetStorage()});
  ASSERT_TRUE(offsets3.HasStorage());
  EXPECT_EQ(GlyphOffset(1, 1), offsets3.GetStorage()[0]);
}

TEST_F(ShapeResultRunTest, GlyphOffsetArrayReverse) {
  GlyphOffsetArray offsets(2);

  offsets.Reverse();
  EXPECT_FALSE(offsets.HasStorage());

  offsets.SetAt(0, GlyphOffset(1, 1));
  ASSERT_TRUE(offsets.HasStorage());
  offsets.Reverse();
  EXPECT_EQ(GlyphOffset(), offsets.GetStorage()[0]);
  EXPECT_EQ(GlyphOffset(1, 1), offsets.GetStorage()[1]);
}

TEST_F(ShapeResultRunTest, GlyphOffsetArraySetAddOffsetHeightAt) {
  GlyphOffsetArray offsets(2);

  offsets.AddHeightAt(1, 1.5f);
  ASSERT_TRUE(offsets.HasStorage());
  EXPECT_EQ(GlyphOffset(0, 1.5f), offsets.GetStorage()[1]);

  offsets.AddHeightAt(1, 2.0f);
  ASSERT_TRUE(offsets.HasStorage());
  EXPECT_EQ(GlyphOffset(0, 3.5f), offsets.GetStorage()[1]);
}

TEST_F(ShapeResultRunTest, GlyphOffsetArraySetAddOffsetWidthAt) {
  GlyphOffsetArray offsets(2);

  offsets.AddWidthAt(1, 1.5f);
  ASSERT_TRUE(offsets.HasStorage());
  EXPECT_EQ(GlyphOffset(1.5f, 0), offsets.GetStorage()[1]);

  offsets.AddWidthAt(1, 2.0f);
  ASSERT_TRUE(offsets.HasStorage());
  EXPECT_EQ(GlyphOffset(3.5f, 0), offsets.GetStorage()[1]);
}

TEST_F(ShapeResultRunTest, GlyphOffsetArraySetAt) {
  GlyphOffsetArray offsets(2);

  offsets.SetAt(0, GlyphOffset());
  EXPECT_FALSE(offsets.HasStorage());

  offsets.SetAt(1, GlyphOffset(1, 1));
  EXPECT_TRUE(offsets.HasStorage());
}

TEST_F(ShapeResultRunTest, GlyphOffsetArrayShrink) {
  GlyphOffsetArray offsets(3);

  offsets.Shrink(2);
  EXPECT_FALSE(offsets.HasStorage());

  offsets.SetAt(0, GlyphOffset(1, 1));
  ASSERT_TRUE(offsets.HasStorage());

  offsets.Shrink(1);
  ASSERT_TRUE(offsets.HasStorage());
  EXPECT_EQ(GlyphOffset(1, 1), offsets.GetStorage()[0]);
}

}  // namespace blink
