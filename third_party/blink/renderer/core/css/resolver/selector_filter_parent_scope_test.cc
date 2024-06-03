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
#include "third_party/blink/renderer/platform/heap/thread_state.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

namespace blink {

class SelectorFilterParentScopeTest : public testing::Test {
 protected:
  void SetUp() override {
    dummy_page_holder_ = std::make_unique<DummyPageHolder>(gfx::Size(800, 600));
    GetDocument().SetCompatibilityMode(Document::kNoQuirksMode);
  }

  void TearDown() override {
    dummy_page_holder_ = nullptr;
    ThreadState::Current()->CollectAllGarbageForTesting();
  }

  Document& GetDocument() { return dummy_page_holder_->GetDocument(); }

 private:
  test::TaskEnvironment task_environment_;
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
          CSSNestingType::kNone, /*parent_rule_for_nesting=*/nullptr,
          /*is_within_scope=*/false, nullptr,
          "html *, body *, .match *, #myId *", arena);
      CSSSelectorList* selectors =
          CSSSelectorList::AdoptSelectorVector(selector_vector);

      for (const CSSSelector* selector = selectors->First(); selector;
           selector = CSSSelectorList::Next(*selector)) {
        Vector<unsigned> selector_hashes;
        filter.CollectIdentifierHashes(*selector, /* style_scope */ nullptr,
                                       selector_hashes);
        EXPECT_NE(selector_hashes.size(), 0u);
        EXPECT_FALSE(filter.FastRejectSelector(selector_hashes));
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
      CSSNestingType::kNone, /*parent_rule_for_nesting=*/nullptr,
      /*is_within_scope=*/false, nullptr,
      "html *, body *, div *, span *, .x *, #y *", arena);
  CSSSelectorList* selectors =
      CSSSelectorList::AdoptSelectorVector(selector_vector);

  for (const CSSSelector* selector = selectors->First(); selector;
       selector = CSSSelectorList::Next(*selector)) {
    Vector<unsigned> selector_hashes;
    filter.CollectIdentifierHashes(*selector, /* style_scope */ nullptr,
                                   selector_hashes);
    EXPECT_NE(selector_hashes.size(), 0u);
    EXPECT_FALSE(filter.FastRejectSelector(selector_hashes));
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

  // Drop the reference to the SVG, which is an `IsolatedSVGDDocument`, and is
  // not destroyed during GC, instead using a separate lifetime system. Without
  // this, something keeps it alive until the next GC after test teardown. This
  // is all the information available at the time of writing.
  //
  // This is a problem because it refers to a `blink::PerformanceMonitor`, which
  // is a `CheckedObserver`, and which must be destroyed before resetting
  // `blink::MainThread` during test teardown, because at that point, it is no
  // longer possible to remove it from `ObserverList`s.
  //
  // TODO(crbug.com/337200890): Update this comment with more information and
  // see whether removing this code is possible once this crashbug's root cause
  // has been determined.
  GetDocument().body()->setInnerHTML(R"HTML(
    <div></div>
  )HTML");
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
      CSSNestingType::kNone, /*parent_rule_for_nesting=*/nullptr,
      /*is_within_scope=*/false, nullptr,
      "[Attr] *, [attr] *, [viewbox] *, [VIEWBOX] *", arena);
  CSSSelectorList* selectors =
      CSSSelectorList::AdoptSelectorVector(selector_vector);

  for (const CSSSelector* selector = selectors->First(); selector;
       selector = CSSSelectorList::Next(*selector)) {
    Vector<unsigned> selector_hashes;
    filter.CollectIdentifierHashes(*selector, /* style_scope */ nullptr,
                                   selector_hashes);
    EXPECT_NE(selector_hashes.size(), 0u);
    EXPECT_FALSE(filter.FastRejectSelector(selector_hashes));
  }
}

}  // namespace blink
