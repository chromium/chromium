// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/view_transition/view_transition_utils.h"

#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/view_transition/view_transition.h"
#include "third_party/blink/renderer/core/view_transition/view_transition_supplement.h"

namespace blink {

// static
ViewTransition* ViewTransitionUtils::GetTransition(const Document& document) {
  auto* supplement = ViewTransitionSupplement::FromIfExists(document);
  if (!supplement) {
    return nullptr;
  }
  ViewTransition* transition = supplement->GetTransition();
  if (!transition || transition->IsDone()) {
    return nullptr;
  }
  return transition;
}

// static
ViewTransition* ViewTransitionUtils::GetIncomingCrossDocumentTransition(
    const Document& document) {
  if (auto* transition = GetTransition(document);
      transition && transition->IsForNavigationOnNewDocument()) {
    return transition;
  }
  return nullptr;
}

// static
ViewTransition* ViewTransitionUtils::GetOutgoingCrossDocumentTransition(
    const Document& document) {
  if (auto* transition = GetTransition(document);
      transition && transition->IsForNavigationSnapshot()) {
    return transition;
  }
  return nullptr;
}

// static
DOMViewTransition* ViewTransitionUtils::GetTransitionScriptDelegate(
    const Document& document) {
  ViewTransition* view_transition =
      ViewTransitionUtils::GetTransition(document);
  if (!view_transition) {
    return nullptr;
  }

  return view_transition->GetScriptDelegate();
}

// static
PseudoElement* ViewTransitionUtils::GetRootPseudo(const Document& document) {
  if (!document.documentElement()) {
    return nullptr;
  }

  PseudoElement* view_transition_pseudo =
      document.documentElement()->GetPseudoElement(kPseudoIdViewTransition);
  DCHECK(!view_transition_pseudo || GetTransition(document));
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

// static
bool ViewTransitionUtils::IsViewTransitionElementExcludingRootFromSupplement(
    const Element& element) {
  ViewTransition* transition = GetTransition(element.GetDocument());
  return transition && transition->IsTransitionElementExcludingRoot(element);
}

// static
bool ViewTransitionUtils::IsViewTransitionParticipantFromSupplement(
    const LayoutObject& object) {
  ViewTransition* transition = GetTransition(object.GetDocument());
  return transition && transition->IsRepresentedViaPseudoElements(object);
}

}  // namespace blink
