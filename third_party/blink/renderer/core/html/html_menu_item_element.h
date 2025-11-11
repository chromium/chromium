// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_HTML_MENU_ITEM_ELEMENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_HTML_MENU_ITEM_ELEMENT_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/html/html_element.h"

namespace blink {

class HTMLFieldSetElement;
class HTMLMenuListElement;
class HTMLMenuOwnerElement;

class CORE_EXPORT HTMLMenuItemElement final : public HTMLElement {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit HTMLMenuItemElement(Document&);
  ~HTMLMenuItemElement() override;
  void Trace(Visitor* visitor) const override;

  int index() const;

  bool IsCheckable() const;
  bool checked() const;
  // This only sets `this` to checked if `IsCheckable()` is true. The return
  // value is true if this is a checkable menu item *and* a containing menu list
  // should be closed after changing the checked state.
  bool setChecked(bool);
  bool ShouldAppearChecked() const;

  HTMLMenuOwnerElement* OwningMenuElement() const;

  bool CanBeCommandInvoker() const override;

  Node::InsertionNotificationRequest InsertedInto(ContainerNode&) override;
  void RemovedFrom(ContainerNode&) override;

  bool IsDisabledFormControl() const override;
  bool IsKeyboardFocusableSlow(
      UpdateBehavior update_behavior =
          UpdateBehavior::kStyleAndLayout) const override;

  void DefaultEventHandler(Event&) override;

 private:
  bool MatchesDefaultPseudoClass() const override;
  bool MatchesEnabledPseudoClass() const override;
  void ParseAttribute(const AttributeModificationParams&) override;

  int DefaultTabIndex() const override;
  FocusableState SupportsFocus(UpdateBehavior update_behavior) const override;
  bool ShouldHaveFocusAppearance() const override;

  HTMLElement* InvokesSubmenuOrPopover() const;
  HTMLMenuListElement* InvokesSubmenu() const;
  // This is generally used when a menuitem has been selected, and the "tree" of
  // menus should now close. It finds the innermost (nearest ancestor) menulist
  // containing this menuitem, and then walks the tree of command invokers up
  // to find any nested containing menulist's. It then closes the outermost
  // such menulist, which (via popover close behavior) closes the tree.
  Element* CloseOutermostContainingMenuList();
  void ActivateMenuItem();
  bool HandleMenuPointerEvents(Event&);
  void HandleMenuKeyboardEvents(Event&);
  bool HasOwnerMenuList() const;

  // Traverse ancestors to find the nearest menubars, menulists, and fieldsets,
  // and cache them.
  void ResetAncestorElementCache();

  Member<HTMLMenuOwnerElement> owning_menu_element_;
  // Could be null: only used to allow `this` to be checkable, if
  // `this` is immediately nested inside a `<fieldset checkable>`.
  Member<HTMLFieldSetElement> nearest_ancestor_field_set_;

  // Represents 'checkedness'.
  bool is_checked_;
  // This is used to avoid double-invoking target menus and popovers.
  bool ignore_next_dom_activate_ = false;

  friend class HTMLMenuItemElementTest;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_HTML_MENU_ITEM_ELEMENT_H_
