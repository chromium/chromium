// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/view_transition/view_transition_utils.h"

#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/dom/pseudo_element.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/style/computed_style_constants.h"
#include "third_party/blink/renderer/core/view_transition/view_transition.h"
#include "third_party/blink/renderer/core/view_transition/view_transition_supplement.h"
#include "third_party/blink/renderer/core/view_transition/view_transition_transition_element.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

// static
void ViewTransitionUtils::ForEachTransitionPseudo(const Element& element,
                                                  PseudoFunctor func,
                                                  Filter filter) {
  if (!element.IsPseudoElement()) {
    if (PseudoElement* transition_element =
            element.GetPseudoElement(kPseudoIdViewTransition)) {
      func(transition_element);
      if (filter == Filter::kDescendants) {
        ForEachTransitionPseudo(*transition_element, func, filter);
      }
    }
    return;
  }

  if (!IsTransitionPseudoElement(element.GetPseudoId())) {
    return;
  }

  const ViewTransitionPseudoElementBase& transition_pseudo =
      To<ViewTransitionPseudoElementBase>(element);
  const AtomicString& self_name = transition_pseudo.view_transition_name();

  switch (transition_pseudo.GetPseudoId()) {
    case kPseudoIdViewTransition:
    case kPseudoIdViewTransitionGroupChildren:
      for (const AtomicString& name :
           transition_pseudo.GetViewTransitionNames()) {
        if (PseudoElement* group = transition_pseudo.GetPseudoElement(
                kPseudoIdViewTransitionGroup, name)) {
          func(group);
          if (filter == Filter::kDescendants) {
            ForEachTransitionPseudo(*group, func, filter);
          }
        }
      }
      break;

    case kPseudoIdViewTransitionGroup:
      if (PseudoElement* image_pair = transition_pseudo.GetPseudoElement(
              kPseudoIdViewTransitionImagePair, self_name)) {
        func(image_pair);
        if (filter == Filter::kDescendants) {
          ForEachTransitionPseudo(*image_pair, func, filter);
        }
      }
      if (PseudoElement* group_children = transition_pseudo.GetPseudoElement(
              kPseudoIdViewTransitionGroupChildren, self_name)) {
        func(group_children);
        if (filter == Filter::kDescendants) {
          ForEachTransitionPseudo(*group_children, func, filter);
        }
      }
      break;

    case kPseudoIdViewTransitionImagePair:
      if (PseudoElement* old_image = transition_pseudo.GetPseudoElement(
              kPseudoIdViewTransitionOld, self_name)) {
        func(old_image);
      }
      if (PseudoElement* new_image = transition_pseudo.GetPseudoElement(
              kPseudoIdViewTransitionNew, self_name)) {
        func(new_image);
      }
      break;

    case kPseudoIdViewTransitionOld:
    case kPseudoIdViewTransitionNew:
      break;

    default:
      NOTREACHED();
  }
}

// static
PseudoElement* ViewTransitionUtils::FindPseudoIf(const Element& element,
                                                 PseudoPredicate condition) {
  auto* transition_pseudo = element.GetPseudoElement(kPseudoIdViewTransition);
  if (!transition_pseudo) {
    return nullptr;
  }
  if (condition(transition_pseudo)) {
    return transition_pseudo;
  }

  for (const auto& view_transition_name :
       To<ViewTransitionPseudoElementBase>(transition_pseudo)
           ->GetViewTransitionNames()) {
    auto* container_pseudo =
        To<ViewTransitionTransitionElement>(transition_pseudo)
            ->FindViewTransitionGroupPseudoElement(view_transition_name);
    if (!container_pseudo) {
      continue;
    }
    if (condition(container_pseudo)) {
      return container_pseudo;
    }

    if (auto* nested_groups = container_pseudo->GetPseudoElement(
            kPseudoIdViewTransitionGroupChildren, view_transition_name);
        nested_groups && condition(nested_groups)) {
      return nested_groups;
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

// static
ViewTransition* ViewTransitionUtils::GetTransition(const Document& document) {
  auto* supplement = document.GetViewTransitionsIfExists();
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
ViewTransition* ViewTransitionUtils::GetTransition(const Element& element) {
  auto* supplement = element.GetDocument().GetViewTransitionsIfExists();
  if (!supplement) {
    return nullptr;
  }
  ViewTransition* transition = supplement->GetTransition(element);
  if (!transition || transition->IsDone()) {
    return nullptr;
  }
  return transition;
}

ViewTransition* ViewTransitionUtils::GetTransition(const Node& node) {
  if (node.IsElementNode()) {
    return GetTransition(To<Element>(node));
  }
  return GetTransition(node.GetDocument());
}

ViewTransition* ViewTransitionUtils::TransitionForTaggedElement(
    const LayoutObject& layout_object) {
  ViewTransition* result = nullptr;
  // Note: an element can't participate in more than one transition at the same
  // time. There may be skipped transitions still waiting for the DOM callback
  // to run, but those should return false from NeedsViewTransitionEffectNode.
  ForEachTransition(
      layout_object.GetDocument(), [&](ViewTransition& transition) {
        if (transition.NeedsViewTransitionEffectNode(layout_object)) {
          result = &transition;
        }
      });
  return result;
}

// static
void ViewTransitionUtils::ForEachTransition(
    const Document& document,
    base::FunctionRef<void(ViewTransition&)> function) {
  if (auto* supplement = document.GetViewTransitionsIfExists()) {
    supplement->ForEachTransition(function);
  }
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
VectorOf<std::unique_ptr<ViewTransitionRequest>>
ViewTransitionUtils::GetPendingRequests(const Document& document) {
  auto* supplement = document.GetViewTransitionsIfExists();
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

ViewTransitionUtils::GetPropertyCSSValueScope::GetPropertyCSSValueScope(
    Document& document,
    PseudoId pseudo_id)
    : document_(document), pseudo_id_(pseudo_id) {
  if (!IsTransitionPseudoElement(pseudo_id_)) {
    return;
  }

  if (auto* supplement = document_.GetViewTransitionsIfExists()) {
    supplement->WillEnterGetComputedStyleScope();
  }
}

ViewTransitionUtils::GetPropertyCSSValueScope::~GetPropertyCSSValueScope() {
  if (!IsTransitionPseudoElement(pseudo_id_)) {
    return;
  }

  if (auto* supplement = document_.GetViewTransitionsIfExists()) {
    supplement->WillExitGetComputedStyleScope();
  }
}

void ViewTransitionUtils::WillUpdateStyleAndLayoutTree(Document& document) {
  if (auto* supplement = document.GetViewTransitionsIfExists()) {
    supplement->WillUpdateStyleAndLayoutTree();
  }
}

}  // namespace blink
