// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/dynamic_pseudo_extractor.h"

#include "third_party/blink/renderer/core/css/css_selector.h"
#include "third_party/blink/renderer/core/css/css_test_helpers.h"
#include "third_party/blink/renderer/core/css/style_rule.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/pseudo_element.h"
#include "third_party/blink/renderer/core/style/computed_style_constants.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

// The dynamic pseudo tests follow the following rules:
//
// A document is loaded with the following HTML:
//
//  <div id=a class=b></div>
//
// This div is then used as the (ultimate) originating element for a chain
// of PseudoElements specified by `pseudo_element_chain`. The innermost
// pseudo-element in that chain is the passed to the ElementResolveContext,
// and we extract the flags in `rule` using that context.
struct PseudoIdFlagsTestData {
  // A chain of pseudo-elements to create, using #a (see above) as the ultimate
  // originating element.
  const std::vector<PseudoId> pseudo_element_chain;
  // The rule to extract flags on, using the innermost pseudo-element in
  // the above chain as the context.
  const char* rule;
  std::vector<PseudoId> expected_flags;
};

PseudoIdFlagsTestData pseudo_id_flags_test_data[] = {
    // clang-format off

    {
      .pseudo_element_chain = {},
      .rule = "* {}",
      .expected_flags = {},
    },
    {
      .pseudo_element_chain = {},
      .rule = "div {}",
      .expected_flags = {},
    },
    {
      .pseudo_element_chain = {},
      .rule = "div::before {}",
      .expected_flags = { kPseudoIdBefore },
    },
    {
      .pseudo_element_chain = {},
      .rule = "#a::before {}",
      .expected_flags = { kPseudoIdBefore },
    },
    {
      .pseudo_element_chain = {},
      .rule = ".b::before {}",
      .expected_flags = { kPseudoIdBefore },
    },

    // The leftmost pseudo-element selector should set the dynamic
    // pseudo flags.
    {
      .pseudo_element_chain = {},
      .rule = "div::before::marker {}",
      .expected_flags = { kPseudoIdBefore },
    },

    // The leftmost pseudo-element selector should set the dynamic
    // pseudo flags.
    {
      .pseudo_element_chain = { kPseudoIdBefore },
      .rule = "div::before::marker {}",
      .expected_flags = { kPseudoIdMarker },
    },

    // Matching this selector against <div><::marker> should not
    // set the ::marker flag nor the ::before flag.
    {
      .pseudo_element_chain = { kPseudoIdMarker },
      .rule = "div::before::marker {}",
      .expected_flags = {},
    },

    // Logical combination pseudo-classes:
    {
      .pseudo_element_chain = {},
      .rule = ":is(div::after) {}",
      .expected_flags = { kPseudoIdAfter },
    },
    {
      .pseudo_element_chain = {},
      .rule = ":is(div::before, div::after) {}",
      .expected_flags = { kPseudoIdBefore, kPseudoIdAfter },
    },
    {
      .pseudo_element_chain = { },
      .rule = ":is(::before, ::after) {}",
      .expected_flags = { kPseudoIdBefore, kPseudoIdAfter },
    },
    {
      .pseudo_element_chain = { },
      .rule = ":is(::before, ::after)::marker {}",
      .expected_flags = { kPseudoIdBefore, kPseudoIdAfter },
    },
    {
      .pseudo_element_chain = { kPseudoIdBefore },
      .rule = ":is(::before, ::after)::marker {}",
      .expected_flags = { kPseudoIdMarker },
    },

    // :where() should work identically:
    {
      .pseudo_element_chain = {},
      .rule = ":where(div::after) {}",
      .expected_flags = { kPseudoIdAfter },
    },

    // For the purposes of selector matching, a regular element does not have
    // a child, descendant, or sibling that is a pseudo-element.
    {
      .pseudo_element_chain = {},
      .rule = "div > :is(::after) {}",
      .expected_flags = { },
    },
    {
      .pseudo_element_chain = {},
      .rule = ":root :is(::after) {}",
      .expected_flags = { },
    },

    // We strictly don't need to set the flag for this selector,
    // but DynamicPseudoExtractor doesn't differentiate between
    // different logical combination pseudo-classes.
    {
      .pseudo_element_chain = { },
      .rule = ":not(::before) {}",
      .expected_flags = { kPseudoIdBefore },
    },

    // Like the :not() case above, we don't really need to
    // set the flags here, but DynamicPseudoExtractor is limited;
    // it doesn't know that the overall selector can never match
    // anything.
    {
      .pseudo_element_chain = { },
      .rule = ":is(::before):is(::after) {}",
      .expected_flags = { kPseudoIdBefore, kPseudoIdAfter },
    },

    // The originating compound part of a selector is matched
    // using the real selector checker, which can prevent
    // dynamic pseudo flags from being set.

    {
      .pseudo_element_chain = { },
      .rule = "p::before {}",
      .expected_flags = { },
    },
    {
      .pseudo_element_chain = { },
      .rule = "#noexist::before {}",
      .expected_flags = { },
    },
    {
      .pseudo_element_chain = { },
      .rule = ".noexist::before {}",
      .expected_flags = { },
    },
    {
      .pseudo_element_chain = { },
      .rule = "[noexist]::before {}",
      .expected_flags = { },
    },
    {
      .pseudo_element_chain = { },
      .rule = ".noexist > div::before {}",
      .expected_flags = { },
    },
    {
      .pseudo_element_chain = { },
      .rule = ".noexist * > div::before {}",
      .expected_flags = { },
    },

    // Partial matches:
    {
      .pseudo_element_chain = { },
      .rule = ":is(.noexist::before, ::after) {}",
      .expected_flags = { kPseudoIdAfter },
    },
    {
      .pseudo_element_chain = { },
      .rule = ":is(.noexist > div::before, div::after::marker) {}",
      .expected_flags = { kPseudoIdAfter },
    },

    // clang-format on
};

class DynamicPseudoExtractorBasic
    : public PageTestBase,
      public testing::WithParamInterface<PseudoIdFlagsTestData> {
 public:
  void SetUp() override {
    PageTestBase::SetUp();
    SetHtmlInnerHTML("<div id=a class=b></div>");
    originating_element_ = GetDocument().getElementById(AtomicString("a"));
    CHECK(originating_element_);
  }

  // Creating a chain of PseudoElements according to `chain`,
  // using `originating_element_` as the ultimate originating element.
  // Returns the innermost pseudo-element in the chain, or the originating
  // element itself, if `chain` is empty.
  Element* AttachPseudoElementChain(const std::vector<PseudoId>& chain) {
    Element* leaf = originating_element_.Get();
    for (PseudoId pseudo_id : chain) {
      leaf = PseudoElement::Create(/*parent=*/leaf, pseudo_id);
    }
    return leaf;
  }

  Persistent<Element> originating_element_;
};

INSTANTIATE_TEST_SUITE_P(SelectorChecker,
                         DynamicPseudoExtractorBasic,
                         testing::ValuesIn(pseudo_id_flags_test_data));

TEST_P(DynamicPseudoExtractorBasic, PseudoElementObjects) {
  ScopedCSSLogicalCombinationPseudoForTest scoped_feature(true);

  PseudoIdFlagsTestData param = GetParam();
  SCOPED_TRACE(param.rule);

  auto* style_rule = DynamicTo<StyleRule>(
      css_test_helpers::ParseRule(GetDocument(), param.rule));
  ASSERT_TRUE(style_rule);

  Element* candidate = AttachPseudoElementChain(param.pseudo_element_chain);
  ASSERT_TRUE(candidate);

  SelectorChecker checker(SelectorChecker::kResolvingStyle);
  SelectorChecker::SelectorCheckingContext context{
      ElementResolveContext(*candidate)};
  context.selector = style_rule->FirstSelector();

  DynamicPseudoExtractor extractor(checker);

  PseudoIdFlags expected;
  for (PseudoId pseudo_id : param.expected_flags) {
    expected.Set(pseudo_id);
  }

  PseudoIdFlags actual = extractor.Extract(context);

  SCOPED_TRACE(testing::Message()
               << "Expected flags: " << css_test_helpers::ToString(expected));
  SCOPED_TRACE(testing::Message()
               << "Actual flags: " << css_test_helpers::ToString(actual));
  EXPECT_EQ(expected, actual);
}

}  // namespace blink
