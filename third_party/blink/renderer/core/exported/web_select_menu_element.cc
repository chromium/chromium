// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/web/web_select_menu_element.h"

#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/renderer/core/html/forms/html_option_element.h"
#include "third_party/blink/renderer/core/html/forms/html_select_menu_element.h"
#include "third_party/blink/renderer/core/html_names.h"

namespace blink {

WebVector<WebElement> WebSelectMenuElement::GetListItems() const {
  HTMLSelectMenuElement::ListItems option_list =
      ConstUnwrap<HTMLSelectMenuElement>()->GetListItems();
  WebVector<WebElement> items;
  for (const Member<HTMLOptionElement>& option : option_list) {
    items.emplace_back(option.Get());
  }
  return items;
}

WebSelectMenuElement::WebSelectMenuElement(HTMLSelectMenuElement* element)
    : WebFormControlElement(element) {}

DEFINE_WEB_NODE_TYPE_CASTS(WebSelectMenuElement,
                           IsA<HTMLSelectMenuElement>(ConstUnwrap<Node>()))

WebSelectMenuElement& WebSelectMenuElement::operator=(
    HTMLSelectMenuElement* element) {
  private_ = element;
  return *this;
}

WebSelectMenuElement::operator HTMLSelectMenuElement*() const {
  return blink::To<HTMLSelectMenuElement>(private_.Get());
}

}  // namespace blink
