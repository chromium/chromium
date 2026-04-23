// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/forms/html_submit_button_behavior.h"

#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/html/custom/element_internals.h"
#include "third_party/blink/renderer/core/html/forms/html_form_element.h"
#include "third_party/blink/renderer/core/html/forms/labels_node_list.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

namespace blink {

HTMLSubmitButtonBehavior* HTMLSubmitButtonBehavior::Create() {
  return MakeGarbageCollected<HTMLSubmitButtonBehavior>();
}

HTMLSubmitButtonBehavior::HTMLSubmitButtonBehavior() = default;

HTMLSubmitButtonBehavior::~HTMLSubmitButtonBehavior() = default;

const AtomicString& HTMLSubmitButtonBehavior::DefaultAriaRole() const {
  DEFINE_STATIC_LOCAL(const AtomicString, button_role, ("button"));
  return button_role;
}

const char* HTMLSubmitButtonBehavior::BehaviorName() const {
  return "HTMLSubmitButtonBehavior";
}

bool HTMLSubmitButtonBehavior::HandleActivation(Event& event) {
  if (IsEffectivelyDisabled()) {
    return false;
  }

  ElementInternals* internals = GetElementInternals();
  CHECK(internals);

  HTMLFormElement* form = internals->Form();
  if (!form) {
    // If it is not associated with a form, it results in no submission.
    return false;
  }

  form->PrepareForSubmission(&event, &internals->Target());
  event.SetDefaultHandled();
  return true;
}

bool HTMLSubmitButtonBehavior::IsEffectivelyDisabled() const {
  ElementInternals* internals = GetElementInternals();
  return disabled_ || (internals && internals->IsActuallyDisabled());
}

ElementInternals* HTMLSubmitButtonBehavior::GetInternalsOrThrow(
    ExceptionState& exception_state) const {
  ElementInternals* internals = GetElementInternals();
  if (!internals) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "The behavior is not attached to an element.");
  }
  return internals;
}

HTMLFormElement* HTMLSubmitButtonBehavior::form(
    ExceptionState& exception_state) const {
  ElementInternals* internals = GetInternalsOrThrow(exception_state);
  // Use ListedElement::Form() from the internals.
  return internals ? internals->Form() : nullptr;
}

LabelsNodeList* HTMLSubmitButtonBehavior::labels(
    ExceptionState& exception_state) const {
  ElementInternals* internals = GetInternalsOrThrow(exception_state);
  // Delegate to ElementInternals::labels() which handles form-associated
  // custom element labels.
  return internals ? internals->labels(exception_state) : nullptr;
}

void HTMLSubmitButtonBehavior::Trace(Visitor* visitor) const {
  ElementBehavior::Trace(visitor);
}

}  // namespace blink
