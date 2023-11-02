// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/forms/submit_event.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_submit_event_init.h"
#include "third_party/blink/renderer/core/event_interface_names.h"
#include "third_party/blink/renderer/core/html/html_element.h"

namespace blink {

SubmitEvent::SubmitEvent(const AtomicString& type,
                         const SubmitEventInit* event_init)
    : Event(type, event_init),
      submitter_(event_init ? event_init->submitter() : nullptr) {}

SubmitEvent* SubmitEvent::Create(const AtomicString& type,
                                 const SubmitEventInit* event_init) {
  return MakeGarbageCollected<SubmitEvent>(type, event_init);
}

void SubmitEvent::Trace(Visitor* visitor) const {
  visitor->Trace(submitter_);
  Event::Trace(visitor);
}

const AtomicString& SubmitEvent::InterfaceName() const {
  return event_interface_names::kSubmitEvent;
}

}  // namespace blink
