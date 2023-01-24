// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_VIEW_TRANSITION_VIEW_TRANSITION_UTILS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_VIEW_TRANSITION_VIEW_TRANSITION_UTILS_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/pseudo_element.h"
#include "third_party/blink/renderer/core/style/computed_style_constants.h"
#include "third_party/blink/renderer/core/view_transition/view_transition.h"

namespace blink {

class CORE_EXPORT ViewTransitionUtils {
 public:
  template <typename Functor>
  static void ForEachTransitionPseudo(Document& document, Functor& func) {
    auto* transition_pseudo =
        document.documentElement()->GetPseudoElement(kPseudoIdViewTransition);
    if (!transition_pseudo)
      return;

    func(transition_pseudo);

    for (const auto& view_transition_name :
         document.GetStyleEngine().ViewTransitionTags()) {
      auto* container_pseudo = transition_pseudo->GetPseudoElement(
          kPseudoIdViewTransitionGroup, view_transition_name);
      if (!container_pseudo)
        continue;

      func(container_pseudo);

      auto* wrapper_pseudo = container_pseudo->GetPseudoElement(
          kPseudoIdViewTransitionImagePair, view_transition_name);
      if (!wrapper_pseudo)
        continue;

      func(wrapper_pseudo);

      if (auto* content = wrapper_pseudo->GetPseudoElement(
              kPseudoIdViewTransitionOld, view_transition_name)) {
        func(content);
      }

      if (auto* content = wrapper_pseudo->GetPseudoElement(
              kPseudoIdViewTransitionNew, view_transition_name)) {
        func(content);
      }
    }
  }

  template <typename Functor>
  static PseudoElement* FindPseudoIf(const Document& document,
                                     const Functor& condition) {
    auto* transition_pseudo =
        document.documentElement()->GetPseudoElement(kPseudoIdViewTransition);
    if (!transition_pseudo) {
      return nullptr;
    }
    if (condition(transition_pseudo)) {
      return transition_pseudo;
    }

    for (const auto& view_transition_name :
         document.GetStyleEngine().ViewTransitionTags()) {
      auto* container_pseudo = transition_pseudo->GetPseudoElement(
          kPseudoIdViewTransitionGroup, view_transition_name);
      if (!container_pseudo) {
        continue;
      }
      if (condition(container_pseudo)) {
        return container_pseudo;
      }

      auto* wrapper_pseudo = container_pseudo->GetPseudoElement(
          kPseudoIdViewTransitionImagePair, view_transition_name);
      if (!wrapper_pseudo) {
        continue;
      }
      if (condition(wrapper_pseudo)) {
        return wrapper_pseudo;
      }

      if (auto* content = wrapper_pseudo->GetPseudoElement(
              kPseudoIdViewTransitionOld, view_transition_name);
          content && condition(content)) {
        return content;
      }

      if (auto* content = wrapper_pseudo->GetPseudoElement(
              kPseudoIdViewTransitionNew, view_transition_name);
          content && condition(content)) {
        return content;
      }
    }

    return nullptr;
  }

  // Returns the active transition from the document, if any.
  static ViewTransition* GetActiveTransition(const Document& document);

  // Returns the ::view-transition pseudo element that is the root of the
  // view-transition DOM hierarchy.
  static PseudoElement* GetRootPseudo(const Document& document);

  // Returns any queued view transition requests.
  static VectorOf<std::unique_ptr<ViewTransitionRequest>> GetPendingRequests(
      const Document& document);

  // Returns true if the given layout object corresponds to the root
  // ::view-transition pseudo element of a view transition hierarchy.
  static bool IsViewTransitionRoot(const LayoutObject& object);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_VIEW_TRANSITION_VIEW_TRANSITION_UTILS_H_
