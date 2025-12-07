/*
 * Copyright (c) 2010 Google Inc. All rights reserved.
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

#include "third_party/blink/renderer/core/html/forms/html_output_element.h"

#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

namespace blink {

using mojom::blink::FormControlType;

HTMLOutputElement::HTMLOutputElement(Document& document)
    : HTMLFormControlElement(html_names::kOutputTag, document),
      is_default_value_mode_(true),
      default_value_(""),
      tokens_(MakeGarbageCollected<DOMTokenList>(*this, html_names::kForAttr)) {
}

HTMLOutputElement::~HTMLOutputElement() = default;

FormControlType HTMLOutputElement::FormControlType() const {
  return FormControlType::kOutput;
}

const AtomicString& HTMLOutputElement::FormControlTypeAsString() const {
  DEFINE_STATIC_LOCAL(const AtomicString, output, ("output"));
  return output;
}

bool HTMLOutputElement::IsDisabledFormControl() const {
  return false;
}

bool HTMLOutputElement::MatchesEnabledPseudoClass() const {
  return false;
}

FocusableState HTMLOutputElement::SupportsFocus(
    UpdateBehavior update_behavior) const {
  // Skip over HTMLFormControl element, which always supports focus.
  return HTMLElement::SupportsFocus(update_behavior);
}

void HTMLOutputElement::ParseAttribute(
    const AttributeModificationParams& params) {
  if (params.name == html_names::kForAttr)
    tokens_->DidUpdateAttributeValue(params.old_value, params.new_value);
  else
    HTMLFormControlElement::ParseAttribute(params);
}

DOMTokenList* HTMLOutputElement::htmlFor() const {
  return tokens_.Get();
}

void HTMLOutputElement::ResetImpl() {
  // The reset algorithm for output elements is to set the element's
  // value mode flag to "default" and then to set the element's textContent
  // attribute to the default value.
  if (defaultValue() == value())
    return;
  setTextContent(defaultValue());
  is_default_value_mode_ = true;
}

String HTMLOutputElement::value() const {
  return textContent();
}

void HTMLOutputElement::setValue(const String& new_value) {
  String old_value = value();

  if (is_default_value_mode_)
    default_value_ = old_value;

  // The value mode flag set to "value" when the value attribute is set.
  is_default_value_mode_ = false;

  if (new_value != old_value)
    setTextContent(new_value);
}

String HTMLOutputElement::defaultValue() const {
  return is_default_value_mode_ ? textContent() : default_value_;
}

void HTMLOutputElement::setDefaultValue(const String& value) {
  if (default_value_ == value)
    return;
  default_value_ = value;
  // The spec requires the value attribute set to the default value
  // when the element's value mode flag to "default".
  if (is_default_value_mode_)
    setTextContent(value);
}

void HTMLOutputElement::Trace(Visitor* visitor) const {
  visitor->Trace(tokens_);
  HTMLFormControlElement::Trace(visitor);
}

}  // namespace blink
