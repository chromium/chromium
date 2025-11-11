// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/menu_item_list.h"

#include "third_party/blink/renderer/core/dom/element_traversal.h"
#include "third_party/blink/renderer/core/html/html_hr_element.h"
#include "third_party/blink/renderer/core/html/html_menu_bar_element.h"
#include "third_party/blink/renderer/core/html/html_menu_item_element.h"
#include "third_party/blink/renderer/core/html/html_menu_list_element.h"

namespace blink {

MenuItemListIterator::MenuItemListIterator(const HTMLElement& owner_menu,
                                           StartingPoint starting_point)
    : owner_menu_(owner_menu) {
  DCHECK(IsA<HTMLMenuBarElement>(owner_menu_) ||
         IsA<HTMLMenuListElement>(owner_menu_));
  switch (starting_point) {
    case StartingPoint::kStart:
      Advance(nullptr);
      break;
    case StartingPoint::kLast:
      Retreat(nullptr);
      break;
    case StartingPoint::kEnd:
      break;
  }
}

HTMLMenuItemElement& MenuItemListIterator::operator*() {
  DCHECK(current_);
  return *current_;
}

HTMLMenuItemElement* MenuItemListIterator::operator->() {
  return current_;
}

MenuItemListIterator& MenuItemListIterator::operator++() {
  if (current_) {
    Advance(current_);
  }
  return *this;
}

MenuItemListIterator& MenuItemListIterator::operator--() {
  if (current_) {
    Retreat(current_);
  }
  return *this;
}

MenuItemListIterator::operator bool() const {
  return current_;
}

bool MenuItemListIterator::operator==(const MenuItemListIterator& other) const {
  return current_ == other.current_;
}

void MenuItemListIterator::Advance(HTMLMenuItemElement* previous) {
  Element* current;
  if (previous) {
    DCHECK_EQ(previous->OwningMenuElement(), owner_menu_);
    current = ElementTraversal::NextSkippingChildren(*previous, &owner_menu_);
  } else {
    current = ElementTraversal::FirstChild(owner_menu_);
  }
  while (current) {
    if (auto* menuitem = DynamicTo<HTMLMenuItemElement>(current)) {
      current_ = menuitem;
      return;
    }
    if (IsA<HTMLMenuBarElement>(current) || IsA<HTMLMenuListElement>(current) ||
        IsA<HTMLHRElement>(current)) {
      current = ElementTraversal::NextSkippingChildren(*current, &owner_menu_);
    } else {
      // TODO: fieldset owner can be a menulist.
      current = ElementTraversal::Next(*current, &owner_menu_);
    }
  }
  current_ = nullptr;
}

void MenuItemListIterator::Retreat(HTMLMenuItemElement* next) {
  Element* current;
  if (next) {
    DCHECK_EQ(next->OwningMenuElement(), owner_menu_);
    current = ElementTraversal::Previous(*next, &owner_menu_);
  } else {
    current = ElementTraversal::LastChild(owner_menu_);
  }

  while (current) {
    if (auto* menuitem = DynamicTo<HTMLMenuItemElement>(current)) {
      current_ = menuitem;
      return;
    }

    if (current == owner_menu_) {
      current = nullptr;
    } else if (IsA<HTMLMenuBarElement>(current) ||
               IsA<HTMLMenuListElement>(current) ||
               IsA<HTMLHRElement>(current)) {
      current =
          ElementTraversal::PreviousAbsoluteSibling(*current, &owner_menu_);
    } else {
      // TODO: fieldset owner can be a menulist.
      current = ElementTraversal::Previous(*current, &owner_menu_);
    }
  }

  current_ = nullptr;
}

MenuItemListIterator MenuItemList::begin() {
  return Iterator(owner_menu_, Iterator::StartingPoint::kStart);
}

MenuItemListIterator MenuItemList::end() {
  return Iterator(owner_menu_, Iterator::StartingPoint::kEnd);
}

MenuItemListIterator MenuItemList::last() {
  return Iterator(owner_menu_, Iterator::StartingPoint::kLast);
}

bool MenuItemList::Empty() {
  return !Iterator(owner_menu_, Iterator::StartingPoint::kStart);
}

HTMLMenuItemElement* MenuItemList::NextFocusableMenuItem(
    HTMLMenuItemElement& menuitem,
    bool inclusive) {
  return FindFocusableMenuItem(menuitem, /*forward*/ true, inclusive);
}

HTMLMenuItemElement* MenuItemList::PreviousFocusableMenuItem(
    HTMLMenuItemElement& menuitem,
    bool inclusive) {
  return FindFocusableMenuItem(menuitem, /*forward*/ false, inclusive);
}

unsigned MenuItemList::size() const {
  unsigned count = 0;
  auto iterator =
      Iterator(owner_menu_, MenuItemListIterator::StartingPoint::kStart);
  while (iterator) {
    ++count;
    ++iterator;
  }
  return count;
}

HTMLMenuItemElement* MenuItemList::FindFocusableMenuItem(
    HTMLMenuItemElement& menuitem,
    bool forward,
    bool inclusive) {
  DCHECK_EQ(menuitem.OwningMenuElement(), owner_menu_);
  DCHECK(!Empty());
  MenuItemListIterator menu_item_list_iterator = begin();
  while (menu_item_list_iterator && *menu_item_list_iterator != menuitem) {
    ++menu_item_list_iterator;
  }
  CHECK_EQ(*menu_item_list_iterator, menuitem);
  while (true) {
    if (!inclusive) {
      if (forward) {
        ++menu_item_list_iterator;
      } else {
        --menu_item_list_iterator;
      }
    }
    if (!menu_item_list_iterator) {
      return nullptr;
    }
    inclusive = false;
    if (menu_item_list_iterator->IsFocusable()) {
      return &*menu_item_list_iterator;
    }
  }
}

}  // namespace blink
