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

#include "third_party/blink/public/web/web_input_element.h"

#include "build/build_config.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/web/web_element_collection.h"
#include "third_party/blink/public/web/web_option_element.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/html/forms/html_data_list_element.h"
#include "third_party/blink/renderer/core/html/forms/html_data_list_options_collection.h"
#include "third_party/blink/renderer/core/html/forms/html_input_element.h"
#include "third_party/blink/renderer/core/html/forms/text_control_inner_elements.h"
#include "third_party/blink/renderer/core/html/shadow/shadow_element_names.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/input_type_names.h"

namespace blink {

using mojom::blink::FormControlType;

bool WebInputElement::IsTextField() const {
  return ConstUnwrap<HTMLInputElement>()->IsTextField();
}

void WebInputElement::SetHasBeenPasswordField() {
  Unwrap<HTMLInputElement>()->SetHasBeenPasswordField();
}

void WebInputElement::SetActivatedSubmit(bool activated) {
  Unwrap<HTMLInputElement>()->SetActivatedSubmit(activated);
}

int WebInputElement::size() const {
  return ConstUnwrap<HTMLInputElement>()->size();
}

bool WebInputElement::IsValidValue(const WebString& value) const {
  return ConstUnwrap<HTMLInputElement>()->IsValidValue(value);
}

void WebInputElement::SetChecked(bool now_checked,
                                 bool send_events,
                                 WebAutofillState autofill_state) {
  Unwrap<HTMLInputElement>()->SetChecked(
      now_checked,
      send_events ? TextFieldEventBehavior::kDispatchInputAndChangeEvent
                  : TextFieldEventBehavior::kDispatchNoEvent,
      autofill_state);
}

bool WebInputElement::IsChecked() const {
  return ConstUnwrap<HTMLInputElement>()->Checked();
}

bool WebInputElement::IsMultiple() const {
  return ConstUnwrap<HTMLInputElement>()->Multiple();
}

WebVector<WebOptionElement> WebInputElement::FilteredDataListOptions() const {
  return WebVector<WebOptionElement>(
      ConstUnwrap<HTMLInputElement>()->FilteredDataListOptions());
}

WebString WebInputElement::LocalizeValue(
    const WebString& proposed_value) const {
  return ConstUnwrap<HTMLInputElement>()->LocalizeValue(proposed_value);
}

void WebInputElement::SetShouldRevealPassword(bool value) {
  Unwrap<HTMLInputElement>()->SetShouldRevealPassword(value);
}

bool WebInputElement::ShouldRevealPassword() const {
  return ConstUnwrap<HTMLInputElement>()->ShouldRevealPassword();
}

#if BUILDFLAG(IS_ANDROID)
bool WebInputElement::IsLastInputElementInForm() {
  return Unwrap<HTMLInputElement>()->IsLastInputElementInForm();
}

void WebInputElement::DispatchSimulatedEnter() {
  Unwrap<HTMLInputElement>()->DispatchSimulatedEnter();
}
#endif

WebInputElement::WebInputElement(HTMLInputElement* elem)
    : WebFormControlElement(elem) {}

DEFINE_WEB_NODE_TYPE_CASTS(WebInputElement,
                           IsA<HTMLInputElement>(ConstUnwrap<Node>()))

WebInputElement& WebInputElement::operator=(HTMLInputElement* elem) {
  private_ = elem;
  return *this;
}

WebInputElement::operator HTMLInputElement*() const {
  return blink::To<HTMLInputElement>(private_.Get());
}

}  // namespace blink
