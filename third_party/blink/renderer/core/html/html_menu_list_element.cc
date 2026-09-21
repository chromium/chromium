// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/html_menu_list_element.h"

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/popover_data.h"
#include "third_party/blink/renderer/core/events/ui_event.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/html/html_menu_item_element.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"

namespace blink {

HTMLMenuListElement::HTMLMenuListElement(Document& document)
    : HTMLMenuOwnerElement(html_names::kMenulistTag, document) {
  UseCounter::Count(document, WebFeature::kHTMLMenuListElement);
  // <menulist> is always a popover and should have popover data with type auto.
  DCHECK(RuntimeEnabledFeatures::MenuElementsEnabled());
  EnsurePopoverData().setType(PopoverValueType::kAuto);
}

bool HTMLMenuListElement::HandleCommandInternal(HTMLElement& invoker,
                                                CommandEventType command,
                                                UIEvent* activate_event) {
  DCHECK(RuntimeEnabledFeatures::MenuElementsEnabled());
  bool result =
      HTMLElement::HandleCommandInternal(invoker, command, activate_event);
  if (result &&
      (command == CommandEventType::kShowPopover ||
       command == CommandEventType::kTogglePopover) &&
      popoverOpen()) {
    if (HTMLMenuItemElement::IsActivationFromKeyboard(activate_event)) {
      FocusTrigger trigger = activate_event->IsFullyTrusted()
                                 ? FocusTrigger::kUserGesture
                                 : FocusTrigger::kScript;
      FocusFirstItem(trigger);
    }
  }
  return result;
}

HTMLMenuItemElement* HTMLMenuListElement::InvokingMenuItem() {
  if (!popoverOpen()) {
    return nullptr;
  }
  return DynamicTo<HTMLMenuItemElement>(GetPopoverData()->invoker());
}

bool HTMLMenuListElement::FocusFirstItem(FocusTrigger focus_trigger) {
  if (auto* first = ItemList().NextFocusableElement(*ItemList().begin(),
                                                    /*inclusive=*/true)) {
    first->Focus(FocusParams(focus_trigger));
    return true;
  }
  return false;
}

bool HTMLMenuListElement::FocusLastItem(FocusTrigger focus_trigger) {
  if (auto* last = ItemList().PreviousFocusableElement(*ItemList().last(),
                                                       /*inclusive=*/true)) {
    last->Focus(FocusParams(focus_trigger));
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
