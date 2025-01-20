// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/style_sheet_contents.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/css/parser/css_parser.h"
#include "third_party/blink/renderer/core/execution_context/security_context.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

TEST(StyleSheetContentsTest, InsertMediaRule) {
  auto* context = MakeGarbageCollected<CSSParserContext>(
      kHTMLStandardMode, SecureContextMode::kInsecureContext);

  auto* style_sheet = MakeGarbageCollected<StyleSheetContents>(context);
  style_sheet->ParseString("@namespace ns url(test);");
  EXPECT_EQ(1U, style_sheet->RuleCount());

  style_sheet->StartMutation();
  style_sheet->WrapperInsertRule(
      CSSParser::ParseRule(context, style_sheet, CSSNestingType::kNone,
                           /*parent_rule_for_nesting=*/nullptr,
                           "@media all { div { color: pink } }"),
      0);
  EXPECT_EQ(1U, style_sheet->RuleCount());
  EXPECT_TRUE(style_sheet->HasMediaQueries());

  style_sheet->WrapperInsertRule(
      CSSParser::ParseRule(context, style_sheet, CSSNestingType::kNone,
                           /*parent_rule_for_nesting=*/nullptr,
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

  style_sheet->StartMutation();
  style_sheet->WrapperInsertRule(
      CSSParser::ParseRule(context, style_sheet, CSSNestingType::kNone,
                           /*parent_rule_for_nesting=*/nullptr,
                           "@font-face { font-family: a }"),
      0);
  EXPECT_EQ(1U, style_sheet->RuleCount());
  EXPECT_TRUE(style_sheet->HasFontFaceRule());

  style_sheet->WrapperInsertRule(
      CSSParser::ParseRule(context, style_sheet, CSSNestingType::kNone,
                           /*parent_rule_for_nesting=*/nullptr,
                           "@font-face { font-family: b }"),
      1);
  EXPECT_EQ(2U, style_sheet->RuleCount());
  EXPECT_TRUE(style_sheet->HasFontFaceRule());
}

TEST(StyleSheetContentsTest,
     HasFailedOrCanceledSubresources_StartingStyleCrash) {
  auto* context = MakeGarbageCollected<CSSParserContext>(
      kHTMLStandardMode, SecureContextMode::kInsecureContext);

  auto* style_sheet = MakeGarbageCollected<StyleSheetContents>(context);
  style_sheet->ParseString("@starting-style {}");
  EXPECT_EQ(1U, style_sheet->RuleCount());

  // This test is a regression test for a CHECK failure for casting
  // StyleRuleStartingStyle to StyleRuleGroup in
  // HasFailedOrCanceledSubresources().
  EXPECT_FALSE(style_sheet->HasFailedOrCanceledSubresources());
}

}  // namespace blink
