// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/html_menu_item_element.h"

#include "third_party/blink/renderer/core/css/selector_checker.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/dom/popover_data.h"
#include "third_party/blink/renderer/core/events/command_event.h"
#include "third_party/blink/renderer/core/events/keyboard_event.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/html/forms/html_field_set_element.h"
#include "third_party/blink/renderer/core/html/html_menu_bar_element.h"
#include "third_party/blink/renderer/core/html/html_menu_list_element.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/keywords.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

HTMLMenuItemElement::HTMLMenuItemElement(Document& document)
    : HTMLElement(html_names::kMenuitemTag, document), is_checked_(false) {}

HTMLMenuItemElement::~HTMLMenuItemElement() = default;

void HTMLMenuItemElement::Trace(Visitor* visitor) const {
  visitor->Trace(nearest_ancestor_menu_bar_);
  visitor->Trace(nearest_ancestor_menu_list_);
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

Element* HTMLMenuItemElement::commandForElement() const {
  if (!IsInTreeScope() || IsDisabledFormControl()) {
    return nullptr;
  }

  return GetElementAttributeResolvingReferenceTarget(
      html_names::kCommandforAttr);
}

void HTMLMenuItemElement::setCommand(const AtomicString& type) {
  setAttribute(html_names::kCommandAttr, type);
}

AtomicString HTMLMenuItemElement::command() const {
  const AtomicString& action = FastGetAttribute(html_names::kCommandAttr);
  CommandEventType type = GetCommandEventType(action);
  switch (type) {
    case CommandEventType::kNone:
      return g_empty_atom;
    case CommandEventType::kCustom:
      return action;
    default: {
      const AtomicString& lower_action = action.LowerASCII();
      DCHECK_EQ(GetCommandEventType(lower_action), type);
      return lower_action;
    }
  }
}

CommandEventType HTMLMenuItemElement::GetCommandEventType(
    const AtomicString& action) const {
  if (action.IsNull() || action.empty()) {
    return CommandEventType::kNone;
  }

  // Custom Invoke Action
  if (action.StartsWith("--")) {
    return CommandEventType::kCustom;
  }

  // Popover cases.
  if (EqualIgnoringASCIICase(action, keywords::kTogglePopover)) {
    return CommandEventType::kTogglePopover;
  }
  if (EqualIgnoringASCIICase(action, keywords::kShowPopover)) {
    return CommandEventType::kShowPopover;
  }
  if (EqualIgnoringASCIICase(action, keywords::kHidePopover)) {
    return CommandEventType::kHidePopover;
  }
  // Menu specific cases.
  if (EqualIgnoringASCIICase(action, keywords::kToggleMenu)) {
    return CommandEventType::kToggleMenu;
  }
  if (EqualIgnoringASCIICase(action, keywords::kShowMenu)) {
    return CommandEventType::kShowMenu;
  }
  if (EqualIgnoringASCIICase(action, keywords::kHideMenu)) {
    return CommandEventType::kHideMenu;
  }
  return CommandEventType::kNone;
}

bool HTMLMenuItemElement::IsCheckable() const {
  return nearest_ancestor_menu_list_ && nearest_ancestor_field_set_ &&
         nearest_ancestor_field_set_->FastGetAttribute(
             html_names::kCheckableAttr);
}

bool HTMLMenuItemElement::checked() const {
  return is_checked_;
}

void HTMLMenuItemElement::setChecked(bool checked) {
  is_dirty_ = true;
  // Some menu items are not "checkable", and the `checked` IDL attribute is
  // only stateful for checkable menu items.
  if (is_checked_ == checked || (checked && !IsCheckable())) {
    return;
  }

  is_checked_ = checked;
  PseudoStateChanged(CSSSelector::kPseudoChecked);

  // Only update the exclusivity of all other menu items rooted under the same
  // fieldset *if* `this` is becoming checked under a fieldset that enforces
  // exclusivity. If it is becoming unchecked, we don't have to worry about
  // manually unchecking other menu items in the exclusive set, because it is
  // permitted to have zero menu items checked.
  DCHECK(nearest_ancestor_field_set_);
  const AtomicString& checkable =
      nearest_ancestor_field_set_->FastGetAttribute(html_names::kCheckableAttr);
  if (is_checked_ && EqualIgnoringASCIICase(checkable, keywords::kSingle)) {
    nearest_ancestor_field_set_->UpdateMenuItemCheckableExclusivity(this);
  }

  // TODO(crbug.com/425682466): Accessibility mapping.
}

bool HTMLMenuItemElement::ShouldAppearChecked() const {
  // `this` should only appear checked if we are checked, and we're in a
  // checkable <fieldset> in a <menulist>.
  return IsCheckable() && checked();
}

void HTMLMenuItemElement::SetDirty(bool value) {
  is_dirty_ = value;
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
  if (nearest_ancestor_menu_list_) {
    return -1;
  }
  return 0;
}

bool HTMLMenuItemElement::ShouldHaveFocusAppearance() const {
  return SelectorChecker::MatchesFocusVisiblePseudoClass(*this);
}

void HTMLMenuItemElement::DefaultEventHandler(Event& event) {
  if (event.type() == event_type_names::kDOMActivate) {
    // A menu item's checkability and ability to invoke a command are
    // exclusive. That is, we don't explicitly disallow checkable menu items
    // that do both, so we always give `setChecked()` the chance to set `this`
    // as checked—this will only take effect if `IsCheckable()` is true.
    setChecked(!checked());

    // Menuitems with a commandfor will dispatch a CommandEvent on the
    // target of the invoker, and run HandleCommandInternal to perform default
    // logic.
    if (auto* command_target = commandForElement()) {
      auto action =
          GetCommandEventType(FastGetAttribute(html_names::kCommandAttr));
      bool is_valid_builtin =
          command_target->IsValidBuiltinCommand(*this, action);
      bool should_dispatch =
          is_valid_builtin || action == CommandEventType::kCustom;
      if (should_dispatch) {
        Event* commandEvent =
            CommandEvent::Create(event_type_names::kCommand, command(), this);
        command_target->DispatchEvent(*commandEvent);
        if (is_valid_builtin && !commandEvent->defaultPrevented()) {
          command_target->HandleCommandInternal(*this, action);
        }
      }
      return;
    }
  }
  // Handle arrow key navigation for menuitems.
  if (HandleKeyboardActivation(event)) {
    return;
  }
  auto* keyboard_event = DynamicTo<KeyboardEvent>(event);
  int tab_ignore_modifiers = WebInputEvent::kControlKey |
                             WebInputEvent::kAltKey | WebInputEvent::kMetaKey;
  int ignore_modifiers = WebInputEvent::kShiftKey | tab_ignore_modifiers;
  FocusParams focus_params(FocusTrigger::kUserGesture);

  if (keyboard_event && event.type() == event_type_names::kKeydown) {
    const AtomicString key(keyboard_event->key());
    // TODO(crbug.com/425708944): This is the same ignore list as option event
    // handling and can be consolidated together.
    if (!(keyboard_event->GetModifiers() & ignore_modifiers)) {
      if ((key == " " || key == keywords::kCapitalEnter)) {
        // TODO(crbug.com/425682465): implement chooseItem(event).
        return;
      }
      if (auto* menulist = OwnerMenuListElement()) {
        MenuItemList menuitems = menulist->GetItemList();
        // Nothing below can do anything, if the list is empty.
        if (menuitems.Empty()) {
          return;
        }
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
          if (auto* first = menuitems.NextFocusableMenuItem(
                  *menuitems.begin(), /*inclusive*/ true)) {
            first->Focus(focus_params);
            event.SetDefaultHandled();
            return;
          }
        } else if (key == keywords::kEnd) {
          if (auto* last = menuitems.PreviousFocusableMenuItem(
                  *menuitems.last(), /*inclusive*/ true)) {
            last->Focus(focus_params);
            event.SetDefaultHandled();
            return;
          }
        } else if (key == keywords::kArrowRight) {
          auto& document = GetDocument();
          // If this invokes a menulist and is itself in a menulist, then
          // arrow right should open the invoked menulist and focus its first
          // menuitem.
          if (auto* invoked_menulist =
                  DynamicTo<HTMLMenuListElement>(commandForElement())) {
            CommandEventType type =
                GetCommandEventType(FastGetAttribute(html_names::kCommandAttr));
            bool can_show =
                (type == CommandEventType::kTogglePopover ||
                 type == CommandEventType::kShowPopover ||
                 type == CommandEventType::kToggleMenu ||
                 type == CommandEventType::kShowMenu) &&
                invoked_menulist->IsPopoverReady(
                    PopoverTriggerAction::kShow,
                    /*exception_state=*/nullptr,
                    /*include_event_handler_text=*/false, &document);
            if (can_show) {
              invoked_menulist->InvokePopover(*this);
            }
            MenuItemList invoked_menuitems = invoked_menulist->GetItemList();
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
            HTMLElement* ancestor_menulist = menulist;
            Element* invoker = nullptr;
            // While the ancestor is an open menulist, it should be closed.
            while (IsA<HTMLMenuListElement>(ancestor_menulist) &&
                   ancestor_menulist->popoverOpen()) {
              invoker = ancestor_menulist->GetPopoverData()->invoker();
              ancestor_menulist = const_cast<HTMLElement*>(
                  HTMLElement::TopLayerElementPopoverAncestor(
                      *ancestor_menulist, TopLayerElementType::kPopover));
            }
            HTMLElement::HideAllPopoversUntil(
                ancestor_menulist, document, HidePopoverFocusBehavior::kNone,
                HidePopoverTransitionBehavior::
                    kFireEventsAndWaitForTransitions);
            if (invoker) {
              // If ancestor menulist is invoked from a menubar, focus on next
              // menuitem within the menubar.
              if (HTMLMenuItemElement* invoker_menuitem =
                      DynamicTo<HTMLMenuItemElement>(invoker)) {
                if (auto* ancestor_menubar =
                        invoker_menuitem->OwnerMenuBarElement()) {
                  MenuItemList ancestor_menuitems =
                      ancestor_menubar->GetItemList();
                  if (auto* next = ancestor_menuitems.NextFocusableMenuItem(
                          *invoker_menuitem)) {
                    next->Focus(focus_params);
                    event.SetDefaultHandled();
                    return;
                  }
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
          Element* invoker = menulist->GetPopoverData()->invoker();
          bool can_hide = menulist->IsPopoverReady(
              PopoverTriggerAction::kHide,
              /*exception_state=*/nullptr,
              /*include_event_handler_text=*/false, &GetDocument());
          if (can_hide) {
            menulist->HidePopoverInternal(
                invoker, HidePopoverFocusBehavior::kNone,
                HidePopoverTransitionBehavior::kFireEventsAndWaitForTransitions,
                /*exception_state=*/nullptr);
          }
          if (auto* invoker_menuitem =
                  DynamicTo<HTMLMenuItemElement>(invoker)) {
            if (auto* first_menubar = invoker_menuitem->OwnerMenuBarElement()) {
              // Focus on previous if it is in menubar.
              MenuItemList first_menuitems = first_menubar->GetItemList();
              if (auto* previous = first_menuitems.PreviousFocusableMenuItem(
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
      } else if (auto* menubar = OwnerMenuBarElement()) {
        MenuItemList menuitems = menubar->GetItemList();
        // Nothing below can do anything, if the list is empty.
        if (menuitems.Empty()) {
          return;
        }
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
          if (auto* first = menuitems.NextFocusableMenuItem(
                  *menuitems.begin(), /*inclusive*/ true)) {
            first->Focus(focus_params);
            event.SetDefaultHandled();
            return;
          }
        } else if (key == keywords::kEnd) {
          if (auto* last = menuitems.PreviousFocusableMenuItem(
                  *menuitems.last(), /*inclusive*/ true)) {
            last->Focus(focus_params);
            event.SetDefaultHandled();
            return;
          }
        } else if (key == keywords::kArrowDown || key == keywords::kArrowUp) {
          // If this invokes a menulist and is in a menubar, then arrow down/up
          // should open the menulist and go to first/last menuitem in it.
          if (auto* invoked_menulist =
                  DynamicTo<HTMLMenuListElement>(commandForElement())) {
            CommandEventType type =
                GetCommandEventType(FastGetAttribute(html_names::kCommandAttr));
            bool can_show =
                (type == CommandEventType::kTogglePopover ||
                 type == CommandEventType::kShowPopover ||
                 type == CommandEventType::kToggleMenu ||
                 type == CommandEventType::kShowMenu) &&
                invoked_menulist->IsPopoverReady(
                    PopoverTriggerAction::kShow,
                    /*exception_state=*/nullptr,
                    /*include_event_handler_text=*/false, &GetDocument());
            if (can_show) {
              invoked_menulist->InvokePopover(*this);
            }
            MenuItemList invoked_menuitems = invoked_menulist->GetItemList();
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
  }
  HTMLElement::DefaultEventHandler(event);
}

HTMLMenuBarElement* HTMLMenuItemElement::OwnerMenuBarElement() const {
  return nearest_ancestor_menu_bar_;
}

HTMLMenuListElement* HTMLMenuItemElement::OwnerMenuListElement() const {
  return nearest_ancestor_menu_list_;
}

void HTMLMenuItemElement::ResetNearestAncestorMenuBarOrMenuList() {
  nearest_ancestor_menu_bar_ = nullptr;
  nearest_ancestor_menu_list_ = nullptr;
  for (Node& ancestor : NodeTraversal::AncestorsOf(*this)) {
    if (auto* menu_bar = DynamicTo<HTMLMenuBarElement>(ancestor)) {
      nearest_ancestor_menu_bar_ = menu_bar;
      break;
    } else if (auto* menu_list = DynamicTo<HTMLMenuListElement>(ancestor)) {
      nearest_ancestor_menu_list_ = menu_list;
      break;
    }
  }
}

void HTMLMenuItemElement::ResetNearestAncestorFieldSet() {
  nearest_ancestor_field_set_ = nullptr;
  // TODO(https://crbug.com/406566432): See if we want to allow ancestor field
  // sets higher up than just the immediate parent.
  HTMLFieldSetElement* field_set = DynamicTo<HTMLFieldSetElement>(parentNode());
  if (!field_set) {
    return;
  }

  nearest_ancestor_field_set_ = field_set;
}

Node::InsertionNotificationRequest HTMLMenuItemElement::InsertedInto(
    ContainerNode& insertion_point) {
  auto return_value = HTMLElement::InsertedInto(insertion_point);

  // Run various ancestor/state resets.
  ResetNearestAncestorMenuBarOrMenuList();
  ResetNearestAncestorFieldSet();
  return return_value;
}

void HTMLMenuItemElement::RemovedFrom(ContainerNode& insertion_point) {
  HTMLElement::RemovedFrom(insertion_point);

  // Run various ancestor/state resets.
  ResetNearestAncestorMenuBarOrMenuList();
  ResetNearestAncestorFieldSet();
  return;
}

}  // namespace blink
