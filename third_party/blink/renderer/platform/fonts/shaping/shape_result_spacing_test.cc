// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/shaping/shape_result_spacing.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

namespace {

unsigned ExpansionOpportunityCount(const String& text,
                                   TextJustify method,
                                   InlineLayoutUnit expansion) {
  ShapeResultSpacing spacing(text);
  {
    ShapeResultSpacing::ExpansionSetup setup(
        expansion, &spacing, /* allows_leading_expansion */ false,
        /* allows_trailing_expansion */ false);
    setup.CountOpportunities(method, spacing.Text(), TextDirection::kLtr);
  }
  return spacing.ExpansionOppotunityCount();
}

}  // namespace

TEST(ShapeResultSpacingTest, ExpansionOppotunityCountZws) {
  // ZERO WIDTH SPACE, one of Default Ignorable Code Point.
  String text(u"\u200B");
  EXPECT_EQ(0u, ExpansionOpportunityCount(text, TextJustify::kAuto,
                                          InlineLayoutUnit(42)));
}

TEST(ShapeResultSpacingTest, ExpansionOppotunityCountBidiControlAndCjk) {
  // A hiragana, LEFT-TO-RIGHT ISOLATE (a Default Ignorable Code Point), and
  // another hiragana.
  String text(u"\u3042\u2066\u3043");
  EXPECT_EQ(1u, ExpansionOpportunityCount(text, TextJustify::kAuto,
                                          InlineLayoutUnit(42)));
}

}  // namespace blink
