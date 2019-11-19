// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/style_sheet_contents.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/css/parser/css_parser.h"
#include "third_party/blink/renderer/platform/heap/heap.h"

namespace blink {

TEST(StyleSheetContentsTest, InsertMediaRule) {
  auto* context = MakeGarbageCollected<CSSParserContext>(
      kHTMLStandardMode, SecureContextMode::kInsecureContext);

  auto* style_sheet = MakeGarbageCollected<StyleSheetContents>(context);
  style_sheet->ParseString("@namespace ns url(test);");
  EXPECT_EQ(1U, style_sheet->RuleCount());

  style_sheet->SetMutable();
  style_sheet->WrapperInsertRule(
      CSSParser::ParseRule(context, style_sheet,
                           "@media all { div { color: pink } }"),
      0);
  EXPECT_EQ(1U, style_sheet->RuleCount());
  EXPECT_TRUE(style_sheet->HasMediaQueries());

  style_sheet->WrapperInsertRule(
      CSSParser::ParseRule(context, style_sheet,
                           "@media all { div { color: green } }"),
      1);
  EXPECT_EQ(2U, style_sheet->RuleCount());
  EXPECT_TRUE(style_sheet->HasMediaQueries());
}

TEST(StyleSheetContentsTest, InsertFontFaceRule) {
  auto* context = MakeGarbageCollected<CSSParserContext>(
      kHTMLStandardMode, SecureContextMode::kInsecureContext);

  auto* style_sheet = MakeGarbageCollected<StyleSheetContents>(context);
  style_sheet->ParseString("@namespace ns url(test);");
  EXPECT_EQ(1U, style_sheet->RuleCount());

  style_sheet->SetMutable();
  style_sheet->WrapperInsertRule(
      CSSParser::ParseRule(context, style_sheet,
                           "@font-face { font-family: a }"),
      0);
  EXPECT_EQ(1U, style_sheet->RuleCount());
  EXPECT_TRUE(style_sheet->HasFontFaceRule());

  style_sheet->WrapperInsertRule(
      CSSParser::ParseRule(context, style_sheet,
                           "@font-face { font-family: b }"),
      1);
  EXPECT_EQ(2U, style_sheet->RuleCount());
  EXPECT_TRUE(style_sheet->HasFontFaceRule());
}

TEST(StyleSheetContentsTest, HasViewportRule) {
  auto* context = MakeGarbageCollected<CSSParserContext>(
      kHTMLStandardMode, SecureContextMode::kInsecureContext);

  auto* style_sheet = MakeGarbageCollected<StyleSheetContents>(context);
  style_sheet->ParseString("@viewport { width: 200px}");
  EXPECT_EQ(1U, style_sheet->RuleCount());
  EXPECT_TRUE(style_sheet->HasViewportRule());
}

TEST(StyleSheetContentsTest, HasViewportRuleAfterInsertion) {
  auto* context = MakeGarbageCollected<CSSParserContext>(
      kHTMLStandardMode, SecureContextMode::kInsecureContext);

  auto* style_sheet = MakeGarbageCollected<StyleSheetContents>(context);
  style_sheet->ParseString("body { color: pink }");
  EXPECT_EQ(1U, style_sheet->RuleCount());
  EXPECT_FALSE(style_sheet->HasViewportRule());

  style_sheet->SetMutable();
  style_sheet->WrapperInsertRule(
      CSSParser::ParseRule(context, style_sheet, "@viewport { width: 200px }"),
      0);
  EXPECT_EQ(2U, style_sheet->RuleCount());
  EXPECT_TRUE(style_sheet->HasViewportRule());
}

TEST(StyleSheetContentsTest, HasViewportRuleAfterInsertionIntoMediaRule) {
  auto* context = MakeGarbageCollected<CSSParserContext>(
      kHTMLStandardMode, SecureContextMode::kInsecureContext);

  auto* style_sheet = MakeGarbageCollected<StyleSheetContents>(context);
  style_sheet->ParseString("@media {}");
  ASSERT_EQ(1U, style_sheet->RuleCount());
  EXPECT_FALSE(style_sheet->HasViewportRule());

  auto* media_rule = To<StyleRuleMedia>(style_sheet->RuleAt(0));
  style_sheet->SetMutable();
  media_rule->WrapperInsertRule(
      0,
      CSSParser::ParseRule(context, style_sheet, "@viewport { width: 200px }"));
  EXPECT_EQ(1U, media_rule->ChildRules().size());
  EXPECT_TRUE(style_sheet->HasViewportRule());
}

}  // namespace blink
