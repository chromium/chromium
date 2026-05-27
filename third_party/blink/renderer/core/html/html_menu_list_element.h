// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_HTML_MENU_LIST_ELEMENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_HTML_MENU_LIST_ELEMENT_H_

#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/html/html_menu_owner_element.h"

namespace blink {

class CORE_EXPORT HTMLMenuListElement final : public HTMLMenuOwnerElement {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit HTMLMenuListElement(Document&);
  ElementType GetElementType() const final {
    return ElementType::kHTMLMenuListElement;
  }
  bool HandleCommandInternal(HTMLElement& invoker,
                             CommandEventType command) override;

  // InvokingMenuItem returns the menuitem element which invoked this menulist
  // element, if this menulist is currently open and was opened by a menuitem.
  HTMLMenuItemElement* InvokingMenuItem();

  PopoverHideResult HidePopoverInternal(
      Element* invoker,
      HidePopoverFocusBehavior focus_behavior,
      HidePopoverTransitionBehavior event_firing,
      ExceptionState* exception_state) override;

  // These methods focus either the first or last of their descendant menuitem
  // elements, and return true if such a focusable menuitem was found and
  // focused.
  bool FocusFirstItem();
  bool FocusLastItem();
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_HTML_MENU_LIST_ELEMENT_H_
