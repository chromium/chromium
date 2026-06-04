// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/html_menu_list_element.h"

#include "third_party/blink/renderer/core/dom/popover_data.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/html/html_menu_item_element.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/input/event_handler.h"

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
  if (command != CommandEventType::kToggleMenu) {
    return false;
  }

  auto& document = GetDocument();
  bool can_show =
      IsPopoverReady(PopoverTriggerAction::kShow,
                     /*exception_state=*/nullptr,
                     /*include_event_handler_text=*/true, &document);
  // command=toggle-menu will never actually close a menu, just move focus into
  // it. If it is demonstrated that closing a menu is ever desired, then we
  // could change this.

  // This flag is used to determine whether we should move focus into this
  // menulist from the invoker. We only move focus into the menulist when
  // handling keyboard input for a few reasons:
  // 1. When clicking an invoker with a pointer input, the invoker gets focused
  //    after command invokers are run. This means that if we try to focus a new
  //    element in this method, it will just get reset to the invoker later.
  // 2. This matches the behavior of the MacOS system menu, which can be
  //    observed by clicking the menubar to open a submenu and then pressing the
  //    down arrow.
  // 3. In the OpenUI discussion about this behavior, it was pointed out that
  //    this behavior may be preferred:
  //    https://github.com/openui/open-ui/issues/1439#issuecomment-4440864582
  bool handling_keyboard_event = false;
  if (LocalFrame* frame = document.GetFrame()) {
    handling_keyboard_event = frame->GetEventHandler().IsHandlingKeyEvent();
  }

  // If the triggering invoker is a `<menuitem>` that is also checkable, then
  // the `return true`'s below will cause the checkable behavior not to fire.
  if (can_show) {
    // TODO(crbug.com/1121840) HandleCommandInternal is called for both
    // `popovertarget` and `commandfor`.
    InvokePopover(invoker);
    if (handling_keyboard_event) {
      FocusFirstItem();
    }
    return true;
  }
  if (popoverOpen()) {
    if (handling_keyboard_event) {
      FocusFirstItem();
    }
    return true;
  }
  return false;
}

HTMLMenuItemElement* HTMLMenuListElement::InvokingMenuItem() {
  if (!popoverOpen()) {
    return nullptr;
  }
  return DynamicTo<HTMLMenuItemElement>(GetPopoverData()->invoker());
}

bool HTMLMenuListElement::FocusFirstItem() {
  if (auto* first = ItemList().NextFocusableElement(*ItemList().begin(),
                                                    /*inclusive=*/true)) {
    first->Focus(FocusParams(FocusTrigger::kUserGesture));
    return true;
  }
  return false;
}

bool HTMLMenuListElement::FocusLastItem() {
  if (auto* last = ItemList().PreviousFocusableElement(*ItemList().last(),
                                                       /*inclusive=*/true)) {
    last->Focus(FocusParams(FocusTrigger::kUserGesture));
    return true;
  }
  return false;
}

PopoverHideResult HTMLMenuListElement::HidePopoverInternal(
    Element* invoker,
    HidePopoverFocusBehavior focus_behavior,
    HidePopoverTransitionBehavior event_firing,
    ExceptionState* exception_state) {
  Element* opening_invoker =
      GetPopoverData() ? GetPopoverData()->invoker() : nullptr;
  PopoverHideResult result = HTMLMenuOwnerElement::HidePopoverInternal(
      invoker, focus_behavior, event_firing, exception_state);
  if (auto* opening_menuitem =
          DynamicTo<HTMLMenuItemElement>(opening_invoker)) {
    // menuitem elements which invoke submenus support the :open pseudo-class.
    // If this menu was closed via hidePopover(), then the menuitem which
    // invoked this menulist should have its :open updated.
    opening_menuitem->OpenPseudoChanged();
  }
  return result;
}

}  // namespace blink
