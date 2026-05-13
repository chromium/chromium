// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/forms/option_list.h"

#include "third_party/blink/renderer/core/dom/element_traversal.h"
#include "third_party/blink/renderer/core/html/forms/html_option_element.h"
#include "third_party/blink/renderer/core/html/forms/html_select_element.h"

namespace blink {

// This function returns any T descendant of owner_.
template <typename OwnerType, typename ItemType>
void ElementListIterator<OwnerType, ItemType>::Advance(ItemType* previous) {
  Element* current;
  if (previous) {
    current = ElementTraversal::NextSkippingChildren(*previous, &owner_);
  } else {
    current = ElementTraversal::FirstChild(owner_);
  }
  while (current) {
    if (auto* element = DynamicTo<ItemType>(current)) {
      current_ = element;
      return;
    }
    if (owner_.ShouldIgnoreDescendantsForElementTraversals(current)) {
      current = ElementTraversal::NextSkippingChildren(*current, &owner_);
    } else {
      current = ElementTraversal::Next(*current, &owner_);
    }
  }
  current_ = nullptr;
}

template <typename OwnerType, typename ItemType>
void ElementListIterator<OwnerType, ItemType>::Retreat(ItemType* next) {
  Element* current;
  if (next) {
    DCHECK_EQ(next->OwnerElementForList(), owner_);
    current = ElementTraversal::Previous(*next, &owner_);
  } else {
    current = ElementTraversal::LastWithin(owner_);
  }

  while (current) {
    if (auto* element = DynamicTo<ItemType>(current)) {
      current_ = element;
      return;
    }

    if (current == owner_) {
      current = nullptr;
    } else if (owner_.ShouldIgnoreDescendantsForElementTraversals(current)) {
      current = ElementTraversal::PreviousAbsoluteSibling(*current, &owner_);
    } else {
      current = ElementTraversal::Previous(*current, &owner_);
    }
  }

  current_ = nullptr;
}

template <typename OwnerType, typename ItemType>
unsigned ElementList<OwnerType, ItemType>::size() const {
  unsigned count = 0;
  auto iterator = Iterator(
      owner_,
      ElementListIterator<OwnerType, ItemType>::IteratorStartingPoint::kStart);
  while (iterator) {
    ++count;
    ++iterator;
  }
  return count;
}

template <typename OwnerType, typename ItemType>
ItemType* ElementList<OwnerType, ItemType>::FindElement(
    ItemType& element,
    ElementMatchingPredicate predicate,
    bool forward,
    bool inclusive) {
  DCHECK_EQ(element.OwnerElementForList(), owner_);
  DCHECK(!Empty());
  ElementListIterator<OwnerType, ItemType> it = begin();
  while (it && *it != element) {
    ++it;
  }
  CHECK_EQ(*it, element);
  while (true) {
    if (!inclusive) {
      if (forward) {
        ++it;
      } else {
        --it;
      }
    }
    if (!it) {
      return nullptr;
    }
    inclusive = false;
    if (predicate(*it)) {
      return &*it;
    }
  }
}

template class CORE_TEMPLATE_EXPORT
    ElementList<HTMLSelectElement, HTMLOptionElement>;

}  // namespace blink
