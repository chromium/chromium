// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/style/applied_text_decoration.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/style/computed_style_base_constants.h"
#include "third_party/blink/renderer/core/style/text_decoration_thickness.h"

namespace blink {

TEST(AppliedTextDecorationTest, OperatorEqual) {
  {
    AppliedTextDecoration instance1(
        TextDecorationLine::kUnderline, ETextDecorationStyle::kSolid, Color(),
        TextDecorationThickness(Length()), Length());
    AppliedTextDecoration instance2(
        TextDecorationLine::kUnderline, ETextDecorationStyle::kSolid, Color(),
        TextDecorationThickness(Length()), Length());
    EXPECT_EQ(instance1, instance2);
  }

  // Only TextDecorationLine is different.
  {
    AppliedTextDecoration instance1(
        TextDecorationLine::kUnderline, ETextDecorationStyle::kSolid, Color(),
        TextDecorationThickness(Length()), Length());
    AppliedTextDecoration instance2(
        TextDecorationLine::kOverline, ETextDecorationStyle::kSolid, Color(),
        TextDecorationThickness(Length()), Length());
    EXPECT_NE(instance1, instance2);
  }

  // Only ETextDecorationStyle is different.
  {
    AppliedTextDecoration instance1(
        TextDecorationLine::kUnderline, ETextDecorationStyle::kSolid, Color(),
        TextDecorationThickness(Length()), Length());
    AppliedTextDecoration instance2(
        TextDecorationLine::kUnderline, ETextDecorationStyle::kDouble, Color(),
        TextDecorationThickness(Length()), Length());
    EXPECT_NE(instance1, instance2);
  }

  // Only Color is different.
  {
    AppliedTextDecoration instance1(
        TextDecorationLine::kUnderline, ETextDecorationStyle::kSolid, Color(),
        TextDecorationThickness(Length()), Length());
    AppliedTextDecoration instance2(
        TextDecorationLine::kUnderline, ETextDecorationStyle::kSolid,
        Color(Color::kBlack), TextDecorationThickness(Length()), Length());
    EXPECT_NE(instance1, instance2);
  }

  // Only TextDecorationThickness is different.
  {
    AppliedTextDecoration instance1(
        TextDecorationLine::kUnderline, ETextDecorationStyle::kSolid, Color(),
        TextDecorationThickness(Length(42, Length::kFixed)), Length());
    AppliedTextDecoration instance2(
        TextDecorationLine::kUnderline, ETextDecorationStyle::kSolid, Color(),
        TextDecorationThickness(Length()), Length());
    EXPECT_NE(instance1, instance2);
  }

  // Only |underline_offset_| is different.
  {
    AppliedTextDecoration instance1(
        TextDecorationLine::kUnderline, ETextDecorationStyle::kSolid, Color(),
        TextDecorationThickness(Length()), Length(13, Length::kPercent));
    AppliedTextDecoration instance2(
        TextDecorationLine::kUnderline, ETextDecorationStyle::kSolid, Color(),
        TextDecorationThickness(Length()), Length(13, Length::kFixed));
    EXPECT_NE(instance1, instance2);
  }
}

TEST(AppliedTextDecorationTest, CopyConstructor) {
  {
    AppliedTextDecoration instance1(
        TextDecorationLine::kUnderline, ETextDecorationStyle::kSolid, Color(),
        TextDecorationThickness(Length()), Length());
    AppliedTextDecoration instance2(instance1);
    EXPECT_EQ(instance1, instance2);
  }
}

TEST(AppliedTextDecorationTest, Assignment) {
  {
    AppliedTextDecoration instance1(
        TextDecorationLine::kUnderline, ETextDecorationStyle::kSolid, Color(),
        TextDecorationThickness(Length()), Length());
    AppliedTextDecoration instance2(
        TextDecorationLine::kOverline, ETextDecorationStyle::kSolid, Color(),
        TextDecorationThickness(Length()), Length());
    EXPECT_NE(instance1, instance2);
    instance2 = instance1;
    EXPECT_EQ(instance1, instance2);
  }
}

}  // namespace blink
