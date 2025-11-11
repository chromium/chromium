// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/html_menu_list_element.h"

#include "third_party/blink/renderer/core/dom/popover_data.h"
#include "third_party/blink/renderer/core/html/html_menu_item_element.h"
#include "third_party/blink/renderer/core/html_names.h"

namespace blink {

HTMLMenuListElement::HTMLMenuListElement(Document& document)
    : HTMLMenuOwnerElement(html_names::kMenulistTag, document) {
  // <menulist> is always a popover and should have popover data with type auto.
  DCHECK(RuntimeEnabledFeatures::MenuElementsEnabled());
  EnsurePopoverData().setType(PopoverValueType::kAuto);
}

bool HTMLMenuListElement::HandleCommandInternal(HTMLElement& invoker,
                                                CommandEventType command) {
  DCHECK(RuntimeEnabledFeatures::MenuElementsEnabled());
  if (!IsValidBuiltinCommand(invoker, command)) {
    return false;
  }
  if (HTMLElement::HandleCommandInternal(invoker, command)) {
    return true;
  }
  if (command != CommandEventType::kToggleMenu &&
      command != CommandEventType::kShowMenu &&
      command != CommandEventType::kHideMenu) {
    return false;
  }

  auto& document = GetDocument();
  bool can_show =
      IsPopoverReady(PopoverTriggerAction::kShow,
                     /*exception_state=*/nullptr,
                     /*include_event_handler_text=*/true, &document) &&
      (command == CommandEventType::kToggleMenu ||
       command == CommandEventType::kShowMenu);
  bool can_hide =
      IsPopoverReady(PopoverTriggerAction::kHide,
                     /*exception_state=*/nullptr,
                     /*include_event_handler_text=*/true, &document) &&
      (command == CommandEventType::kToggleMenu ||
       command == CommandEventType::kHideMenu);
  // If the triggering invoker is a `<menuitem>` that is also checkable, then
  // the `return true`'s below will cause the checkable behavior not to fire.
  if (can_hide) {
    HidePopoverInternal(
        &invoker, HidePopoverFocusBehavior::kFocusPreviousElement,
        HidePopoverTransitionBehavior::kFireEventsAndWaitForTransitions,
        /*exception_state=*/nullptr);
    return true;
  } else if (can_show) {
    // TODO(crbug.com/1121840) HandleCommandInternal is called for both
    // `popovertarget` and `commandfor`.
    InvokePopover(invoker);
    return true;
  }
  return false;
}

}  // namespace blink
