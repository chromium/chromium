// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/view_transition/view_transition_transition_element.h"

#include "third_party/blink/renderer/core/style/computed_style_constants.h"
#include "third_party/blink/renderer/core/view_transition/view_transition_pseudo_element_base.h"
#include "third_party/blink/renderer/core/view_transition/view_transition_style_tracker.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {
ViewTransitionTransitionElement::ViewTransitionTransitionElement(
    Element* parent,
    const ViewTransitionStyleTracker* style_tracker)
    : ViewTransitionPseudoElementBase(parent,
                                      PseudoId::kPseudoIdViewTransition,
                                      g_null_atom,
                                      style_tracker) {}

PseudoElement*
ViewTransitionTransitionElement::FindViewTransitionGroupPseudoElement(
    const AtomicString& view_transition_name) {
  auto* parent =
      FindViewTransitionGroupPseudoElementParent(view_transition_name);
  if (!parent) {
    return nullptr;
  }

  return parent->GetPseudoElement(PseudoId::kPseudoIdViewTransitionGroup,
                                  view_transition_name);
}

PseudoElement*
ViewTransitionTransitionElement::FindViewTransitionGroupPseudoElementParent(
    const AtomicString& view_transition_name) {
  AtomicString containing_group_name =
      style_tracker_->GetContainingGroupName(view_transition_name);
  return containing_group_name
             ? FindViewTransitionGroupPseudoElement(containing_group_name)
             : this;
}

}  // namespace blink
