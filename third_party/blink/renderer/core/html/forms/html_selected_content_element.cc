// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/forms/html_selected_content_element.h"

#include "third_party/blink/renderer/core/html/forms/html_option_element.h"
#include "third_party/blink/renderer/core/html/forms/html_select_element.h"

namespace blink {

HTMLSelectedContentElement::HTMLSelectedContentElement(Document& document)
    : HTMLElement(html_names::kSelectedcontentTag, document) {}

void HTMLSelectedContentElement::CloneContentsFromOptionElement(
    const HTMLOptionElement* option) {
  // TODO(crbug.com/458113204): This disabled check does not exist in the spec.
  // It should be added to the spec or removed.
  if (disabled_) {
    return;
  }

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
  HTMLElement::InsertedInto(insertion_point);

  if (RuntimeEnabledFeatures::SelectedcontentSpecEnabled()) {
    // We need to call SelectedContentElementInserted in InsertedInto instead of
    // DidNotifySubtreeInsertionsToDocument because
    // DidNotifySubtreeInsertionsToDocument calls other methods which iterate
    // through all descendant selectedcontent elements within the nearest
    // ancestor select element, which we optimize with a TreeOrderedList which
    // gets updated in SelectedContentElementInserted.
    HTMLSelectElement* new_nearest_ancestor_select =
        Traversal<HTMLSelectElement>::FirstAncestor(*this);
    if (nearest_ancestor_select_ != new_nearest_ancestor_select) {
      CHECK(!nearest_ancestor_select_);
      nearest_ancestor_select_ = new_nearest_ancestor_select;
      nearest_ancestor_select_->SelectedContentElementInserted(this);
    }
  }

  return Node::InsertionNotificationRequest::
      kInsertionShouldCallDidNotifySubtreeInsertions;
}

void HTMLSelectedContentElement::DidNotifySubtreeInsertionsToDocument() {
  // Clone from the nearest ancestor select element if this element isn't
  // disabled.
  // TODO(crbug.com/40236878): Use a flat tree traversal here.
  if (RuntimeEnabledFeatures::SelectedcontentSpecEnabled()) {
    // TODO(crbug.com/458113204): Calculate disabled state in InsertedInto
    // instead of this method.
    disabled_ = false;
    HTMLSelectElement* first_ancestor_select = nullptr;
    for (auto* ancestor = parentNode(); ancestor;
         ancestor = ancestor->parentNode()) {
      if (IsA<HTMLOptionElement>(ancestor) ||
          IsA<HTMLSelectedContentElement>(ancestor)) {
        // Putting a <selectedcontent> inside an <option> or another
        // <selectedcontent> can lead to infinite loops.
        disabled_ = true;
        break;
      }
      if (auto* select = DynamicTo<HTMLSelectElement>(ancestor)) {
        if (first_ancestor_select) {
          // If there are multiple ancestor selects, then cloning can lead to
          // infinite loops, so disable this element.
          disabled_ = true;
          break;
        }
        first_ancestor_select = select;
        CHECK_EQ(first_ancestor_select, nearest_ancestor_select_);
      }
    }

    if (!disabled_ && nearest_ancestor_select_ &&
        !nearest_ancestor_select_->IsMultiple()) {
      CloneContentsFromOptionElement(
          nearest_ancestor_select_->SelectedOption());
    }
  } else {
    disabled_ = false;
    HTMLSelectElement* first_ancestor_select = nullptr;
    for (auto* ancestor = parentNode(); ancestor;
         ancestor = ancestor->parentNode()) {
      if (IsA<HTMLOptionElement>(ancestor) ||
          IsA<HTMLSelectedContentElement>(ancestor)) {
        // Putting a <selectedcontent> inside an <option> or another
        // <seletedoption> can lead to infinite loops.
        disabled_ = true;
      } else if (auto* select = DynamicTo<HTMLSelectElement>(ancestor)) {
        if (first_ancestor_select) {
          // If there are multiple ancestor selects, then cloning can lead to
          // infinite loops, so disable this element.
          disabled_ = true;
          break;
        }
        first_ancestor_select = select;
        select->SelectedContentElementInsertedLegacy(this);
      }
    }
  }
}

void HTMLSelectedContentElement::RemovedFrom(ContainerNode& removed_from) {
  HTMLElement::RemovedFrom(removed_from);
  if (RuntimeEnabledFeatures::SelectedcontentSpecEnabled()) {
    auto* new_nearest_ancestor_select =
        Traversal<HTMLSelectElement>::FirstAncestor(*this);
    if (new_nearest_ancestor_select == nearest_ancestor_select_) {
      return;
    }
    CHECK(nearest_ancestor_select_);
    CHECK(!new_nearest_ancestor_select);
    nearest_ancestor_select_->SelectedContentElementRemoved(this);
    nearest_ancestor_select_ = new_nearest_ancestor_select;
    disabled_ = false;
  } else {
    if (!Traversal<HTMLSelectElement>::FirstAncestor(*this)) {
      if (auto* select =
              Traversal<HTMLSelectElement>::FirstAncestor(removed_from)) {
        select->SelectedContentElementRemoved(this);
      }
    }
    disabled_ = false;
  }
}

void HTMLSelectedContentElement::Trace(Visitor* visitor) const {
  HTMLElement::Trace(visitor);
  visitor->Trace(nearest_ancestor_select_);
}

}  // namespace blink
