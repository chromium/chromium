// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/document_transition/document_transition_pseudo_element_base.h"

#include "third_party/blink/renderer/core/document_transition/document_transition_style_tracker.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/style/computed_style.h"

namespace blink {

DocumentTransitionPseudoElementBase::DocumentTransitionPseudoElementBase(
    Element* parent,
    PseudoId pseudo_id,
    const AtomicString& document_transition_tag,
    const DocumentTransitionStyleTracker* style_tracker)
    : PseudoElement(parent, pseudo_id, document_transition_tag),
      style_tracker_(style_tracker) {
  DCHECK(IsTransitionPseudoElement(pseudo_id));
  DCHECK(pseudo_id == kPseudoIdPageTransition || document_transition_tag);
  DCHECK(style_tracker_);
}

bool DocumentTransitionPseudoElementBase::CanGeneratePseudoElement(
    PseudoId pseudo_id) const {
  switch (GetPseudoId()) {
    case kPseudoIdPageTransition:
      return pseudo_id == kPseudoIdPageTransitionContainer;
    case kPseudoIdPageTransitionContainer:
      return pseudo_id == kPseudoIdPageTransitionImageWrapper;
    case kPseudoIdPageTransitionImageWrapper:
      return pseudo_id == kPseudoIdPageTransitionOutgoingImage ||
             pseudo_id == kPseudoIdPageTransitionIncomingImage;
    case kPseudoIdPageTransitionOutgoingImage:
    case kPseudoIdPageTransitionIncomingImage:
      return false;
    default:
      NOTREACHED();
      return false;
  }
}

scoped_refptr<ComputedStyle>
DocumentTransitionPseudoElementBase::CustomStyleForLayoutObject(
    const StyleRecalcContext& style_recalc_context) {
  Element* parent = ParentOrShadowHostElement();
  auto style_request = StyleRequest(GetPseudoId(), parent->GetComputedStyle(),
                                    document_transition_tag());
  style_request.rules_to_include = style_tracker_->StyleRulesToInclude();
  return parent->StyleForPseudoElement(style_recalc_context, style_request);
}

void DocumentTransitionPseudoElementBase::Trace(Visitor* visitor) const {
  PseudoElement::Trace(visitor);
  visitor->Trace(style_tracker_);
}

}  // namespace blink
