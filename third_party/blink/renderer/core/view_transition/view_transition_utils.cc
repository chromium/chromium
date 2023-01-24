// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/view_transition/view_transition_utils.h"

#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/view_transition/view_transition.h"
#include "third_party/blink/renderer/core/view_transition/view_transition_supplement.h"

namespace blink {

// static
ViewTransition* ViewTransitionUtils::GetActiveTransition(
    const Document& document) {
  auto* supplement = ViewTransitionSupplement::FromIfExists(document);
  if (!supplement) {
    return nullptr;
  }
  auto* transition = supplement->GetActiveTransition();
  if (!transition || transition->IsDone()) {
    return nullptr;
  }
  return transition;
}

// static
PseudoElement* ViewTransitionUtils::GetRootPseudo(const Document& document) {
  if (!document.documentElement()) {
    return nullptr;
  }

  PseudoElement* view_transition_pseudo =
      document.documentElement()->GetPseudoElement(kPseudoIdViewTransition);
  DCHECK(!view_transition_pseudo || GetActiveTransition(document));
  return view_transition_pseudo;
}

// static
VectorOf<std::unique_ptr<ViewTransitionRequest>>
ViewTransitionUtils::GetPendingRequests(const Document& document) {
  auto* supplement = ViewTransitionSupplement::FromIfExists(document);
  if (supplement) {
    return supplement->TakePendingRequests();
  }
  return {};
}

// static
bool ViewTransitionUtils::IsViewTransitionRoot(const LayoutObject& object) {
  return object.GetNode() &&
         object.GetNode()->GetPseudoId() == kPseudoIdViewTransition;
}

}  // namespace blink
