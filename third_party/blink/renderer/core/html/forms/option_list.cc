// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/forms/option_list.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_scroll_into_view_options.h"
#include "third_party/blink/renderer/core/dom/element_traversal.h"
#include "third_party/blink/renderer/core/dom/focus_params.h"
#include "third_party/blink/renderer/core/html/forms/html_option_element.h"
#include "third_party/blink/renderer/core/html/forms/html_select_element.h"
#include "third_party/blink/renderer/core/html/html_menu_item_element_base.h"
#include "third_party/blink/renderer/core/html/html_menu_owner_element.h"

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
      // This can be called during InsertedInto, so we have to allow null
      // OwnerElementForList.
      DCHECK(!current_->OwnerElementForList() ||
             current_->OwnerElementForList() == owner_);
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
  // last_non_ignored_descendant lets us avoid using
  // ElementTraversal::Previous, which can take many last-child steps to find
  // a deep element.  We need to check
  // ShouldIgnoreDescendantsForElementTraversals each time we step from parent
  // to child.
  auto last_non_ignored_descendant =
      [this](const Element& container) -> Element* {
    Element* e = ElementTraversal::LastChild(container);
    if (!e) {
      return nullptr;
    }
    while (!owner_.ShouldIgnoreDescendantsForElementTraversals(e)) {
      Element* child = ElementTraversal::LastChild(*e);
      if (!child) {
        break;
      }
      e = child;
    }
    return e;
  };

  Element* current = next;
  do {
    if (!current) {
      // This is the first time through the loop.
      current = last_non_ignored_descendant(owner_);
    } else {
      Element* previous_sibling = ElementTraversal::PreviousSibling(*current);
      if (previous_sibling) {
        if (owner_.ShouldIgnoreDescendantsForElementTraversals(
                previous_sibling)) {
          current = previous_sibling;
        } else {
          current = last_non_ignored_descendant(*previous_sibling);
          if (!current) {
            current = previous_sibling;
          }
        }
      } else {
        current = current->parentElement();
        if (current == owner_) {
          current_ = nullptr;
          return;
        }
      }
    }

    if (auto* element = DynamicTo<ItemType>(current)) {
      current_ = element;
      DCHECK_EQ(current_->OwnerElementForList(), owner_);
      return;
    }
  } while (current);

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
    bool inclusive,
    bool wrap) {
  DCHECK_EQ(element.OwnerElementForList(), owner_);
  DCHECK(!Empty());
  ElementListIterator<OwnerType, ItemType> it = begin();
  while (it && *it != element) {
    ++it;
  }
  CHECK_EQ(*it, element);
  ItemType* first_item_tested = nullptr;
  while (true) {
    if (!inclusive) {
      if (forward) {
        ++it;
      } else {
        --it;
      }
    }
    if (!it) {
      if (!wrap) {
        return nullptr;
      }
      it = forward ? begin() : last();
    }
    inclusive = false;
    ItemType* this_item = &*it;
    CHECK(this_item);
    if (first_item_tested) {
      if (this_item == first_item_tested) {
        // We've tested all the items and none matched.
        return nullptr;
      }
    } else {
      first_item_tested = this_item;
    }
    if (predicate(*this_item)) {
      return this_item;
    }
  }
}

template <typename OwnerType, typename ItemType>
ItemType* ElementList<OwnerType, ItemType>::NextFocusableElement(
    ItemType& element,
    bool inclusive,
    bool wrap) {
  return FindNextElement(
      element, [](ItemType& el) -> bool { return el.IsFocusable(); }, inclusive,
      wrap);
}

template <typename OwnerType, typename ItemType>
ItemType* ElementList<OwnerType, ItemType>::PreviousFocusableElement(
    ItemType& element,
    bool inclusive,
    bool wrap) {
  return FindPreviousElement(
      element, [](ItemType& el) -> bool { return el.IsFocusable(); }, inclusive,
      wrap);
}

template <typename OwnerType, typename T>
void ElementList<OwnerType, T>::HandlePageUpDown(T& element,
                                                 PageKey page_key,
                                                 FocusParams& focus_params) {
  if (!element.IsVisibleInViewport()) {
    // If the element isn't visible at all right now, just scroll it into view
    // instead of moving focus to another element.
    element.scrollIntoViewIfNeeded(/*center_if_needed=*/false);
    return;
  }

  switch (page_key) {
    case PageKey::kDown: {
      T* next_item = NextFocusableElement(element);
      if (next_item && !next_item->IsVisibleInViewport()) {
        // The next item isn't visible, which means we were at the very bottom.
        // Scroll the current item to the top, and then focus the bottom one.
        ScrollIntoViewOptions* scroll_into_view_options =
            ScrollIntoViewOptions::Create();
        scroll_into_view_options->setBlock(
            V8ScrollLogicalPosition::Enum::kStart);
        scroll_into_view_options->setInlinePosition(
            V8ScrollLogicalPosition::Enum::kNearest);
        element.scrollIntoViewWithOptions(scroll_into_view_options);
      }
      // Then find the last item that is still in the view.
      T* next_focus = &element;
      for (auto* current = &element; current && current->IsVisibleInViewport();
           current = NextFocusableElement(*current)) {
        next_focus = current;
      }
      next_focus->Focus(focus_params);
      break;
    }
    case PageKey::kUp: {
      T* previous_item = PreviousFocusableElement(element);
      if (previous_item && !previous_item->IsVisibleInViewport()) {
        // The previous item isn't visible, which means we were at the very top.
        // Scroll the current item to the bottom, and then focus the top one.
        ScrollIntoViewOptions* scroll_into_view_options =
            ScrollIntoViewOptions::Create();
        scroll_into_view_options->setBlock(V8ScrollLogicalPosition::Enum::kEnd);
        scroll_into_view_options->setInlinePosition(
            V8ScrollLogicalPosition::Enum::kNearest);
        element.scrollIntoViewWithOptions(scroll_into_view_options);
      }
      // Then find the first item that is in the view.
      T* next_focus = &element;
      for (auto* current = &element; current && current->IsVisibleInViewport();
           current = PreviousFocusableElement(*current)) {
        next_focus = current;
      }
      next_focus->Focus(focus_params);
      break;
    }
  }
}

template class CORE_TEMPLATE_EXPORT
    ElementListIterator<HTMLSelectElement, HTMLOptionElement>;
template class CORE_TEMPLATE_EXPORT
    ElementList<HTMLSelectElement, HTMLOptionElement>;
template class CORE_TEMPLATE_EXPORT
    ElementListIterator<HTMLMenuOwnerElement, HTMLMenuItemElementBase>;
template class CORE_TEMPLATE_EXPORT
    ElementList<HTMLMenuOwnerElement, HTMLMenuItemElementBase>;

}  // namespace blink
