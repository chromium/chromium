// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/presentation/presentation_connection_available_event.h"

#include "third_party/blink/renderer/bindings/modules/v8/v8_presentation_connection_available_event_init.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"

namespace blink {

PresentationConnectionAvailableEvent::~PresentationConnectionAvailableEvent() =
    default;

PresentationConnectionAvailableEvent::PresentationConnectionAvailableEvent(
    const AtomicString& event_type,
    PresentationConnection* connection)
    : Event(event_type, Bubbles::kNo, Cancelable::kNo),
      connection_(connection) {}

PresentationConnectionAvailableEvent::PresentationConnectionAvailableEvent(
    const AtomicString& event_type,
    const PresentationConnectionAvailableEventInit* initializer)
    : Event(event_type, initializer), connection_(initializer->connection()) {}

const AtomicString& PresentationConnectionAvailableEvent::InterfaceName()
    const {
  return event_interface_names::kPresentationConnectionAvailableEvent;
}

void PresentationConnectionAvailableEvent::Trace(Visitor* visitor) const {
  visitor->Trace(connection_);
  Event::Trace(visitor);
}

}  // namespace blink
