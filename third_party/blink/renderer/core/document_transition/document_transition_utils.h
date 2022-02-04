// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_DOCUMENT_TRANSITION_DOCUMENT_TRANSITION_UTILS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_DOCUMENT_TRANSITION_DOCUMENT_TRANSITION_UTILS_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/pseudo_element.h"
#include "third_party/blink/renderer/core/style/computed_style_constants.h"

namespace blink {

class CORE_EXPORT DocumentTransitionUtils {
 public:
  template <typename Functor>
  static void ForEachTransitionPseudo(Document& document, Functor& func) {
    auto* transition_pseudo =
        document.documentElement()->GetPseudoElement(kPseudoIdTransition);
    if (!transition_pseudo)
      return;

    func(transition_pseudo);

    for (const auto& document_transition_tag :
         document.GetStyleEngine().DocumentTransitionTags()) {
      auto* container_pseudo = transition_pseudo->GetPseudoElement(
          kPseudoIdTransitionContainer, document_transition_tag);
      if (!container_pseudo)
        continue;

      func(container_pseudo);

      if (auto* content = container_pseudo->GetPseudoElement(
              kPseudoIdTransitionOldContent, document_transition_tag))
        func(content);

      if (auto* content = container_pseudo->GetPseudoElement(
              kPseudoIdTransitionNewContent, document_transition_tag))
        func(content);
    }
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_DOCUMENT_TRANSITION_DOCUMENT_TRANSITION_UTILS_H_
