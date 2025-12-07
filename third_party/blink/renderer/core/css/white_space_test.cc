// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/white_space.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/css/css_identifier_value.h"

namespace blink {

class WhiteSpaceValuesTest : public testing::Test,
                             public testing::WithParamInterface<EWhiteSpace> {};

static const EWhiteSpace whitespace_values[] = {
    EWhiteSpace::kNormal,  EWhiteSpace::kNowrap,  EWhiteSpace::kPre,
    EWhiteSpace::kPreLine, EWhiteSpace::kPreWrap, EWhiteSpace::kBreakSpaces,
};

INSTANTIATE_TEST_SUITE_P(WhiteSpaceTest,
                         WhiteSpaceValuesTest,
                         testing::ValuesIn(whitespace_values));

TEST_P(WhiteSpaceValuesTest, CSSValue) {
  const EWhiteSpace whitespace = GetParam();
  const CSSValueID css_value_id = PlatformEnumToCSSValueID(whitespace);
  EXPECT_NE(css_value_id, CSSValueID::kInvalid);
  EXPECT_NE(css_value_id, CSSValueID::kNone);
  EXPECT_EQ(whitespace, CssValueIDToPlatformEnum<EWhiteSpace>(css_value_id));

  const CSSIdentifierValue* css_value = CSSIdentifierValue::Create(whitespace);
  EXPECT_TRUE(css_value);
  EXPECT_EQ(whitespace, css_value->ConvertTo<EWhiteSpace>());
}

TEST(WhiteSpaceTest, Normal) {
  const EWhiteSpace normal = EWhiteSpace::kNormal;
  EXPECT_FALSE(ShouldPreserveWhiteSpaces(ToWhiteSpaceCollapse(normal)));
  EXPECT_FALSE(ShouldPreserveBreaks(ToWhiteSpaceCollapse(normal)));
  EXPECT_FALSE(ShouldBreakSpaces(ToWhiteSpaceCollapse(normal)));
  EXPECT_TRUE(ShouldWrapLine(ToTextWrapMode(normal)));
}

TEST(WhiteSpaceTest, Nowrap) {
  const EWhiteSpace nowrap = EWhiteSpace::kNowrap;
  EXPECT_FALSE(ShouldPreserveWhiteSpaces(ToWhiteSpaceCollapse(nowrap)));
  EXPECT_FALSE(ShouldPreserveBreaks(ToWhiteSpaceCollapse(nowrap)));
  EXPECT_FALSE(ShouldBreakSpaces(ToWhiteSpaceCollapse(nowrap)));
  EXPECT_FALSE(ShouldWrapLine(ToTextWrapMode(nowrap)));
}

TEST(WhiteSpaceTest, Pre) {
  const EWhiteSpace pre = EWhiteSpace::kPre;
  EXPECT_TRUE(ShouldPreserveWhiteSpaces(ToWhiteSpaceCollapse(pre)));
  EXPECT_TRUE(ShouldPreserveBreaks(ToWhiteSpaceCollapse(pre)));
  EXPECT_FALSE(ShouldBreakSpaces(ToWhiteSpaceCollapse(pre)));
  EXPECT_FALSE(ShouldWrapLine(ToTextWrapMode(pre)));
}

TEST(WhiteSpaceTest, PreLine) {
  const EWhiteSpace pre_line = EWhiteSpace::kPreLine;
  EXPECT_FALSE(ShouldPreserveWhiteSpaces(ToWhiteSpaceCollapse(pre_line)));
  EXPECT_TRUE(ShouldPreserveBreaks(ToWhiteSpaceCollapse(pre_line)));
  EXPECT_FALSE(ShouldBreakSpaces(ToWhiteSpaceCollapse(pre_line)));
  EXPECT_TRUE(ShouldWrapLine(ToTextWrapMode(pre_line)));
}

TEST(WhiteSpaceTest, PreWrap) {
  const EWhiteSpace pre_wrap = EWhiteSpace::kPreWrap;
  EXPECT_TRUE(ShouldPreserveWhiteSpaces(ToWhiteSpaceCollapse(pre_wrap)));
  EXPECT_TRUE(ShouldPreserveBreaks(ToWhiteSpaceCollapse(pre_wrap)));
  EXPECT_FALSE(ShouldBreakSpaces(ToWhiteSpaceCollapse(pre_wrap)));
  EXPECT_TRUE(ShouldWrapLine(ToTextWrapMode(pre_wrap)));
}

TEST(WhiteSpaceTest, BreakSpaces) {
  const EWhiteSpace break_spaces = EWhiteSpace::kBreakSpaces;
  EXPECT_TRUE(ShouldPreserveWhiteSpaces(ToWhiteSpaceCollapse(break_spaces)));
  EXPECT_TRUE(ShouldPreserveBreaks(ToWhiteSpaceCollapse(break_spaces)));
  EXPECT_TRUE(ShouldBreakSpaces(ToWhiteSpaceCollapse(break_spaces)));
  EXPECT_TRUE(ShouldWrapLine(ToTextWrapMode(break_spaces)));
}

}  // namespace blink
