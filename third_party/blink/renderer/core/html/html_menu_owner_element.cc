// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/html_menu_owner_element.h"

#include "third_party/blink/renderer/core/dom/popover_data.h"
#include "third_party/blink/renderer/core/html/html_menu_item_element.h"
#include "third_party/blink/renderer/core/html/menu_item_list.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

HTMLMenuOwnerElement::HTMLMenuOwnerElement(HTMLQualifiedName tag_name,
                                           Document& document)
    : HTMLElement(tag_name, document) {
  DCHECK(RuntimeEnabledFeatures::MenuElementsEnabled());
}

bool HTMLMenuOwnerElement::IsValidBuiltinCommand(HTMLElement& invoker,
                                                 CommandEventType command) {
  return HTMLElement::IsValidBuiltinCommand(invoker, command) ||
         command == CommandEventType::kToggleMenu ||
         command == CommandEventType::kShowMenu ||
         command == CommandEventType::kHideMenu;
}

MenuItemList HTMLMenuOwnerElement::ItemList() {
  return MenuItemList(*this);
}

}  // namespace blink
