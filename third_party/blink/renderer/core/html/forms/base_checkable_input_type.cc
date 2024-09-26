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

#include "third_party/blink/renderer/core/html/forms/base_checkable_input_type.h"

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/events/simulated_click_options.h"
#include "third_party/blink/renderer/core/events/keyboard_event.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/html/forms/form_controller.h"
#include "third_party/blink/renderer/core/html/forms/form_data.h"
#include "third_party/blink/renderer/core/html/forms/html_input_element.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/keywords.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"

namespace blink {

void BaseCheckableInputType::Trace(Visitor* visitor) const {
  InputTypeView::Trace(visitor);
  InputType::Trace(visitor);
}

InputTypeView* BaseCheckableInputType::CreateView() {
  return this;
}

FormControlState BaseCheckableInputType::SaveFormControlState() const {
  return FormControlState(GetElement().Checked() ? keywords::kOn
                                                 : keywords::kOff);
}

void BaseCheckableInputType::RestoreFormControlState(
    const FormControlState& state) {
  GetElement().SetChecked(state[0] == keywords::kOn);
}

void BaseCheckableInputType::AppendToFormData(FormData& form_data) const {
  if (GetElement().Checked())
    form_data.AppendFromElement(GetElement().GetName(), GetElement().Value());
}

void BaseCheckableInputType::HandleKeydownEvent(KeyboardEvent& event) {
  if (event.key() == " ") {
    GetElement().SetActive(true);
    // No setDefaultHandled(), because IE dispatches a keypress in this case
    // and the caller will only dispatch a keypress if we don't call
    // setDefaultHandled().
  }
}

void BaseCheckableInputType::HandleKeypressEvent(KeyboardEvent& event) {
  if (event.charCode() == ' ') {
    // Prevent scrolling down the page.
    event.SetDefaultHandled();
  }
}

bool BaseCheckableInputType::CanSetStringValue() const {
  return false;
}

// FIXME: Could share this with KeyboardClickableInputTypeView and
// RangeInputType if we had a common base class.
void BaseCheckableInputType::AccessKeyAction(
    SimulatedClickCreationScope creation_scope) {
  InputTypeView::AccessKeyAction(creation_scope);
  GetElement().DispatchSimulatedClick(nullptr, creation_scope);
}

bool BaseCheckableInputType::MatchesDefaultPseudoClass() {
  return GetElement().FastHasAttribute(html_names::kCheckedAttr);
}

InputType::ValueMode BaseCheckableInputType::GetValueMode() const {
  return ValueMode::kDefaultOn;
}

void BaseCheckableInputType::SetValue(const String& sanitized_value,
                                      bool,
                                      TextFieldEventBehavior,
                                      TextControlSetValueSelection) {
  GetElement().setAttribute(html_names::kValueAttr,
                            AtomicString(sanitized_value));
}

void BaseCheckableInputType::ReadingChecked() const {
  if (is_in_click_handler_) {
    UseCounter::Count(GetElement().GetDocument(),
                      WebFeature::kReadingCheckedInClickHandler);
  }
}

bool BaseCheckableInputType::IsCheckable() {
  return true;
}

void BaseCheckableInputType::HandleBlurEvent() {
  // The input might be the control element of a label
  // that is in :active state. In that case the control should
  // remain :active to avoid crbug.com/40934455.
  HTMLInputElement& element = GetElement();
  if (!RuntimeEnabledFeatures::KeepActiveIfLabelActiveEnabled() ||
      !element.HasActiveLabel()) {
    element.SetActive(false);
  }
}

}  // namespace blink
