// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_HTML_MENU_ITEM_CHECKBOX_ELEMENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_HTML_MENU_ITEM_CHECKBOX_ELEMENT_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/html/html_menu_item_element_base.h"

namespace blink {

class CORE_EXPORT HTMLMenuItemCheckboxElement final
    : public HTMLMenuItemCheckableElementBase {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit HTMLMenuItemCheckboxElement(Document&);
  ~HTMLMenuItemCheckboxElement() override;
  ElementType GetElementType() const final {
    return ElementType::kHTMLMenuItemCheckboxElement;
  }

 protected:
  bool ClosesMenuListOnActivation() const override { return false; }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_HTML_MENU_ITEM_CHECKBOX_ELEMENT_H_
