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
  DCHECK(pseudo_id == kPseudoIdTransition || document_transition_tag);
}

bool DocumentTransitionPseudoElementBase::CanGeneratePseudoElement(
    PseudoId pseudo_id) const {
  switch (GetPseudoId()) {
    case kPseudoIdTransition:
      return pseudo_id == kPseudoIdTransitionContainer;
    case kPseudoIdTransitionContainer:
      return pseudo_id == kPseudoIdTransitionOldContent ||
             pseudo_id == kPseudoIdTransitionNewContent;
    case kPseudoIdTransitionOldContent:
    case kPseudoIdTransitionNewContent:
      return false;
    default:
      NOTREACHED();
      return false;
  }
}

}  // namespace blink
