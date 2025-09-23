// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_MENU_ITEM_LIST_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_MENU_ITEM_LIST_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class HTMLElement;
class HTMLMenuItemElement;

class CORE_EXPORT MenuItemListIterator final {
  STACK_ALLOCATED();

 public:
  enum class StartingPoint {
    kStart,
    kEnd,
    kLast,
  };
  explicit MenuItemListIterator(
      const HTMLElement& owner_menu,
      StartingPoint starting_point = StartingPoint::kStart);

  HTMLMenuItemElement& operator*() {
    DCHECK(current_);
    return *current_;
  }
  HTMLMenuItemElement* operator->() { return current_; }
  MenuItemListIterator& operator++() {
    if (current_) {
      Advance(current_);
    }
    return *this;
  }
  MenuItemListIterator& operator--() {
    if (current_) {
      Retreat(current_);
    }
    return *this;
  }
  explicit operator bool() const { return current_; }
  bool operator==(const MenuItemListIterator& other) const {
    return current_ == other.current_;
  }

 private:
  // These functions returns only a MENUITEM descendant of owner_menu_.
  void Advance(HTMLMenuItemElement* current);
  void Retreat(HTMLMenuItemElement* current);

  const HTMLElement& owner_menu_;
  HTMLMenuItemElement* current_ = nullptr;  // nullptr means we reached the end.
};

class MenuItemList final {
  STACK_ALLOCATED();

 public:
  explicit MenuItemList(const HTMLElement& owner_menu)
      : owner_menu_(owner_menu) {}

  using Iterator = MenuItemListIterator;
  Iterator begin() {
    return Iterator(owner_menu_, MenuItemListIterator::StartingPoint::kStart);
  }
  Iterator end() {
    return Iterator(owner_menu_, MenuItemListIterator::StartingPoint::kEnd);
  }
  Iterator last() {
    return Iterator(owner_menu_, MenuItemListIterator::StartingPoint::kLast);
  }
  bool Empty() {
    return !Iterator(owner_menu_, MenuItemListIterator::StartingPoint::kStart);
  }
  unsigned size() const;
  HTMLMenuItemElement* NextFocusableMenuItem(HTMLMenuItemElement& menuitem,
                                             bool inclusive = false) {
    return FindFocusableMenuItem(menuitem, /*forward*/ true, inclusive);
  }
  HTMLMenuItemElement* PreviousFocusableMenuItem(HTMLMenuItemElement& menuitem,
                                                 bool inclusive = false) {
    return FindFocusableMenuItem(menuitem, /*forward*/ false, inclusive);
  }

 private:
  HTMLMenuItemElement* FindFocusableMenuItem(HTMLMenuItemElement& menuitem,
                                             bool forward,
                                             bool inclusive);

  const HTMLElement& owner_menu_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_MENU_ITEM_LIST_H_
