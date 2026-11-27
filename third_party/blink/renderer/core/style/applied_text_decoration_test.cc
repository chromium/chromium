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

TEST(AppliedTextDecorationTest, VisitedColorEquality) {
  const Color red = Color::FromRGB(255, 0, 0);
  const Color blue = Color::FromRGB(0, 0, 255);
  const Color green = Color::FromRGB(0, 128, 0);

  AppliedTextDecoration unvisited(
      TextDecorationLine::kUnderline, ETextDecorationStyle::kSolid, red, blue,
      /*is_visited=*/false, TextDecorationThickness(Length()), Length(),
      TextDecorationInset(), EBoxDecorationBreak::kSlice);
  AppliedTextDecoration visited(
      TextDecorationLine::kUnderline, ETextDecorationStyle::kSolid, red, blue,
      /*is_visited=*/true, TextDecorationThickness(Length()), Length(),
      TextDecorationInset(), EBoxDecorationBreak::kSlice);

  // GetColor() reflects the link's visited state, while operator== must not.
  EXPECT_EQ(unvisited.GetColor(), red);
  EXPECT_EQ(visited.GetColor(), blue);
  EXPECT_EQ(unvisited, visited);

  // Changing unvisited color makes both unvisited and visited decorations
  // unequal to the previous decoration, even when unvisited matches visited.
  AppliedTextDecoration unvisited_toggled(
      TextDecorationLine::kUnderline, ETextDecorationStyle::kSolid, blue, blue,
      /*is_visited=*/false, TextDecorationThickness(Length()), Length(),
      TextDecorationInset(), EBoxDecorationBreak::kSlice);
  AppliedTextDecoration visited_toggled(
      TextDecorationLine::kUnderline, ETextDecorationStyle::kSolid, blue, blue,
      /*is_visited=*/true, TextDecorationThickness(Length()), Length(),
      TextDecorationInset(), EBoxDecorationBreak::kSlice);
  EXPECT_NE(unvisited, unvisited_toggled);
  EXPECT_NE(visited, visited_toggled);
  EXPECT_EQ(unvisited_toggled, visited_toggled);

  // Changing visited color also makes both unequal.
  AppliedTextDecoration visited_color_changed(
      TextDecorationLine::kUnderline, ETextDecorationStyle::kSolid, red, green,
      /*is_visited=*/false, TextDecorationThickness(Length()), Length(),
      TextDecorationInset(), EBoxDecorationBreak::kSlice);
  EXPECT_NE(unvisited, visited_color_changed);
  EXPECT_NE(visited, visited_color_changed);

  // SetColor() updates both channels so painted color and equality stay
  // consistent.
  visited.SetColor(Color::kTransparent);
  unvisited.SetColor(Color::kTransparent);
  EXPECT_EQ(visited.GetColor(), Color::kTransparent);
  EXPECT_EQ(unvisited.GetColor(), Color::kTransparent);
  EXPECT_EQ(unvisited, visited);
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
