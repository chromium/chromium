// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/dynamic_pseudo_extractor.h"

#include "third_party/blink/renderer/core/css/css_selector.h"
#include "third_party/blink/renderer/core/style/computed_style_constants.h"

namespace blink {

// [1] CSSSelectorParser::SplitCompoundAtImplicitCombinator
PseudoIdFlags DynamicPseudoExtractor::Extract(
    SelectorCheckingContext& context) {
  PseudoIdFlags flags;

  for (const CSSSelector* simple = context.selector; simple;
       simple = simple->NextSimpleSelector()) {
    if (simple->Match() == CSSSelector::kPseudoElement) {
      // We've found a pseudo-element selector, e.g. ::before;
      // set the corresponding flag. This does not automatically mean
      // that we'll ultimately return this flag from DynamicPseudoExtractor;
      // `remaining` needs to match `context` for that to happen.
      flags.MaybeSet(CSSSelector::GetPseudoId(simple->GetPseudoType()));
    } else if (simple->Match() == CSSSelector::kPseudoClass) {
      // Extract flags within :is(), :where(), etc.
      flags |= ExtractFromPseudoClass(context, *simple);
    }

    const CSSSelector* remaining = simple->NextSimpleSelector();

    switch (simple->Relation()) {
      case CSSSelector::kPseudoChild:
        // We've reached the end of a pseudo-element compound. The selector
        // `remaining` now matches the originating element (which may be
        // another pseudo-element).
        if (MatchRemaining(context, remaining)) {
          return flags;
        } else {
          // The originating compound didn't match; reset the flags and keep
          // on going in case this is nested pseudo-element. E.g. when
          // matching 'div::before::marker' against a <div>, the check for
          // '::marker' fails because its originating part ('div::before')
          // does not match the <div>. We reset flags and continue, eventually
          // reaching this switch again at 'div::before', this time treating
          // 'div' as the originating part (which does match).
          flags = PseudoIdFlags();
        }
        break;
      case CSSSelector::kSubSelector:
        break;
      default:
        // We can reach here with a selector like `div > :is(::after)`.
        // Such selectors can never match; for the purposes of selector
        // matching, a regular element does not have a child, descendant,
        // or sibling that is a pseudo-element.
        return PseudoIdFlags();
    }
  }

  return flags;
}

PseudoIdFlags DynamicPseudoExtractor::ExtractFromPseudoClass(
    SelectorCheckingContext& context,
    const CSSSelector& pseudo_class) {
  DCHECK_EQ(CSSSelector::kPseudoClass, pseudo_class.Match());

  switch (pseudo_class.GetPseudoType()) {
    case CSSSelector::kPseudoIs:
    case CSSSelector::kPseudoWhere:
    case CSSSelector::kPseudoNot:
      return ExtractFromList(context, pseudo_class.SelectorListOrParent());
    default:
      // No list in this pseudo-class.
      return PseudoIdFlags();
  }
}

PseudoIdFlags DynamicPseudoExtractor::ExtractFromList(
    SelectorCheckingContext& context,
    const CSSSelector* list) {
  PseudoIdFlags flags;

  for (const CSSSelector* item = list; item;
       item = CSSSelectorList::Next(*item)) {
    SelectorCheckingContext item_context = context;
    item_context.selector = item;
    flags |= Extract(item_context);
  }

  return flags;
}

bool DynamicPseudoExtractor::MatchRemaining(SelectorCheckingContext& context,
                                            const CSSSelector* remaining) {
  SelectorCheckingContext context_remaining = context;
  context_remaining.selector = remaining;
  return selector_checker_.Match(context_remaining);
}

}  // namespace blink
