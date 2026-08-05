// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/style/applied_text_decoration.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/style/computed_style_base_constants.h"
#include "third_party/blink/renderer/core/style/text_decoration_thickness.h"

namespace blink {

namespace {

AppliedTextDecoration MakeDecoration(
    TextDecorationLine line,
    ETextDecorationStyle style,
    Color color,
    TextDecorationThickness thickness,
    Length underline_offset,
    TextDecorationInset inset = TextDecorationInset(),
    EBoxDecorationBreak box_decoration_break = EBoxDecorationBreak::kSlice) {
  return AppliedTextDecoration(line, style, color, thickness, underline_offset,
                               inset, box_decoration_break);
}

}  // namespace

TEST(AppliedTextDecorationTest, OperatorEqual) {
  {
    AppliedTextDecoration instance1 = MakeDecoration(
        TextDecorationLine::kUnderline, ETextDecorationStyle::kSolid, Color(),
        TextDecorationThickness(Length()), Length());
    AppliedTextDecoration instance2 = MakeDecoration(
        TextDecorationLine::kUnderline, ETextDecorationStyle::kSolid, Color(),
        TextDecorationThickness(Length()), Length());
    EXPECT_EQ(instance1, instance2);
  }

  // Only TextDecorationLine is different.
  {
    AppliedTextDecoration instance1 = MakeDecoration(
        TextDecorationLine::kUnderline, ETextDecorationStyle::kSolid, Color(),
        TextDecorationThickness(Length()), Length());
    AppliedTextDecoration instance2 = MakeDecoration(
        TextDecorationLine::kOverline, ETextDecorationStyle::kSolid, Color(),
        TextDecorationThickness(Length()), Length());
    EXPECT_NE(instance1, instance2);
  }

  // Only ETextDecorationStyle is different.
  {
    AppliedTextDecoration instance1 = MakeDecoration(
        TextDecorationLine::kUnderline, ETextDecorationStyle::kSolid, Color(),
        TextDecorationThickness(Length()), Length());
    AppliedTextDecoration instance2 = MakeDecoration(
        TextDecorationLine::kUnderline, ETextDecorationStyle::kDouble, Color(),
        TextDecorationThickness(Length()), Length());
    EXPECT_NE(instance1, instance2);
  }

  // Only Color is different.
  {
    AppliedTextDecoration instance1 = MakeDecoration(
        TextDecorationLine::kUnderline, ETextDecorationStyle::kSolid, Color(),
        TextDecorationThickness(Length()), Length());
    AppliedTextDecoration instance2 = MakeDecoration(
        TextDecorationLine::kUnderline, ETextDecorationStyle::kSolid,
        Color(Color::kBlack), TextDecorationThickness(Length()), Length());
    EXPECT_NE(instance1, instance2);
  }

  // Only TextDecorationThickness is different.
  {
    AppliedTextDecoration instance1 = MakeDecoration(
        TextDecorationLine::kUnderline, ETextDecorationStyle::kSolid, Color(),
        TextDecorationThickness(Length(42, Length::kFixed)), Length());
    AppliedTextDecoration instance2 = MakeDecoration(
        TextDecorationLine::kUnderline, ETextDecorationStyle::kSolid, Color(),
        TextDecorationThickness(Length()), Length());
    EXPECT_NE(instance1, instance2);
  }

  // Only |underline_offset_| is different.
  {
    AppliedTextDecoration instance1 = MakeDecoration(
        TextDecorationLine::kUnderline, ETextDecorationStyle::kSolid, Color(),
        TextDecorationThickness(Length()), Length(13, Length::kPercent));
    AppliedTextDecoration instance2 = MakeDecoration(
        TextDecorationLine::kUnderline, ETextDecorationStyle::kSolid, Color(),
        TextDecorationThickness(Length()), Length(13, Length::kFixed));
    EXPECT_NE(instance1, instance2);
  }

  // Only |text_decoration_inset_| is different.
  {
    AppliedTextDecoration instance1 = MakeDecoration(
        TextDecorationLine::kUnderline, ETextDecorationStyle::kSolid, Color(),
        TextDecorationThickness(Length()), Length(),
        TextDecorationInset(Length::Fixed(5), Length::Fixed(-5)));
    AppliedTextDecoration instance2 = MakeDecoration(
        TextDecorationLine::kUnderline, ETextDecorationStyle::kSolid, Color(),
        TextDecorationThickness(Length()), Length(),
        TextDecorationInset(Length::Fixed(4), Length::Fixed(-4)));
    EXPECT_NE(instance1, instance2);
  }

  // Only |box_decoration_break_| is different.
  {
    AppliedTextDecoration instance1 = MakeDecoration(
        TextDecorationLine::kUnderline, ETextDecorationStyle::kSolid, Color(),
        TextDecorationThickness(Length()), Length(), TextDecorationInset(),
        EBoxDecorationBreak::kSlice);
    AppliedTextDecoration instance2 = MakeDecoration(
        TextDecorationLine::kUnderline, ETextDecorationStyle::kSolid, Color(),
        TextDecorationThickness(Length()), Length(), TextDecorationInset(),
        EBoxDecorationBreak::kClone);
    EXPECT_NE(instance1, instance2);
  }
}

TEST(AppliedTextDecorationTest, CopyConstructor) {
  {
    AppliedTextDecoration instance1 = MakeDecoration(
        TextDecorationLine::kUnderline, ETextDecorationStyle::kSolid, Color(),
        TextDecorationThickness(Length()), Length());
    AppliedTextDecoration instance2(instance1);
    EXPECT_EQ(instance1, instance2);
  }
}

TEST(AppliedTextDecorationTest, Assignment) {
  {
    AppliedTextDecoration instance1 = MakeDecoration(
        TextDecorationLine::kUnderline, ETextDecorationStyle::kSolid, Color(),
        TextDecorationThickness(Length()), Length());
    AppliedTextDecoration instance2 = MakeDecoration(
        TextDecorationLine::kOverline, ETextDecorationStyle::kSolid, Color(),
        TextDecorationThickness(Length()), Length());
    EXPECT_NE(instance1, instance2);
    instance2 = instance1;
    EXPECT_EQ(instance1, instance2);
  }
}

}  // namespace blink
