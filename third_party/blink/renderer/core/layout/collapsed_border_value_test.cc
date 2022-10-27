// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/collapsed_border_value.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/style/computed_style.h"

namespace blink {

class CollapsedBorderValueTest : public testing::Test {
 protected:
  void ExpectInvisible(const CollapsedBorderValue& v,
                       unsigned expected_width = 0) {
    EXPECT_EQ(expected_width, v.Width());
    EXPECT_FALSE(v.IsVisible());
    EXPECT_FALSE(v.LessThan(v));
    EXPECT_FALSE(v.LessThan(CollapsedBorderValue()));
    EXPECT_TRUE(v.VisuallyEquals(v));
    EXPECT_TRUE(v.VisuallyEquals(CollapsedBorderValue()));
    EXPECT_TRUE(CollapsedBorderValue().VisuallyEquals(v));
    EXPECT_TRUE(v.IsSameIgnoringColor(v));
  }

  void ExpectVisible(const CollapsedBorderValue& v, unsigned expected_width) {
    EXPECT_EQ(expected_width, v.Width());
    EXPECT_TRUE(CollapsedBorderValue().LessThan(v));
    EXPECT_FALSE(v.LessThan(CollapsedBorderValue()));
    EXPECT_TRUE(v.IsVisible());
    EXPECT_FALSE(v.VisuallyEquals(CollapsedBorderValue()));
    EXPECT_FALSE(CollapsedBorderValue().VisuallyEquals(v));
    EXPECT_TRUE(v.IsSameIgnoringColor(v));
  }

  CollapsedBorderValue Border(
      unsigned width,
      EBorderStyle border_style,
      const Color& color = Color::kBlack,
      EBorderPrecedence precedence = kBorderPrecedenceCell) {
    ComputedStyleBuilder builder(*initial_style_);
    builder.SetBorderLeftWidth(LayoutUnit(width));
    builder.SetBorderLeftStyle(border_style);
    scoped_refptr<const ComputedStyle> style = builder.TakeStyle();
    CollapsedBorderValue v(style->BorderLeft(), color, precedence);
    EXPECT_EQ(border_style, v.Style());
    EXPECT_EQ(color, v.GetColor());
    EXPECT_TRUE(v.Exists());
    EXPECT_FALSE(v.LessThan(v));
    EXPECT_TRUE(v.VisuallyEquals(v));
    return v;
  }

  void SetUp() override {
    initial_style_ = ComputedStyle::CreateInitialStyleSingleton();
  }

 private:
  scoped_refptr<const ComputedStyle> initial_style_;
};

TEST_F(CollapsedBorderValueTest, Default) {
  CollapsedBorderValue v;
  ExpectInvisible(v);
  EXPECT_FALSE(v.Exists());
}

TEST_F(CollapsedBorderValueTest, ZeroWidth) {
  ExpectInvisible(Border(0, EBorderStyle::kSolid));
}

TEST_F(CollapsedBorderValueTest, Transparent) {
  ExpectInvisible(Border(5, EBorderStyle::kSolid, Color()), 5);
}

TEST_F(CollapsedBorderValueTest, None) {
  ExpectInvisible(Border(5, EBorderStyle::kNone));
}

TEST_F(CollapsedBorderValueTest, Hidden) {
  ExpectInvisible(Border(5, EBorderStyle::kHidden));
}

TEST_F(CollapsedBorderValueTest, Visible) {
  ExpectVisible(Border(5, EBorderStyle::kSolid), 5);
}

TEST_F(CollapsedBorderValueTest, Compare) {
  const size_t kCount = 13;
  CollapsedBorderValue values[kCount] = {
      CollapsedBorderValue(),
      Border(1, EBorderStyle::kNone),
      Border(0, EBorderStyle::kSolid),
      Border(1, EBorderStyle::kDashed, Color::kWhite, kBorderPrecedenceTable),
      Border(1, EBorderStyle::kDashed),
      Border(1, EBorderStyle::kSolid, Color::kWhite, kBorderPrecedenceTable),
      Border(1, EBorderStyle::kSolid),
      Border(1, EBorderStyle::kSolid, Color::kWhite),
      Border(1, EBorderStyle::kSolid, Color()),
      Border(5, EBorderStyle::kSolid, Color()),
      Border(10, EBorderStyle::kSolid, Color::kWhite, kBorderPrecedenceTable),
      Border(10, EBorderStyle::kSolid),
      Border(1, EBorderStyle::kHidden)};
  const char* explanations[kCount] = {
      "default",
      "border style none",
      "zero width solid",
      "dashed white thin table",
      "dashed white thin cell",
      "solid white thin table",
      "solid black thin cell",
      "solid white thin cell",
      "solid transparent thin cell",
      "medium transparent",
      "solid black thick table",
      "solid black thick cell",
      "border style hidden",
  };
  int rank[kCount] = {
      0,         // The default ranks the lowest.
      1,         // Then border-style: none.
      2,         // Then zero width hidden.
      3,         // Dashed thin table border.
      4,         // Dashed thin cell border.
      5,         // Solid thin table border.
      6,  6, 6,  // Solid thin cell borders. Color doesn't affect ranking.
      7,         // Medium transparent.
      8,         // Thick table border.
      9,         // Thick cell border.
      10,        // The hidden border ranks the highest.
  };
  static constexpr int kExpectedCoversJoint[kCount][kCount] = {
      // An invisible border doesn't cover joint.
      {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
      {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
      {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
      // A visible border cover joint if the other border is invisible has
      // lower priority.
      {1, 1, 1, 1, 0, 0, 0, 0, 1, 1, 0, 0, 1},
      {1, 1, 1, 1, 1, 0, 0, 0, 1, 1, 0, 0, 1},
      {1, 1, 1, 1, 1, 1, 0, 0, 1, 1, 0, 0, 1},
      {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 1},
      {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 1},
      // A transparent border which is invisible doesn't cover joint.
      {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
      {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
      {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1},
      {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
      // A hidden border which is invisible doesn't cover joint.
      {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
  };

  for (unsigned i = 0; i < kCount; ++i) {
    SCOPED_TRACE(
        String::Format("i: %u %s rank=%d", i, explanations[i], rank[i]));
    for (unsigned j = 0; j < kCount; ++j) {
      SCOPED_TRACE(
          String::Format("j: %u %s rank=%d", j, explanations[j], rank[j]));
      // j is put before i to make testing logs easier to understand because
      // SCOPED_TRACE prints j first.
      EXPECT_EQ(rank[j] < rank[i], values[j].LessThan(values[i]));
      EXPECT_EQ(rank[j] == rank[i], values[j].IsSameIgnoringColor(values[i]));
      EXPECT_EQ(static_cast<bool>(kExpectedCoversJoint[j][i]),
                values[j].CoversJoint(values[i]));
    }
  }
}

}  // namespace blink
