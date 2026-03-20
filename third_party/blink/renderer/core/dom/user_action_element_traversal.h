// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_DOM_USER_ACTION_ELEMENT_TRAVERSAL_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_DOM_USER_ACTION_ELEMENT_TRAVERSAL_H_

#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/flat_tree_traversal.h"
#include "third_party/blink/renderer/core/html/forms/html_select_element.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

// Traversing the DOM tree for the purpose of propagating user action
// pseudo-classes (like :hover, :active, and :focus-within) requires stopping
// at the first top-layer element encountered.
// See: https://www.w3.org/TR/selectors-4/#useraction-pseudos
class UserActionElementTraversal {
 public:
  using Traversal = FlatTreeTraversal;
  using TraversalNodeType = Element;
  static TraversalNodeType* Next(const TraversalNodeType& element) {
    if (RuntimeEnabledFeatures::UserActionPseudosStopAtTopLayerEnabled()) {
      if (element.IsInTopLayer()) {
        return nullptr;
      }
    } else {
      if (HTMLSelectElement::IsPopoverPickerElement(&element)) {
        return nullptr;
      }
    }
    return Traversal::ParentElement(element);
  }
};

// Returns the parent node in the flat tree, but stops (returns nullptr) if the
// element itself is in the top layer. This prevents propagating focus, hover,
// and active states from a top-layer element (like a modal dialog, popover, or
// customizable select picker) out into the rest of the document.
inline ContainerNode* UserActionElementParent(const Node& node) {
  if (const auto* element = DynamicTo<Element>(node)) {
    if (RuntimeEnabledFeatures::UserActionPseudosStopAtTopLayerEnabled()) {
      if (element->IsInTopLayer()) {
        return nullptr;
      }
    } else {
      if (HTMLSelectElement::IsPopoverPickerElement(element)) {
        return nullptr;
      }
    }
  }
  return FlatTreeTraversal::ParentElement(node);
}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_DOM_USER_ACTION_ELEMENT_TRAVERSAL_H_
