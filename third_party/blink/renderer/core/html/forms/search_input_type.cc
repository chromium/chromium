/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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

#include "third_party/blink/renderer/core/html/forms/search_input_type.h"

#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/events/keyboard_event.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/html/forms/html_input_element.h"
#include "third_party/blink/renderer/core/html/forms/text_control_inner_elements.h"
#include "third_party/blink/renderer/core/html/shadow/shadow_element_names.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/input_type_names.h"
#include "third_party/blink/renderer/core/layout/layout_search_field.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/heap.h"

namespace blink {

SearchInputType::SearchInputType(HTMLInputElement& element)
    : BaseTextInputType(element),
      search_event_timer_(
          element.GetDocument().GetTaskRunner(TaskType::kUserInteraction),
          this,
          &SearchInputType::SearchEventTimerFired) {}

void SearchInputType::CountUsage() {
  CountUsageIfVisible(WebFeature::kInputTypeSearch);
}

LayoutObject* SearchInputType::CreateLayoutObject(const ComputedStyle&,
                                                  LegacyLayout) const {
  return new LayoutSearchField(&GetElement());
}

const AtomicString& SearchInputType::FormControlType() const {
  return input_type_names::kSearch;
}

bool SearchInputType::NeedsContainer() const {
  return true;
}

void SearchInputType::CreateShadowSubtree() {
  TextFieldInputType::CreateShadowSubtree();
  Element* container = ContainerElement();
  Element* view_port = GetElement().UserAgentShadowRoot()->getElementById(
      shadow_element_names::EditingViewPort());
  DCHECK(container);
  DCHECK(view_port);
  container->InsertBefore(MakeGarbageCollected<SearchFieldCancelButtonElement>(
                              GetElement().GetDocument()),
                          view_port->nextSibling());
}

void SearchInputType::HandleKeydownEvent(KeyboardEvent& event) {
  if (GetElement().IsDisabledOrReadOnly()) {
    TextFieldInputType::HandleKeydownEvent(event);
    return;
  }

  if (event.key() == "Escape") {
    GetElement().SetValueForUser("");
    GetElement().OnSearch();
    event.SetDefaultHandled();
    return;
  }
  TextFieldInputType::HandleKeydownEvent(event);
}

void SearchInputType::StartSearchEventTimer() {
  DCHECK(GetElement().GetLayoutObject());
  unsigned length = GetElement().InnerEditorValue().length();

  if (!length) {
    search_event_timer_.Stop();
    GetElement()
        .GetDocument()
        .GetTaskRunner(TaskType::kUserInteraction)
        ->PostTask(FROM_HERE, WTF::Bind(&HTMLInputElement::OnSearch,
                                        WrapPersistent(&GetElement())));
    return;
  }

  // After typing the first key, we wait 500ms.
  // After the second key, 400ms, then 300, then 200 from then on.
  unsigned step = std::min(length, 4u) - 1;
  base::TimeDelta timeout = base::TimeDelta::FromMilliseconds(500 - 100 * step);
  search_event_timer_.StartOneShot(timeout, FROM_HERE);
}

void SearchInputType::DispatchSearchEvent() {
  search_event_timer_.Stop();
  GetElement().DispatchEvent(*Event::CreateBubble(event_type_names::kSearch));
}

void SearchInputType::SearchEventTimerFired(TimerBase*) {
  GetElement().OnSearch();
}

bool SearchInputType::SearchEventsShouldBeDispatched() const {
  return GetElement().FastHasAttribute(html_names::kIncrementalAttr);
}

void SearchInputType::DidSetValueByUserEdit() {
  UpdateCancelButtonVisibility();

  // If the incremental attribute is set, then dispatch the search event
  if (SearchEventsShouldBeDispatched())
    StartSearchEventTimer();

  TextFieldInputType::DidSetValueByUserEdit();
}

void SearchInputType::UpdateView() {
  BaseTextInputType::UpdateView();
  UpdateCancelButtonVisibility();
}

void SearchInputType::UpdateCancelButtonVisibility() {
  Element* button = GetElement().UserAgentShadowRoot()->getElementById(
      shadow_element_names::SearchClearButton());
  if (!button)
    return;
  if (GetElement().value().IsEmpty()) {
    button->SetInlineStyleProperty(CSSPropertyID::kOpacity, 0.0,
                                   CSSPrimitiveValue::UnitType::kNumber);
    button->SetInlineStyleProperty(CSSPropertyID::kPointerEvents,
                                   CSSValueID::kNone);
  } else {
    button->RemoveInlineStyleProperty(CSSPropertyID::kOpacity);
    button->RemoveInlineStyleProperty(CSSPropertyID::kPointerEvents);
  }
}

bool SearchInputType::SupportsInputModeAttribute() const {
  return true;
}

}  // namespace blink
