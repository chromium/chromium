// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/html_menu_item_element.h"

#include "third_party/blink/renderer/core/css/css_selector.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/popover_data.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/html/forms/option_list.h"
#include "third_party/blink/renderer/core/html/html_menu_list_element.h"
#include "third_party/blink/renderer/core/html/html_menu_owner_element.h"
#include "third_party/blink/renderer/core/html/html_sub_menu_element.h"
#include "third_party/blink/renderer/core/html_names.h"

namespace blink {

HTMLMenuItemElement::HTMLMenuItemElement(Document& document)
    : HTMLMenuItemElementBase(html_names::kMenuitemTag, document) {}

HTMLMenuItemElement::~HTMLMenuItemElement() = default;

bool HTMLMenuItemElement::IsSubmenuOpen() const {
  if (HTMLMenuListElement* invoked_menulist = GetInvokedSubmenu()) {
    return invoked_menulist->InvokingMenuItem() == this;
  }
  return false;
}

void HTMLMenuItemElement::OpenPseudoChanged() {
  PseudoStateChanged(CSSSelector::kPseudoOpen);
  if (owning_menu_element_) {
    owning_menu_element_->PseudoStateChanged(
        CSSSelector::kPseudoHasOpenMenuitem);
  }
}

HTMLMenuListElement* HTMLMenuItemElement::GetInvokedSubmenu() const {
  if (auto* submenu = DynamicTo<HTMLSubMenuElement>(parentNode())) {
    return submenu->MenuList();
  }
  return nullptr;
}

bool HTMLMenuItemElement::CanBeCommandInvoker() const {
  return !FastHasAttribute(html_names::kDisabledAttr);
}

bool HTMLMenuItemElement::IsValidInterestInvoker(Element& target) const {
  // Menu items need to be enabled in order to support interest invokers.
  return !FastHasAttribute(html_names::kDisabledAttr);
}

void HTMLMenuItemElement::ActivateMenuItem(
    ActivationKeyboardEventType activation_type) {
  if (!GetInvokedSubmenu()) {
    CloseOutermostContainingMenuList();
  }
  if (HTMLMenuListElement* submenu = GetInvokedSubmenu()) {
    submenu->InvokePopover(*this, PopoverInvokedVia::kCommand);

    if (submenu->popoverOpen()) {
      if (activation_type != ActivationKeyboardEventType::kNotKeyboard) {
        FocusTrigger trigger =
            activation_type == ActivationKeyboardEventType::kTrustedKeyboard
                ? FocusTrigger::kUserGesture
                : FocusTrigger::kScript;
        submenu->FocusFirstItem(trigger);
      } else {
        // If this is resulting from a click, this is happening *before* that
        // click has had a chance to focus this menuitem.  However, we should
        // behave as though it did.
        submenu->GetPopoverData()->setPreviouslyFocusedElement(this);
      }
    }
  }
}

Node::InsertionNotificationRequest HTMLMenuItemElement::InsertedInto(
    ContainerNode& insertion_point) {
  if (insertion_point.HasTagName(html_names::kMenuTag)) {
    // Count things that look like the old HTML context menu markup.
    // (Chromium's implementation required only an ancestor-descendant
    // relationship but Gecko's required a parent-child relationship, so it
    // seems likely that most such content used parent-child.  It doesn't seem
    // worth the cost of a tree walk to make this measurement slightly more
    // accurate.)
    UseCounter::Count(GetDocument(), WebFeature::kHTMLMenuItemInMenu);
  }

  return HTMLMenuItemElementBase::InsertedInto(insertion_point);
}

bool HTMLMenuItemElement::ShouldHaveExpandIcon() const {
  return HasOwnerMenuList() && GetInvokedSubmenu();
}

}  // namespace blink
