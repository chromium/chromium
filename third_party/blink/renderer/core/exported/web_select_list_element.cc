// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/web/web_select_list_element.h"

#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/renderer/core/html/forms/html_option_element.h"
#include "third_party/blink/renderer/core/html/forms/html_select_list_element.h"
#include "third_party/blink/renderer/core/html_names.h"

namespace blink {

WebVector<WebElement> WebSelectListElement::GetListItems() const {
  HTMLSelectListElement::ListItems option_list =
      ConstUnwrap<HTMLSelectListElement>()->GetListItems();
  WebVector<WebElement> items;
  for (const Member<HTMLOptionElement>& option : option_list) {
    items.emplace_back(option.Get());
  }
  return items;
}

bool WebSelectListElement::HasFocusableChild() const {
  return blink::To<HTMLSelectListElement>(private_.Get())->GetFocusableArea();
}

WebSelectListElement::WebSelectListElement(HTMLSelectListElement* element)
    : WebFormControlElement(element) {}

DEFINE_WEB_NODE_TYPE_CASTS(WebSelectListElement,
                           IsA<HTMLSelectListElement>(ConstUnwrap<Node>()))

WebSelectListElement& WebSelectListElement::operator=(
    HTMLSelectListElement* element) {
  private_ = element;
  return *this;
}

WebSelectListElement::operator HTMLSelectListElement*() const {
  return blink::To<HTMLSelectListElement>(private_.Get());
}

}  // namespace blink
