// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/forms/form_data_event.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_form_data_event_init.h"
#include "third_party/blink/renderer/core/event_interface_names.h"
#include "third_party/blink/renderer/core/event_type_names.h"
#include "third_party/blink/renderer/core/html/forms/form_data.h"

namespace blink {

FormDataEvent::FormDataEvent(FormData& form_data)
    : Event(event_type_names::kFormdata, Bubbles::kYes, Cancelable::kNo),
      form_data_(form_data) {}

FormDataEvent::FormDataEvent(const AtomicString& type,
                             const FormDataEventInit* event_init)
    : Event(type, event_init), form_data_(event_init->formData()) {}

FormDataEvent* FormDataEvent::Create(FormData& form_data) {
  return MakeGarbageCollected<FormDataEvent>(form_data);
}

FormDataEvent* FormDataEvent::Create(const AtomicString& type,
                                     const FormDataEventInit* event_init) {
  // FormDataEventInit contains the required 'formData' member.
  // Binding-generated code guarantees that event_init contains non-null
  // |formData|.
  DCHECK(event_init);
  DCHECK(event_init->formData());
  return MakeGarbageCollected<FormDataEvent>(type, event_init);
}

void FormDataEvent::Trace(Visitor* visitor) const {
  visitor->Trace(form_data_);
  Event::Trace(visitor);
}

const AtomicString& FormDataEvent::InterfaceName() const {
  return event_interface_names::kFormDataEvent;
}

}  // namespace blink
