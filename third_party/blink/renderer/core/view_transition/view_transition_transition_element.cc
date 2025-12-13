// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/view_transition/view_transition_transition_element.h"

#include "third_party/blink/renderer/core/style/computed_style_constants.h"
#include "third_party/blink/renderer/core/view_transition/view_transition_pseudo_element_base.h"
#include "third_party/blink/renderer/core/view_transition/view_transition_style_tracker.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {
void ViewTransitionTransitionElement::LogSubtree(PseudoElement* node,
                                                 int indent) {
  if (!node) {
    return;
  }
  VLOG(2) << std::string(indent, ' ') << node->DebugName();
  LogSubtree(node->GetPseudoElement(PseudoId::kPseudoIdViewTransitionImagePair,
                                    node->GetPseudoArgument()),
             indent + 2);
  LogSubtree(node->GetPseudoElement(PseudoId::kPseudoIdViewTransitionOld,
                                    node->GetPseudoArgument()),
             indent + 2);
  LogSubtree(node->GetPseudoElement(PseudoId::kPseudoIdViewTransitionNew,
                                    node->GetPseudoArgument()),
             indent + 2);
  LogSubtree(
      node->GetPseudoElement(PseudoId::kPseudoIdViewTransitionGroupChildren,
                             node->GetPseudoArgument()),
      indent + 2);
  for (auto& name : style_tracker_->GetViewTransitionNames()) {
    LogSubtree(
        node->GetPseudoElement(PseudoId::kPseudoIdViewTransitionGroup, name),
        indent + 2);
  }
}

ViewTransitionTransitionElement::ViewTransitionTransitionElement(
    Element* parent,
    const ViewTransitionStyleTracker* style_tracker)
    : ViewTransitionPseudoElementBase(parent,
                                      PseudoId::kPseudoIdViewTransition,
                                      g_null_atom,
                                      /*is_generated_name=*/false,
                                      style_tracker) {}

PseudoElement*
ViewTransitionTransitionElement::FindViewTransitionGroupPseudoElement(
    const AtomicString& name) {
  // First try to get the pseudo directly. This will work in all cases without
  // nesting. Note that this is a load bearing optimization, since the parent
  // search below uses the up to date containing group structure, which may
  // not match the actual tree used during capture (where everything is
  // flattened). This is required to invalidate the groups correctly.
  if (auto* target =
          GetPseudoElement(PseudoId::kPseudoIdViewTransitionGroup, name)) {
    return target;
  }

  PseudoElement* parent = FindViewTransitionGroupPseudoElementParent(name);
  if (!parent) {
    return nullptr;
  }
  if (parent != this) {
    parent =
        parent->GetPseudoElement(PseudoId::kPseudoIdViewTransitionGroupChildren,
                                 parent->GetPseudoArgument());
  }
  return parent ? parent->GetPseudoElement(
                      PseudoId::kPseudoIdViewTransitionGroup, name)
                : nullptr;
}

PseudoElement*
ViewTransitionTransitionElement::FindViewTransitionGroupPseudoElementParent(
    const AtomicString& name) {
  Vector<AtomicString> chain = BuildChainFromThisToNestedGroup(name);
  PseudoElement* current = this;
  while (current && !chain.empty()) {
    AtomicString next = chain.back();
    chain.pop_back();

    // Note that if this is called via FindViewTransitionGroupPseudoElement then
    // we may not have a proper chain constructed yet, so we will return
    // nullptr.
    current =
        current->GetPseudoElement(PseudoId::kPseudoIdViewTransitionGroup, next);
    if (!current) {
      break;
    }

    // We need to go deeper, so we should have a nested groups element.
    if (!chain.empty()) {
      current = current->GetPseudoElement(
          PseudoId::kPseudoIdViewTransitionGroupChildren, next);
    }
  }
  return current;
}

Vector<AtomicString>
ViewTransitionTransitionElement::BuildChainFromThisToNestedGroup(
    const AtomicString& target) {
  Vector<AtomicString> result;
  AtomicString containing_group_name =
      style_tracker_->GetContainingGroupName(target);
  while (containing_group_name) {
    result.push_back(containing_group_name);
    containing_group_name =
        style_tracker_->GetContainingGroupName(containing_group_name);
  }
  return result;
}

}  // namespace blink
