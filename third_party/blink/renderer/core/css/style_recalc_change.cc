// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/style_recalc_change.h"

#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/dom/pseudo_element.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

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
         node.GetForceReattachLayoutTree() || RecalcContainerQueryDependent() ||
         node.NeedsLayoutSubtreeUpdate();
}

bool StyleRecalcChange::ShouldRecalcStyleFor(const Node& node) const {
  if (flags_ & kSuppressRecalc)
    return false;
  if (RecalcChildren())
    return true;
  if (node.NeedsStyleRecalc())
    return true;
  // Early exit before getting the computed style.
  if (!RecalcContainerQueryDependent())
    return false;
  const ComputedStyle* old_style = node.GetComputedStyle();
  // Container queries may affect display:none elements, and we since we store
  // that dependency on ComputedStyle we need to recalc style for display:none
  // subtree roots.
  return !old_style || old_style->DependsOnSizeContainerQueries();
}

bool StyleRecalcChange::ShouldUpdatePseudoElement(
    const PseudoElement& pseudo_element) const {
  if (UpdatePseudoElements())
    return true;
  if (pseudo_element.NeedsStyleRecalc())
    return true;
  if (pseudo_element.NeedsLayoutSubtreeUpdate())
    return true;
  return RecalcContainerQueryDependent() &&
         pseudo_element.ComputedStyleRef().DependsOnSizeContainerQueries();
}

String StyleRecalcChange::ToString() const {
  StringBuilder builder;
  builder.Append("StyleRecalcChange{propagate=");
  switch (propagate_) {
    case kNo:
      builder.Append("kNo");
      break;
    case kUpdatePseudoElements:
      builder.Append("kUpdatePseudoElements");
      break;
    case kIndependentInherit:
      builder.Append("kIndependentInherit");
      break;
    case kRecalcChildren:
      builder.Append("kRecalcChildren");
      break;
    case kRecalcDescendants:
      builder.Append("kRecalcDescendants");
      break;
  }
  builder.Append(", flags=");
  if (!flags_) {
    builder.Append("kNoFlags");
  } else {
    Flags flags = flags_;
    // Make sure we don't loop forever if we aren't handling some case.
    Flags previous_flags = 0;
    String separator = "";
    while (flags && flags != previous_flags) {
      previous_flags = flags;
      builder.Append(separator);
      separator = "|";
      if (flags & kRecalcContainer) {
        builder.Append("kRecalcContainer");
        flags &= ~kRecalcContainer;
      } else if (flags & kRecalcDescendantContainers) {
        builder.Append("kRecalcDescendantContainers");
        flags &= ~kRecalcDescendantContainers;
      } else if (flags & kReattach) {
        builder.Append("kReattach");
        flags &= ~kReattach;
      } else if (flags & kSuppressRecalc) {
        builder.Append("kSuppressRecalc");
        flags &= ~kSuppressRecalc;
      }
    }
    if (flags) {
      builder.Append(separator);
      builder.Append("UnknownFlag=");
      builder.Append(flags);
    }
  }
  builder.Append("}");
  return builder.ToString();
}

StyleRecalcChange::Flags StyleRecalcChange::FlagsForChildren(
    const Element& element) const {
  Flags result = flags_;

  // Note that kSuppressRecalc is used on the root container for the
  // interleaved style recalc.
  if ((result & (kRecalcContainerFlags | kSuppressRecalc)) ==
      kRecalcContainer) {
    if (IsShadowHost(element)) {
      // Since the nearest container is found in shadow-including ancestors and
      // not in flat tree ancestors, and style recalc traversal happens in flat
      // tree order, we need to invalidate inside flat tree descendant
      // containers if such containers are inside shadow trees.
      result |= kRecalcDescendantContainers;
    } else {
      // Don't traverse into children if we hit a descendant container while
      // recalculating container queries. If the queries for this container also
      // changes, we will enter another container query recalc for this subtree
      // from layout.
      const ComputedStyle* old_style = element.GetComputedStyle();
      if (old_style && old_style->CanMatchSizeContainerQueries(element))
        result &= ~kRecalcContainer;
    }
  }

  // kSuppressRecalc should only take effect for the query container itself, not
  // for children. Also make sure the kMarkReattach flag survives one level past
  // the container for ::first-line re-attachments initiated from
  // UpdateStyleAndLayoutTreeForContainer().
  if (result & kSuppressRecalc)
    result &= ~kSuppressRecalc;
  else
    result &= ~kMarkReattach;

  return result;
}

bool StyleRecalcChange::IndependentInherit(
    const ComputedStyle& old_style) const {
  // During UpdateStyleAndLayoutTreeForContainer(), if the old_style is marked
  // as depending on container queries, we need to do a proper recalc for the
  // element.
  return propagate_ == kIndependentInherit &&
         (!RecalcContainerQueryDependent() ||
          !old_style.DependsOnSizeContainerQueries());
}

}  // namespace blink
