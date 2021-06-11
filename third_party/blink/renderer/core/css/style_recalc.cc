// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/style_recalc.h"

#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/dom/pseudo_element.h"

namespace blink {

bool StyleRecalcChange::TraverseChildren(const Element& element) const {
  return RecalcChildren() || RecalcContainerQueryDependent() ||
         element.ChildNeedsStyleRecalc();
}

bool StyleRecalcChange::TraversePseudoElements(const Element& element) const {
  return UpdatePseudoElements() || RecalcContainerQueryDependent() ||
         element.ChildNeedsStyleRecalc();
}

bool StyleRecalcChange::TraverseChild(const Node& node) const {
  return ShouldRecalcStyleFor(node) || node.ChildNeedsStyleRecalc() ||
         RecalcContainerQueryDependent();
}

bool StyleRecalcChange::ShouldRecalcStyleFor(const Node& node) const {
  if (flags_ & kSuppressRecalc)
    return false;
  if (RecalcChildren())
    return true;
  if (node.NeedsStyleRecalc())
    return true;
  if (node.GetForceReattachLayoutTree())
    return true;
  // Early exit before getting the computed style.
  if (propagate_ != kClearEnsured && !RecalcContainerQueryDependent())
    return false;
  if (const ComputedStyle* old_style = node.GetComputedStyle()) {
    return (propagate_ == kClearEnsured &&
            old_style->IsEnsuredInDisplayNone()) ||
           (RecalcContainerQueryDependent() &&
            old_style->DependsOnContainerQueries());
  }
  // Container queries may affect display:none elements, and we since we store
  // that dependency on ComputedStyle we need to recalc style for display:none
  // subtree roots.
  return RecalcContainerQueryDependent();
}

bool StyleRecalcChange::ShouldUpdatePseudoElement(
    const PseudoElement& pseudo_element) const {
  if (UpdatePseudoElements())
    return true;
  if (pseudo_element.NeedsStyleRecalc())
    return true;
  return RecalcContainerQueryDependent() &&
         pseudo_element.ComputedStyleRef().DependsOnContainerQueries();
}

StyleRecalcChange::Flags StyleRecalcChange::FlagsForChildren(
    const Element& element) const {
  Flags result = flags_;

  // Note that kSuppressRecalc is used on the root container for the
  // interleaved style recalc.
  if ((result & (kRecalcContainerFlags | kSuppressRecalc)) ==
      kRecalcContainer) {
    // Don't traverse into children if we hit a descendant container while
    // recalculating container queries. If the queries for this container also
    // changes, we will enter another container query recalc for this subtree
    // from layout.
    const ComputedStyle* old_style = element.GetComputedStyle();
    if (old_style && old_style->IsContainerForContainerQueries())
      result &= ~kRecalcContainer;
  }

  result &= ~kSuppressRecalc;

  return result;
}

StyleRecalcContext StyleRecalcContext::FromAncestors(Element& element) {
  Element* ancestor = &element;
  // TODO(crbug.com/1145970): Avoid this work if we're not inside a container.
  while ((ancestor = DynamicTo<Element>(
              LayoutTreeBuilderTraversal::Parent(*ancestor)))) {
    if (ancestor->GetContainerQueryEvaluator())
      return StyleRecalcContext{ancestor};
  }

  return StyleRecalcContext();
}

}  // namespace blink
