// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_DYNAMIC_PSEUDO_EXTRACTOR_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_DYNAMIC_PSEUDO_EXTRACTOR_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/css/css_selector.h"
#include "third_party/blink/renderer/core/css/selector_checker.h"
#include "third_party/blink/renderer/core/style/computed_style_constants.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

// A "dynamic pseudo" is a flag that exist as a hint to the style recalc
// process that a PseudoElement will be needed. For example, when considering
// an element <div class=foo> under the following rules:
//
//  .foo::before {}
//  .foo::before::marker {}
//  .baz::after {}
//
// Then we need to set the kPseudoIdBefore dynamic pseudo flag, and we need
// to *not* set the kPseudoIdAfter flag (since .baz doesn't match).
// Note also that we don't set kPseudoIdMarker; this would be set when
// considering the selectors against the appropriate pseudo-element
// (<::before>), not <div class=foo>.
//
// DynamicPseudoExtractor does this by collecting any PseudoIds found
// to the right of the implicit pseudo combinator [1], *without* checking that
// part for a match. Then, if the partial selector to the *left* of the implicit
// combinator matches (determined by SelectorChecker), we return the PseudoIds
// collected previously.
//
// [1] See CSSSelectorParser::SplitCompoundAtImplicitCombinator() (impl.)
class CORE_EXPORT DynamicPseudoExtractor {
  STACK_ALLOCATED();

 public:
  explicit DynamicPseudoExtractor(SelectorChecker& selector_checker)
      : selector_checker_(selector_checker) {}

  using SelectorCheckingContext = SelectorChecker::SelectorCheckingContext;

  PseudoIdFlags Extract(SelectorCheckingContext&);
  PseudoIdFlags ExtractFromPseudoClass(SelectorCheckingContext&,
                                       const CSSSelector& pseudo_class);
  PseudoIdFlags ExtractFromList(SelectorCheckingContext&,
                                const CSSSelector* list);
  bool MatchRemaining(SelectorCheckingContext&, const CSSSelector* remaining);

 private:
  SelectorChecker& selector_checker_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_DYNAMIC_PSEUDO_EXTRACTOR_H_
