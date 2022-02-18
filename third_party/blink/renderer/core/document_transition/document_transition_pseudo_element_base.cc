// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/document_transition/document_transition_pseudo_element_base.h"

namespace blink {

DocumentTransitionPseudoElementBase::DocumentTransitionPseudoElementBase(
    Element* parent,
    PseudoId pseudo_id,
    const AtomicString& document_transition_tag)
    : PseudoElement(parent, pseudo_id, document_transition_tag) {
  DCHECK(IsTransitionPseudoElement(pseudo_id));
  DCHECK(pseudo_id == kPseudoIdPageTransition || document_transition_tag);
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

}  // namespace blink
