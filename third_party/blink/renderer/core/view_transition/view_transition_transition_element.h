// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_VIEW_TRANSITION_VIEW_TRANSITION_TRANSITION_ELEMENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_VIEW_TRANSITION_VIEW_TRANSITION_TRANSITION_ELEMENT_H_

#include "third_party/blink/renderer/core/dom/pseudo_element.h"
#include "third_party/blink/renderer/core/style/computed_style_constants.h"
#include "third_party/blink/renderer/core/view_transition/view_transition_pseudo_element_base.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_map.h"
#include "third_party/blink/renderer/platform/heap/forward.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"

namespace blink {
class ViewTransitionStyleTracker;

// This class represents the ::view-transition pseudo-element.
// Once the transition starts, it tracks the nested ::view-transition-group(*)
// pseudo-elements for quick lookup.
class CORE_EXPORT ViewTransitionTransitionElement
    : public ViewTransitionPseudoElementBase {
 public:
  ViewTransitionTransitionElement(
      Element* parent,
      const ViewTransitionStyleTracker* style_tracker);
  ~ViewTransitionTransitionElement() override = default;

  PseudoElement* FindViewTransitionGroupPseudoElement(
      const AtomicString& view_transition_name);

  // This returns either ::view-transition or the containing
  // ::view-transition-group, which technically isn't the parent, since there
  // needs to be a ::view-transition-group-children. The problem is that the
  // nested groups is only created if we actually have a nested group, so it
  // wouldn't find it for the first nested group that we're visiting. As a
  // result, this returns the group with an expectation that the calling code
  // would then recurse into the nested groups if needed.
  PseudoElement* FindViewTransitionGroupPseudoElementParent(
      const AtomicString& view_transition_name);

  // Build a chain of names that all contain each other to the ultimate vt name
  // target. This list excludes the target itself. This means "empty" means
  // `this` is the direct parent of the target group.
  Vector<AtomicString> BuildChainFromThisToNestedGroup(
      const AtomicString& target);

  // Logs the view transition subtree starting from the ::view-transition()
  // element.
  void LogSubtree() { LogSubtree(this); }

 private:
  // Logs the view transition subtree starting from this element.
  void LogSubtree(PseudoElement*, int indent = 0);
};

template <>
struct DowncastTraits<ViewTransitionTransitionElement> {
  static bool AllowFrom(const Node& node) {
    return node.GetPseudoId() == kPseudoIdViewTransition;
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_VIEW_TRANSITION_VIEW_TRANSITION_TRANSITION_ELEMENT_H_
