// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/html_menu_owner_element.h"

#include "third_party/blink/renderer/core/dom/popover_data.h"
#include "third_party/blink/renderer/core/events/keyboard_event.h"
#include "third_party/blink/renderer/core/html/html_hr_element.h"
#include "third_party/blink/renderer/core/html/html_menu_bar_element.h"
#include "third_party/blink/renderer/core/html/html_menu_item_element.h"
#include "third_party/blink/renderer/core/html/html_menu_list_element.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

HTMLMenuOwnerElement::HTMLMenuOwnerElement(HTMLQualifiedName tag_name,
                                           Document& document)
    : HTMLElement(tag_name, document), type_ahead_(this) {
  DCHECK(RuntimeEnabledFeatures::MenuElementsEnabled());
}

bool HTMLMenuOwnerElement::IsValidBuiltinCommand(HTMLElement& invoker,
                                                 CommandEventType command) {
  return HTMLElement::IsValidBuiltinCommand(invoker, command) ||
         command == CommandEventType::kToggleMenu ||
         command == CommandEventType::kShowMenu ||
         command == CommandEventType::kHideMenu;
}

MenuItemList HTMLMenuOwnerElement::ItemList() const {
  return MenuItemList(*this);
}

bool HTMLMenuOwnerElement::ShouldIgnoreDescendantsForElementTraversals(
    Element* element) const {
  // TODO: fieldset owner can be a menulist.
  return IsA<HTMLMenuBarElement>(element) ||
         IsA<HTMLMenuListElement>(element) || IsA<HTMLHRElement>(element);
}

void HTMLMenuOwnerElement::DefaultEventHandler(Event& event) {
  if (auto* keyboard_event = DynamicTo<KeyboardEvent>(event)) {
    if (TypeAhead::ShouldHandleKeyboardEvent(*keyboard_event)) {
      int index = type_ahead_.HandleEvent(
          *keyboard_event, keyboard_event->charCode(),
          TypeAhead::kMatchPrefix | TypeAhead::kCycleFirstChar);
      if (index >= 0) {
        ItemList()
            .at((unsigned)index)
            .Focus(FocusParams(FocusTrigger::kScript));
      }

      event.SetDefaultHandled();
      return;
    }
  }

  HTMLElement::DefaultEventHandler(event);
}

int HTMLMenuOwnerElement::IndexOfSelectedOption() const {
  auto* focused_menuitem =
      DynamicTo<HTMLMenuItemElement>(GetDocument().FocusedElement());
  if (!focused_menuitem) {
    return -1;
  }

  int index = 0;
  for (HTMLMenuItemElement& menuitem : ItemList()) {
    if (menuitem == focused_menuitem) {
      return index;
    }
    index++;
  }

  return -1;
}

int HTMLMenuOwnerElement::OptionCount() const {
  return ItemList().size();
}

String HTMLMenuOwnerElement::OptionAtIndex(int index) const {
  CHECK_GE(index, 0);
  DCHECK_LE((unsigned)index, ItemList().size());
  return ItemList().at((unsigned)index).textContent();
}

}  // namespace blink
