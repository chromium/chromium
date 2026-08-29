// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_HTML_MENU_OWNER_ELEMENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_HTML_MENU_OWNER_ELEMENT_H_

#include "third_party/blink/renderer/core/html/forms/option_list.h"
#include "third_party/blink/renderer/core/html/forms/type_ahead.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/html_names.h"

namespace blink {

class HTMLMenuItemElement;
class MenuMutationObserver;
class CORE_EXPORT HTMLMenuOwnerElement : public HTMLElement,
                                         public TypeAheadDataSource {
 public:
  // This returns an iterable list of menuitems whose owner is this.
  MenuItemList ItemList() const;

  bool ShouldIgnoreDescendantsForElementTraversals(Element* element) const;
  bool IsTopLevelOwner() const;

  void DefaultEventHandler(Event&) override;

  void Trace(Visitor*) const override;

  // TypeAheadDataSource implementation
  int IndexOfSelectedOption() const override;
  int OptionCount() const override;
  String OptionAtIndex(int index) const override;
  bool IsInDialogMode() const;
  void IncreaseContentModelViolationCount();
  void DecreaseContentModelViolationCount();

  // Iterates through the hierarchy of HTMLMenuOwnerElements (this menu and all
  // descendant submenus) and updates their computed ARIA roles. Called as a
  // microtask. Usually scheduled only on the root menu, but acts as a no-op
  // if called on a non-root.
  void UpdateDialogModeForMenuHierarchy();
  void ScheduleDialogModeUpdate();

  Node::InsertionNotificationRequest InsertedInto(ContainerNode&) override;
  void RemovedFrom(ContainerNode&) override;

 protected:
  HTMLMenuOwnerElement(HTMLQualifiedName, Document&);

  TypeAhead type_ahead_;

 private:
  Member<HTMLMenuItemElement> last_mouseup_menu_item_;
  bool processing_click_ = false;

  Member<MenuMutationObserver> menu_mutation_observer_;
  // Maintained during mutation observer callbacks and DOM
  // attachment/removal. This value is only meaningful and updated on the
  // root node of the menu hierarchy. UpdateDialogModeForMenuHierarchy checks
  // the root's value to determine if the entire hierarchy should switch its
  // ARIA role.
  int total_violations_in_tree_ = 0;
  // Maintained asynchronously by UpdateDialogModeForMenuHierarchy on every
  // HTMLMenuOwnerElement in the menu hierarchy.
  // UpdateDialogModeForMenuHierarchy compares this against the root menu's
  // (total_violations_in_tree_ > 0) to avoid calling
  // AXObjectCache::HandleAttributeChanged redundantly.
  bool is_in_dialog_mode_ = false;
  bool is_dialog_mode_update_scheduled_ = false;
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
