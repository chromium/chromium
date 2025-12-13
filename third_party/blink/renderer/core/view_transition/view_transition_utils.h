// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_VIEW_TRANSITION_VIEW_TRANSITION_UTILS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_VIEW_TRANSITION_VIEW_TRANSITION_UTILS_H_

#include "base/functional/function_ref.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/style/computed_style_constants.h"
#include "third_party/blink/renderer/core/view_transition/view_transition_request_forward.h"
#include "third_party/blink/renderer/platform/heap/heap_traits.h"

namespace blink {

class Document;
class Element;
class LayoutObject;
class Node;
class PseudoElement;
class ViewTransition;

class CORE_EXPORT ViewTransitionUtils {
 public:
  // Scope class used during getComputedStyle to ensure we don't expose internal
  // pseudo-elements before the start phase on the transition.
  class GetPropertyCSSValueScope {
    STACK_ALLOCATED();

   public:
    GetPropertyCSSValueScope(Document& document, PseudoId pseudo_id);
    ~GetPropertyCSSValueScope();

   private:
    Document& document_;
    PseudoId pseudo_id_;
  };

  using PseudoFunctor = base::FunctionRef<void(PseudoElement*)>;
  using PseudoPredicate = base::FunctionRef<bool(PseudoElement*)>;

  enum class Filter { kDirectChildren, kDescendants };

  static void ForEachTransitionPseudo(const Element&, PseudoFunctor, Filter);
  static PseudoElement* FindPseudoIf(const Element&, PseudoPredicate);

  // Returns the view transition in-progress in the given document, if one
  // exists.
  static ViewTransition* GetTransition(const Document& document);

  static ViewTransition* GetTransition(const Element& element);

  static ViewTransition* GetTransition(const Node& node);

  // Returns the view transition that the element associated with the specified
  // layout object is participating in, if one exists.
  static ViewTransition* TransitionForTaggedElement(const LayoutObject&);

  // Calls the supplied function for every active transition (document-level or
  // element-scoped).
  // Note: making this a function template blows up compile size.
  // TODO(crbug.com/394052227): Consider converting other ForEach* methods in
  // this class to take base::FunctionRef instead of being templates.
  static void ForEachTransition(const Document& document,
                                base::FunctionRef<void(ViewTransition&)>);

  // Return the incoming cross-document view transition, if one exists.
  static ViewTransition* GetIncomingCrossDocumentTransition(
      const Document& document);

  // Return the outgoing cross-document view transition, if one exists.
  static ViewTransition* GetOutgoingCrossDocumentTransition(
      const Document& document);

  // Returns any queued view transition requests.
  static VectorOf<std::unique_ptr<ViewTransitionRequest>> GetPendingRequests(
      const Document& document);

  // Returns true if the given layout object corresponds to the root
  // ::view-transition pseudo-element of a view transition hierarchy.
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

  // Called when the lifecycle will update style and layout tree for the given
  // document. Used to invalidate pseudo styles if necessary.
  static void WillUpdateStyleAndLayoutTree(Document& document);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_VIEW_TRANSITION_VIEW_TRANSITION_UTILS_H_
