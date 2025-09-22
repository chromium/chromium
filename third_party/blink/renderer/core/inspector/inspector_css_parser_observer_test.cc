// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/inspector/inspector_css_parser_observer.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/css/parser/css_parser.h"
#include "third_party/blink/renderer/core/css/style_sheet_contents.h"
#include "third_party/blink/renderer/core/inspector/inspector_highlight.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

namespace blink {

namespace {

String Substring(String text, SourceRange range) {
  return text.Substring(range.start, range.length());
}

}  // namespace

std::ostream& operator<<(std::ostream& stream, const SourceRange& range) {
  stream << "SourceRange{";
  stream << range.start;
  stream << ",";
  stream << range.end;
  stream << "}";
  return stream;
}

class InspectorCSSParserObserverTest : public testing::Test {
 protected:
  void SetUp() override;

  Document& GetDocument() { return dummy_page_holder_->GetDocument(); }

  CSSRuleSourceDataList Parse(String text) {
    CSSRuleSourceDataList data;
    InspectorCSSParserObserver observer(
        text, &GetDocument(),
        /* result */ &data, /* issue_reporting_context */ std::nullopt);
    auto* context = MakeGarbageCollected<CSSParserContext>(GetDocument());
    auto* contents = MakeGarbageCollected<StyleSheetContents>(context);
    CSSParser::ParseSheetForInspector(context, contents, text, observer);
    return data;
  }

 private:
  test::TaskEnvironment task_environment_;

  std::unique_ptr<DummyPageHolder> dummy_page_holder_;
};

void InspectorCSSParserObserverTest::SetUp() {
  dummy_page_holder_ = std::make_unique<DummyPageHolder>(gfx::Size(800, 600));
}

TEST_F(InspectorCSSParserObserverTest, DeclRangeNormal) {
  String text = ".a { left:1px; right:2px; }";
  CSSRuleSourceDataList data = Parse(text);
  ASSERT_EQ(1u, data.size());
  EXPECT_EQ(" left:1px; right:2px; ",
            Substring(text, data[0]->rule_body_range));
  EXPECT_EQ(data[0]->rule_body_range, data[0]->rule_declarations_range);
}

TEST_F(InspectorCSSParserObserverTest, DeclRangeWithChildRule) {
  String text = ".a { left:1px; right:2px; & {} }";
  CSSRuleSourceDataList data = Parse(text);
  ASSERT_EQ(1u, data.size());
  EXPECT_EQ(" left:1px; right:2px; & {} ",
            Substring(text, data[0]->rule_body_range));
  EXPECT_EQ(" left:1px; right:2px;",
            Substring(text, data[0]->rule_declarations_range));
}

TEST_F(InspectorCSSParserObserverTest, DeclRangeWithNestedDecl) {
  String text = ".a { left:1px; right:2px; & {} --nested:1; }";
  CSSRuleSourceDataList data = Parse(text);
  ASSERT_EQ(1u, data.size());
  EXPECT_EQ(" left:1px; right:2px; & {} --nested:1; ",
            Substring(text, data[0]->rule_body_range));
  EXPECT_EQ(" left:1px; right:2px;",
            Substring(text, data[0]->rule_declarations_range));
}

// When parsing with an observer, we should always emit CSSNestedDeclaration
// rules, even when they are empty.
TEST_F(InspectorCSSParserObserverTest, EmptyNestedDeclarations) {
  // The string `mark` shows where in `text` we expect empty
  // CSSNestedDeclarations.
  String text = ".a { @media (width) { & { } } }";
  String mark = "                     A     B C ";
  ASSERT_EQ(text.length(), mark.length());
  CSSRuleSourceDataList data = Parse(text);
  ASSERT_EQ(1u, data.size());
  ASSERT_EQ(2u, data[0]->child_rules.size());

  // Expect an empty CSSNestedDeclarations rule as the final child of .a.
  {
    SourceRange range = data[0]->child_rules[1]->rule_body_range;
    EXPECT_EQ(0u, range.length());
    EXPECT_EQ("C", Substring(mark, SourceRange(range.start, range.start + 1u)));
  }

  // Expect an empty rule before and after &{}.
  const CSSRuleSourceData& media = *data[0]->child_rules[0];
  ASSERT_EQ(3u, media.child_rules.size());
  {
    SourceRange range = media.child_rules[0]->rule_body_range;
    EXPECT_EQ(0u, range.length());
    EXPECT_EQ("A", Substring(mark, SourceRange(range.start, range.start + 1u)));
  }
  {
    SourceRange range = media.child_rules[2]->rule_body_range;
    EXPECT_EQ(0u, range.length());
    EXPECT_EQ("B", Substring(mark, SourceRange(range.start, range.start + 1u)));
  }
}

TEST_F(InspectorCSSParserObserverTest, NestedDeclarationsNonEmpty) {
  String text = ".a { left:1px; & { } right:2px; & { } top:3px; }";
  CSSRuleSourceDataList data = Parse(text);
  ASSERT_EQ(1u, data.size());
  ASSERT_EQ(4u, data[0]->child_rules.size());

  EXPECT_EQ("right:2px;",
            Substring(text, data[0]->child_rules[1]->rule_body_range));
  EXPECT_EQ("top:3px;",
            Substring(text, data[0]->child_rules[3]->rule_body_range));
}

TEST_F(InspectorCSSParserObserverTest, NestedDeclarationsComment) {
  String text = ".a { & { } /* left:1px; */ & { } /* right:2px; */ }";
  CSSRuleSourceDataList data = Parse(text);
  ASSERT_EQ(1u, data.size());
  ASSERT_EQ(4u, data[0]->child_rules.size());
  EXPECT_EQ("/* left:1px; */",
            Substring(text, data[0]->child_rules[1]->rule_body_range));
  EXPECT_EQ("/* right:2px; */",
            Substring(text, data[0]->child_rules[3]->rule_body_range));
}

TEST_F(InspectorCSSParserObserverTest, NestedDeclarationsInvalid) {
  String text =
      ".a { & { } dino-affinity:t-rex; & { } dino-name:--rex-ruthor; }";
  CSSRuleSourceDataList data = Parse(text);
  ASSERT_EQ(1u, data.size());
  ASSERT_EQ(4u, data[0]->child_rules.size());
  EXPECT_EQ("dino-affinity:t-rex;",
            Substring(text, data[0]->child_rules[1]->rule_body_range));
  EXPECT_EQ("dino-name:--rex-ruthor;",
            Substring(text, data[0]->child_rules[3]->rule_body_range));
}

TEST_F(InspectorCSSParserObserverTest, NestedDeclarationsCommentMedia) {
  String text = ".a { @media (width) { /* left:1px; */ } }";
  CSSRuleSourceDataList data = Parse(text);
  ASSERT_EQ(1u, data.size());
  ASSERT_EQ(2u, data[0]->child_rules.size());

  const CSSRuleSourceData& media = *data[0]->child_rules[0];
  ASSERT_EQ(1u, media.child_rules.size());

  EXPECT_EQ("/* left:1px; */",
            Substring(text, media.child_rules[0]->rule_body_range));
}

TEST_F(InspectorCSSParserObserverTest, NestedDeclarationsInvalidMedia) {
  String text = ".a { @media (width) { dino-affinity:t-rex; } }";
  CSSRuleSourceDataList data = Parse(text);
  ASSERT_EQ(1u, data.size());
  ASSERT_EQ(2u, data[0]->child_rules.size());

  const CSSRuleSourceData& media = *data[0]->child_rules[0];
  ASSERT_EQ(1u, media.child_rules.size());

  EXPECT_EQ("dino-affinity:t-rex;",
            Substring(text, media.child_rules[0]->rule_body_range));
}

TEST_F(InspectorCSSParserObserverTest, NestedDeclarationsInvalidPrecedingRule) {
  // Note: We will first try to parse 'span:dino(t-rex){}' as a declaration,
  // then as a nested rule. It is not valid as either, so the observer needs
  // to decide whether we treat it as an invalid nested rule, or as an invalid
  // declaration. We currently treat all such ambiguous cases as invalid
  // declarations for compatibility with how the observer worked before
  // CSS Nesting.
  String text = "div { span { } span:dino(t-rex) { } }";
  // Don't crash, crbug.com/372623082.
  CSSRuleSourceDataList data = Parse(text);
  ASSERT_EQ(1u, data.size());
  ASSERT_EQ(2u, data[0]->child_rules.size());
  EXPECT_EQ("span",
            Substring(text, data[0]->child_rules[0]->rule_header_range));
  // Being an invalid selector, this is treated as an invalid *declaration*
  // by the parser, hence the CSSNestedDeclarations rule will contain that
  // (invalid) declaration in its body.
  ASSERT_EQ(1u, data[0]->child_rules[1]->property_data.size());
  EXPECT_EQ("span", data[0]->child_rules[1]->property_data[0].name);
  EXPECT_EQ("dino(t-rex)", data[0]->child_rules[1]->property_data[0].value);
}

TEST_F(InspectorCSSParserObserverTest, MixinWithNestedDeclarations) {
  String text = "@mixin --m1() { color: green; }";
  CSSRuleSourceDataList data = Parse(text);
  ASSERT_EQ(1u, data.size());
  EXPECT_EQ(" color: green; ", Substring(text, data[0]->rule_body_range));
  EXPECT_EQ(" color: green; ",
            Substring(text, data[0]->rule_declarations_range));

  ASSERT_EQ(1u, data[0]->child_rules.size());
  ASSERT_EQ(1u, data[0]->child_rules[0]->property_data.size());
  EXPECT_EQ("color", data[0]->child_rules[0]->property_data[0].name);
  EXPECT_EQ("green", data[0]->child_rules[0]->property_data[0].value);
}

TEST_F(InspectorCSSParserObserverTest, MixinApplyWithNoBlock) {
  String text = "div { @apply --m1; color: green; }";
  CSSRuleSourceDataList data = Parse(text);
  ASSERT_EQ(1u, data.size());
  ASSERT_EQ(2u, data[0]->child_rules.size());
  ASSERT_EQ(1u, data[0]->child_rules[1]->property_data.size());
  EXPECT_EQ("color", data[0]->child_rules[1]->property_data[0].name);
  EXPECT_EQ("green", data[0]->child_rules[1]->property_data[0].value);
}

}  // namespace blink
