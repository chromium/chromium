// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_HTML_MENU_ITEM_ELEMENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_HTML_MENU_ITEM_ELEMENT_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/html/html_element.h"

namespace blink {
class CORE_EXPORT HTMLMenuItemElement final : public HTMLElement {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit HTMLMenuItemElement(Document&);
  ~HTMLMenuItemElement() override;

  int index() const;

  bool Checked() const;
  void setChecked(bool);

  // TODO
  // HTMLMenuBarElement* OwnerMenuBarElement(bool skip_check = false) const;
  // HTMLMenuListElement* OwnerMenuListElement(bool skip_check = false) const;
  // void SetOwnerMenuBarElement(HTMLMenuBarElement*);
  // void SetOwnerMenuListElement(HTMLMenuListElement*);

  bool IsDisabledFormControl() const override;
  void DefaultEventHandler(Event&) override;

  void SetDirty(bool);

 private:
  bool MatchesDefaultPseudoClass() const override;
  bool MatchesEnabledPseudoClass() const override;
  void ParseAttribute(const AttributeModificationParams&) override;

  // TODO
  // Member<HTMLMenuBarElement> nearest_ancestor_menu_bar_;
  // Member<HTMLMenuBarElement> nearest_ancestor_menu_list_;

  // Represents 'checkedness'.
  bool is_checked_;
  // Represents 'dirty checkness flag'. This controls whether changing the
  // checked attribute has any effect on whether the element is checked or not.
  bool is_dirty_ = false;

  friend class HTMLMenuItemElementTest;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_HTML_MENU_ITEM_ELEMENT_H_
