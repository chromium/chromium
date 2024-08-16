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
#include "third_party/blink/renderer/core/view_transition/view_transition_request_forward.h"
#include "third_party/blink/renderer/core/view_transition/view_transition_transition_element.h"

namespace blink {

class DOMViewTransition;
class ViewTransition;

class CORE_EXPORT ViewTransitionUtils {
 public:
  template <typename Functor>
  static void ForEachTransitionPseudo(Document& document, Functor& func) {
    if (!document.documentElement()) {
      return;
    }

    auto* transition_pseudo =
        document.documentElement()->GetPseudoElement(kPseudoIdViewTransition);
    if (!transition_pseudo)
      return;

    func(transition_pseudo);

    for (const auto& view_transition_name :
         document.GetStyleEngine().ViewTransitionTags()) {
      auto* container_pseudo =
          To<ViewTransitionTransitionElement>(transition_pseudo)
              ->FindViewTransitionGroupPseudoElement(view_transition_name);
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
    if (!document.documentElement()) {
      return nullptr;
    }

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
      auto* container_pseudo =
          To<ViewTransitionTransitionElement>(transition_pseudo)
              ->FindViewTransitionGroupPseudoElement(view_transition_name);
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

  template <typename Functor>
  static void ForEachDirectTransitionPseudo(const Element* element,
                                            Functor& func) {
    if (element->IsDocumentElement()) {
      if (auto* pseudo = element->GetPseudoElement(kPseudoIdViewTransition)) {
        func(pseudo);
      }
      return;
    }

    if (!IsTransitionPseudoElement(element->GetPseudoId())) {
      return;
    }

    switch (element->GetPseudoId()) {
      case kPseudoIdViewTransition:
        for (auto name :
             element->GetDocument().GetStyleEngine().ViewTransitionTags()) {
          if (auto* pseudo = element->GetPseudoElement(
                  kPseudoIdViewTransitionGroup, name)) {
            func(pseudo);
          }
        }
        break;
      case kPseudoIdViewTransitionGroup:
        if (auto* pseudo =
                element->GetPseudoElement(kPseudoIdViewTransitionImagePair)) {
          func(pseudo);
        }
        break;
      case kPseudoIdViewTransitionImagePair:
        if (auto* pseudo =
                element->GetPseudoElement(kPseudoIdViewTransitionOld)) {
          func(pseudo);
        }
        if (auto* pseudo =
                element->GetPseudoElement(kPseudoIdViewTransitionNew)) {
          func(pseudo);
        }
        break;
      case kPseudoIdViewTransitionOld:
      case kPseudoIdViewTransitionNew:
        break;
      default:
        NOTREACHED_IN_MIGRATION();
    }
  }

  // Returns the view transition in-progress in the given document, if one
  // exists.
  static ViewTransition* GetTransition(const Document& document);

  // Return the incoming cross-document view transition, if one exists.
  static ViewTransition* GetIncomingCrossDocumentTransition(
      const Document& document);

  // Return the outgoing cross-document view transition, if one exists.
  static ViewTransition* GetOutgoingCrossDocumentTransition(
      const Document& document);

  // If the given document has an in-progress view transition, this will return
  // the script delegate associated with that view transition (which may be
  // null).
  static DOMViewTransition* GetTransitionScriptDelegate(
      const Document& document);

  // Returns the ::view-transition pseudo element that is the root of the
  // view-transition DOM hierarchy.
  static PseudoElement* GetRootPseudo(const Document& document);

  // Returns any queued view transition requests.
  static VectorOf<std::unique_ptr<ViewTransitionRequest>> GetPendingRequests(
      const Document& document);

  // Returns true if the given layout object corresponds to the root
  // ::view-transition pseudo element of a view transition hierarchy.
  static bool IsViewTransitionRoot(const LayoutObject& object);

  // Returns true if this element is a view transition participant. This is a
  // slow check that walks all of the view transition elements in the
  // ViewTransitionStyleTracker.
  static bool IsViewTransitionElementExcludingRootFromSupplement(
      const Element& element);

  // Returns true if this object represents an element that is a view transition
  // participant. This is a slow check that walks all of the view transition
  // elements in the ViewTransitionStyleTracker.
  static bool IsViewTransitionParticipantFromSupplement(
      const LayoutObject& object);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_VIEW_TRANSITION_VIEW_TRANSITION_UTILS_H_
