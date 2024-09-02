// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/shaping/shape_result_spacing.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

TEST(ShapeResultSpacingTest, ExpansionOppotunityCountZws) {
  // ZERO WIDTH SPACE, one of Default Ignorable Code Point.
  String text(u"\u200B");
  ShapeResultSpacing<String> spacing(text);
  spacing.SetExpansion(InlineLayoutUnit(42), TextDirection::kLtr);
  EXPECT_EQ(0u, spacing.ExpansionOppotunityCount());
}

TEST(ShapeResultSpacingTest, ExpansionOppotunityCountBidiControlAndCjk) {
  // A hiragana, LEFT-TO-RIGHT ISOLATE (a Default Ignorable Code Point), and
  // another hiragana.
  String text(u"\u3042\u2066\u3043");
  ShapeResultSpacing<String> spacing(text);
  spacing.SetExpansion(InlineLayoutUnit(42), TextDirection::kLtr);
  EXPECT_EQ(1u, spacing.ExpansionOppotunityCount());
}

}  // namespace blink
