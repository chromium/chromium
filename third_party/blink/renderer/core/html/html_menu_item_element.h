// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_HTML_MENU_ITEM_ELEMENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_HTML_MENU_ITEM_ELEMENT_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/html/html_element.h"

namespace blink {

class HTMLMenuBarElement;
class HTMLMenuListElement;

class CORE_EXPORT HTMLMenuItemElement final : public HTMLElement {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit HTMLMenuItemElement(Document&);
  ~HTMLMenuItemElement() override;
  void Trace(Visitor* visitor) const override;

  int index() const;

  bool Checked() const;
  void setChecked(bool);

  HTMLMenuBarElement* OwnerMenuBarElement() const;
  HTMLMenuListElement* OwnerMenuListElement() const;

  // Invoker Commands (https://github.com/whatwg/html/pull/9841)
  // TODO: Have command and commandfor attributes specced for menuitem.
  Element* commandForElement() const;
  AtomicString command() const;
  void setCommand(const AtomicString& type);
  CommandEventType GetCommandEventType(const AtomicString& type) const;

  Node::InsertionNotificationRequest InsertedInto(ContainerNode&) override;
  void RemovedFrom(ContainerNode&) override;

  bool IsDisabledFormControl() const override;
  void DefaultEventHandler(Event&) override;

  void SetDirty(bool);

 private:
  bool MatchesDefaultPseudoClass() const override;
  bool MatchesEnabledPseudoClass() const override;
  void ParseAttribute(const AttributeModificationParams&) override;

  int DefaultTabIndex() const override;
  FocusableState SupportsFocus(UpdateBehavior update_behavior) const override;
  bool IsKeyboardFocusableSlow(
      UpdateBehavior update_behavior =
          UpdateBehavior::kStyleAndLayout) const override;
  bool ShouldHaveFocusAppearance() const override;

  // Traverse ancestors to find the nearest menubar or menulist ancestor.
  void ResetNearestAncestorMenuBarOrMenuList();

  Member<HTMLMenuBarElement> nearest_ancestor_menu_bar_;
  Member<HTMLMenuListElement> nearest_ancestor_menu_list_;

  // Represents 'checkedness'.
  bool is_checked_;
  // Represents 'dirty checkness flag'. This controls whether changing the
  // checked attribute has any effect on whether the element is checked or not.
  bool is_dirty_ = false;

  friend class HTMLMenuItemElementTest;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_HTML_MENU_ITEM_ELEMENT_H_
