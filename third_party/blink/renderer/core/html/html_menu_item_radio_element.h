// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_HTML_MENU_ITEM_RADIO_ELEMENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_HTML_MENU_ITEM_RADIO_ELEMENT_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/html/html_menu_item_element_base.h"

namespace blink {

class CORE_EXPORT HTMLMenuItemRadioElement final
    : public HTMLMenuItemCheckableElementBase {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit HTMLMenuItemRadioElement(Document&);
  ~HTMLMenuItemRadioElement() override;
  ElementType GetElementType() const final {
    return ElementType::kHTMLMenuItemRadioElement;
  }

  Element* RadioGroupAncestor() const;

 protected:
  bool ClosesMenuListOnActivation() const override { return true; }
  void UpdateForExclusivity() override;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_HTML_MENU_ITEM_RADIO_ELEMENT_H_
