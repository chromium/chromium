// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/html_menu_list_element.h"

#include "third_party/blink/renderer/core/dom/popover_data.h"
#include "third_party/blink/renderer/core/html/html_menu_item_element.h"
#include "third_party/blink/renderer/core/html_names.h"

namespace blink {

HTMLMenuListElement::HTMLMenuListElement(Document& document)
    : HTMLElement(html_names::kMenulistTag, document) {
  // menulist is always a popover and should have popover data with type auto.
  DCHECK(RuntimeEnabledFeatures::MenuElementsEnabled());
  EnsurePopoverData().setType(PopoverValueType::kAuto);
}

bool HTMLMenuListElement::IsValidBuiltinCommand(HTMLElement& invoker,
                                                CommandEventType command) {
  return HTMLElement::IsValidBuiltinCommand(invoker, command) ||
         command == CommandEventType::kToggleMenu ||
         command == CommandEventType::kShowMenu ||
         command == CommandEventType::kHideMenu;
}

}  // namespace blink
