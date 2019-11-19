// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/css/css_style_sheet.h"
#include "third_party/blink/renderer/core/css/parser/css_lazy_parsing_state.h"
#include "third_party/blink/renderer/core/css/parser/css_parser.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_context.h"
#include "third_party/blink/renderer/core/css/style_rule.h"
#include "third_party/blink/renderer/core/css/style_sheet_contents.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class CSSLazyParsingTest : public testing::Test {
 public:
  bool HasParsedProperties(StyleRule* rule) {
    return rule->HasParsedProperties();
  }

  StyleRule* RuleAt(StyleSheetContents* sheet, wtf_size_t index) {
    return To<StyleRule>(sheet->ChildRules()[index].Get());
  }

 protected:
  Persistent<StyleSheetContents> cached_contents_;
};

TEST_F(CSSLazyParsingTest, Simple) {
  auto* context = MakeGarbageCollected<CSSParserContext>(
      kHTMLStandardMode, SecureContextMode::kInsecureContext);
  auto* style_sheet = MakeGarbageCollected<StyleSheetContents>(context);

  String sheet_text = "body { background-color: red; }";
  CSSParser::ParseSheet(context, style_sheet, sheet_text,
                        CSSDeferPropertyParsing::kYes);
  StyleRule* rule = RuleAt(style_sheet, 0);
  EXPECT_FALSE(HasParsedProperties(rule));
  rule->Properties();
  EXPECT_TRUE(HasParsedProperties(rule));
}

TEST_F(CSSLazyParsingTest, LazyParseBeforeAfter) {
  auto* context = MakeGarbageCollected<CSSParserContext>(
      kHTMLStandardMode, SecureContextMode::kInsecureContext);
  auto* style_sheet = MakeGarbageCollected<StyleSheetContents>(context);

  String sheet_text =
      "p::before { content: 'foo' } p .class::after { content: 'bar' } ";
  CSSParser::ParseSheet(context, style_sheet, sheet_text,
                        CSSDeferPropertyParsing::kYes);

  EXPECT_FALSE(HasParsedProperties(RuleAt(style_sheet, 0)));
  EXPECT_FALSE(HasParsedProperties(RuleAt(style_sheet, 1)));
}

// Test for crbug.com/664115 where |shouldConsiderForMatchingRules| would flip
// from returning false to true if the lazy property was parsed. This is a
// dangerous API because callers will expect the set of matching rules to be
// identical if the stylesheet is not mutated.
TEST_F(CSSLazyParsingTest, ShouldConsiderForMatchingRulesDoesntChange1) {
  auto* context = MakeGarbageCollected<CSSParserContext>(
      kHTMLStandardMode, SecureContextMode::kInsecureContext);
  auto* style_sheet = MakeGarbageCollected<StyleSheetContents>(context);

  String sheet_text = "p::first-letter { ,badness, } ";
  CSSParser::ParseSheet(context, style_sheet, sheet_text,
                        CSSDeferPropertyParsing::kYes);

  StyleRule* rule = RuleAt(style_sheet, 0);
  EXPECT_FALSE(HasParsedProperties(rule));
  EXPECT_TRUE(
      rule->ShouldConsiderForMatchingRules(false /* includeEmptyRules */));

  // Parse the rule.
  rule->Properties();

  // Now, we should still consider this for matching rules even if it is empty.
  EXPECT_TRUE(HasParsedProperties(rule));
  EXPECT_TRUE(
      rule->ShouldConsiderForMatchingRules(false /* includeEmptyRules */));
}

// Test the same thing as above with lazy parsing off to ensure that we perform
// the optimization where possible.
TEST_F(CSSLazyParsingTest, ShouldConsiderForMatchingRulesSimple) {
  auto* context = MakeGarbageCollected<CSSParserContext>(
      kHTMLStandardMode, SecureContextMode::kInsecureContext);
  auto* style_sheet = MakeGarbageCollected<StyleSheetContents>(context);

  String sheet_text = "p::before { ,badness, } ";
  CSSParser::ParseSheet(context, style_sheet, sheet_text,
                        CSSDeferPropertyParsing::kNo);

  StyleRule* rule = RuleAt(style_sheet, 0);
  EXPECT_TRUE(HasParsedProperties(rule));
  EXPECT_FALSE(
      rule->ShouldConsiderForMatchingRules(false /* includeEmptyRules */));
}

// Regression test for crbug.com/660290 where we change the underlying owning
// document from the StyleSheetContents without changing the UseCounter. This
// test ensures that the new UseCounter is used when doing new parsing work.
TEST_F(CSSLazyParsingTest, ChangeDocuments) {
  auto dummy_holder = std::make_unique<DummyPageHolder>(IntSize(500, 500));
  Page::InsertOrdinaryPageForTesting(&dummy_holder->GetPage());

  auto* context = MakeGarbageCollected<CSSParserContext>(
      kHTMLStandardMode, SecureContextMode::kInsecureContext,
      CSSParserContext::kLiveProfile, &dummy_holder->GetDocument());
  cached_contents_ = MakeGarbageCollected<StyleSheetContents>(context);
  {
    auto* sheet = MakeGarbageCollected<CSSStyleSheet>(
        cached_contents_, dummy_holder->GetDocument());
    DCHECK(sheet);

    String sheet_text = "body { background-color: red; } p { color: orange;  }";
    CSSParser::ParseSheet(context, cached_contents_, sheet_text,
                          CSSDeferPropertyParsing::kYes);

    // Parse the first property set with the first document as owner.
    StyleRule* rule = RuleAt(cached_contents_, 0);
    EXPECT_FALSE(HasParsedProperties(rule));
    rule->Properties();
    EXPECT_TRUE(HasParsedProperties(rule));

    EXPECT_EQ(&dummy_holder->GetDocument(),
              cached_contents_->SingleOwnerDocument());
    UseCounterHelper& use_counter1 =
        dummy_holder->GetDocument().Loader()->GetUseCounterHelper();
    EXPECT_TRUE(
        use_counter1.IsCounted(CSSPropertyID::kBackgroundColor,
                               UseCounterHelper::CSSPropertyType::kDefault));
    EXPECT_FALSE(use_counter1.IsCounted(
        CSSPropertyID::kColor, UseCounterHelper::CSSPropertyType::kDefault));

    // Change owner document.
    cached_contents_->UnregisterClient(sheet);
    dummy_holder.reset();
  }
  // Ensure no stack references to oilpan objects.
  ThreadState::Current()->CollectAllGarbageForTesting();

  auto dummy_holder2 = std::make_unique<DummyPageHolder>(IntSize(500, 500));
  Page::InsertOrdinaryPageForTesting(&dummy_holder2->GetPage());
  auto* sheet2 = MakeGarbageCollected<CSSStyleSheet>(
      cached_contents_, dummy_holder2->GetDocument());

  EXPECT_EQ(&dummy_holder2->GetDocument(),
            cached_contents_->SingleOwnerDocument());

  // Parse the second property set with the second document as owner.
  StyleRule* rule2 = RuleAt(cached_contents_, 1);
  EXPECT_FALSE(HasParsedProperties(rule2));
  rule2->Properties();
  EXPECT_TRUE(HasParsedProperties(rule2));

  UseCounterHelper& use_counter2 =
      dummy_holder2->GetDocument().Loader()->GetUseCounterHelper();
  EXPECT_TRUE(sheet2);
  EXPECT_TRUE(use_counter2.IsCounted(
      CSSPropertyID::kColor, UseCounterHelper::CSSPropertyType::kDefault));

  EXPECT_FALSE(
      use_counter2.IsCounted(CSSPropertyID::kBackgroundColor,
                             UseCounterHelper::CSSPropertyType::kDefault));
}

}  // namespace blink
