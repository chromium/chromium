// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_DOCUMENT_TRANSITION_DOCUMENT_TRANSITION_UTILS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_DOCUMENT_TRANSITION_DOCUMENT_TRANSITION_UTILS_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/document_transition/document_transition.h"
#include "third_party/blink/renderer/core/document_transition/document_transition_supplement.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/pseudo_element.h"
#include "third_party/blink/renderer/core/style/computed_style_constants.h"

namespace blink {

class CORE_EXPORT DocumentTransitionUtils {
 public:
  template <typename Functor>
  static void ForEachTransitionPseudo(Document& document, Functor& func) {
    auto* transition_pseudo =
        document.documentElement()->GetPseudoElement(kPseudoIdPageTransition);
    if (!transition_pseudo)
      return;

    func(transition_pseudo);

    for (const auto& document_transition_tag :
         document.GetStyleEngine().DocumentTransitionTags()) {
      auto* container_pseudo = transition_pseudo->GetPseudoElement(
          kPseudoIdPageTransitionContainer, document_transition_tag);
      if (!container_pseudo)
        continue;

      func(container_pseudo);

      auto* wrapper_pseudo = container_pseudo->GetPseudoElement(
          kPseudoIdPageTransitionImageWrapper, document_transition_tag);
      if (!wrapper_pseudo)
        continue;

      func(wrapper_pseudo);

      if (auto* content = wrapper_pseudo->GetPseudoElement(
              kPseudoIdPageTransitionOutgoingImage, document_transition_tag)) {
        func(content);
      }

      if (auto* content = wrapper_pseudo->GetPseudoElement(
              kPseudoIdPageTransitionIncomingImage, document_transition_tag)) {
        func(content);
      }
    }
  }

  static DocumentTransition* GetActiveTransition(const Document& document) {
    auto* supplement = DocumentTransitionSupplement::FromIfExists(document);
    if (!supplement)
      return nullptr;
    auto* transition = supplement->GetActiveTransition();
    if (!transition || transition->IsDone())
      return nullptr;
    return transition;
  }

  static VectorOf<std::unique_ptr<DocumentTransitionRequest>>
  GetPendingRequests(const Document& document) {
    auto* supplement = DocumentTransitionSupplement::FromIfExists(document);
    if (supplement)
      return supplement->TakePendingRequests();
    return {};
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_DOCUMENT_TRANSITION_DOCUMENT_TRANSITION_UTILS_H_
