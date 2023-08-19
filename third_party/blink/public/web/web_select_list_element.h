// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_SELECT_LIST_ELEMENT_H_
#define THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_SELECT_LIST_ELEMENT_H_

#include "third_party/blink/public/platform/web_common.h"
#include "third_party/blink/public/platform/web_vector.h"
#include "third_party/blink/public/web/web_form_control_element.h"
#include "third_party/blink/public/web/web_option_element.h"

namespace blink {

class HTMLSelectListElement;

// Provides readonly access to some properties of a DOM selectlist element
// node.
class BLINK_EXPORT WebSelectListElement final : public WebFormControlElement {
 public:
  WebSelectListElement() = default;
  WebSelectListElement(const WebSelectListElement& element) = default;

  WebSelectListElement& operator=(const WebSelectListElement& element) {
    WebFormControlElement::Assign(element);
    return *this;
  }
  void Assign(const WebSelectListElement& element) {
    WebFormControlElement::Assign(element);
  }

  // Returns list of WebOptionElement which are direct children of the
  // WebSelectListElement.
  WebVector<WebElement> GetListItems() const;

  // Returns whether a child element in one of the selectlist's slots is
  // focusable.
  bool HasFocusableChild() const;

#if INSIDE_BLINK
  explicit WebSelectListElement(HTMLSelectListElement*);
  WebSelectListElement& operator=(HTMLSelectListElement*);
  explicit operator HTMLSelectListElement*() const;
#endif
};

DECLARE_WEB_NODE_TYPE_CASTS(WebSelectListElement);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_SELECT_LIST_ELEMENT_H_
