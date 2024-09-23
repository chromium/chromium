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

#include "third_party/blink/renderer/core/html/forms/hidden_input_type.h"

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/events/simulated_click_options.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/html/forms/form_controller.h"
#include "third_party/blink/renderer/core/html/forms/form_data.h"
#include "third_party/blink/renderer/core/html/forms/html_input_element.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/input_type_names.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"

namespace blink {

void HiddenInputType::CountUsage() {
  UseCounter::Count(GetElement().GetDocument(), WebFeature::kInputTypeHidden);
}

void HiddenInputType::Trace(Visitor* visitor) const {
  InputTypeView::Trace(visitor);
  InputType::Trace(visitor);
}

InputTypeView* HiddenInputType::CreateView() {
  return this;
}

bool HiddenInputType::ShouldSaveAndRestoreFormControlState() const {
  return false;
}

bool HiddenInputType::SupportsValidation() const {
  return false;
}

LayoutObject* HiddenInputType::CreateLayoutObject(const ComputedStyle&) const {
  NOTREACHED_IN_MIGRATION();
  return nullptr;
}

void HiddenInputType::AccessKeyAction(SimulatedClickCreationScope) {}

bool HiddenInputType::LayoutObjectIsNeeded() {
  return false;
}

InputType::ValueMode HiddenInputType::GetValueMode() const {
  return ValueMode::kDefault;
}

void HiddenInputType::SetValue(const String& sanitized_value,
                               bool,
                               TextFieldEventBehavior,
                               TextControlSetValueSelection) {
  GetElement().setAttribute(html_names::kValueAttr,
                            AtomicString(sanitized_value));
}

void HiddenInputType::AppendToFormData(FormData& form_data) const {
  if (EqualIgnoringASCIICase(GetElement().GetName(), "_charset_")) {
    form_data.AppendFromElement(GetElement().GetName(),
                                form_data.Encoding().GetName());
    return;
  }
  InputType::AppendToFormData(form_data);
}

bool HiddenInputType::ShouldRespectHeightAndWidthAttributes() {
  return true;
}

bool HiddenInputType::IsAutoDirectionalityFormAssociated() const {
  return true;
}

void HiddenInputType::ValueAttributeChanged() {
  UpdateView();
  // Hidden input need to adjust directionality explicitly since it has no
  // descendant to propagate dir from.
  if (GetElement().HasDirectionAuto()) {
    GetElement().UpdateAncestorWithDirAuto(
        Element::UpdateAncestorTraversal::IncludeSelf);
  }
}

}  // namespace blink
