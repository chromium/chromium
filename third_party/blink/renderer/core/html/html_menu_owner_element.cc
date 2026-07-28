// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/html_menu_owner_element.h"

#include "third_party/blink/renderer/core/accessibility/ax_object_cache.h"
#include "third_party/blink/renderer/core/dom/element_traversal.h"
#include "third_party/blink/renderer/core/dom/popover_data.h"
#include "third_party/blink/renderer/core/event_type_names.h"
#include "third_party/blink/renderer/core/events/keyboard_event.h"
#include "third_party/blink/renderer/core/events/mouse_event.h"
#include "third_party/blink/renderer/core/html/html_hr_element.h"
#include "third_party/blink/renderer/core/html/html_menu_bar_element.h"
#include "third_party/blink/renderer/core/html/html_menu_item_element.h"
#include "third_party/blink/renderer/core/html/html_menu_list_element.h"
#include "third_party/blink/renderer/core/html/html_sub_menu_element.h"
#include "third_party/blink/renderer/core/html/menu_mutation_observer.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

HTMLMenuOwnerElement::HTMLMenuOwnerElement(HTMLQualifiedName tag_name,
                                           Document& document)
    : HTMLElement(tag_name, document), type_ahead_(this) {
  DCHECK(RuntimeEnabledFeatures::MenuElementsEnabled());
  menu_mutation_observer_ = MakeGarbageCollected<MenuMutationObserver>(*this);
}

bool HTMLMenuOwnerElement::IsInDialogMode() const {
  return content_model_violations_count_ > 0U;
}

void HTMLMenuOwnerElement::IncreaseContentModelViolationCount() {
  bool dialog_mode_changed = !content_model_violations_count_;
  ++content_model_violations_count_;
  if (dialog_mode_changed) {
    if (AXObjectCache* cache = GetDocument().ExistingAXObjectCache()) {
      // Unlike <select>, which keeps its internal role as kComboBoxSelect and
      // relies on an accessor adjustment in AXObject::RoleValue() to expose
      // kDialog, <menu> and <menubar> change their internal role to kDialog.
      // Because the role changes here, we must use
      // HandleAttributeChanged(kRoleAttr) to force the accessibility object to
      // be destroyed and recreated, rather than just using MarkElementDirty()
      // to refresh its properties.
      cache->HandleAttributeChanged(html_names::kRoleAttr, this);
    }
  }
}

void HTMLMenuOwnerElement::DecreaseContentModelViolationCount() {
  DCHECK_GT(content_model_violations_count_, 0U);
  bool dialog_mode_changed = content_model_violations_count_ == 1;
  --content_model_violations_count_;
  if (dialog_mode_changed) {
    if (AXObjectCache* cache = GetDocument().ExistingAXObjectCache()) {
      cache->HandleAttributeChanged(html_names::kRoleAttr, this);
    }
  }
}

void HTMLMenuOwnerElement::Trace(Visitor* visitor) const {
  visitor->Trace(menu_mutation_observer_);
  visitor->Trace(last_mouseup_menu_item_);
  HTMLElement::Trace(visitor);
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

bool HTMLMenuOwnerElement::IsTopLevelOwner() const {
  return IsA<HTMLMenuBarElement>(this) ||
         (IsA<HTMLMenuListElement>(this) &&
          !IsA<HTMLSubMenuElement>(parentNode()));
}

void HTMLMenuOwnerElement::DefaultEventHandler(Event& event) {
  auto menuitem_for_event = [this, &event]() -> HTMLMenuItemElement* {
    for (NodeEventContext& node_context : event.GetEventPath()) {
      Node& node = node_context.GetNode();
      if (auto* menu_item = DynamicTo<HTMLMenuItemElement>(node)) {
        return menu_item;
      }
      if (&node == this) {
        return nullptr;
      }
    }
    NOTREACHED();
  };

  if (IsA<MouseEvent>(event) && IsTopLevelOwner()) {
    if (event.type() == event_type_names::kMousedown) {
      last_mouseup_menu_item_ = nullptr;
    } else if (event.type() == event_type_names::kMouseup) {
      last_mouseup_menu_item_ = menuitem_for_event();
    } else if (event.type() == event_type_names::kClick) {
      if (!processing_click_ && last_mouseup_menu_item_ &&
          last_mouseup_menu_item_ != menuitem_for_event()) {
        HTMLMenuItemElement* item_to_click = last_mouseup_menu_item_;
        last_mouseup_menu_item_ = nullptr;
        processing_click_ = true;
        item_to_click->DispatchSimulatedClick(&event);
        processing_click_ = false;
        event.SetDefaultHandled();
        return;
      }
      last_mouseup_menu_item_ = nullptr;
    }
  }

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
