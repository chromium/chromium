/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/public/web/web_form_element.h"

#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/web/web_form_control_element.h"
#include "third_party/blink/public/web/web_input_element.h"
#include "third_party/blink/renderer/core/html/forms/html_form_control_element.h"
#include "third_party/blink/renderer/core/html/forms/html_form_element.h"
#include "third_party/blink/renderer/core/html/forms/html_input_element.h"
#include "third_party/blink/renderer/core/html_names.h"

namespace blink {

bool WebFormElement::AutoComplete() const {
  return ConstUnwrap<HTMLFormElement>()->ShouldAutocomplete();
}

WebString WebFormElement::Action() const {
  return ConstUnwrap<HTMLFormElement>()->FastGetAttribute(
      html_names::kActionAttr);
}

WebString WebFormElement::GetName() const {
  return ConstUnwrap<HTMLFormElement>()->GetName();
}

WebString WebFormElement::Method() const {
  return ConstUnwrap<HTMLFormElement>()->method();
}

WebVector<WebFormControlElement> WebFormElement::GetFormControlElements()
    const {
  const HTMLFormElement* form = ConstUnwrap<HTMLFormElement>();
  Vector<WebFormControlElement> form_control_elements;
  for (const auto& element :
       form->ListedElements(/*include_shadow_trees=*/true)) {
    if (auto* form_control =
            blink::DynamicTo<HTMLFormControlElement>(element.Get())) {
      form_control_elements.push_back(form_control);
    }
  }

  return form_control_elements;
}

WebFormElement::WebFormElement(HTMLFormElement* e) : WebElement(e) {}

DEFINE_WEB_NODE_TYPE_CASTS(WebFormElement,
                           IsA<HTMLFormElement>(ConstUnwrap<Node>()))

WebFormElement& WebFormElement::operator=(HTMLFormElement* e) {
  private_ = e;
  return *this;
}

WebFormElement::operator HTMLFormElement*() const {
  return blink::To<HTMLFormElement>(private_.Get());
}

}  // namespace blink
