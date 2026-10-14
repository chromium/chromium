// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_HTML_MENU_ITEM_ELEMENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_HTML_MENU_ITEM_ELEMENT_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/html/html_menu_item_element_base.h"

namespace blink {

class HTMLMenuListElement;

class CORE_EXPORT HTMLMenuItemElement final : public HTMLMenuItemElementBase {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit HTMLMenuItemElement(Document&);
  ~HTMLMenuItemElement() override;
  ElementType GetElementType() const final {
    return ElementType::kHTMLMenuItemElement;
  }

  Node::InsertionNotificationRequest InsertedInto(ContainerNode&) override;

  bool CanBeCommandInvoker() const override;
  bool IsValidInterestInvoker(Element& target) const override;

  HTMLMenuListElement* GetInvokedSubmenu() const;
  bool ShouldHaveExpandIcon() const;
  bool IsSubmenuOpen() const;
  void OpenPseudoChanged();

 protected:
  void ActivateMenuItem(ActivationKeyboardEventType) override;

 private:
  friend class HTMLMenuItemElementTest;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_HTML_MENU_ITEM_ELEMENT_H_
