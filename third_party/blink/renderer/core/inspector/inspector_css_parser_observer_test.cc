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

}  // namespace blink
