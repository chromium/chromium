// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/ng_ink_overflow.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"

namespace blink {

namespace {

PhysicalRect FromFloatRound(const gfx::RectF& rect) {
  return {LayoutUnit::FromFloatRound(rect.x()),
          LayoutUnit::FromFloatRound(rect.y()),
          LayoutUnit::FromFloatRound(rect.width()),
          LayoutUnit::FromFloatRound(rect.height())};
}

using testing::ElementsAre;

class NGInkOverflowTest : public testing::Test, private ScopedLayoutNGForTest {
 public:
  NGInkOverflowTest() : ScopedLayoutNGForTest(true) {}
};

TEST_F(NGInkOverflowTest, Empty) {
  NGInkOverflow overflow;
  NGInkOverflow::Type type =
      overflow.Set(NGInkOverflow::Type::kNotSet, {0, 0, 100, 117},
                   // This does not affect the visual rect even if the offset is
                   // outside, because the size is empty.
                   {-24, 50, 0, 0}, {100, 117});
  EXPECT_EQ(type, NGInkOverflow::Type::kNone);
}

#define MIN_LARGE32 4
#define MIN_LARGE64 1024
#if UINTPTR_MAX == 0xFFFFFFFF
#define MIN_LARGE MIN_LARGE32
#elif UINTPTR_MAX == 0xFFFFFFFFFFFFFFFFu
#define MIN_LARGE MIN_LARGE64
#endif
#define MAX_SMALL (LayoutUnit(MIN_LARGE) - LayoutUnit::Epsilon())

struct RectData {
  PhysicalSize size;
  gfx::RectF rect;
  gfx::RectF expect;
  NGInkOverflow::Type type;

  NGInkOverflow::Type ExpectedTypeForContents() const {
    if (type == NGInkOverflow::Type::kSelf)
      return NGInkOverflow::Type::kContents;
    if (type == NGInkOverflow::Type::kSmallSelf)
      return NGInkOverflow::Type::kSmallContents;
    return type;
  }
} rect_data[] = {
    {{20, 10}, {0, 0, 0, 0}, {0, 0, 20, 10}, NGInkOverflow::Type::kNone},
    {{20, 10}, {0, 0, 20, 10}, {0, 0, 20, 10}, NGInkOverflow::Type::kNone},

    // 2: One of values is max small, all others are 0.
    {{20, 10},
     {0, 0, MAX_SMALL + 20, 10},
     {0, 0, MAX_SMALL + 20, 10},
     NGInkOverflow::Type::kSmallSelf},
    {{20, 10},
     {0, 0, 20, MAX_SMALL + 10},
     {0, 0, 20, MAX_SMALL + 10},
     NGInkOverflow::Type::kSmallSelf},
    {{20, 10},
     {-MAX_SMALL, 0, MAX_SMALL + 20, 10},
     {-MAX_SMALL, 0, MAX_SMALL + 20, 10},
     NGInkOverflow::Type::kSmallSelf},
    {{20, 10},
     {0, -MAX_SMALL, 20, MAX_SMALL + 10},
     {0, -MAX_SMALL, 20, MAX_SMALL + 10},
     NGInkOverflow::Type::kSmallSelf},

    // 6: One of values is large, all others are 0.
    {{20, 10},
     {0, 0, MIN_LARGE + 20, 10},
     {0, 0, MIN_LARGE + 20, 10},
     NGInkOverflow::Type::kSelf},
    {{20, 10},
     {0, 0, 20, MIN_LARGE + 10},
     {0, 0, 20, MIN_LARGE + 10},
     NGInkOverflow::Type::kSelf},
    {{20, 10},
     {-MIN_LARGE, 0, MIN_LARGE + 20, 10},
     {-MIN_LARGE, 0, MIN_LARGE + 20, 10},
     NGInkOverflow::Type::kSelf},
    {{20, 10},
     {0, -MIN_LARGE, 20, MIN_LARGE + 10},
     {0, -MIN_LARGE, 20, MIN_LARGE + 10},
     NGInkOverflow::Type::kSelf},

    // 10: All values are the max small values.
    {{20, 10},
     {-MAX_SMALL, -MAX_SMALL, MAX_SMALL * 2 + 20, MAX_SMALL * 2 + 10},
     {-MAX_SMALL, -MAX_SMALL, MAX_SMALL * 2 + 20, MAX_SMALL * 2 + 10},
     NGInkOverflow::Type::kSmallSelf},
};

class RectDataTest : public NGInkOverflowTest,
                     public testing::WithParamInterface<RectData> {};

INSTANTIATE_TEST_SUITE_P(NGInkOverflowTest,
                         RectDataTest,
                         testing::ValuesIn(rect_data));

TEST_P(RectDataTest, Self) {
  const RectData data = GetParam();
  NGInkOverflow ink_overflow;
  NGInkOverflow::Type type = ink_overflow.SetSelf(
      NGInkOverflow::Type::kNotSet, FromFloatRound(data.rect), data.size);
  EXPECT_EQ(type, data.type);
  PhysicalRect result = ink_overflow.Self(type, data.size);
  EXPECT_EQ(result, FromFloatRound(data.expect));
  ink_overflow.Reset(type);
}

TEST_P(RectDataTest, Contents) {
  const RectData data = GetParam();
  NGInkOverflow ink_overflow;
  NGInkOverflow::Type type = ink_overflow.Set(
      NGInkOverflow::Type::kNotSet, {}, FromFloatRound(data.rect), data.size);
  EXPECT_EQ(type, data.ExpectedTypeForContents());
  PhysicalRect result = ink_overflow.SelfAndContents(type, data.size);
  EXPECT_EQ(result, FromFloatRound(data.expect));
  ink_overflow.Reset(type);
}

TEST_P(RectDataTest, Copy) {
  const RectData data = GetParam();
  NGInkOverflow original;
  NGInkOverflow::Type type = original.SetSelf(
      NGInkOverflow::Type::kNotSet, FromFloatRound(data.rect), data.size);
  NGInkOverflow copy(type, original);
  EXPECT_EQ(copy.Self(type, data.size), original.Self(type, data.size));
  original.Reset(type);
  copy.Reset(type);
}

struct SelfAndContentsData {
  PhysicalSize size;
  PhysicalRect self;
  PhysicalRect contents;
  NGInkOverflow::Type type;
} self_and_contents_data[] = {
    {{10, 10}, {0, 0, 10, 10}, {0, 0, 10, 10}, NGInkOverflow::Type::kNone},
    {{10, 10},
     {-1, -1, 12, 12},
     {0, 0, 20, 20},
     NGInkOverflow::Type::kSelfAndContents},
};

class SelfAndContentsDataTest
    : public NGInkOverflowTest,
      public testing::WithParamInterface<SelfAndContentsData> {};

INSTANTIATE_TEST_SUITE_P(NGInkOverflowTest,
                         SelfAndContentsDataTest,
                         testing::ValuesIn(self_and_contents_data));

TEST_P(SelfAndContentsDataTest, SelfAndContents) {
  const SelfAndContentsData data = GetParam();
  NGInkOverflow ink_overflow;
  NGInkOverflow::Type type = ink_overflow.Set(
      NGInkOverflow::Type::kNotSet, data.self, data.contents, data.size);
  EXPECT_EQ(type, data.type);
  EXPECT_EQ(ink_overflow.Self(type, data.size), data.self);
  EXPECT_EQ(ink_overflow.SelfAndContents(type, data.size),
            UnionRect(data.self, data.contents));
  ink_overflow.Reset(type);
}

}  // namespace

}  // namespace blink
