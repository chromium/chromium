// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_attr_value_tainting.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/css/css_string_value.h"
#include "third_party/blink/renderer/core/css/css_test_helpers.h"
#include "third_party/blink/renderer/core/css/css_value.h"
#include "third_party/blink/renderer/core/css/css_value_list.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"

namespace blink {

namespace {

class CSSAttrValueTaintingTest : public PageTestBase {};

TEST_F(CSSAttrValueTaintingTest, StringValue) {
  String text = String("\"abc\"%T").Replace("%T", GetCSSAttrTaintToken());
  const CSSValue* value =
      css_test_helpers::ParseValue(GetDocument(), "<string>", text);
  ASSERT_NE(value, nullptr);
  EXPECT_EQ(text, value->CssText());
  EXPECT_EQ("\"abc\"", value->UntaintedCopy()->CssText());
}

TEST_F(CSSAttrValueTaintingTest, CommaSeparatedStrings) {
  String text =
      String("\"a\", \"b\"%T, \"c\"").Replace("%T", GetCSSAttrTaintToken());
  const CSSValue* value =
      css_test_helpers::ParseValue(GetDocument(), "<string>#", text);
  ASSERT_NE(value, nullptr);
  EXPECT_EQ(text, value->CssText());
  EXPECT_EQ("\"a\", \"b\", \"c\"", value->UntaintedCopy()->CssText());
}

TEST_F(CSSAttrValueTaintingTest, SpaceSeparatedStrings) {
  String text =
      String("\"a\" \"b\"%T \"c\"").Replace("%T", GetCSSAttrTaintToken());
  const CSSValue* value =
      css_test_helpers::ParseValue(GetDocument(), "<string>+", text);
  ASSERT_NE(value, nullptr);
  EXPECT_EQ(text, value->CssText());
  EXPECT_EQ("\"a\" \"b\" \"c\"", value->UntaintedCopy()->CssText());
}

TEST_F(CSSAttrValueTaintingTest, Equality) {
  String tainted_text =
      String("\"abc\"%T").Replace("%T", GetCSSAttrTaintToken());
  const CSSValue* tainted_value =
      css_test_helpers::ParseValue(GetDocument(), "<string>", tainted_text);

  String non_tainted_text = String("\"abc\"");
  const CSSValue* non_tainted_value =
      css_test_helpers::ParseValue(GetDocument(), "<string>", non_tainted_text);

  ASSERT_NE(tainted_value, nullptr);
  ASSERT_NE(non_tainted_value, nullptr);
  EXPECT_NE(*tainted_value, *non_tainted_value);
}

}  // namespace

}  // namespace blink
