// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/html_menu_item_element.h"

#include "third_party/blink/public/common/input/web_pointer_properties.h"
#include "third_party/blink/renderer/core/css/selector_checker.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
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
  // TODO(406566432): This should consider the `defaultchecked` when
  // implemented.
  return false;
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
             html_names::kCheckableAttr);
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
  // Menuitems in menulist should be traversed using arrow keys and not tabbing.
  if (HasOwnerMenuList()) {
    return -1;
  }
  return 0;
}

bool HTMLMenuItemElement::ShouldHaveFocusAppearance() const {
  return SelectorChecker::MatchesFocusVisiblePseudoClass(*this);
}

HTMLElement* HTMLMenuItemElement::InvokesSubmenuOrPopover() const {
  HTMLElement* invoked_element = DynamicTo<HTMLElement>(commandForElement());
  if (!invoked_element || !invoked_element->IsPopover()) {
    return nullptr;
  }
  CommandEventType type = GetCommandEventType(
      FastGetAttribute(html_names::kCommandAttr), GetExecutionContext());
  if (type != CommandEventType::kTogglePopover &&
      type != CommandEventType::kShowPopover &&
      type != CommandEventType::kHidePopover &&
      type != CommandEventType::kToggleMenu &&
      type != CommandEventType::kShowMenu &&
      type != CommandEventType::kHideMenu) {
    return nullptr;
  }
  return invoked_element;
}

HTMLMenuListElement* HTMLMenuItemElement::InvokesSubmenu() const {
  return DynamicTo<HTMLMenuListElement>(InvokesSubmenuOrPopover());
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
    // a sub-menu or a popover.
    return !InvokesSubmenuOrPopover();
  }

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
  if (!IsCheckable() && InvokesSubmenuOrPopover()) {
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
  if ((key == " " || key == keywords::kCapitalEnter)) {
    // TODO(crbug.com/425682465): implement chooseItem(event).
    // TODO: Do we need this?
    return;
  }

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

bool HTMLMenuItemElement::HandleMenuPointerEvents(Event& event) {
  // This implements the special "mouse down, drag to menu item, mouse up"
  // behavior. This is a mouse-only behavior - it should not apply to pointer
  // events for touchscreens. Touch events will be handled by the normal input
  // system behavior of sending a DOMActivate event. This also does not apply
  // to checkable menu items, which also rely on DOMActivate.
  const auto* mouse_event = DynamicTo<MouseEvent>(event);
  if (!mouse_event || mouse_event->FromTouch() ||
      mouse_event->button() !=
          static_cast<int16_t>(WebPointerProperties::Button::kLeft) ||
      (event.type() != event_type_names::kMouseup &&
       event.type() != event_type_names::kMousedown)) {
    return false;
  }

  if (event.type() == event_type_names::kMouseup) {
    // We leave the picker open, and do not "pick" a menu item, iff:
    //  1. The mousedown was on a <menuitem> that triggers a sub-menu via
    //     `commandfor`, so we have a mousedown location stored, and
    //  2. The mouseup on this <menuitem> was within kEpsilon layout units
    //     (post zoom, page-relative) of the location of the mousedown. I.e.
    //     the mouse was not dragged between mousedown and mouseup. I.e. this
    //     was just a "click" to open the menuitem's sub-menu - it shouldn't
    //     pick anything yet.
    std::optional<gfx::PointF> mouse_down_loc =
        GetDocument().PopoverPickerMousedownLocation();
    GetDocument().SetPopoverPickerMousedownLocation(std::nullopt);
    // TODO(masonf) This kEpsilon should be combined with the one in
    // html_option_element.cc.
    constexpr float kEpsilon = 5;  // 5 pixels in any direction
    bool activate_menu_item = !mouse_down_loc.has_value() ||
                              !mouse_down_loc->IsWithinDistance(
                                  mouse_event->AbsoluteLocation(), kEpsilon);
    if (activate_menu_item) {
      // The mouse moved, so select this menu item.
      ActivateMenuItem();
    }
    // TODO(crbug.com/406566432): This is a hack, and isn't strictly correct.
    // We need a better way to ignore the synthetic `click` that triggers the
    // `DOMActivate` that would double-trigger the menu in this case.
    ignore_next_dom_activate_ = true;
  } else {
    DCHECK_EQ(event.type(), event_type_names::kMousedown);
    if (!InvokesSubmenu()) {
      return false;
    }
    GetDocument().SetPopoverPickerMousedownLocation(
        mouse_event->FromTouch()
            ? std::nullopt
            : std::optional(mouse_event->AbsoluteLocation()));
    // Activate sub-menus on mouse *down*, so that the user can drag and release
    // to choose a sub-menu item.
    ActivateMenuItem();
  }
  return true;
}

void HTMLMenuItemElement::DefaultEventHandler(Event& event) {
  if (HandleKeyboardActivation(event)) {
    return;
  }
  if (event.type() == event_type_names::kDOMActivate) {
    if (ignore_next_dom_activate_) {
      ignore_next_dom_activate_ = false;
    } else {
      ActivateMenuItem();
    }
    return;
  }
  if (HandleMenuPointerEvents(event)) {
    return;
  }
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
  return return_value;
}

void HTMLMenuItemElement::RemovedFrom(ContainerNode& insertion_point) {
  HTMLElement::RemovedFrom(insertion_point);

  // Run various ancestor/state resets.
  ResetAncestorElementCache();
  return;
}

}  // namespace blink
