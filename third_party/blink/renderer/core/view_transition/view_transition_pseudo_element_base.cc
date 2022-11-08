// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/view_transition/view_transition_pseudo_element_base.h"

#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/view_transition/view_transition_style_tracker.h"

namespace blink {

ViewTransitionPseudoElementBase::ViewTransitionPseudoElementBase(
    Element* parent,
    PseudoId pseudo_id,
    const AtomicString& view_transition_name,
    const ViewTransitionStyleTracker* style_tracker)
    : PseudoElement(parent, pseudo_id, view_transition_name),
      style_tracker_(style_tracker) {
  DCHECK(IsTransitionPseudoElement(pseudo_id));
  DCHECK(pseudo_id == kPseudoIdViewTransition || view_transition_name);
  DCHECK(style_tracker_);
}

bool ViewTransitionPseudoElementBase::CanGeneratePseudoElement(
    PseudoId pseudo_id) const {
  switch (GetPseudoId()) {
    case kPseudoIdViewTransition:
      return pseudo_id == kPseudoIdViewTransitionGroup;
    case kPseudoIdViewTransitionGroup:
      return pseudo_id == kPseudoIdViewTransitionImagePair;
    case kPseudoIdViewTransitionImagePair:
      return pseudo_id == kPseudoIdViewTransitionOld ||
             pseudo_id == kPseudoIdViewTransitionNew;
    case kPseudoIdViewTransitionOld:
    case kPseudoIdViewTransitionNew:
      return false;
    default:
      NOTREACHED();
      return false;
  }
}

scoped_refptr<ComputedStyle>
ViewTransitionPseudoElementBase::CustomStyleForLayoutObject(
    const StyleRecalcContext& style_recalc_context) {
  Element* parent = ParentOrShadowHostElement();
  auto style_request = StyleRequest(GetPseudoId(), parent->GetComputedStyle(),
                                    view_transition_name());
  style_request.rules_to_include = style_tracker_->StyleRulesToInclude();
  return parent->StyleForPseudoElement(style_recalc_context, style_request);
}

void ViewTransitionPseudoElementBase::Trace(Visitor* visitor) const {
  PseudoElement::Trace(visitor);
  visitor->Trace(style_tracker_);
}

}  // namespace blink
