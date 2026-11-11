// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/style_sheet_contents.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/css/parser/css_parser.h"
#include "third_party/blink/renderer/core/css/style_rule_import.h"
#include "third_party/blink/renderer/core/execution_context/security_context.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

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

TEST(StyleSheetContentsTest, HasMixinRules) {
  ScopedCSSMixinsForTest scoped_css_mixins(true);
  auto* context = MakeGarbageCollected<CSSParserContext>(
      kHTMLStandardMode, SecureContextMode::kInsecureContext);
  const struct {
    const char* text;
    bool has_mixins;
  } cases[] = {
      {"#target { color: green; }", false},
      {"@media all { #target { color: green; } }", false},
      {"#target { @apply --mixin; }", false},
      {"@mixin --mixin() { @result { color: green; } }", true},
      {"@media not all { @mixin --mixin() {} }", true},
      {"@supports (unknown: value) { @mixin --mixin() {} }", true},
  };
  for (const auto& test : cases) {
    SCOPED_TRACE(test.text);
    auto* style_sheet = MakeGarbageCollected<StyleSheetContents>(context);
    EXPECT_FALSE(style_sheet->HasMixinRules());
    style_sheet->ParseString(test.text);
    EXPECT_EQ(test.has_mixins, style_sheet->HasMixinRules());
    EXPECT_EQ(test.has_mixins, style_sheet->Copy()->HasMixinRules());
  }
}

TEST(StyleSheetContentsTest, InsertMixinRule) {
  ScopedCSSMixinsForTest scoped_css_mixins(true);
  auto* context = MakeGarbageCollected<CSSParserContext>(
      kHTMLStandardMode, SecureContextMode::kInsecureContext);
  for (const char* text :
       {"@mixin --mixin() {}", "@media not all { @mixin --mixin() {} }",
        "@supports (display: block) { @mixin --mixin() {} }"}) {
    SCOPED_TRACE(text);
    auto* style_sheet = MakeGarbageCollected<StyleSheetContents>(context);
    style_sheet->ParseString("#target { color: green; }");
    EXPECT_FALSE(style_sheet->HasMixinRules());
    style_sheet->StartMutation();
    StyleRuleBase* rule =
        CSSParser::ParseRule(context, style_sheet, CSSNestingType::kNone,
                             /*parent_rule_for_nesting=*/nullptr, text);
    ASSERT_TRUE(rule);
    ASSERT_TRUE(style_sheet->WrapperInsertRule(rule, 0));
    EXPECT_TRUE(style_sheet->HasMixinRules());
    EXPECT_TRUE(style_sheet->Copy()->HasMixinRules());
  }
}

TEST(StyleSheetContentsTest, ImportedMixinPresencePropagatesToAncestors) {
  ScopedCSSMixinsForTest scoped_css_mixins(true);
  auto* context = MakeGarbageCollected<CSSParserContext>(
      kHTMLStandardMode, SecureContextMode::kInsecureContext);
  auto* root = MakeGarbageCollected<StyleSheetContents>(context);
  root->ParseString("@import url(child.css);");
  auto* child = MakeGarbageCollected<StyleSheetContents>(
      context, String(), root->ImportRules()[0].Get());
  child->ParseString("@import url(grandchild.css);");
  auto* grandchild = MakeGarbageCollected<StyleSheetContents>(
      context, String(), child->ImportRules()[0].Get());
  EXPECT_FALSE(root->HasMixinRules());
  EXPECT_FALSE(child->HasMixinRules());
  EXPECT_FALSE(grandchild->HasMixinRules());

  grandchild->ParseString("@media not all { @mixin --mixin() {} }");
  EXPECT_TRUE(root->HasMixinRules());
  EXPECT_TRUE(child->HasMixinRules());
  EXPECT_TRUE(grandchild->HasMixinRules());
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
  EXPECT_FALSE(style_sheet->HasFailedOrCanceledSubresources(
      /*fetcher=*/nullptr));
}

}  // namespace blink
