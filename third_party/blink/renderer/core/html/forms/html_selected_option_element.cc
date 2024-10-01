// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/forms/html_selected_option_element.h"

#include "third_party/blink/renderer/core/dom/events/mutation_event_suppression_scope.h"
#include "third_party/blink/renderer/core/html/forms/html_option_element.h"
#include "third_party/blink/renderer/core/html/forms/html_select_element.h"

namespace blink {

HTMLSelectedOptionElement::HTMLSelectedOptionElement(Document& document)
    : HTMLElement(html_names::kSelectedoptionTag, document) {
  CHECK(RuntimeEnabledFeatures::CustomizableSelectEnabled());
}

void HTMLSelectedOptionElement::CloneContentsFromOptionElement(
    const HTMLOptionElement* option) {
  MutationEventSuppressionScope dont_fire_mutation_events(GetDocument());

  VectorOf<Node> nodes;
  if (option) {
    for (Node& child : NodeTraversal::ChildrenOf(*option)) {
      nodes.push_back(child.cloneNode(/*deep=*/true));
    }
  }
  // `ASSERT_NO_EXCEPTION` is safe here because `ReplaceChildren()` only
  // throws exceptions when encountering DOM hierarchy errors, which
  // shouldn't happen here.
  ReplaceChildren(nodes, ASSERT_NO_EXCEPTION);
}

Node::InsertionNotificationRequest HTMLSelectedOptionElement::InsertedInto(
    ContainerNode& insertion_point) {
  // Call SelectedOptionElementInserted on the first ancestor <select> if we
  // just got inserted into a <select> and there are no other <select>s in
  // between.
  // TODO(crbug.com/40236878): Use a flat tree traversal here.
  bool passed_insertion_point = false;
  for (auto* ancestor = parentNode(); ancestor;
       ancestor = ancestor->parentNode()) {
    if (ancestor == insertion_point) {
      passed_insertion_point = true;
    }
    if (auto* select = DynamicTo<HTMLSelectElement>(ancestor)) {
      if (passed_insertion_point) {
        select->SelectedOptionElementInserted(this);
      }
    }
  }
  return HTMLElement::InsertedInto(insertion_point);
}

void HTMLSelectedOptionElement::RemovedFrom(ContainerNode& container) {
  HTMLElement::RemovedFrom(container);
  // Call SelectedOptionElementRemoved on the first ancestor <select> if we just
  // got detached from it.
  if (!Traversal<HTMLSelectElement>::FirstAncestor(*this)) {
    if (auto* select = Traversal<HTMLSelectElement>::FirstAncestor(container)) {
      select->SelectedOptionElementRemoved(this);
    }
  }
}

}  // namespace blink
