// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/parser/css_parser_impl.h"

#include "testing/gtest/include/gtest/gtest.h"

#include "third_party/blink/renderer/core/css/parser/css_parser_observer.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_token_stream.h"
#include "third_party/blink/renderer/core/css/parser/css_tokenizer.h"
#include "third_party/blink/renderer/core/css/style_sheet_contents.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

namespace blink {

class TestCSSParserObserver : public CSSParserObserver {
 public:
  void StartRuleHeader(StyleRule::RuleType rule_type,
                       unsigned offset) override {
    rule_type_ = rule_type;
    rule_header_start_ = offset;
  }
  void EndRuleHeader(unsigned offset) override { rule_header_end_ = offset; }

  void ObserveSelector(unsigned start_offset, unsigned end_offset) override {}
  void StartRuleBody(unsigned offset) override { rule_body_start_ = offset; }
  void EndRuleBody(unsigned offset) override { rule_body_end_ = offset; }
  void ObserveProperty(unsigned start_offset,
                       unsigned end_offset,
                       bool is_important,
                       bool is_parsed) override {}
  void ObserveComment(unsigned start_offset, unsigned end_offset) override {}

  StyleRule::RuleType rule_type_ = StyleRule::RuleType::kStyle;
  unsigned rule_header_start_ = 0;
  unsigned rule_header_end_ = 0;
  unsigned rule_body_start_ = 0;
  unsigned rule_body_end_ = 0;
};

TEST(CSSParserImplTest, AtImportOffsets) {
  String sheet_text = "@import 'test.css';";
  auto* context = MakeGarbageCollected<CSSParserContext>(
      kHTMLStandardMode, SecureContextMode::kInsecureContext);
  auto* style_sheet = MakeGarbageCollected<StyleSheetContents>(context);
  TestCSSParserObserver test_css_parser_observer;
  CSSParserImpl::ParseStyleSheetForInspector(sheet_text, context, style_sheet,
                                             test_css_parser_observer);
  EXPECT_EQ(style_sheet->ImportRules().size(), 1u);
  EXPECT_EQ(test_css_parser_observer.rule_type_, StyleRule::RuleType::kImport);
  EXPECT_EQ(test_css_parser_observer.rule_header_start_, 18u);
  EXPECT_EQ(test_css_parser_observer.rule_header_end_, 18u);
  EXPECT_EQ(test_css_parser_observer.rule_body_start_, 18u);
  EXPECT_EQ(test_css_parser_observer.rule_body_end_, 18u);
}

TEST(CSSParserImplTest, AtMediaOffsets) {
  String sheet_text = "@media screen { }";
  auto* context = MakeGarbageCollected<CSSParserContext>(
      kHTMLStandardMode, SecureContextMode::kInsecureContext);
  auto* style_sheet = MakeGarbageCollected<StyleSheetContents>(context);
  TestCSSParserObserver test_css_parser_observer;
  CSSParserImpl::ParseStyleSheetForInspector(sheet_text, context, style_sheet,
                                             test_css_parser_observer);
  EXPECT_EQ(style_sheet->ChildRules().size(), 1u);
  EXPECT_EQ(test_css_parser_observer.rule_type_, StyleRule::RuleType::kMedia);
  EXPECT_EQ(test_css_parser_observer.rule_header_start_, 7u);
  EXPECT_EQ(test_css_parser_observer.rule_header_end_, 14u);
  EXPECT_EQ(test_css_parser_observer.rule_body_start_, 15u);
  EXPECT_EQ(test_css_parser_observer.rule_body_end_, 16u);
}

TEST(CSSParserImplTest, AtSupportsOffsets) {
  String sheet_text = "@supports (display:none) { }";
  auto* context = MakeGarbageCollected<CSSParserContext>(
      kHTMLStandardMode, SecureContextMode::kInsecureContext);
  auto* style_sheet = MakeGarbageCollected<StyleSheetContents>(context);
  TestCSSParserObserver test_css_parser_observer;
  CSSParserImpl::ParseStyleSheetForInspector(sheet_text, context, style_sheet,
                                             test_css_parser_observer);
  EXPECT_EQ(style_sheet->ChildRules().size(), 1u);
  EXPECT_EQ(test_css_parser_observer.rule_type_,
            StyleRule::RuleType::kSupports);
  EXPECT_EQ(test_css_parser_observer.rule_header_start_, 10u);
  EXPECT_EQ(test_css_parser_observer.rule_header_end_, 25u);
  EXPECT_EQ(test_css_parser_observer.rule_body_start_, 26u);
  EXPECT_EQ(test_css_parser_observer.rule_body_end_, 27u);
}

TEST(CSSParserImplTest, AtViewportOffsets) {
  String sheet_text = "@viewport { }";
  auto* context = MakeGarbageCollected<CSSParserContext>(
      kUASheetMode, SecureContextMode::kInsecureContext);
  auto* style_sheet = MakeGarbageCollected<StyleSheetContents>(context);
  TestCSSParserObserver test_css_parser_observer;
  CSSParserImpl::ParseStyleSheetForInspector(sheet_text, context, style_sheet,
                                             test_css_parser_observer);
  EXPECT_EQ(style_sheet->ChildRules().size(), 1u);
  EXPECT_EQ(test_css_parser_observer.rule_type_,
            StyleRule::RuleType::kViewport);
  EXPECT_EQ(test_css_parser_observer.rule_header_start_, 10u);
  EXPECT_EQ(test_css_parser_observer.rule_header_end_, 10u);
  EXPECT_EQ(test_css_parser_observer.rule_body_start_, 10u);
  EXPECT_EQ(test_css_parser_observer.rule_body_end_, 10u);
}

TEST(CSSParserImplTest, AtFontFaceOffsets) {
  String sheet_text = "@font-face { }";
  auto* context = MakeGarbageCollected<CSSParserContext>(
      kHTMLStandardMode, SecureContextMode::kInsecureContext);
  auto* style_sheet = MakeGarbageCollected<StyleSheetContents>(context);
  TestCSSParserObserver test_css_parser_observer;
  CSSParserImpl::ParseStyleSheetForInspector(sheet_text, context, style_sheet,
                                             test_css_parser_observer);
  EXPECT_EQ(style_sheet->ChildRules().size(), 1u);
  EXPECT_EQ(test_css_parser_observer.rule_type_,
            StyleRule::RuleType::kFontFace);
  EXPECT_EQ(test_css_parser_observer.rule_header_start_, 11u);
  EXPECT_EQ(test_css_parser_observer.rule_header_end_, 11u);
  EXPECT_EQ(test_css_parser_observer.rule_body_start_, 11u);
  EXPECT_EQ(test_css_parser_observer.rule_body_end_, 11u);
}

TEST(CSSParserImplTest, AtKeyframesOffsets) {
  String sheet_text = "@keyframes test { }";
  auto* context = MakeGarbageCollected<CSSParserContext>(
      kHTMLStandardMode, SecureContextMode::kInsecureContext);
  auto* style_sheet = MakeGarbageCollected<StyleSheetContents>(context);
  TestCSSParserObserver test_css_parser_observer;
  CSSParserImpl::ParseStyleSheetForInspector(sheet_text, context, style_sheet,
                                             test_css_parser_observer);
  EXPECT_EQ(style_sheet->ChildRules().size(), 1u);
  EXPECT_EQ(test_css_parser_observer.rule_type_,
            StyleRule::RuleType::kKeyframes);
  EXPECT_EQ(test_css_parser_observer.rule_header_start_, 11u);
  EXPECT_EQ(test_css_parser_observer.rule_header_end_, 16u);
  EXPECT_EQ(test_css_parser_observer.rule_body_start_, 17u);
  EXPECT_EQ(test_css_parser_observer.rule_body_end_, 18u);
}

TEST(CSSParserImplTest, AtPageOffsets) {
  String sheet_text = "@page :first { }";
  auto* context = MakeGarbageCollected<CSSParserContext>(
      kHTMLStandardMode, SecureContextMode::kInsecureContext);
  auto* style_sheet = MakeGarbageCollected<StyleSheetContents>(context);
  TestCSSParserObserver test_css_parser_observer;
  CSSParserImpl::ParseStyleSheetForInspector(sheet_text, context, style_sheet,
                                             test_css_parser_observer);
  EXPECT_EQ(style_sheet->ChildRules().size(), 1u);
  EXPECT_EQ(test_css_parser_observer.rule_type_, StyleRule::RuleType::kPage);
  EXPECT_EQ(test_css_parser_observer.rule_header_start_, 6u);
  EXPECT_EQ(test_css_parser_observer.rule_header_end_, 13u);
  EXPECT_EQ(test_css_parser_observer.rule_body_start_, 14u);
  EXPECT_EQ(test_css_parser_observer.rule_body_end_, 15u);
}

TEST(CSSParserImplTest, AtPropertyOffsets) {
  ScopedCSSVariables2AtPropertyForTest scoped_feature(true);

  String sheet_text = "@property --test { }";
  auto* context = MakeGarbageCollected<CSSParserContext>(
      kHTMLStandardMode, SecureContextMode::kInsecureContext);
  auto* style_sheet = MakeGarbageCollected<StyleSheetContents>(context);
  TestCSSParserObserver test_css_parser_observer;
  CSSParserImpl::ParseStyleSheetForInspector(sheet_text, context, style_sheet,
                                             test_css_parser_observer);
  EXPECT_EQ(style_sheet->ChildRules().size(), 1u);
  EXPECT_EQ(test_css_parser_observer.rule_type_,
            StyleRule::RuleType::kProperty);
  EXPECT_EQ(test_css_parser_observer.rule_header_start_, 10u);
  EXPECT_EQ(test_css_parser_observer.rule_header_end_, 17u);
  EXPECT_EQ(test_css_parser_observer.rule_body_start_, 18u);
  EXPECT_EQ(test_css_parser_observer.rule_body_end_, 19u);
}

TEST(CSSParserImplTest, AtScrollTimelineOffsets) {
  ScopedCSSScrollTimelineForTest scoped_feature(true);

  String sheet_text = "@scroll-timeline test { }";
  auto* context = MakeGarbageCollected<CSSParserContext>(
      kHTMLStandardMode, SecureContextMode::kInsecureContext);
  auto* style_sheet = MakeGarbageCollected<StyleSheetContents>(context);
  TestCSSParserObserver test_css_parser_observer;
  CSSParserImpl::ParseStyleSheetForInspector(sheet_text, context, style_sheet,
                                             test_css_parser_observer);
  EXPECT_EQ(style_sheet->ChildRules().size(), 1u);
  EXPECT_EQ(test_css_parser_observer.rule_type_,
            StyleRule::RuleType::kScrollTimeline);
  EXPECT_EQ(test_css_parser_observer.rule_header_start_, 17u);
  EXPECT_EQ(test_css_parser_observer.rule_header_end_, 22u);
  EXPECT_EQ(test_css_parser_observer.rule_body_start_, 23u);
  EXPECT_EQ(test_css_parser_observer.rule_body_end_, 24u);
}

TEST(CSSParserImplTest, AtCounterStyleOffsets) {
  ScopedCSSAtRuleCounterStyleForTest scoped_feature(true);

  String sheet_text = "@counter-style test { }";
  auto* context = MakeGarbageCollected<CSSParserContext>(
      kHTMLStandardMode, SecureContextMode::kInsecureContext);
  auto* style_sheet = MakeGarbageCollected<StyleSheetContents>(context);
  TestCSSParserObserver test_css_parser_observer;
  CSSParserImpl::ParseStyleSheetForInspector(sheet_text, context, style_sheet,
                                             test_css_parser_observer);
  EXPECT_EQ(style_sheet->ChildRules().size(), 1u);
  EXPECT_EQ(test_css_parser_observer.rule_type_,
            StyleRule::RuleType::kCounterStyle);
  EXPECT_EQ(test_css_parser_observer.rule_header_start_, 15u);
  EXPECT_EQ(test_css_parser_observer.rule_header_end_, 20u);
  EXPECT_EQ(test_css_parser_observer.rule_body_start_, 21u);
  EXPECT_EQ(test_css_parser_observer.rule_body_end_, 22u);
}

TEST(CSSParserImplTest, AtCounterStyleDisabled) {
  ScopedCSSAtRuleCounterStyleForTest scoped_feature(false);

  String sheet_text = "@counter-style test { }";
  auto* context = MakeGarbageCollected<CSSParserContext>(
      kHTMLStandardMode, SecureContextMode::kInsecureContext);
  auto* style_sheet = MakeGarbageCollected<StyleSheetContents>(context);
  TestCSSParserObserver test_css_parser_observer;
  CSSParserImpl::ParseStyleSheetForInspector(sheet_text, context, style_sheet,
                                             test_css_parser_observer);
  EXPECT_EQ(style_sheet->ChildRules().size(), 0u);
}

TEST(CSSParserImplTest, RemoveImportantAnnotationIfPresent) {
  struct TestCase {
    String input;
    String expected_text;
    bool expected_is_important;
  };
  static const TestCase test_cases[] = {
      {"", "", false},
      {"!important", "", true},
      {" !important", " ", true},
      {"!", "!", false},
      {"1px", "1px", false},
      {"2px!important", "2px", true},
      {"3px !important", "3px ", true},
      {"4px ! important", "4px ", true},
      {"5px !important ", "5px ", true},
      {"6px !!important", "6px !", true},
      {"7px !important !important", "7px !important ", true},
      {"8px important", "8px important", false},
  };
  for (auto current_case : test_cases) {
    CSSTokenizer tokenizer(current_case.input);
    CSSParserTokenStream stream(tokenizer);
    CSSTokenizedValue tokenized_value = CSSParserImpl::ConsumeValue(stream);
    SCOPED_TRACE(current_case.input);
    bool is_important =
        CSSParserImpl::RemoveImportantAnnotationIfPresent(tokenized_value);
    EXPECT_EQ(is_important, current_case.expected_is_important);
    EXPECT_EQ(tokenized_value.text.ToString(), current_case.expected_text);
  }
}

}  // namespace blink
