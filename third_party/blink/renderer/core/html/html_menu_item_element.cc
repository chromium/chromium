// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/html_menu_item_element.h"

#include "third_party/blink/public/common/input/web_pointer_properties.h"
#include "third_party/blink/renderer/core/css/selector_checker.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/dom/flat_tree_traversal.h"
#include "third_party/blink/renderer/core/dom/popover_data.h"
#include "third_party/blink/renderer/core/event_type_names.h"
#include "third_party/blink/renderer/core/events/command_event.h"
#include "third_party/blink/renderer/core/events/keyboard_event.h"
#include "third_party/blink/renderer/core/events/mouse_event.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/html/forms/html_field_set_element.h"
#include "third_party/blink/renderer/core/html/html_menu_bar_element.h"
#include "third_party/blink/renderer/core/html/html_menu_list_element.h"
#include "third_party/blink/renderer/core/html/menu_item_list.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/keywords.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

HTMLMenuItemElement::HTMLMenuItemElement(Document& document)
    : HTMLElement(html_names::kMenuitemTag, document), is_checked_(false) {}

HTMLMenuItemElement::~HTMLMenuItemElement() = default;

void HTMLMenuItemElement::Trace(Visitor* visitor) const {
  visitor->Trace(owning_menu_element_);
  visitor->Trace(nearest_ancestor_field_set_);
  HTMLElement::Trace(visitor);
}

bool HTMLMenuItemElement::MatchesDefaultPseudoClass() const {
  return FastHasAttribute(html_names::kDefaultcheckedAttr);
}

bool HTMLMenuItemElement::MatchesEnabledPseudoClass() const {
  return !IsDisabledFormControl();
}

void HTMLMenuItemElement::ParseAttribute(
    const AttributeModificationParams& params) {
  const QualifiedName& name = params.name;
  if (name == html_names::kDisabledAttr) {
    if (params.old_value.IsNull() != params.new_value.IsNull()) {
      PseudoStateChanged(CSSSelector::kPseudoDisabled);
      PseudoStateChanged(CSSSelector::kPseudoEnabled);
    }
  } else if (name == html_names::kDefaultcheckedAttr) {
    // If the default value has not been overridden yet, then allow setting the
    // `defaultchecked` attribute to influence the checkedness.
    //
    // Keep this logic in sync with the logic at the bottom of `InsertedInto()`.
    if (!is_default_checkedness_overridden_) {
      setChecked(!params.new_value.IsNull());
      // Re-unset this flag, since `SetChecked()` set it to true by default.
      is_default_checkedness_overridden_ = false;
    }
    // The `:default` pseudo-class should match the default checkedness,
    // regardless of whether the default checkedness controls the underlying
    // checked state anymore.
    PseudoStateChanged(CSSSelector::kPseudoDefault);
  } else {
    HTMLElement::ParseAttribute(params);
  }
}

bool HTMLMenuItemElement::HasOwnerMenuList() const {
  return owning_menu_element_ &&
         IsA<HTMLMenuListElement>(*owning_menu_element_);
}

bool HTMLMenuItemElement::IsCheckable() const {
  return HasOwnerMenuList() && nearest_ancestor_field_set_ &&
         nearest_ancestor_field_set_->FastGetAttribute(
             html_names::kCheckableAttr) &&
         !InvokesSubmenu();
}

bool HTMLMenuItemElement::checked() const {
  return is_checked_;
}

bool HTMLMenuItemElement::ShouldAppearChecked() const {
  // `this` should only appear checked if we are checked, and we're in a
  // checkable <fieldset> in a <menulist>.
  return IsCheckable() && checked();
}

bool HTMLMenuItemElement::IsDisabledFormControl() const {
  return FastHasAttribute(html_names::kDisabledAttr);
}

FocusableState HTMLMenuItemElement::SupportsFocus(UpdateBehavior) const {
  return IsDisabledFormControl() ? FocusableState::kNotFocusable
                                 : FocusableState::kFocusable;
}

bool HTMLMenuItemElement::IsKeyboardFocusableSlow(
    UpdateBehavior update_behavior) const {
  // Menuitems are keyboard focusable if they are focusable and don't have a
  // negative tabindex set.
  return IsFocusable(update_behavior) && tabIndex() >= 0;
}

int HTMLMenuItemElement::DefaultTabIndex() const {
  return 0;
}

bool HTMLMenuItemElement::ShouldHaveFocusAppearance() const {
  return SelectorChecker::MatchesFocusVisiblePseudoClass(*this);
}

HTMLMenuListElement* HTMLMenuItemElement::InvokesSubmenu() const {
  auto* invoked_element = DynamicTo<HTMLMenuListElement>(commandForElement());
  if (!invoked_element || !invoked_element->IsPopover()) {
    return nullptr;
  }
  CommandEventType type = GetCommandEventType(
      FastGetAttribute(html_names::kCommandAttr), GetExecutionContext());
  if (type != CommandEventType::kToggleMenu &&
      type != CommandEventType::kShowMenu &&
      type != CommandEventType::kHideMenu) {
    return nullptr;
  }
  return invoked_element;
}

bool HTMLMenuItemElement::CanBeCommandInvoker() const {
  return !FastHasAttribute(html_names::kDisabledAttr);
}

bool HTMLMenuItemElement::setChecked(bool checked) {
  bool checkable = IsCheckable();
  is_checked_ = checked && checkable;
  PseudoStateChanged(CSSSelector::kPseudoChecked);

  if (!checkable) {
    // Not checkable - close the containing menulist unless this item invokes
    // a sub-menu.
    return !InvokesSubmenu();
  }
  DCHECK(!InvokesSubmenu());

  is_default_checkedness_overridden_ = true;

  // Only update the exclusivity of all other menu items rooted under the same
  // fieldset *if* `this` is becoming checked under a fieldset that enforces
  // exclusivity. If it is becoming unchecked, we don't have to worry about
  // manually unchecking other menu items in the exclusive set, because it is
  // permitted to have zero menu items checked.
  DCHECK(nearest_ancestor_field_set_);
  const AtomicString& checkable_keyword =
      nearest_ancestor_field_set_->FastGetAttribute(html_names::kCheckableAttr);
  if (is_checked_ &&
      EqualIgnoringASCIICase(checkable_keyword, keywords::kSingle)) {
    nearest_ancestor_field_set_->UpdateMenuItemCheckableExclusivity(this);
    // Exclusive checkbox - close the containing menulist after changing.
    return true;
  } else {
    // Nop-exclusive checkbox - don't close the containing menulist, so that
    // multiple values can be chosen.
    return false;
  }
}

void HTMLMenuItemElement::ActivateMenuItem() {
  // A menu item's checkability and ability to invoke a command are
  // exclusive. If the item is checkable, that takes precedence, and the sub-
  // menu invoker will NOT be respected.
  bool close_containing_menulist = setChecked(!checked());

  // If this menu item isn't a submenu invoker, or it's a checkable menu item
  // that wants us to close after changing, then close the containing menu.
  if (close_containing_menulist) {
    DCHECK(IsCheckable() || !InvokesSubmenu());
    CloseOutermostContainingMenuList();
  }
  if (InvokesSubmenu()) {
    DCHECK(!IsCheckable());
    HandleCommandForActivation();
  }
}

Element* HTMLMenuItemElement::CloseOutermostContainingMenuList() {
  HTMLMenuListElement* containing_menulist =
      DynamicTo<HTMLMenuListElement>(owning_menu_element_.Get());
  if (!containing_menulist) {
    // This <menuitem> isn't inside a <menulist>.
    return nullptr;
  }
  while (true) {
    auto* invoking_menulist = DynamicTo<HTMLMenuListElement>(
        HTMLElement::TopLayerElementPopoverAncestor(
            *containing_menulist, TopLayerElementType::kPopover));
    if (!invoking_menulist) {
      break;
    }
    containing_menulist = const_cast<HTMLMenuListElement*>(invoking_menulist);
  }
  Element* upstream_invoker = containing_menulist->GetPopoverData()->invoker();
  containing_menulist->HidePopoverInternal(
      upstream_invoker, HidePopoverFocusBehavior::kNone,
      HidePopoverTransitionBehavior::kFireEventsAndWaitForTransitions,
      /*exception_state=*/nullptr);
  return upstream_invoker;
}

void HTMLMenuItemElement::HandleMenuKeyboardEvents(Event& event) {
  auto* keyboard_event = DynamicTo<KeyboardEvent>(event);
  if (!keyboard_event || event.type() != event_type_names::kKeydown) {
    return;
  }
  // If any key modifiers are pressed, don't do anything.
  if (keyboard_event->GetModifiers() & WebInputEvent::kKeyModifiers) {
    return;
  }

  FocusParams focus_params(FocusTrigger::kUserGesture);
  const AtomicString key(keyboard_event->key());

  // Nothing else below does anything if we're not inside an owner menu that has
  // at least one menu item.
  if (!owning_menu_element_) {
    return;
  }
  MenuItemList menuitems = owning_menu_element_->ItemList();
  if (menuitems.Empty()) {
    return;
  }

  if (IsA<HTMLMenuListElement>(*owning_menu_element_)) {
    if (key == keywords::kArrowUp) {
      if (auto* previous = menuitems.PreviousFocusableMenuItem(*this)) {
        previous->Focus(focus_params);
      }
      event.SetDefaultHandled();
      return;
    } else if (key == keywords::kArrowDown) {
      if (auto* next = menuitems.NextFocusableMenuItem(*this)) {
        next->Focus(focus_params);
      }
      event.SetDefaultHandled();
      return;
    } else if (key == keywords::kHome) {
      if (auto* first = menuitems.NextFocusableMenuItem(*menuitems.begin(),
                                                        /*inclusive=*/true)) {
        first->Focus(focus_params);
        event.SetDefaultHandled();
        return;
      }
    } else if (key == keywords::kEnd) {
      if (auto* last = menuitems.PreviousFocusableMenuItem(
              *menuitems.last(), /*inclusive=*/true)) {
        last->Focus(focus_params);
        event.SetDefaultHandled();
        return;
      }
    } else if (key == keywords::kArrowRight) {
      // If this invokes a menulist and is itself in a menulist, then
      // arrow right should open the invoked menulist and focus its first
      // menuitem.
      if (auto* invoked_menulist = InvokesSubmenu()) {
        if (!invoked_menulist->popoverOpen()) {
          invoked_menulist->InvokePopover(*this);
        }
        MenuItemList invoked_menuitems = invoked_menulist->ItemList();
        if (auto* first = invoked_menuitems.NextFocusableMenuItem(
                *invoked_menuitems.begin(), /*inclusive=*/true)) {
          first->Focus(focus_params);
          event.SetDefaultHandled();
          return;
        }
      } else {
        // Else, this menuitem does not invoke a menulist and we close all
        // ancestor menulists. Loop to find the invoker of the lowest layer
        // menulist ancestor.
        auto* invoker = CloseOutermostContainingMenuList();
        // If ancestor menulist is invoked from a menubar, focus on next
        // menuitem within the menubar.
        if (auto* invoker_menuitem = DynamicTo<HTMLMenuItemElement>(invoker)) {
          if (auto* ancestor_menubar = invoker_menuitem->OwningMenuElement()) {
            MenuItemList ancestor_menuitems = ancestor_menubar->ItemList();
            if (auto* next = ancestor_menuitems.NextFocusableMenuItem(
                    *invoker_menuitem)) {
              next->Focus(focus_params);
              event.SetDefaultHandled();
              return;
            }
          }
          // Else, focus on the invoker (it can be a menuitem or a button).
          invoker->Focus(focus_params);
          event.SetDefaultHandled();
          return;
        }
      }

    } else if (key == keywords::kArrowLeft) {
      // If this is itself in a menulist, then arrow left should close the
      // current menulist.
      Element* invoker = owning_menu_element_->GetPopoverData()->invoker();
      bool can_hide = owning_menu_element_->IsPopoverReady(
          PopoverTriggerAction::kHide,
          /*exception_state=*/nullptr,
          /*include_event_handler_text=*/false, &GetDocument());
      if (can_hide) {
        owning_menu_element_->HidePopoverInternal(
            invoker, HidePopoverFocusBehavior::kNone,
            HidePopoverTransitionBehavior::kFireEventsAndWaitForTransitions,
            /*exception_state=*/nullptr);
      }
      if (auto* invoker_menuitem = DynamicTo<HTMLMenuItemElement>(invoker)) {
        if (auto* invoker_menubar = DynamicTo<HTMLMenuBarElement>(
                invoker_menuitem->OwningMenuElement())) {
          // Focus on previous if it is in menubar.
          MenuItemList invoker_menuitems = invoker_menubar->ItemList();
          if (auto* previous = invoker_menuitems.PreviousFocusableMenuItem(
                  *invoker_menuitem)) {
            previous->Focus(focus_params);
            event.SetDefaultHandled();
            return;
          }
        }
        // Else, focus on invoker (it can be a button, a menuitem in a
        // menulist or a standalone menuitem).
        invoker->Focus(focus_params);
        event.SetDefaultHandled();
        return;
      }
    }
    // TODO(crbug.com/425682464): implement scrolling to visible menuitem,
    // for kPageDown/kPageUp.
  } else {
    CHECK(IsA<HTMLMenuBarElement>(*owning_menu_element_));
    if (key == keywords::kArrowLeft) {
      if (auto* previous = menuitems.PreviousFocusableMenuItem(*this)) {
        previous->Focus(focus_params);
      }
      event.SetDefaultHandled();
      return;
    } else if (key == keywords::kArrowRight) {
      if (auto* next = menuitems.NextFocusableMenuItem(*this)) {
        next->Focus(focus_params);
      }
      event.SetDefaultHandled();
      return;
    } else if (key == keywords::kHome) {
      if (auto* first = menuitems.NextFocusableMenuItem(*menuitems.begin(),
                                                        /*inclusive=*/true)) {
        first->Focus(focus_params);
        event.SetDefaultHandled();
        return;
      }
    } else if (key == keywords::kEnd) {
      if (auto* last = menuitems.PreviousFocusableMenuItem(
              *menuitems.last(), /*inclusive=*/true)) {
        last->Focus(focus_params);
        event.SetDefaultHandled();
        return;
      }
    } else if (key == keywords::kArrowDown || key == keywords::kArrowUp) {
      // If this invokes a menulist and is in a menubar, then arrow down/up
      // should open the menulist and go to first/last menuitem in it.
      if (auto* invoked_menulist = InvokesSubmenu()) {
        if (!invoked_menulist->popoverOpen()) {
          invoked_menulist->InvokePopover(*this);
        }
        MenuItemList invoked_menuitems = invoked_menulist->ItemList();
        if (key == keywords::kArrowDown) {
          if (auto* first = invoked_menuitems.NextFocusableMenuItem(
                  *invoked_menuitems.begin(), /*inclusive=*/true)) {
            first->Focus(focus_params);
            event.SetDefaultHandled();
            return;
          }
        } else if (key == keywords::kArrowUp) {
          if (auto* last = invoked_menuitems.PreviousFocusableMenuItem(
                  *invoked_menuitems.last(), /*inclusive=*/true)) {
            last->Focus(focus_params);
            event.SetDefaultHandled();
            return;
          }
        }
      }
    }
  }
}

void HTMLMenuItemElement::HandleMenuPointerEvents(Event& event) {
  // This implements the special "mouse down, drag to menu item, mouse up"
  // behavior, which is mouse-only and does not apply to touchscreens. The
  // remainder of normal mouse/touch behavior is handled by the normal
  // DOMActivate event system.
  const auto* mouse_event = DynamicTo<MouseEvent>(event);
  if (!mouse_event || mouse_event->FromTouch() ||
      mouse_event->button() !=
          static_cast<int16_t>(WebPointerProperties::Button::kLeft) ||
      (event.type() != event_type_names::kMouseup &&
       event.type() != event_type_names::kMousedown)) {
    return;
  }

  if (event.type() == event_type_names::kMouseup) {
    auto mouse_down_info = GetDocument().PopoverPickerPointerdown();
    GetDocument().SetPopoverPickerPointerdown({.target = nullptr});
    HTMLMenuItemElement* mouse_down_menuitem = nullptr;
    for (Node* node = mouse_down_info.target; node;
         node = FlatTreeTraversal::Parent(*node)) {
      if (auto* item = DynamicTo<HTMLMenuItemElement>(node)) {
        mouse_down_menuitem = item;
        break;
      }
    }
    bool same_element = this == mouse_down_menuitem;
    // TODO(masonf) This kEpsilon should be combined with the one in
    // html_option_element.cc.
    constexpr float kEpsilon = 5;  // 5 pixels in any direction
    bool mouse_moved = !mouse_down_info.location.IsWithinDistance(
        mouse_event->AbsoluteLocation(), kEpsilon);
    // We "pick" a menu item here, iff:
    //  1. This was a mouse, not touchscreen, interaction,
    //  2. The mousedown was on a <menuitem> that triggers a sub-menu via
    //     `commandfor`, so we have a mousedown location stored,
    //  3. The mouseup is on a different menuitem than the mouseup, and
    //  4. The mouseup on this <menuitem> is *not* within kEpsilon layout units
    //  (post zoom, page-relative) of the location of the mousedown. I.e. the
    //  mouse was dragged at least a little bit between mousedown and mouseup.
    //  This ensures that if the new sub-menu is rendered over the top of the
    //  triggering menuitem, and the user is just "clicking" to activate the
    //  sub-menu, the menuitem under the cursor isn't selected.

    bool activate_menu_item =
        mouse_down_menuitem && !same_element && mouse_moved;
    if (!activate_menu_item) {
      return;
    }
    ActivateMenuItem();
    // This activation came from a mouse-down on a submenu invoker, so we need
    // to clear the ignore_next_command_ flag for that menuitem.
    mouse_down_menuitem->ignore_next_command_ = false;
  } else {
    DCHECK_EQ(event.type(), event_type_names::kMousedown);
    GetDocument().SetPopoverPickerPointerdown(
        {.target = this, .location = mouse_event->AbsoluteLocation()});
    if (!InvokesSubmenu()) {
      return;
    }
    // Activate sub-menus on mouse *down*, so that the user can drag and
    // release to choose a sub-menu item.
    ActivateMenuItem();
    // Because we're activating this menu item here, in mousedown, we want to
    // avoid re-triggering the same menu again in the synthetic
    // click/DOMActivate triggered command invocation.
    ignore_next_command_ = true;
  }
}

bool HTMLMenuItemElement::HandleCommandForActivation() {
  if (ignore_next_command_) {
    DCHECK(InvokesSubmenu());
    ignore_next_command_ = false;
    return false;
  }
  return HTMLElement::HandleCommandForActivation();
}

void HTMLMenuItemElement::DefaultEventHandler(Event& event) {
  if (event.type() == event_type_names::kDOMActivate && !InvokesSubmenu()) {
    // If this isn't a submenu invoker, activate it now. If it is a command
    // invoker of any kind, HTMLElement::DefaultEventHandler() will take care of
    // it, so we can't early-return here.
    ActivateMenuItem();
  }
  if (HandleKeyboardActivation(event)) {
    return;
  }
  HandleMenuPointerEvents(event);
  HandleMenuKeyboardEvents(event);
  HTMLElement::DefaultEventHandler(event);
}

HTMLMenuOwnerElement* HTMLMenuItemElement::OwningMenuElement() const {
  return owning_menu_element_;
}

void HTMLMenuItemElement::ResetAncestorElementCache() {
  owning_menu_element_ = nullptr;
  nearest_ancestor_field_set_ = nullptr;
  for (Node& ancestor : NodeTraversal::AncestorsOf(*this)) {
    if (auto* owning_menu = DynamicTo<HTMLMenuOwnerElement>(ancestor)) {
      owning_menu_element_ = owning_menu;
      break;
    }
  }
  // TODO(https://crbug.com/406566432): See if we want to allow ancestor field
  // sets higher up than just the immediate parent.
  if (HTMLFieldSetElement* field_set =
          DynamicTo<HTMLFieldSetElement>(parentNode())) {
    nearest_ancestor_field_set_ = field_set;
  }
}

Node::InsertionNotificationRequest HTMLMenuItemElement::InsertedInto(
    ContainerNode& insertion_point) {
  auto return_value = HTMLElement::InsertedInto(insertion_point);

  // Run various ancestor/state resets.
  ResetAncestorElementCache();

  // Keep this logic in sync with the checkedness logic in `ParseAttribute()`.
  if (!is_default_checkedness_overridden_) {
    const bool default_checked =
        FastHasAttribute(html_names::kDefaultcheckedAttr);
    setChecked(default_checked);
    // Re-unset this flag, since `SetChecked()` set it to true by default.
    is_default_checkedness_overridden_ = false;
  }
  return return_value;
}

void HTMLMenuItemElement::RemovedFrom(ContainerNode& insertion_point) {
  HTMLElement::RemovedFrom(insertion_point);

  // Run various ancestor/state resets.
  ResetAncestorElementCache();
  return;
}

}  // namespace blink
