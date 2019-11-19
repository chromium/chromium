/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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

#include "third_party/blink/renderer/core/html/forms/base_button_input_type.h"

#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/html/forms/html_form_element.h"
#include "third_party/blink/renderer/core/html/forms/html_input_element.h"
#include "third_party/blink/renderer/core/html/parser/html_parser_idioms.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/layout/layout_button.h"

namespace blink {

BaseButtonInputType::BaseButtonInputType(HTMLInputElement& element)
    : InputType(element), KeyboardClickableInputTypeView(element) {}

void BaseButtonInputType::Trace(Visitor* visitor) {
  KeyboardClickableInputTypeView::Trace(visitor);
  InputType::Trace(visitor);
}

InputTypeView* BaseButtonInputType::CreateView() {
  return this;
}

void BaseButtonInputType::CreateShadowSubtree() {
  DCHECK(GetElement().UserAgentShadowRoot());
  GetElement().UserAgentShadowRoot()->AppendChild(
      Text::Create(GetElement().GetDocument(), DisplayValue()));
}

void BaseButtonInputType::ValueAttributeChanged() {
  To<Text>(GetElement().UserAgentShadowRoot()->firstChild())
      ->setData(DisplayValue());
}

String BaseButtonInputType::DisplayValue() const {
  return GetElement().ValueOrDefaultLabel().RemoveCharacters(IsHTMLLineBreak);
}

bool BaseButtonInputType::ShouldSaveAndRestoreFormControlState() const {
  return false;
}

void BaseButtonInputType::AppendToFormData(FormData&) const {}

LayoutObject* BaseButtonInputType::CreateLayoutObject(const ComputedStyle&,
                                                      LegacyLayout) const {
  return new LayoutButton(&GetElement());
}

InputType::ValueMode BaseButtonInputType::GetValueMode() const {
  return ValueMode::kDefault;
}

void BaseButtonInputType::SetValue(const String& sanitized_value,
                                   bool,
                                   TextFieldEventBehavior,
                                   TextControlSetValueSelection) {
  GetElement().setAttribute(html_names::kValueAttr,
                            AtomicString(sanitized_value));
}

bool BaseButtonInputType::MatchesDefaultPseudoClass() {
  // HTMLFormElement::findDefaultButton() traverses the tree. So we check
  // canBeSuccessfulSubmitButton() first for early return.
  return CanBeSuccessfulSubmitButton() && GetElement().Form() &&
         GetElement().Form()->FindDefaultButton() == &GetElement();
}

}  // namespace blink
