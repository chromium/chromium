// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/parser/css_parser_impl.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/css/css_font_family_value.h"
#include "third_party/blink/renderer/core/css/css_test_helpers.h"
#include "third_party/blink/renderer/core/css/css_to_length_conversion_data.h"
#include "third_party/blink/renderer/core/css/css_value_list.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_observer.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_token_stream.h"
#include "third_party/blink/renderer/core/css/parser/css_tokenizer.h"
#include "third_party/blink/renderer/core/css/parser/css_variable_parser.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"
#include "third_party/blink/renderer/core/css/style_rule.h"
#include "third_party/blink/renderer/core/css/style_rule_font_feature_values.h"
#include "third_party/blink/renderer/core/css/style_rule_font_palette_values.h"
#include "third_party/blink/renderer/core/css/style_rule_import.h"
#include "third_party/blink/renderer/core/css/style_rule_nested_declarations.h"
#include "third_party/blink/renderer/core/css/style_sheet_contents.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/execution_context/security_context.h"
#include "third_party/blink/renderer/core/testing/null_execution_context.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

namespace blink {

class TestCSSParserObserver : public CSSParserObserver {
 public:
  void StartRuleHeader(StyleRule::RuleType rule_type,
                       unsigned offset) override {
    if (IsAtTargetLevel()) {
      rule_type_ = rule_type;
      rule_header_start_ = offset;
    }
  }
  void EndRuleHeader(unsigned offset) override {
    if (IsAtTargetLevel()) {
      rule_header_end_ = offset;
    }
  }

  void ObserveSelector(unsigned start_offset, unsigned end_offset) override {}
  void StartRuleBody(unsigned offset) override {
    if (IsAtTargetLevel()) {
      rule_body_start_ = offset;
    }
    current_nesting_level_++;
  }
  void EndRuleBody(unsigned offset) override {
    current_nesting_level_--;
    if (IsAtTargetLevel()) {
      rule_body_end_ = offset;
    }
  }
  void ObserveProperty(unsigned start_offset,
                       unsigned end_offset,
                       bool is_important,
                       bool is_parsed) override {
    if (IsAtTargetLevel()) {
      property_start_ = start_offset;
    }
  }
  void ObserveComment(unsigned start_offset, unsigned end_offset) override {}
  void ObserveErroneousAtRule(
      unsigned start_offset,
      CSSAtRuleID id,
      const Vector<CSSPropertyID, 2>& invalid_properties) override {}
  void ObserveNestedDeclarations(wtf_size_t insert_rule_index) override {
    nested_declarations_insert_rule_index = insert_rule_index;
  }

  bool IsAtTargetLevel() const {
    return target_nesting_level_ == kEverything ||
           target_nesting_level_ == current_nesting_level_;
  }

  const int kEverything = -1;

  // Set to >= 0 to only observe events at a certain level. If kEverything, it
  // will observe everything.
  int target_nesting_level_ = kEverything;

  int current_nesting_level_ = 0;

  StyleRule::RuleType rule_type_ = StyleRule::RuleType::kStyle;
  unsigned property_start_ = 0;
  unsigned rule_header_start_ = 0;
  unsigned rule_header_end_ = 0;
  unsigned rule_body_start_ = 0;
  unsigned rule_body_end_ = 0;
  unsigned nested_declarations_insert_rule_index = 0;
};

// Exists solely to access private parts of CSSParserImpl.
class TestCSSParserImpl {
  STACK_ALLOCATED();

 public:
  TestCSSParserImpl()
      : impl_(MakeGarbageCollected<CSSParserContext>(
            kHTMLStandardMode,
            SecureContextMode::kInsecureContext)) {}

  StyleRule* ConsumeStyleRule(CSSParserTokenStream& stream,
                              CSSNestingType nesting_type,
                              StyleRule* parent_rule_for_nesting,
                              bool nested,
                              bool& invalid_rule_error) {
    return impl_.ConsumeStyleRule(stream, nesting_type, parent_rule_for_nesting,
                                  nested, invalid_rule_error);
  }

 private:
  CSSParserImpl impl_;
};

TEST(CSSParserImplTest, AtImportOffsets) {
  test::TaskEnvironment task_environment;
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
  test::TaskEnvironment task_environment;
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
  test::TaskEnvironment task_environment;
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

TEST(CSSParserImplTest, AtFontFaceOffsets) {
  test::TaskEnvironment task_environment;
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
  test::TaskEnvironment task_environment;
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
  test::TaskEnvironment task_environment;
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

TEST(CSSParserImplTest, AtPageMarginOffsets) {
  test::TaskEnvironment task_environment;
  String sheet_text = "@page :first { @top-left { content: 'A'; } }";
  auto* context = MakeGarbageCollected<CSSParserContext>(
      kHTMLStandardMode, SecureContextMode::kInsecureContext);
  auto* style_sheet = MakeGarbageCollected<StyleSheetContents>(context);
  TestCSSParserObserver test_css_parser_observer;

  // Ignore @page, look for @top-left.
  test_css_parser_observer.target_nesting_level_ = 1;

  CSSParserImpl::ParseStyleSheetForInspector(sheet_text, context, style_sheet,
                                             test_css_parser_observer);
  EXPECT_EQ(style_sheet->ChildRules().size(), 1u);
  EXPECT_EQ(test_css_parser_observer.rule_type_,
            StyleRule::RuleType::kPageMargin);
  EXPECT_EQ(test_css_parser_observer.rule_header_start_, 25u);
  EXPECT_EQ(test_css_parser_observer.rule_header_end_, 25u);
  EXPECT_EQ(test_css_parser_observer.rule_body_start_, 26u);
  EXPECT_EQ(test_css_parser_observer.rule_body_end_, 41u);
}

TEST(CSSParserImplTest, AtPropertyOffsets) {
  test::TaskEnvironment task_environment;
  String sheet_text = "@property --test { syntax: '*'; inherits: false }";
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
  EXPECT_EQ(test_css_parser_observer.rule_body_end_, 48u);
}

TEST(CSSParserImplTest, AtCounterStyleOffsets) {
  test::TaskEnvironment task_environment;
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

TEST(CSSParserImplTest, AtContainerOffsets) {
  test::TaskEnvironment task_environment;
  String sheet_text = "@container (max-width: 100px) { }";

  auto* context = MakeGarbageCollected<CSSParserContext>(
      kHTMLStandardMode, SecureContextMode::kInsecureContext);
  auto* style_sheet = MakeGarbageCollected<StyleSheetContents>(context);
  TestCSSParserObserver test_css_parser_observer;
  CSSParserImpl::ParseStyleSheetForInspector(sheet_text, context, style_sheet,
                                             test_css_parser_observer);
  EXPECT_EQ(style_sheet->ChildRules().size(), 1u);
  EXPECT_EQ(test_css_parser_observer.rule_type_,
            StyleRule::RuleType::kContainer);
  EXPECT_EQ(test_css_parser_observer.rule_header_start_, 11u);
  EXPECT_EQ(test_css_parser_observer.rule_header_end_, 30u);
  EXPECT_EQ(test_css_parser_observer.rule_body_start_, 31u);
  EXPECT_EQ(test_css_parser_observer.rule_body_end_, 32u);
}

TEST(CSSParserImplTest, DirectNesting) {
  test::TaskEnvironment task_environment;
  String sheet_text =
      ".element { color: green; &.other { color: red; margin-left: 10px; }}";

  auto* context = MakeGarbageCollected<CSSParserContext>(
      kHTMLStandardMode, SecureContextMode::kInsecureContext);
  auto* sheet = MakeGarbageCollected<StyleSheetContents>(context);
  TestCSSParserObserver test_css_parser_observer;
  CSSParserImpl::ParseStyleSheetForInspector(sheet_text, context, sheet,
                                             test_css_parser_observer);

  ASSERT_EQ(1u, sheet->ChildRules().size());
  StyleRule* parent = DynamicTo<StyleRule>(sheet->ChildRules()[0].Get());
  ASSERT_NE(nullptr, parent);
  EXPECT_EQ("color: green;", parent->Properties().AsText());
  EXPECT_EQ(".element", parent->SelectorsText());

  ASSERT_EQ(1u, parent->ChildRules()->size());
  const StyleRule* child =
      DynamicTo<StyleRule>((*parent->ChildRules())[0].Get());
  ASSERT_NE(nullptr, child);
  EXPECT_EQ("color: red; margin-left: 10px;", child->Properties().AsText());
  EXPECT_EQ("&.other", child->SelectorsText());
}

TEST(CSSParserImplTest, RuleNotStartingWithAmpersand) {
  test::TaskEnvironment task_environment;
  String sheet_text = ".element { color: green;  .outer & { color: red; }}";

  auto* context = MakeGarbageCollected<CSSParserContext>(
      kHTMLStandardMode, SecureContextMode::kInsecureContext);
  auto* sheet = MakeGarbageCollected<StyleSheetContents>(context);
  TestCSSParserObserver test_css_parser_observer;
  CSSParserImpl::ParseStyleSheetForInspector(sheet_text, context, sheet,
                                             test_css_parser_observer);

  ASSERT_EQ(1u, sheet->ChildRules().size());
  StyleRule* parent = DynamicTo<StyleRule>(sheet->ChildRules()[0].Get());
  ASSERT_NE(nullptr, parent);
  EXPECT_EQ("color: green;", parent->Properties().AsText());
  EXPECT_EQ(".element", parent->SelectorsText());

  ASSERT_NE(nullptr, parent->ChildRules());
  ASSERT_EQ(1u, parent->ChildRules()->size());
  const StyleRule* child =
      DynamicTo<StyleRule>((*parent->ChildRules())[0].Get());
  ASSERT_NE(nullptr, child);
  EXPECT_EQ("color: red;", child->Properties().AsText());
  EXPECT_EQ(".outer &", child->SelectorsText());
}

TEST(CSSParserImplTest, ImplicitDescendantSelectors) {
  test::TaskEnvironment task_environment;
  String sheet_text =
      ".element { color: green; .outer, .outer2 { color: red; }}";

  auto* context = MakeGarbageCollected<CSSParserContext>(
      kHTMLStandardMode, SecureContextMode::kInsecureContext);
  auto* sheet = MakeGarbageCollected<StyleSheetContents>(context);
  TestCSSParserObserver test_css_parser_observer;
  CSSParserImpl::ParseStyleSheetForInspector(sheet_text, context, sheet,
                                             test_css_parser_observer);

  ASSERT_EQ(1u, sheet->ChildRules().size());
  StyleRule* parent = DynamicTo<StyleRule>(sheet->ChildRules()[0].Get());
  ASSERT_NE(nullptr, parent);
  EXPECT_EQ("color: green;", parent->Properties().AsText());
  EXPECT_EQ(".element", parent->SelectorsText());

  ASSERT_NE(nullptr, parent->ChildRules());
  ASSERT_EQ(1u, parent->ChildRules()->size());
  const StyleRule* child =
      DynamicTo<StyleRule>((*parent->ChildRules())[0].Get());
  ASSERT_NE(nullptr, child);
  EXPECT_EQ("color: red;", child->Properties().AsText());
  EXPECT_EQ("& .outer, & .outer2", child->SelectorsText());
}

TEST(CSSParserImplTest, NestedRelativeSelector) {
  test::TaskEnvironment task_environment;
  String sheet_text = ".element { color: green; > .inner { color: red; }}";

  auto* context = MakeGarbageCollected<CSSParserContext>(
      kHTMLStandardMode, SecureContextMode::kInsecureContext);
  auto* sheet = MakeGarbageCollected<StyleSheetContents>(context);
  TestCSSParserObserver test_css_parser_observer;
  CSSParserImpl::ParseStyleSheetForInspector(sheet_text, context, sheet,
                                             test_css_parser_observer);

  ASSERT_EQ(1u, sheet->ChildRules().size());
  StyleRule* parent = DynamicTo<StyleRule>(sheet->ChildRules()[0].Get());
  ASSERT_NE(nullptr, parent);
  EXPECT_EQ("color: green;", parent->Properties().AsText());
  EXPECT_EQ(".element", parent->SelectorsText());

  ASSERT_NE(nullptr, parent->ChildRules());
  ASSERT_EQ(1u, parent->ChildRules()->size());
  const StyleRule* child =
      DynamicTo<StyleRule>((*parent->ChildRules())[0].Get());
  ASSERT_NE(nullptr, child);
  EXPECT_EQ("color: red;", child->Properties().AsText());
  EXPECT_EQ("& > .inner", child->SelectorsText());
}

TEST(CSSParserImplTest, NestingAtTopLevelIsLegalThoughIsMatchesNothing) {
  test::TaskEnvironment task_environment;
  String sheet_text = "&.element { color: orchid; }";

  auto* context = MakeGarbageCollected<CSSParserContext>(
      kHTMLStandardMode, SecureContextMode::kInsecureContext);
  auto* sheet = MakeGarbageCollected<StyleSheetContents>(context);
  TestCSSParserObserver test_css_parser_observer;
  CSSParserImpl::ParseStyleSheetForInspector(sheet_text, context, sheet,
                                             test_css_parser_observer);

  ASSERT_EQ(1u, sheet->ChildRules().size());
  const StyleRule* rule = DynamicTo<StyleRule>(sheet->ChildRules()[0].Get());
  EXPECT_EQ("color: orchid;", rule->Properties().AsText());
  EXPECT_EQ("&.element", rule->SelectorsText());
}

TEST(CSSParserImplTest, ErrorRecoveryEatsOnlyFirstDeclaration) {
  test::TaskEnvironment task_environment;
  // Note the colon after the opening bracket.
  String sheet_text = R"CSS(
    .element {:
      color: orchid;
      background-color: plum;
      accent-color: hotpink;
    }
    )CSS";

  auto* context = MakeGarbageCollected<CSSParserContext>(
      kHTMLStandardMode, SecureContextMode::kInsecureContext);
  auto* sheet = MakeGarbageCollected<StyleSheetContents>(context);
  TestCSSParserObserver test_css_parser_observer;
  CSSParserImpl::ParseStyleSheetForInspector(sheet_text, context, sheet,
                                             test_css_parser_observer);

  ASSERT_EQ(1u, sheet->ChildRules().size());
  const StyleRule* rule = DynamicTo<StyleRule>(sheet->ChildRules()[0].Get());
  EXPECT_EQ("background-color: plum; accent-color: hotpink;",
            rule->Properties().AsText());
  EXPECT_EQ(".element", rule->SelectorsText());
}

TEST(CSSParserImplTest, NestedEmptySelectorCrash) {
  test::TaskEnvironment task_environment;
  String sheet_text = "y{ :is() {} }";

  auto* context = MakeGarbageCollected<CSSParserContext>(
      kHTMLStandardMode, SecureContextMode::kInsecureContext);
  auto* sheet = MakeGarbageCollected<StyleSheetContents>(context);
  TestCSSParserObserver test_css_parser_observer;
  CSSParserImpl::ParseStyleSheetForInspector(sheet_text, context, sheet,
                                             test_css_parser_observer);

  // We only really care that it doesn't crash.
}

TEST(CSSParserImplTest, NestedRulesInsideMediaQueries) {
  test::TaskEnvironment task_environment;
  String sheet_text = R"CSS(
    .element {
      color: green;
      @media (width < 1000px) {
        color: navy;
        font-size: 12px;
        & + #foo { color: red; }
      }
    }
    )CSS";

  auto* context = MakeGarbageCollected<CSSParserContext>(
      kHTMLStandardMode, SecureContextMode::kInsecureContext);
  auto* sheet = MakeGarbageCollected<StyleSheetContents>(context);
  TestCSSParserObserver test_css_parser_observer;
  CSSParserImpl::ParseStyleSheetForInspector(sheet_text, context, sheet,
                                             test_css_parser_observer);

  ASSERT_EQ(1u, sheet->ChildRules().size());
  StyleRule* parent = DynamicTo<StyleRule>(sheet->ChildRules()[0].Get());
  ASSERT_NE(nullptr, parent);
  EXPECT_EQ("color: green;", parent->Properties().AsText());
  EXPECT_EQ(".element", parent->SelectorsText());

  ASSERT_NE(nullptr, parent->ChildRules());
  ASSERT_EQ(1u, parent->ChildRules()->size());
  const StyleRuleMedia* media_query =
      DynamicTo<StyleRuleMedia>((*parent->ChildRules())[0].Get());
  ASSERT_NE(nullptr, media_query);

  ASSERT_EQ(2u, media_query->ChildRules().size());

  // Implicit CSSNestedDeclarations rule around the properties.
  const StyleRuleNestedDeclarations* child0 =
      DynamicTo<StyleRuleNestedDeclarations>(
          media_query->ChildRules()[0].Get());
  ASSERT_NE(nullptr, child0);
  EXPECT_EQ("color: navy; font-size: 12px;", child0->Properties().AsText());

  const StyleRule* child1 =
      DynamicTo<StyleRule>(media_query->ChildRules()[1].Get());
  ASSERT_NE(nullptr, child1);
  EXPECT_EQ("color: red;", child1->Properties().AsText());
  EXPECT_EQ("& + #foo", child1->SelectorsText());
}

// A version of NestedRulesInsideMediaQueries where CSSNestedDeclarations
// is disabled. Can be removed when the CSSNestedDeclarations is removed.
TEST(CSSParserImplTest,
     NestedRulesInsideMediaQueries_CSSNestedDeclarationsDisabled) {
  ScopedCSSNestedDeclarationsForTest nested_declarations_enabled(false);

  test::TaskEnvironment task_environment;
  String sheet_text = R"CSS(
    .element {
      color: green;
      @media (width < 1000px) {
        color: navy;
        font-size: 12px;
        & + #foo { color: red; }
      }
    }
    )CSS";

  auto* context = MakeGarbageCollected<CSSParserContext>(
      kHTMLStandardMode, SecureContextMode::kInsecureContext);
  auto* sheet = MakeGarbageCollected<StyleSheetContents>(context);
  TestCSSParserObserver test_css_parser_observer;
  CSSParserImpl::ParseStyleSheetForInspector(sheet_text, context, sheet,
                                             test_css_parser_observer);

  ASSERT_EQ(1u, sheet->ChildRules().size());
  StyleRule* parent = DynamicTo<StyleRule>(sheet->ChildRules()[0].Get());
  ASSERT_NE(nullptr, parent);
  EXPECT_EQ("color: green;", parent->Properties().AsText());
  EXPECT_EQ(".element", parent->SelectorsText());

  ASSERT_NE(nullptr, parent->ChildRules());
  ASSERT_EQ(1u, parent->ChildRules()->size());
  const StyleRuleMedia* media_query =
      DynamicTo<StyleRuleMedia>((*parent->ChildRules())[0].Get());
  ASSERT_NE(nullptr, media_query);

  ASSERT_EQ(2u, media_query->ChildRules().size());

  // Implicit & {} rule around the properties.
  const StyleRule* child0 =
      DynamicTo<StyleRule>(media_query->ChildRules()[0].Get());
  ASSERT_NE(nullptr, child0);
  EXPECT_EQ("color: navy; font-size: 12px;", child0->Properties().AsText());
  EXPECT_EQ("&", child0->SelectorsText());

  const StyleRule* child1 =
      DynamicTo<StyleRule>(media_query->ChildRules()[1].Get());
  ASSERT_NE(nullptr, child1);
  EXPECT_EQ("color: red;", child1->Properties().AsText());
  EXPECT_EQ("& + #foo", child1->SelectorsText());
}

TEST(CSSParserImplTest, ObserveNestedMediaQuery) {
  test::TaskEnvironment task_environment;
  String sheet_text = R"CSS(
    .element {
      color: green;
      @media (width < 1000px) {
        color: navy;
      }
    }
    )CSS";

  auto* context = MakeGarbageCollected<CSSParserContext>(
      kHTMLStandardMode, SecureContextMode::kInsecureContext);
  auto* sheet = MakeGarbageCollected<StyleSheetContents>(context);
  TestCSSParserObserver test_css_parser_observer;
  // Observe the @media rule.
  test_css_parser_observer.target_nesting_level_ = 1;
  CSSParserImpl::ParseStyleSheetForInspector(sheet_text, context, sheet,
                                             test_css_parser_observer);

  EXPECT_EQ(test_css_parser_observer.rule_type_, StyleRule::RuleType::kMedia);
  EXPECT_EQ(test_css_parser_observer.rule_header_start_, 49u);
  EXPECT_EQ(test_css_parser_observer.rule_header_end_, 66u);
  EXPECT_EQ(test_css_parser_observer.rule_body_start_, 67u);
  EXPECT_EQ(test_css_parser_observer.rule_body_end_, 95u);
}

TEST(CSSParserImplTest, ObserveNestedLayer) {
  test::TaskEnvironment task_environment;
  String sheet_text = R"CSS(
    .element {
      color: green;
      @layer foo {
        color: navy;
      }
    }
    )CSS";

  auto* context = MakeGarbageCollected<CSSParserContext>(
      kHTMLStandardMode, SecureContextMode::kInsecureContext);
  auto* sheet = MakeGarbageCollected<StyleSheetContents>(context);
  TestCSSParserObserver test_css_parser_observer;
  // Observe the @layer rule.
  test_css_parser_observer.target_nesting_level_ = 1;
  CSSParserImpl::ParseStyleSheetForInspector(sheet_text, context, sheet,
                                             test_css_parser_observer);

  EXPECT_EQ(test_css_parser_observer.rule_type_,
            StyleRule::RuleType::kLayerBlock);
  EXPECT_EQ(test_css_parser_observer.rule_header_start_, 49u);
  EXPECT_EQ(test_css_parser_observer.rule_header_end_, 53u);
  EXPECT_EQ(test_css_parser_observer.rule_body_start_, 54u);
  EXPECT_EQ(test_css_parser_observer.rule_body_end_, 82u);
}

TEST(CSSParserImplTest, NestedIdent) {
  test::TaskEnvironment task_environment;

  String sheet_text = "div { p:hover { } }";
  auto* context = MakeGarbageCollected<CSSParserContext>(
      kHTMLStandardMode, SecureContextMode::kInsecureContext);
  auto* style_sheet = MakeGarbageCollected<StyleSheetContents>(context);
  TestCSSParserObserver test_css_parser_observer;
  CSSParserImpl::ParseStyleSheetForInspector(sheet_text, context, style_sheet,
                                             test_css_parser_observer);

  // 'p:hover { }' should be reported both as a failed declaration,
  // and as a style rule (at the same location).
  EXPECT_EQ(test_css_parser_observer.property_start_, 6u);
  EXPECT_EQ(test_css_parser_observer.rule_header_start_, 6u);
}

TEST(CSSParserImplTest, ObserveNestedDeclarations_Interleaved) {
  test::TaskEnvironment task_environment;

  String sheet_text = R"CSS(
    .element {
      left: 1px;
      right: 2px;
      .a {}
      .b {}
      top: 3px;
      bottom: 4px;
      .c {}
    }
    )CSS";
  auto* context = MakeGarbageCollected<CSSParserContext>(
      kHTMLStandardMode, SecureContextMode::kInsecureContext);
  auto* style_sheet = MakeGarbageCollected<StyleSheetContents>(context);
  TestCSSParserObserver test_css_parser_observer;
  CSSParserImpl::ParseStyleSheetForInspector(sheet_text, context, style_sheet,
                                             test_css_parser_observer);

  // The nested declarations rule should be inserted before the .c child rule,
  // at index 2.
  EXPECT_EQ(test_css_parser_observer.nested_declarations_insert_rule_index, 2u);
}

TEST(CSSParserImplTest, ObserveNestedDeclarations_Trailing) {
  test::TaskEnvironment task_environment;

  String sheet_text = R"CSS(
    .element {
      left: 1px;
      right: 2px;
      .a {}
      .b {}
      .c {}
      top: 3px;
      bottom: 4px;
    }
    )CSS";
  auto* context = MakeGarbageCollected<CSSParserContext>(
      kHTMLStandardMode, SecureContextMode::kInsecureContext);
  auto* style_sheet = MakeGarbageCollected<StyleSheetContents>(context);
  TestCSSParserObserver test_css_parser_observer;
  CSSParserImpl::ParseStyleSheetForInspector(sheet_text, context, style_sheet,
                                             test_css_parser_observer);

  // The nested declarations rule should be inserted at the end
  // of the child rules vector, at index 3.
  EXPECT_EQ(test_css_parser_observer.nested_declarations_insert_rule_index, 3u);
}

TEST(CSSParserImplTest,
     ConsumeUnparsedDeclarationRemovesImportantAnnotationIfPresent) {
  test::TaskEnvironment task_environment;
  struct TestCase {
    String input;
    String expected_text;
    bool expected_is_important;
  };
  static const TestCase test_cases[] = {
      {"", "", false},
      {"!important", "", true},
      {" !important", "", true},
      {"!", "PARSE ERROR", false},
      {"1px", "1px", false},
      {"2px!important", "2px", true},
      {"3px !important", "3px", true},
      {"4px ! important", "4px", true},
      {"5px !important ", "5px", true},
      {"6px !!important", "PARSE ERROR", true},
      {"7px !important !important", "PARSE ERROR", true},
      {"8px important", "8px important", false},
  };
  for (auto current_case : test_cases) {
    SCOPED_TRACE(current_case.input);
    CSSParserTokenStream stream(current_case.input);
    bool is_important;
    CSSVariableData* data = CSSVariableParser::ConsumeUnparsedDeclaration(
        stream, /*allow_important_annotation=*/true,
        /*is_animation_tainted=*/false,
        /*must_contain_variable_reference=*/false,
        /*restricted_value=*/true, /*comma_ends_declaration=*/false,
        is_important,
        /*context=*/nullptr);
    if (current_case.expected_text == "PARSE ERROR") {
      EXPECT_FALSE(data);
    } else {
      EXPECT_TRUE(data);
      if (data) {
        EXPECT_EQ(is_important, current_case.expected_is_important);
        EXPECT_EQ(data->OriginalText().ToString(), current_case.expected_text);
      }
    }
  }
}

TEST(CSSParserImplTest, InvalidLayerRules) {
  test::TaskEnvironment task_environment;
  using css_test_helpers::ParseRule;
  ScopedNullExecutionContext execution_context;
  Document* document =
      Document::CreateForTest(execution_context.GetExecutionContext());

  // At most one layer name in an @layer block rule
  EXPECT_FALSE(ParseRule(*document, "@layer foo, bar { }"));

  // Layers must be named in an @layer statement rule
  EXPECT_FALSE(ParseRule(*document, "@layer ;"));
  EXPECT_FALSE(ParseRule(*document, "@layer foo, , bar;"));

  // Invalid layer names
  EXPECT_FALSE(ParseRule(*document, "@layer foo.bar. { }"));
  EXPECT_FALSE(ParseRule(*document, "@layer foo.bar.;"));
  EXPECT_FALSE(ParseRule(*document, "@layer .foo.bar { }"));
  EXPECT_FALSE(ParseRule(*document, "@layer .foo.bar;"));
  EXPECT_FALSE(ParseRule(*document, "@layer foo. bar { }"));
  EXPECT_FALSE(ParseRule(*document, "@layer foo. bar;"));
  EXPECT_FALSE(ParseRule(*document, "@layer foo bar { }"));
  EXPECT_FALSE(ParseRule(*document, "@layer foo bar;"));
  EXPECT_FALSE(ParseRule(*document, "@layer foo/bar { }"));
  EXPECT_FALSE(ParseRule(*document, "@layer foo/bar;"));
}

TEST(CSSParserImplTest, ValidLayerBlockRule) {
  test::TaskEnvironment task_environment;
  using css_test_helpers::ParseRule;
  ScopedNullExecutionContext execution_context;
  Document* document =
      Document::CreateForTest(execution_context.GetExecutionContext());

  // Basic named layer
  {
    String rule = "@layer foo { }";
    auto* parsed = DynamicTo<StyleRuleLayerBlock>(ParseRule(*document, rule));
    ASSERT_TRUE(parsed);
    ASSERT_EQ(1u, parsed->GetName().size());
    EXPECT_EQ("foo", parsed->GetName()[0]);
  }

  // Unnamed layer
  {
    String rule = "@layer { }";
    auto* parsed = DynamicTo<StyleRuleLayerBlock>(ParseRule(*document, rule));
    ASSERT_TRUE(parsed);
    ASSERT_EQ(1u, parsed->GetName().size());
    EXPECT_EQ(g_empty_atom, parsed->GetName()[0]);
  }

  // Sub-layer declared directly
  {
    String rule = "@layer foo.bar { }";
    auto* parsed = DynamicTo<StyleRuleLayerBlock>(ParseRule(*document, rule));
    ASSERT_TRUE(parsed);
    ASSERT_EQ(2u, parsed->GetName().size());
    EXPECT_EQ("foo", parsed->GetName()[0]);
    EXPECT_EQ("bar", parsed->GetName()[1]);
  }
}

TEST(CSSParserImplTest, ValidLayerStatementRule) {
  test::TaskEnvironment task_environment;
  using css_test_helpers::ParseRule;
  ScopedNullExecutionContext execution_context;
  Document* document =
      Document::CreateForTest(execution_context.GetExecutionContext());

  {
    String rule = "@layer foo;";
    auto* parsed =
        DynamicTo<StyleRuleLayerStatement>(ParseRule(*document, rule));
    ASSERT_TRUE(parsed);
    ASSERT_EQ(1u, parsed->GetNames().size());
    ASSERT_EQ(1u, parsed->GetNames()[0].size());
    EXPECT_EQ("foo", parsed->GetNames()[0][0]);
  }

  {
    String rule = "@layer foo, bar;";
    auto* parsed =
        DynamicTo<StyleRuleLayerStatement>(ParseRule(*document, rule));
    ASSERT_TRUE(parsed);
    ASSERT_EQ(2u, parsed->GetNames().size());
    ASSERT_EQ(1u, parsed->GetNames()[0].size());
    EXPECT_EQ("foo", parsed->GetNames()[0][0]);
    ASSERT_EQ(1u, parsed->GetNames()[1].size());
    EXPECT_EQ("bar", parsed->GetNames()[1][0]);
  }

  {
    String rule = "@layer foo, bar.baz;";
    auto* parsed =
        DynamicTo<StyleRuleLayerStatement>(ParseRule(*document, rule));
    ASSERT_TRUE(parsed);
    ASSERT_EQ(2u, parsed->GetNames().size());
    ASSERT_EQ(1u, parsed->GetNames()[0].size());
    EXPECT_EQ("foo", parsed->GetNames()[0][0]);
    ASSERT_EQ(2u, parsed->GetNames()[1].size());
    EXPECT_EQ("bar", parsed->GetNames()[1][0]);
    EXPECT_EQ("baz", parsed->GetNames()[1][1]);
  }
}

TEST(CSSParserImplTest, NestedLayerRules) {
  test::TaskEnvironment task_environment;
  using css_test_helpers::ParseRule;
  ScopedNullExecutionContext execution_context;
  Document* document =
      Document::CreateForTest(execution_context.GetExecutionContext());

  // Block rule as a child rule.
  {
    String rule = "@layer foo { @layer bar { } }";
    auto* foo = DynamicTo<StyleRuleLayerBlock>(ParseRule(*document, rule));
    ASSERT_TRUE(foo);
    ASSERT_EQ(1u, foo->GetName().size());
    EXPECT_EQ("foo", foo->GetName()[0]);
    ASSERT_EQ(1u, foo->ChildRules().size());

    auto* bar = DynamicTo<StyleRuleLayerBlock>(foo->ChildRules()[0].Get());
    ASSERT_TRUE(bar);
    ASSERT_EQ(1u, bar->GetName().size());
    EXPECT_EQ("bar", bar->GetName()[0]);
  }

  // Statement rule as a child rule.
  {
    String rule = "@layer foo { @layer bar, baz; }";
    auto* foo = DynamicTo<StyleRuleLayerBlock>(ParseRule(*document, rule));
    ASSERT_TRUE(foo);
    ASSERT_EQ(1u, foo->GetName().size());
    EXPECT_EQ("foo", foo->GetName()[0]);
    ASSERT_EQ(1u, foo->ChildRules().size());

    auto* barbaz =
        DynamicTo<StyleRuleLayerStatement>(foo->ChildRules()[0].Get());
    ASSERT_TRUE(barbaz);
    ASSERT_EQ(2u, barbaz->GetNames().size());
    ASSERT_EQ(1u, barbaz->GetNames()[0].size());
    EXPECT_EQ("bar", barbaz->GetNames()[0][0]);
    ASSERT_EQ(1u, barbaz->GetNames()[1].size());
    EXPECT_EQ("baz", barbaz->GetNames()[1][0]);
  }

  // Nested in an unnamed layer.
  {
    String rule = "@layer { @layer foo; @layer bar { } }";
    auto* parent = DynamicTo<StyleRuleLayerBlock>(ParseRule(*document, rule));
    ASSERT_TRUE(parent);
    ASSERT_EQ(1u, parent->GetName().size());
    EXPECT_EQ(g_empty_atom, parent->GetName()[0]);
    ASSERT_EQ(2u, parent->ChildRules().size());

    auto* foo =
        DynamicTo<StyleRuleLayerStatement>(parent->ChildRules()[0].Get());
    ASSERT_TRUE(foo);
    ASSERT_EQ(1u, foo->GetNames().size());
    ASSERT_EQ(1u, foo->GetNames()[0].size());
    EXPECT_EQ("foo", foo->GetNames()[0][0]);

    auto* bar = DynamicTo<StyleRuleLayerBlock>(parent->ChildRules()[1].Get());
    ASSERT_TRUE(bar);
    ASSERT_EQ(1u, bar->GetName().size());
    EXPECT_EQ("bar", bar->GetName()[0]);
  }
}

TEST(CSSParserImplTest, LayeredImportRules) {
  test::TaskEnvironment task_environment;
  using css_test_helpers::ParseRule;
  ScopedNullExecutionContext execution_context;
  Document* document =
      Document::CreateForTest(execution_context.GetExecutionContext());

  {
    String rule = "@import url(foo.css) layer;";
    auto* parsed = DynamicTo<StyleRuleImport>(ParseRule(*document, rule));
    ASSERT_TRUE(parsed);
    ASSERT_TRUE(parsed->IsLayered());
    ASSERT_EQ(1u, parsed->GetLayerName().size());
    EXPECT_EQ(g_empty_atom, parsed->GetLayerName()[0]);
  }

  {
    String rule = "@import url(foo.css) layer(bar);";
    auto* parsed = DynamicTo<StyleRuleImport>(ParseRule(*document, rule));
    ASSERT_TRUE(parsed);
    ASSERT_TRUE(parsed->IsLayered());
    ASSERT_EQ(1u, parsed->GetLayerName().size());
    EXPECT_EQ("bar", parsed->GetLayerName()[0]);
  }

  {
    String rule = "@import url(foo.css) layer(bar.baz);";
    auto* parsed = DynamicTo<StyleRuleImport>(ParseRule(*document, rule));
    ASSERT_TRUE(parsed);
    ASSERT_TRUE(parsed->IsLayered());
    ASSERT_EQ(2u, parsed->GetLayerName().size());
    EXPECT_EQ("bar", parsed->GetLayerName()[0]);
    EXPECT_EQ("baz", parsed->GetLayerName()[1]);
  }
}

TEST(CSSParserImplTest, LayeredImportRulesInvalid) {
  test::TaskEnvironment task_environment;
  using css_test_helpers::ParseRule;
  ScopedNullExecutionContext execution_context;
  Document* document =
      Document::CreateForTest(execution_context.GetExecutionContext());

  // Invalid layer declarations in @import rules should not make the entire rule
  // invalid. They should be parsed as <general-enclosed> and have no effect.

  {
    String rule = "@import url(foo.css) layer();";
    auto* parsed = DynamicTo<StyleRuleImport>(ParseRule(*document, rule));
    ASSERT_TRUE(parsed);
    EXPECT_FALSE(parsed->IsLayered());
  }

  {
    String rule = "@import url(foo.css) layer(bar, baz);";
    auto* parsed = DynamicTo<StyleRuleImport>(ParseRule(*document, rule));
    ASSERT_TRUE(parsed);
    EXPECT_FALSE(parsed->IsLayered());
  }

  {
    String rule = "@import url(foo.css) layer(bar.baz.);";
    auto* parsed = DynamicTo<StyleRuleImport>(ParseRule(*document, rule));
    ASSERT_TRUE(parsed);
    EXPECT_FALSE(parsed->IsLayered());
  }
}

TEST(CSSParserImplTest, ImportRulesWithSupports) {
  test::TaskEnvironment task_environment;
  using css_test_helpers::ParseRule;
  ScopedNullExecutionContext execution_context;
  Document* document =
      Document::CreateForTest(execution_context.GetExecutionContext());

  {
    String rule =
        "@import url(foo.css) layer(bar.baz) supports(display: block);";
    auto* parsed = DynamicTo<StyleRuleImport>(ParseRule(*document, rule));
    ASSERT_TRUE(parsed);
    EXPECT_TRUE(parsed->IsSupported());
  }

  {
    String rule = "@import url(foo.css) supports(display: block);";
    auto* parsed = DynamicTo<StyleRuleImport>(ParseRule(*document, rule));
    ASSERT_TRUE(parsed);
    EXPECT_TRUE(parsed->IsSupported());
  }

  {
    String rule =
        "@import url(foo.css)   supports((display: block) and (color: green));";
    auto* parsed = DynamicTo<StyleRuleImport>(ParseRule(*document, rule));
    ASSERT_TRUE(parsed);
    EXPECT_TRUE(parsed->IsSupported());
  }

  {
    String rule =
        "@import url(foo.css) supports((foo: bar) and (color: green));";
    auto* parsed = DynamicTo<StyleRuleImport>(ParseRule(*document, rule));
    ASSERT_TRUE(parsed);
    EXPECT_FALSE(parsed->IsSupported());
  }

  {
    String rule = "@import url(foo.css) supports());";
    auto* parsed = DynamicTo<StyleRuleImport>(ParseRule(*document, rule));
    EXPECT_FALSE(parsed);
  }

  {
    String rule = "@import url(foo.css) supports(color: green) (width >= 0px);";
    auto* parsed = DynamicTo<StyleRuleImport>(ParseRule(*document, rule));
    ASSERT_TRUE(parsed);
    EXPECT_TRUE(parsed->IsSupported());
    EXPECT_TRUE(parsed->MediaQueries());
    EXPECT_EQ(parsed->MediaQueries()->QueryVector().size(), 1u);
    EXPECT_EQ(parsed->MediaQueries()->MediaText(), String("(width >= 0px)"));
  }
}

TEST(CSSParserImplTest, LayeredImportRulesMultipleLayers) {
  test::TaskEnvironment task_environment;
  using css_test_helpers::ParseRule;
  ScopedNullExecutionContext execution_context;
  Document* document =
      Document::CreateForTest(execution_context.GetExecutionContext());

  // If an @import rule has more than one layer keyword/function, only the first
  // one is parsed as layer, and the remaining ones are parsed as
  // <general-enclosed> and hence have no effect.

  {
    String rule = "@import url(foo.css) layer layer;";
    auto* parsed = DynamicTo<StyleRuleImport>(ParseRule(*document, rule));
    ASSERT_TRUE(parsed);
    ASSERT_TRUE(parsed->IsLayered());
    ASSERT_EQ(1u, parsed->GetLayerName().size());
    EXPECT_EQ(g_empty_atom, parsed->GetLayerName()[0]);
    EXPECT_EQ("not all", parsed->MediaQueries()->MediaText());
  }

  {
    String rule = "@import url(foo.css) layer layer(bar);";
    auto* parsed = DynamicTo<StyleRuleImport>(ParseRule(*document, rule));
    ASSERT_TRUE(parsed);
    ASSERT_TRUE(parsed->IsLayered());
    ASSERT_EQ(1u, parsed->GetLayerName().size());
    EXPECT_EQ(g_empty_atom, parsed->GetLayerName()[0]);
  }

  {
    String rule = "@import url(foo.css) layer(bar) layer;";
    auto* parsed = DynamicTo<StyleRuleImport>(ParseRule(*document, rule));
    ASSERT_TRUE(parsed);
    ASSERT_TRUE(parsed->IsLayered());
    ASSERT_EQ(1u, parsed->GetLayerName().size());
    EXPECT_EQ("bar", parsed->GetLayerName()[0]);
    EXPECT_EQ("not all", parsed->MediaQueries()->MediaText());
  }
}

TEST(CSSParserImplTest, CorrectAtRuleOrderingWithLayers) {
  test::TaskEnvironment task_environment;
  String sheet_text = R"CSS(
    @layer foo;
    @import url(bar.css) layer(bar);
    @namespace url(http://www.w3.org/1999/xhtml);
    @layer baz;
    @layer qux { }
  )CSS";
  auto* context = MakeGarbageCollected<CSSParserContext>(
      kHTMLStandardMode, SecureContextMode::kInsecureContext);
  auto* sheet = MakeGarbageCollected<StyleSheetContents>(context);
  CSSParserImpl::ParseStyleSheet(sheet_text, context, sheet);

  // All rules should parse successfully.
  EXPECT_EQ(1u, sheet->PreImportLayerStatementRules().size());
  EXPECT_EQ(1u, sheet->ImportRules().size());
  EXPECT_EQ(1u, sheet->NamespaceRules().size());
  EXPECT_EQ(2u, sheet->ChildRules().size());
}

TEST(CSSParserImplTest, EmptyLayerStatementsAtWrongPositions) {
  test::TaskEnvironment task_environment;
  {
    // @layer interleaving with @import rules
    String sheet_text = R"CSS(
      @layer foo;
      @import url(bar.css) layer(bar);
      @layer baz;
      @import url(qux.css);
    )CSS";
    auto* context = MakeGarbageCollected<CSSParserContext>(
        kHTMLStandardMode, SecureContextMode::kInsecureContext);
    auto* sheet = MakeGarbageCollected<StyleSheetContents>(context);
    CSSParserImpl::ParseStyleSheet(sheet_text, context, sheet);

    EXPECT_EQ(1u, sheet->PreImportLayerStatementRules().size());
    EXPECT_EQ(1u, sheet->ChildRules().size());

    // After parsing @layer baz, @import rules are no longer allowed, so the
    // second @import rule should be ignored.
    ASSERT_EQ(1u, sheet->ImportRules().size());
    EXPECT_TRUE(sheet->ImportRules()[0]->IsLayered());
  }

  {
    // @layer between @import and @namespace rules
    String sheet_text = R"CSS(
      @layer foo;
      @import url(bar.css) layer(bar);
      @layer baz;
      @namespace url(http://www.w3.org/1999/xhtml);
    )CSS";
    auto* context = MakeGarbageCollected<CSSParserContext>(
        kHTMLStandardMode, SecureContextMode::kInsecureContext);
    auto* sheet = MakeGarbageCollected<StyleSheetContents>(context);
    CSSParserImpl::ParseStyleSheet(sheet_text, context, sheet);

    EXPECT_EQ(1u, sheet->PreImportLayerStatementRules().size());
    EXPECT_EQ(1u, sheet->ImportRules().size());
    EXPECT_EQ(1u, sheet->ChildRules().size());

    // After parsing @layer baz, @namespace rules are no longer allowed.
    EXPECT_EQ(0u, sheet->NamespaceRules().size());
  }
}

TEST(CSSParserImplTest, EmptyLayerStatementAfterRegularRule) {
  test::TaskEnvironment task_environment;
  // Empty @layer statements after regular rules are parsed as regular rules.

  String sheet_text = R"CSS(
    .element { color: green; }
    @layer foo, bar;
  )CSS";
  auto* context = MakeGarbageCollected<CSSParserContext>(
      kHTMLStandardMode, SecureContextMode::kInsecureContext);
  auto* sheet = MakeGarbageCollected<StyleSheetContents>(context);
  CSSParserImpl::ParseStyleSheet(sheet_text, context, sheet);

  EXPECT_EQ(0u, sheet->PreImportLayerStatementRules().size());
  EXPECT_EQ(2u, sheet->ChildRules().size());
  EXPECT_TRUE(sheet->ChildRules()[0]->IsStyleRule());
  EXPECT_TRUE(sheet->ChildRules()[1]->IsLayerStatementRule());
}

TEST(CSSParserImplTest, FontPaletteValuesDisabled) {
  test::TaskEnvironment task_environment;
  // @font-palette-values rules should be ignored when the feature is disabled.

  using css_test_helpers::ParseRule;
  ScopedNullExecutionContext execution_context;
  Document* document =
      Document::CreateForTest(execution_context.GetExecutionContext());
  EXPECT_FALSE(ParseRule(*document, "@font-palette-values foo;"));
  EXPECT_FALSE(ParseRule(*document, "@font-palette-values foo { }"));
  EXPECT_FALSE(ParseRule(*document, "@font-palette-values foo.bar { }"));
  EXPECT_FALSE(ParseRule(*document, "@font-palette-values { }"));
}

TEST(CSSParserImplTest, FontPaletteValuesBasicRuleParsing) {
  test::TaskEnvironment task_environment;
  using css_test_helpers::ParseRule;
  ScopedNullExecutionContext execution_context;
  Document* document =
      Document::CreateForTest(execution_context.GetExecutionContext());
  String rule = R"CSS(@font-palette-values --myTestPalette {
    font-family: testFamily;
    base-palette: 0;
    override-colors: 0 red, 1 blue;
  })CSS";
  auto* parsed =
      DynamicTo<StyleRuleFontPaletteValues>(ParseRule(*document, rule));
  ASSERT_TRUE(parsed);
  ASSERT_EQ("--myTestPalette", parsed->GetName());
  ASSERT_EQ("testFamily", parsed->GetFontFamily()->CssText());
  ASSERT_EQ(0, DynamicTo<CSSPrimitiveValue>(parsed->GetBasePalette())
                   ->ComputeInteger(CSSToLengthConversionData()));
  ASSERT_TRUE(parsed->GetOverrideColors()->IsValueList());
  ASSERT_EQ(2u, DynamicTo<CSSValueList>(parsed->GetOverrideColors())->length());
}

TEST(CSSParserImplTest, FontPaletteValuesMultipleFamiliesParsing) {
  test::TaskEnvironment task_environment;
  using css_test_helpers::ParseRule;
  ScopedNullExecutionContext execution_context;
  Document* document =
      Document::CreateForTest(execution_context.GetExecutionContext());
  String rule = R"CSS(@font-palette-values --myTestPalette {
    font-family: testFamily1, testFamily2;
    base-palette: 0;
  })CSS";
  auto* parsed =
      DynamicTo<StyleRuleFontPaletteValues>(ParseRule(*document, rule));
  ASSERT_TRUE(parsed);
  ASSERT_EQ("--myTestPalette", parsed->GetName());
  ASSERT_EQ("testFamily1, testFamily2", parsed->GetFontFamily()->CssText());
  ASSERT_EQ(0, DynamicTo<CSSPrimitiveValue>(parsed->GetBasePalette())
                   ->ComputeInteger(CSSToLengthConversionData()));
}

// Font-family descriptor inside @font-palette-values should not contain generic
// families, compare:
// https://drafts.csswg.org/css-fonts/#descdef-font-palette-values-font-family.
TEST(CSSParserImplTest, FontPaletteValuesGenericFamiliesNotParsing) {
  test::TaskEnvironment task_environment;
  using css_test_helpers::ParseRule;
  ScopedNullExecutionContext execution_context;
  Document* document =
      Document::CreateForTest(execution_context.GetExecutionContext());
  String rule = R"CSS(@font-palette-values --myTestPalette {
    font-family: testFamily1, testFamily2, serif;
    base-palette: 0;
  })CSS";
  auto* parsed =
      DynamicTo<StyleRuleFontPaletteValues>(ParseRule(*document, rule));
  ASSERT_TRUE(parsed);
  ASSERT_EQ("--myTestPalette", parsed->GetName());
  ASSERT_FALSE(parsed->GetFontFamily());
  ASSERT_EQ(0, DynamicTo<CSSPrimitiveValue>(parsed->GetBasePalette())
                   ->ComputeInteger(CSSToLengthConversionData()));
}

TEST(CSSParserImplTest, FontFeatureValuesRuleParsing) {
  test::TaskEnvironment task_environment;
  using css_test_helpers::ParseRule;
  ScopedNullExecutionContext execution_context;
  Document* document =
      Document::CreateForTest(execution_context.GetExecutionContext());
  String rule = R"CSS(@font-feature-values fontFam1, fontFam2 {
    @styleset { curly: 4 3 2 1; wavy: 2; cool: 3; }
    @swash { thrown: 1; }
    @styleset { yo: 1; }
  })CSS";
  auto* parsed =
      DynamicTo<StyleRuleFontFeatureValues>(ParseRule(*document, rule));
  ASSERT_TRUE(parsed);
  auto& families = parsed->GetFamilies();
  ASSERT_EQ(AtomicString("fontFam1"), families[0]);
  ASSERT_EQ(AtomicString("fontFam2"), families[1]);
  ASSERT_EQ(parsed->GetStyleset()->size(), 4u);
  ASSERT_TRUE(parsed->GetStyleset()->Contains(AtomicString("cool")));
  ASSERT_EQ(parsed->GetStyleset()->at(AtomicString("curly")).indices,
            Vector<uint32_t>({4, 3, 2, 1}));
}

TEST(CSSParserImplTest, FontFeatureValuesOffsets) {
  test::TaskEnvironment task_environment;
  String sheet_text = "@font-feature-values myFam { @styleset { curly: 1; } }";
  auto* context = MakeGarbageCollected<CSSParserContext>(
      kHTMLStandardMode, SecureContextMode::kInsecureContext);
  auto* style_sheet = MakeGarbageCollected<StyleSheetContents>(context);
  TestCSSParserObserver test_css_parser_observer;
  CSSParserImpl::ParseStyleSheetForInspector(sheet_text, context, style_sheet,
                                             test_css_parser_observer);
  EXPECT_EQ(style_sheet->ChildRules().size(), 1u);
  EXPECT_EQ(test_css_parser_observer.rule_type_,
            StyleRule::RuleType::kFontFeatureValues);
  EXPECT_EQ(test_css_parser_observer.rule_header_start_, 21u);
  EXPECT_EQ(test_css_parser_observer.rule_header_end_, 27u);
  EXPECT_EQ(test_css_parser_observer.rule_body_start_, 28u);
  EXPECT_EQ(test_css_parser_observer.rule_body_end_, 53u);
}

TEST(CSSParserImplTest, CSSFunction) {
  test::TaskEnvironment task_environment;

  String sheet_text = R"CSS(
    @function --foo(): color {
      @return red;
    }
  )CSS";
  auto* context = MakeGarbageCollected<CSSParserContext>(
      kHTMLStandardMode, SecureContextMode::kInsecureContext);
  auto* sheet = MakeGarbageCollected<StyleSheetContents>(context);
  CSSParserImpl::ParseStyleSheet(sheet_text, context, sheet);
  ASSERT_EQ(sheet->ChildRules().size(), 1u);

  const StyleRuleFunction* rule =
      DynamicTo<StyleRuleFunction>(sheet->ChildRules()[0].Get());
  EXPECT_TRUE(rule);

  EXPECT_EQ("red", rule->GetFunctionBody().OriginalText());
}

static String RoundTripProperty(Document& document, String property_text) {
  String rule_text = "p { " + property_text + " }";
  StyleRule* style_rule =
      To<StyleRule>(css_test_helpers::ParseRule(document, rule_text));
  if (!style_rule) {
    return "";
  }
  return style_rule->Properties().AsText();
}

TEST(CSSParserImplTest, AllPropertiesCanParseImportant) {
  test::TaskEnvironment task_environment;
  ScopedNullExecutionContext execution_context;
  Document* document =
      Document::CreateForTest(execution_context.GetExecutionContext());
  const ComputedStyle& initial_style =
      *ComputedStyle::GetInitialStyleSingleton();

  int broken_properties = 0;

  for (CSSPropertyID property_id : CSSPropertyIDList()) {
    const CSSProperty& property = CSSProperty::Get(property_id);
    if (!property.IsWebExposed() || property.IsSurrogate()) {
      continue;
    }

    // Get some reasonable value that we can use for testing parsing.
    const CSSValue* computed_value = property.CSSValueFromComputedStyle(
        initial_style,
        /*layout_object=*/nullptr,
        /*allow_visited_style=*/true, CSSValuePhase::kComputedValue);
    if (!computed_value) {
      continue;
    }

    // TODO(b/338535751): We have some properties that don't properly
    // round-trip even without !important, so we cannot easily
    // test them using this test. Remove this test when everything
    // is fixed.
    String property_text = property.GetPropertyNameString() + ": " +
                           computed_value->CssText() + ";";
    if (RoundTripProperty(*document, property_text) != property_text) {
      ++broken_properties;
      continue;
    }

    // Now for the actual test.
    property_text = property.GetPropertyNameString() + ": " +
                    computed_value->CssText() + " !important;";
    EXPECT_EQ(RoundTripProperty(*document, property_text), property_text);
  }

  // So that we don't introduce more, or break the entire test inadvertently.
  EXPECT_EQ(broken_properties, 18);
}

TEST(CSSParserImplTest, ParseSupportsBlinkFeature) {
  test::TaskEnvironment task_environment;
  String sheet_text = R"CSS(
    @supports blink-feature(TestFeatureStable) {
      div { color: red; }
      span { color: green; }
    }
  )CSS";
  auto* context = MakeGarbageCollected<CSSParserContext>(
      kUASheetMode, SecureContextMode::kInsecureContext);
  auto* sheet = MakeGarbageCollected<StyleSheetContents>(context);
  CSSParserImpl::ParseStyleSheet(sheet_text, context, sheet);
  ASSERT_EQ(sheet->ChildRules().size(), 1u);

  StyleRuleBase* rule = sheet->ChildRules()[0].Get();
  ASSERT_EQ(rule->GetType(), StyleRuleBase::RuleType::kSupports);
  StyleRuleSupports* supports_rule = DynamicTo<StyleRuleSupports>(rule);
  ASSERT_TRUE(supports_rule->ConditionIsSupported());

  HeapVector<Member<StyleRuleBase>> child_rules = supports_rule->ChildRules();
  ASSERT_EQ(child_rules.size(), 2u);
  ASSERT_EQ(String("div"),
            To<StyleRule>(child_rules[0].Get())->SelectorsText());
  ASSERT_EQ(String("span"),
            To<StyleRule>(child_rules[1].Get())->SelectorsText());
}

TEST(CSSParserImplTest, ParseSupportsBlinkFeatureAuthorStylesheet) {
  test::TaskEnvironment task_environment;
  String sheet_text = R"CSS(
    @supports blink-feature(TestFeatureStable) {
      div { color: red; }
      span { color: green; }
    }
  )CSS";
  auto* context = MakeGarbageCollected<CSSParserContext>(
      kHTMLStandardMode, SecureContextMode::kInsecureContext);
  auto* sheet = MakeGarbageCollected<StyleSheetContents>(context);
  CSSParserImpl::ParseStyleSheet(sheet_text, context, sheet);
  ASSERT_EQ(sheet->ChildRules().size(), 1u);

  StyleRuleBase* rule = sheet->ChildRules()[0].Get();
  ASSERT_EQ(rule->GetType(), StyleRuleBase::RuleType::kSupports);
  StyleRuleSupports* supports_rule = DynamicTo<StyleRuleSupports>(rule);
  EXPECT_FALSE(supports_rule->ConditionIsSupported());
}

TEST(CSSParserImplTest, ParseSupportsBlinkFeatureDisabledFeature) {
  test::TaskEnvironment task_environment;
  String sheet_text = R"CSS(
    @supports blink-feature(TestFeature) {
      div { color: red; }
      span { color: green; }
    }
  )CSS";
  auto* context = MakeGarbageCollected<CSSParserContext>(
      kUASheetMode, SecureContextMode::kInsecureContext);
  auto* sheet = MakeGarbageCollected<StyleSheetContents>(context);
  CSSParserImpl::ParseStyleSheet(sheet_text, context, sheet);
  ASSERT_EQ(sheet->ChildRules().size(), 1u);

  StyleRuleBase* rule = sheet->ChildRules()[0].Get();
  ASSERT_EQ(rule->GetType(), StyleRuleBase::RuleType::kSupports);
  StyleRuleSupports* supports_rule = DynamicTo<StyleRuleSupports>(rule);
  ASSERT_FALSE(supports_rule->ConditionIsSupported());

  HeapVector<Member<StyleRuleBase>> child_rules = supports_rule->ChildRules();
  ASSERT_EQ(child_rules.size(), 2u);
  ASSERT_EQ(String("div"),
            To<StyleRule>(child_rules[0].Get())->SelectorsText());
  ASSERT_EQ(String("span"),
            To<StyleRule>(child_rules[1].Get())->SelectorsText());
}

// Test that we behave correctly for rules that look like custom properties.
//
// https://drafts.csswg.org/css-syntax/#consume-qualified-rule

TEST(CSSParserImplTest, CustomPropertyAmbiguityTopLevel) {
  test::TaskEnvironment task_environment;

  String text = "--x:hover { } foo; bar";
  CSSParserTokenStream stream(text);

  bool invalid_rule_error = false;

  TestCSSParserImpl parser;
  const StyleRule* rule = parser.ConsumeStyleRule(
      stream, CSSNestingType::kNone, /* parent_rule_for_nesting */ nullptr,
      /* nested */ false, invalid_rule_error);

  // "If nested is false, consume a block from input, and return nothing."
  EXPECT_EQ(nullptr, rule);
  EXPECT_FALSE(invalid_rule_error);
  EXPECT_EQ(" foo; bar", stream.RemainingText());
}

TEST(CSSParserImplTest, CustomPropertyAmbiguityNested) {
  test::TaskEnvironment task_environment;

  String text = "--x:hover { } foo; bar";
  CSSParserTokenStream stream(text);

  bool invalid_rule_error = false;

  TestCSSParserImpl parser;
  const StyleRule* rule = parser.ConsumeStyleRule(
      stream, CSSNestingType::kNesting, /* parent_rule_for_nesting */ nullptr,
      /* nested */ true, invalid_rule_error);

  // "If nested is true, consume the remnants of a bad declaration from input,
  //  with nested set to true, and return nothing."
  EXPECT_EQ(nullptr, rule);
  EXPECT_FALSE(invalid_rule_error);
  // "Consume the remnants of a bad declaration" should consume everything
  // until the next semicolon, but we leave that to the caller.
  EXPECT_EQ("{ } foo; bar", stream.RemainingText());
}

// https://drafts.csswg.org/css-syntax/#invalid-rule-error

TEST(CSSParserImplTest, InvalidRuleError) {
  test::TaskEnvironment task_environment;

  String text = "<<::any-invalid-selector::>> { } foo; bar";
  CSSParserTokenStream stream(text);

  bool invalid_rule_error = false;

  TestCSSParserImpl parser;
  const StyleRule* rule = parser.ConsumeStyleRule(
      stream, CSSNestingType::kNone, /* parent_rule_for_nesting */ nullptr,
      /* nested */ false, invalid_rule_error);

  EXPECT_EQ(nullptr, rule);
  EXPECT_TRUE(invalid_rule_error);
  EXPECT_EQ(" foo; bar", stream.RemainingText());
}

}  // namespace blink
