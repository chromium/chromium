// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/html_menu_owner_element.h"

#include "third_party/blink/renderer/core/accessibility/ax_object_cache.h"
#include "third_party/blink/renderer/core/dom/element_traversal.h"
#include "third_party/blink/renderer/core/dom/node_traversal.h"
#include "third_party/blink/renderer/core/dom/popover_data.h"
#include "third_party/blink/renderer/core/event_type_names.h"
#include "third_party/blink/renderer/core/events/keyboard_event.h"
#include "third_party/blink/renderer/core/events/mouse_event.h"
#include "third_party/blink/renderer/core/execution_context/agent.h"
#include "third_party/blink/renderer/core/html/html_hr_element.h"
#include "third_party/blink/renderer/core/html/html_menu_bar_element.h"
#include "third_party/blink/renderer/core/html/html_menu_item_element.h"
#include "third_party/blink/renderer/core/html/html_menu_list_element.h"
#include "third_party/blink/renderer/core/html/html_sub_menu_element.h"
#include "third_party/blink/renderer/core/html/menu_mutation_observer.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/scheduler/public/event_loop.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

namespace {
HTMLMenuOwnerElement* FindMenuRoot(Node* node) {
  HTMLMenuOwnerElement* root = nullptr;
  for (Node* ancestor = node; ancestor; ancestor = ancestor->parentNode()) {
    if (auto* menu = DynamicTo<HTMLMenuOwnerElement>(ancestor)) {
      root = menu;
    }
  }
  return root;
}
}  // namespace

HTMLMenuOwnerElement::HTMLMenuOwnerElement(HTMLQualifiedName tag_name,
                                           Document& document)
    : HTMLElement(tag_name, document), type_ahead_(this) {
  DCHECK(RuntimeEnabledFeatures::MenuElementsEnabled());
}

bool HTMLMenuOwnerElement::IsInDialogMode() const {
  return is_in_dialog_mode_;
}

void HTMLMenuOwnerElement::ScheduleDialogModeUpdate() {
  HTMLMenuOwnerElement* root = FindMenuRoot(this);
  DCHECK(root);
  if (root->is_dialog_mode_update_scheduled_) {
    return;
  }
  root->is_dialog_mode_update_scheduled_ = true;
  GetDocument().GetAgent().event_loop()->EnqueueMicrotask(
      BindOnce(&HTMLMenuOwnerElement::UpdateDialogModeForMenuHierarchy,
               WrapWeakPersistent(root)));
}

void HTMLMenuOwnerElement::UpdateDialogModeForMenuHierarchy() {
  is_dialog_mode_update_scheduled_ = false;

  if (!menu_mutation_observer_) {
    // If our observer has been destroyed, that means we are no longer the root
    // menu. The new root menu will set the ARIA roles for its descendant menus,
    // including |this|.
    return;
  }
  DCHECK_EQ(this, FindMenuRoot(this))
      << "We only intend for the root menu to update its subtrees.";

  const bool is_content_model_violated = total_violations_in_tree_ > 0;

  if (!is_content_model_violated && !is_in_dialog_mode_) {
#if EXPENSIVE_DCHECKS_ARE_ON()
    for (Node* node = this; node; node = NodeTraversal::Next(*node, this)) {
      if (auto* menu = DynamicTo<HTMLMenuOwnerElement>(node)) {
        DCHECK(!menu->is_in_dialog_mode_)
            << "Invariant failed: Root is not in dialog mode, target is not "
               "dialog mode, but a descendant menu is in dialog mode.";
      }
    }
#endif
    // We don't have to update anything if the tree has never had any
    // violations.
    return;
  }

  for (Node* node = this; node; node = NodeTraversal::Next(*node, this)) {
    if (auto* menu = DynamicTo<HTMLMenuOwnerElement>(node)) {
      if (menu->is_in_dialog_mode_ != is_content_model_violated) {
        menu->is_in_dialog_mode_ = is_content_model_violated;
        if (AXObjectCache* cache = GetDocument().ExistingAXObjectCache()) {
          cache->HandleAttributeChanged(html_names::kRoleAttr, menu);
        }
      }
    }
  }
}

void HTMLMenuOwnerElement::IncreaseContentModelViolationCount() {
  ++total_violations_in_tree_;
  if (total_violations_in_tree_ == 1) {
    ScheduleDialogModeUpdate();
  }
}

void HTMLMenuOwnerElement::DecreaseContentModelViolationCount() {
  DCHECK_GT(total_violations_in_tree_, 0);
  --total_violations_in_tree_;
  if (total_violations_in_tree_ == 0) {
    ScheduleDialogModeUpdate();
  }
}

Node::InsertionNotificationRequest HTMLMenuOwnerElement::InsertedInto(
    ContainerNode& insertion_point) {
  auto result = HTMLElement::InsertedInto(insertion_point);
  if (!isConnected()) {
    // We don't need an observer until this subtree is in the document.
    return result;
  }

  DCHECK(!is_in_dialog_mode_)
      << "Before attaching we haven't been checking for violations";

  HTMLMenuOwnerElement* root = FindMenuRoot(parentNode());
  if (root) {
    if (root->is_in_dialog_mode_) {
      // A menu in our hierarchy has a violation, so we are going to dialog
      // mode.
      is_in_dialog_mode_ = true;
      if (AXObjectCache* cache = GetDocument().ExistingAXObjectCache()) {
        cache->HandleAttributeChanged(html_names::kRoleAttr, this);
      }
    }
    // We are a submenu, and our root already has the observer.
    return result;
  }
  // We are the topmost menu!
  DCHECK(!menu_mutation_observer_);
  total_violations_in_tree_ = 0;
  menu_mutation_observer_ = MakeGarbageCollected<MenuMutationObserver>(*this);
  HeapHashSet<Member<Node>> visited_nodes;
  menu_mutation_observer_->CheckNodeAndDescendantsForViolations(
      this, visited_nodes, /*disconnected_parent=*/nullptr);

  if (total_violations_in_tree_ > 0) {
    ScheduleDialogModeUpdate();
  }

  return result;
}

void HTMLMenuOwnerElement::RemovedFrom(ContainerNode& insertion_point) {
  HTMLElement::RemovedFrom(insertion_point);
  if (menu_mutation_observer_) {
    menu_mutation_observer_->Disconnect();
    menu_mutation_observer_ = nullptr;
    total_violations_in_tree_ = 0;
  }
  is_in_dialog_mode_ = false;
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
