// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/css/css_style_sheet.h"
#include "third_party/blink/renderer/core/css/parser/css_lazy_parsing_state.h"
#include "third_party/blink/renderer/core/css/parser/css_parser.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_context.h"
#include "third_party/blink/renderer/core/css/style_rule.h"
#include "third_party/blink/renderer/core/css/style_sheet_contents.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/execution_context/security_context.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/thread_state.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

#if defined(__SSE2__) || defined(__ARM_NEON__)

class CSSLazyParsingTest : public testing::Test {
 public:
  bool HasParsedProperties(StyleRule* rule) {
    return rule->HasParsedProperties();
  }

  StyleRule* RuleAt(StyleSheetContents* sheet, wtf_size_t index) {
    return To<StyleRule>(sheet->ChildRules()[index].Get());
  }

 protected:
  test::TaskEnvironment task_environment_;
  Persistent<StyleSheetContents> cached_contents_;
};

TEST_F(CSSLazyParsingTest, Simple) {
  for (const bool fast_path : {false, true}) {
    ScopedCSSLazyParsingFastPathForTest fast_path_enabled(fast_path);
    auto* context = MakeGarbageCollected<CSSParserContext>(
        kHTMLStandardMode, SecureContextMode::kInsecureContext);
    auto* style_sheet = MakeGarbageCollected<StyleSheetContents>(context);

    String sheet_text = "body { background-color: red; }/*padding1234567890*/";
    CSSParser::ParseSheet(context, style_sheet, sheet_text,
                          CSSDeferPropertyParsing::kYes);
    StyleRule* rule = RuleAt(style_sheet, 0);
    EXPECT_FALSE(HasParsedProperties(rule));
    rule->Properties();
    EXPECT_TRUE(HasParsedProperties(rule));
  }
}

TEST_F(CSSLazyParsingTest, LazyParseBeforeAfter) {
  for (const bool fast_path : {false, true}) {
    ScopedCSSLazyParsingFastPathForTest fast_path_enabled(fast_path);
    auto* context = MakeGarbageCollected<CSSParserContext>(
        kHTMLStandardMode, SecureContextMode::kInsecureContext);
    auto* style_sheet = MakeGarbageCollected<StyleSheetContents>(context);

    String sheet_text =
        "p::before { content: 'foo' } p .class::after { content: 'bar' } "
        "/*padding1234567890*/";
    CSSParser::ParseSheet(context, style_sheet, sheet_text,
                          CSSDeferPropertyParsing::kYes);

    EXPECT_FALSE(HasParsedProperties(RuleAt(style_sheet, 0)));
    EXPECT_FALSE(HasParsedProperties(RuleAt(style_sheet, 1)));
  }
}

// Regression test for crbug.com/660290 where we change the underlying owning
// document from the StyleSheetContents without changing the UseCounter. This
// test ensures that the new UseCounter is used when doing new parsing work.
TEST_F(CSSLazyParsingTest, ChangeDocuments) {
  for (const bool fast_path : {false, true}) {
    ScopedCSSLazyParsingFastPathForTest fast_path_enabled(fast_path);
    auto dummy_holder = std::make_unique<DummyPageHolder>(gfx::Size(500, 500));
    Page::InsertOrdinaryPageForTesting(&dummy_holder->GetPage());

    auto* context = MakeGarbageCollected<CSSParserContext>(
        kHTMLStandardMode, SecureContextMode::kInsecureContext,
        &dummy_holder->GetDocument());
    cached_contents_ = MakeGarbageCollected<StyleSheetContents>(context);
    {
      auto* sheet = MakeGarbageCollected<CSSStyleSheet>(
          cached_contents_, dummy_holder->GetDocument());
      DCHECK(sheet);

      String sheet_text =
          "body { background-color: red; } p { color: orange;  "
          "}/*padding1234567890*/";
      CSSParser::ParseSheet(context, cached_contents_, sheet_text,
                            CSSDeferPropertyParsing::kYes);

      // Parse the first property set with the first document as owner.
      StyleRule* rule = RuleAt(cached_contents_, 0);
      EXPECT_FALSE(HasParsedProperties(rule));
      rule->Properties();
      EXPECT_TRUE(HasParsedProperties(rule));

      EXPECT_EQ(&dummy_holder->GetDocument(),
                cached_contents_->SingleOwnerDocument());
      UseCounterImpl& use_counter1 =
          dummy_holder->GetDocument().Loader()->GetUseCounter();
      EXPECT_TRUE(
          use_counter1.IsCounted(CSSPropertyID::kBackgroundColor,
                                 UseCounterImpl::CSSPropertyType::kDefault));
      EXPECT_FALSE(use_counter1.IsCounted(
          CSSPropertyID::kColor, UseCounterImpl::CSSPropertyType::kDefault));

      // Change owner document.
      cached_contents_->UnregisterClient(sheet);
      dummy_holder.reset();
    }
    // Ensure no stack references to oilpan objects.
    ThreadState::Current()->CollectAllGarbageForTesting();

    auto dummy_holder2 = std::make_unique<DummyPageHolder>(gfx::Size(500, 500));
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

    UseCounterImpl& use_counter2 =
        dummy_holder2->GetDocument().Loader()->GetUseCounter();
    EXPECT_TRUE(sheet2);
    EXPECT_TRUE(use_counter2.IsCounted(
        CSSPropertyID::kColor, UseCounterImpl::CSSPropertyType::kDefault));

    EXPECT_FALSE(
        use_counter2.IsCounted(CSSPropertyID::kBackgroundColor,
                               UseCounterImpl::CSSPropertyType::kDefault));
  }
}

TEST_F(CSSLazyParsingTest, NoLazyParsingForNestedRules) {
  for (const bool fast_path : {false, true}) {
    ScopedCSSLazyParsingFastPathForTest fast_path_enabled(fast_path);
    auto* context = MakeGarbageCollected<CSSParserContext>(
        kHTMLStandardMode, SecureContextMode::kInsecureContext);
    auto* style_sheet = MakeGarbageCollected<StyleSheetContents>(context);

    String sheet_text = "body { color: green; & div { color: red; } }";
    CSSParser::ParseSheet(context, style_sheet, sheet_text,
                          CSSDeferPropertyParsing::kYes);
    StyleRule* rule = RuleAt(style_sheet, 0);
    EXPECT_TRUE(HasParsedProperties(rule));
    EXPECT_EQ("color: green;", rule->Properties().AsText());
    EXPECT_TRUE(HasParsedProperties(rule));
  }
}

// A version of NoLazyParsingForNestedRules where CSSNestedDeclarations
// is disabled. Can be removed when the CSSNestedDeclarations is removed.
TEST_F(CSSLazyParsingTest,
       NoLazyParsingForNestedRules_CSSNestedDeclarationsDisabled) {
  ScopedCSSNestedDeclarationsForTest nested_declarations_enabled(false);

  for (const bool fast_path : {false, true}) {
    ScopedCSSLazyParsingFastPathForTest fast_path_enabled(fast_path);
    auto* context = MakeGarbageCollected<CSSParserContext>(
        kHTMLStandardMode, SecureContextMode::kInsecureContext);
    auto* style_sheet = MakeGarbageCollected<StyleSheetContents>(context);

    String sheet_text = "body { & div { color: red; } color: green; }";
    CSSParser::ParseSheet(context, style_sheet, sheet_text,
                          CSSDeferPropertyParsing::kYes);
    StyleRule* rule = RuleAt(style_sheet, 0);
    EXPECT_TRUE(HasParsedProperties(rule));
    EXPECT_EQ("color: green;", rule->Properties().AsText());
    EXPECT_TRUE(HasParsedProperties(rule));
  }
}

#endif  // SIMD

}  // namespace blink
