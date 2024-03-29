// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/forms/html_selected_option_element.h"

#include "third_party/blink/renderer/core/dom/events/mutation_event_suppression_scope.h"
#include "third_party/blink/renderer/core/html/forms/html_option_element.h"
#include "third_party/blink/renderer/core/html/forms/html_select_element.h"

namespace blink {

SelectedOptionMutationObserver::SelectedOptionMutationObserver(
    HTMLSelectedOptionElement* selectedoption)
    : selectedoption_(selectedoption) {
  SetDocument(&selectedoption->GetDocument());
}

void SelectedOptionMutationObserver::ChildrenOrAttributeChanged(
    const ContainerNode& node) {
  HTMLOptionElement* selected_option_element =
      selectedoption_->SelectedOptionElement();
  if (!selected_option_element) {
    return;
  }

  // TODO(crbug.com/1511354): Calling IsDescendantOf here for all DOM mutations
  // in the entire document is too slow. See comment in the header.
  if (node != selected_option_element &&
      !node.IsDescendantOf(selected_option_element)) {
    return;
  }

  selectedoption_->CloneContentsFromOptionElement();
}

void SelectedOptionMutationObserver::DidChangeChildren(
    const ContainerNode& container,
    const ContainerNode::ChildrenChange& change) {
  ChildrenOrAttributeChanged(container);
}

void SelectedOptionMutationObserver::AttributeChanged(
    const Element& target,
    const QualifiedName& name,
    const AtomicString& old_value,
    const AtomicString& new_value) {
  ChildrenOrAttributeChanged(target);
}

void SelectedOptionMutationObserver::Trace(Visitor* visitor) const {
  SynchronousMutationObserver::Trace(visitor);
  visitor->Trace(selectedoption_);
}

HTMLSelectedOptionElement::HTMLSelectedOptionElement(Document& document)
    : HTMLElement(html_names::kSelectedoptionTag, document) {
  CHECK(RuntimeEnabledFeatures::StylableSelectEnabled() ||
        RuntimeEnabledFeatures::HTMLSelectListElementEnabled());
}

HTMLSelectElement* HTMLSelectedOptionElement::FirstAncestorSelectElement()
    const {
  return first_ancestor_select_element_;
}

void HTMLSelectedOptionElement::UpdateFirstAncestorSelectElement() {
  HTMLSelectElement* new_ancestor = nullptr;
  for (auto& ancestor : NodeTraversal::AncestorsOf(*this)) {
    if (auto* select = DynamicTo<HTMLSelectElement>(ancestor)) {
      new_ancestor = select;
      break;
    }
  }

  if (first_ancestor_select_element_ != new_ancestor) {
    if (first_ancestor_select_element_) {
      first_ancestor_select_element_->SelectedOptionElementRemoved(this);
    }
    first_ancestor_select_element_ = new_ancestor;
    if (first_ancestor_select_element_) {
      first_ancestor_select_element_->SelectedOptionElementInserted(this);
      selected_option_element_ =
          first_ancestor_select_element_->SelectedOption();
      if (!mutation_observer_) {
        mutation_observer_ =
            MakeGarbageCollected<SelectedOptionMutationObserver>(this);
      }
    } else {
      if (mutation_observer_) {
        mutation_observer_->SetDocument(nullptr);
      }
      mutation_observer_ = nullptr;
      selected_option_element_ = nullptr;
    }
    CloneContentsFromOptionElement();
  }
}

void HTMLSelectedOptionElement::SelectedOptionElementChanged(
    HTMLOptionElement* option) {
  if (selected_option_element_ == option) {
    return;
  }
  selected_option_element_ = option;
  CloneContentsFromOptionElement();
}

void HTMLSelectedOptionElement::CloneContentsFromOptionElement() {
  MutationEventSuppressionScope dont_fire_mutation_events(GetDocument());

  VectorOf<Node> nodes;
  if (selected_option_element_) {
    for (Node& child : NodeTraversal::ChildrenOf(*selected_option_element_)) {
      nodes.push_back(child.cloneNode(/*deep=*/true));
    }
  }
  ReplaceChildren(nodes, ASSERT_NO_EXCEPTION);
}

Node::InsertionNotificationRequest HTMLSelectedOptionElement::InsertedInto(
    ContainerNode& container) {
  Node::InsertionNotificationRequest return_value =
      HTMLElement::InsertedInto(container);
  UpdateFirstAncestorSelectElement();
  return return_value;
}

void HTMLSelectedOptionElement::RemovedFrom(ContainerNode& container) {
  HTMLElement::RemovedFrom(container);
  UpdateFirstAncestorSelectElement();
}

void HTMLSelectedOptionElement::Trace(Visitor* visitor) const {
  HTMLElement::Trace(visitor);
  visitor->Trace(first_ancestor_select_element_);
  visitor->Trace(selected_option_element_);
  visitor->Trace(mutation_observer_);
}

}  // namespace blink
