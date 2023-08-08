// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/resolver/selector_filter_parent_scope.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/css/parser/css_parser.h"
#include "third_party/blink/renderer/core/execution_context/security_context.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

class SelectorFilterParentScopeTest : public testing::Test {
 protected:
  void SetUp() override {
    dummy_page_holder_ = std::make_unique<DummyPageHolder>(gfx::Size(800, 600));
    GetDocument().SetCompatibilityMode(Document::kNoQuirksMode);
  }

  Document& GetDocument() { return dummy_page_holder_->GetDocument(); }

  static constexpr size_t max_identifier_hashes = 4;

 private:
  std::unique_ptr<DummyPageHolder> dummy_page_holder_;
};

TEST_F(SelectorFilterParentScopeTest, ParentScope) {
  HeapVector<CSSSelector> arena;
  GetDocument().body()->setAttribute(html_names::kClassAttr,
                                     AtomicString("match"));
  GetDocument().documentElement()->SetIdAttribute(AtomicString("myId"));
  auto* div = GetDocument().CreateRawElement(html_names::kDivTag);
  GetDocument().body()->appendChild(div);
  SelectorFilter& filter = GetDocument().GetStyleResolver().GetSelectorFilter();
  GetDocument().Lifecycle().AdvanceTo(DocumentLifecycle::kInStyleRecalc);

  SelectorFilterRootScope root_scope(nullptr);
  SelectorFilterParentScope html_scope(*GetDocument().documentElement());
  {
    SelectorFilterParentScope body_scope(*GetDocument().body());
    SelectorFilterParentScope::EnsureParentStackIsPushed();
    {
      SelectorFilterParentScope div_scope(*div);
      SelectorFilterParentScope::EnsureParentStackIsPushed();

      base::span<CSSSelector> selector_vector = CSSParser::ParseSelector(
          MakeGarbageCollected<CSSParserContext>(
              kHTMLStandardMode, SecureContextMode::kInsecureContext),
          CSSNestingType::kNone, /*parent_rule_for_nesting=*/nullptr, nullptr,
          "html *, body *, .match *, #myId *", arena);
      CSSSelectorList* selectors =
          CSSSelectorList::AdoptSelectorVector(selector_vector);

      for (const CSSSelector* selector = selectors->First(); selector;
           selector = CSSSelectorList::Next(*selector)) {
        unsigned selector_hashes[max_identifier_hashes];
        filter.CollectIdentifierHashes(*selector, /* style_scope */ nullptr,
                                       selector_hashes, max_identifier_hashes);
        EXPECT_NE(selector_hashes[0], 0u);
        EXPECT_FALSE(
            filter.FastRejectSelector<max_identifier_hashes>(selector_hashes));
      }
    }
  }
}

TEST_F(SelectorFilterParentScopeTest, RootScope) {
  GetDocument().body()->setInnerHTML(R"HTML(
    <div class=x>
      <span id=y></span>
    </div>
  )HTML");
  SelectorFilter& filter = GetDocument().GetStyleResolver().GetSelectorFilter();
  GetDocument().Lifecycle().AdvanceTo(DocumentLifecycle::kInStyleRecalc);

  SelectorFilterRootScope span_scope(
      GetDocument().getElementById(AtomicString("y")));
  SelectorFilterParentScope::EnsureParentStackIsPushed();

  HeapVector<CSSSelector> arena;
  base::span<CSSSelector> selector_vector = CSSParser::ParseSelector(
      MakeGarbageCollected<CSSParserContext>(
          kHTMLStandardMode, SecureContextMode::kInsecureContext),
      CSSNestingType::kNone, /*parent_rule_for_nesting=*/nullptr, nullptr,
      "html *, body *, div *, span *, .x *, #y *", arena);
  CSSSelectorList* selectors =
      CSSSelectorList::AdoptSelectorVector(selector_vector);

  for (const CSSSelector* selector = selectors->First(); selector;
       selector = CSSSelectorList::Next(*selector)) {
    unsigned selector_hashes[max_identifier_hashes];
    filter.CollectIdentifierHashes(*selector, /* style_scope */ nullptr,
                                   selector_hashes, max_identifier_hashes);
    EXPECT_NE(selector_hashes[0], 0u);
    EXPECT_FALSE(
        filter.FastRejectSelector<max_identifier_hashes>(selector_hashes));
  }
}

TEST_F(SelectorFilterParentScopeTest, ReentrantSVGImageLoading) {
  GetDocument().body()->setInnerHTML(R"HTML(
    <style>
      div::before {
        content: url('data:image/svg+xml,<svg xmlns="http://www.w3.org/2000/svg"></svg>');
      }
    </style>
    <div></div>
  )HTML");

  // The SVG image is loaded synchronously from style recalc re-entering style
  // recalc for the SVG image Document. Without supporting re-entrancy for
  // SelectorFilterParentScope with a SelectorFilterRootScope, this update may
  // cause DCHECKs to fail.
  GetDocument().UpdateStyleAndLayoutTree();
}

TEST_F(SelectorFilterParentScopeTest, AttributeFilter) {
  GetDocument().body()->setInnerHTML(
      R"HTML(<div ATTR><svg VIewBox></svg></div>)HTML");
  auto* outer = To<Element>(GetDocument().body()->firstChild());
  auto* svg = To<Element>(outer->firstChild());
  auto* inner = GetDocument().CreateRawElement(html_names::kDivTag);
  svg->appendChild(inner);

  ASSERT_TRUE(outer->hasAttributes());
  EXPECT_EQ("attr", outer->Attributes()[0].GetName().LocalName());

  ASSERT_TRUE(svg->hasAttributes());
  EXPECT_EQ("viewBox", svg->Attributes()[0].GetName().LocalName());

  SelectorFilter& filter = GetDocument().GetStyleResolver().GetSelectorFilter();
  GetDocument().Lifecycle().AdvanceTo(DocumentLifecycle::kInStyleRecalc);

  SelectorFilterRootScope span_scope(inner);
  SelectorFilterParentScope::EnsureParentStackIsPushed();

  HeapVector<CSSSelector> arena;
  base::span<CSSSelector> selector_vector = CSSParser::ParseSelector(
      MakeGarbageCollected<CSSParserContext>(
          kHTMLStandardMode, SecureContextMode::kInsecureContext),
      CSSNestingType::kNone, /*parent_rule_for_nesting=*/nullptr, nullptr,
      "[Attr] *, [attr] *, [viewbox] *, [VIEWBOX] *", arena);
  CSSSelectorList* selectors =
      CSSSelectorList::AdoptSelectorVector(selector_vector);

  for (const CSSSelector* selector = selectors->First(); selector;
       selector = CSSSelectorList::Next(*selector)) {
    unsigned selector_hashes[max_identifier_hashes];
    filter.CollectIdentifierHashes(*selector, /* style_scope */ nullptr,
                                   selector_hashes, max_identifier_hashes);
    EXPECT_NE(selector_hashes[0], 0u);
    EXPECT_FALSE(
        filter.FastRejectSelector<max_identifier_hashes>(selector_hashes));
  }
}

}  // namespace blink
