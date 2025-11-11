// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_HTML_MENU_OWNER_ELEMENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_HTML_MENU_OWNER_ELEMENT_H_

#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/html_names.h"

namespace blink {

class MenuItemList;

class CORE_EXPORT HTMLMenuOwnerElement : public HTMLElement {
 public:
  bool IsValidBuiltinCommand(HTMLElement& invoker,
                             CommandEventType command) override;
  // This returns an iterable list of menuitems whose owner is this.
  MenuItemList ItemList();

 protected:
  HTMLMenuOwnerElement(HTMLQualifiedName, Document&);
};

template <>
struct DowncastTraits<HTMLMenuOwnerElement> {
  static bool AllowFrom(const Node& node) {
    return node.HasTagName(html_names::kMenulistTag) ||
           node.HasTagName(html_names::kMenubarTag);
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_HTML_MENU_OWNER_ELEMENT_H_
