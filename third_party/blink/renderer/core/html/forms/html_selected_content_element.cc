// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/forms/html_selected_content_element.h"

#include "third_party/blink/renderer/core/dom/events/mutation_event_suppression_scope.h"
#include "third_party/blink/renderer/core/html/forms/html_option_element.h"
#include "third_party/blink/renderer/core/html/forms/html_select_element.h"

namespace blink {

HTMLSelectedContentElement::HTMLSelectedContentElement(Document& document)
    : HTMLElement(html_names::kSelectedcontentTag, document) {
  CHECK(RuntimeEnabledFeatures::CustomizableSelectEnabled());
}

void HTMLSelectedContentElement::CloneContentsFromOptionElement(
    const HTMLOptionElement* option) {
  if (disabled_) {
    return;
  }

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

Node::InsertionNotificationRequest HTMLSelectedContentElement::InsertedInto(
    ContainerNode& insertion_point) {
  // Call SelectedContentElementInserted on the first ancestor <select> if we
  // just got inserted into a <select> and there are no other <select>s in
  // between.
  // TODO(crbug.com/40236878): Use a flat tree traversal here.
  disabled_ = false;
  bool passed_insertion_point = false;
  HTMLSelectElement* first_ancestor_select = nullptr;
  for (auto* ancestor = parentNode(); ancestor;
       ancestor = ancestor->parentNode()) {
    if (ancestor == insertion_point) {
      passed_insertion_point = true;
    }
    if (IsA<HTMLOptionElement>(ancestor) ||
        IsA<HTMLSelectedContentElement>(ancestor)) {
      // Putting a <selectedcontent> inside an <option> or another
      // <seletedoption> can lead to infinite loops.
      disabled_ = true;
    }
    if (auto* select = DynamicTo<HTMLSelectElement>(ancestor)) {
      if (first_ancestor_select) {
        // If there are multiple ancestor selects, then cloning can lead to
        // infinite loops, so disable this element.
        disabled_ = true;
      }
      first_ancestor_select = select;
      if (passed_insertion_point) {
        select->SelectedContentElementInserted(this);
      }
    }
  }
  return HTMLElement::InsertedInto(insertion_point);
}

void HTMLSelectedContentElement::RemovedFrom(ContainerNode& container) {
  HTMLElement::RemovedFrom(container);
  // Call SelectedContentElementRemoved on the first ancestor <select> if we
  // just got detached from it.
  if (!Traversal<HTMLSelectElement>::FirstAncestor(*this)) {
    if (auto* select = Traversal<HTMLSelectElement>::FirstAncestor(container)) {
      select->SelectedContentElementRemoved(this);
    }
  }
  disabled_ = false;
}

}  // namespace blink
