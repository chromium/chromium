// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_SELECT_MENU_ELEMENT_H_
#define THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_SELECT_MENU_ELEMENT_H_

#include "third_party/blink/public/platform/web_common.h"
#include "third_party/blink/public/platform/web_vector.h"
#include "third_party/blink/public/web/web_form_control_element.h"
#include "third_party/blink/public/web/web_option_element.h"

namespace blink {

class HTMLSelectMenuElement;

// Provides readonly access to some properties of a DOM selectmenu element
// node.
class BLINK_EXPORT WebSelectMenuElement final : public WebFormControlElement {
 public:
  WebSelectMenuElement() = default;
  WebSelectMenuElement(const WebSelectMenuElement& element) = default;

  WebSelectMenuElement& operator=(const WebSelectMenuElement& element) {
    WebFormControlElement::Assign(element);
    return *this;
  }
  void Assign(const WebSelectMenuElement& element) {
    WebFormControlElement::Assign(element);
  }

  // Returns list of WebOptionElement which are direct children of the
  // WebSelectMenuElement.
  WebVector<WebElement> GetListItems() const;

  // Returns whether a child element in one of the selectmenu's slots is
  // focusable.
  bool HasFocusableChild() const;

#if INSIDE_BLINK
  explicit WebSelectMenuElement(HTMLSelectMenuElement*);
  WebSelectMenuElement& operator=(HTMLSelectMenuElement*);
  explicit operator HTMLSelectMenuElement*() const;
#endif
};

DECLARE_WEB_NODE_TYPE_CASTS(WebSelectMenuElement);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_SELECT_MENU_ELEMENT_H_
