// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/html_menu_item_element.h"

#include "third_party/blink/renderer/core/css/selector_checker.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/events/command_event.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
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
  HTMLElement::Trace(visitor);
}

bool HTMLMenuItemElement::MatchesDefaultPseudoClass() const {
  return FastHasAttribute(html_names::kCheckedAttr);
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
  } else if (name == html_names::kCheckedAttr) {
    if (params.old_value.IsNull() != params.new_value.IsNull() && !is_dirty_) {
      setChecked(!params.new_value.IsNull());
      is_dirty_ = false;
    }
    PseudoStateChanged(CSSSelector::kPseudoDefault);
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
  return CommandEventType::kNone;
}

bool HTMLMenuItemElement::Checked() const {
  return is_checked_;
}

void HTMLMenuItemElement::setChecked(bool checked) {
  is_dirty_ = true;
  if (is_checked_ == checked) {
    return;
  }

  is_checked_ = checked;
  PseudoStateChanged(CSSSelector::kPseudoChecked);

  // TODO: Accessibility mapping.
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
  // Interest invoker targets with partial interest aren't keyboard focusable.
  if (IsInPartialInterestPopover()) {
    CHECK(RuntimeEnabledFeatures::HTMLInterestTargetAttributeEnabled(
        GetDocument().GetExecutionContext()));
    return false;
  }
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
  // TODO: Handle activation behavior events.
  // TODO: Handle arrow key navigation for menuitems in menulist.
  if (event.type() == event_type_names::kDOMActivate) {
    // Menuitems with a commandfor will dispatch a CommandEvent on the
    // invoker, and run HandleCommandInternal to perform default logic.
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

  if (HandleKeyboardActivation(event)) {
    return;
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

Node::InsertionNotificationRequest HTMLMenuItemElement::InsertedInto(
    ContainerNode& insertion_point) {
  auto return_value = HTMLElement::InsertedInto(insertion_point);
  ResetNearestAncestorMenuBarOrMenuList();
  return return_value;
}

void HTMLMenuItemElement::RemovedFrom(ContainerNode& insertion_point) {
  HTMLElement::RemovedFrom(insertion_point);
  ResetNearestAncestorMenuBarOrMenuList();
  return;
}

}  // namespace blink
