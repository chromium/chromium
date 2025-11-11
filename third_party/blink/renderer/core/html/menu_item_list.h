// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_MENU_ITEM_LIST_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_MENU_ITEM_LIST_H_

#include "base/memory/stack_allocated.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/html/html_menu_owner_element.h"
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

  HTMLMenuItemElement& operator*();
  HTMLMenuItemElement* operator->();
  MenuItemListIterator& operator++();
  MenuItemListIterator& operator--();
  explicit operator bool() const;
  bool operator==(const MenuItemListIterator& other) const;

 private:
  // These functions returns only a <menuitem> descendant of owner_menu_.
  void Advance(HTMLMenuItemElement* current);
  void Retreat(HTMLMenuItemElement* current);

  const HTMLElement& owner_menu_;
  HTMLMenuItemElement* current_ = nullptr;  // nullptr means we reached the end.
};

class MenuItemList final {
  STACK_ALLOCATED();

 public:
  explicit MenuItemList(HTMLMenuOwnerElement& owner_menu)
      : owner_menu_(owner_menu) {}

  using Iterator = MenuItemListIterator;
  Iterator begin();
  Iterator end();
  Iterator last();
  bool Empty();
  unsigned size() const;
  HTMLMenuItemElement* NextFocusableMenuItem(HTMLMenuItemElement& menuitem,
                                             bool inclusive = false);
  HTMLMenuItemElement* PreviousFocusableMenuItem(HTMLMenuItemElement& menuitem,
                                                 bool inclusive = false);

 private:
  HTMLMenuItemElement* FindFocusableMenuItem(HTMLMenuItemElement& menuitem,
                                             bool forward,
                                             bool inclusive);

  const HTMLMenuOwnerElement& owner_menu_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_MENU_ITEM_LIST_H_
