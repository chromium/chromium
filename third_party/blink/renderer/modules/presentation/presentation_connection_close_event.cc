// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/presentation/presentation_connection_close_event.h"

#include "third_party/blink/renderer/bindings/modules/v8/v8_presentation_connection_close_event_init.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"

namespace blink {

PresentationConnectionCloseEvent::PresentationConnectionCloseEvent(
    const AtomicString& event_type,
    const String& reason,
    const String& message)
    : Event(event_type, Bubbles::kNo, Cancelable::kNo),
      reason_(reason),
      message_(message) {}

PresentationConnectionCloseEvent::PresentationConnectionCloseEvent(
    const AtomicString& event_type,
    const PresentationConnectionCloseEventInit* initializer)
    : Event(event_type, initializer),
      reason_(initializer->reason()),
      message_(initializer->hasMessage() ? initializer->message()
                                         : g_empty_string) {}

const AtomicString& PresentationConnectionCloseEvent::InterfaceName() const {
  return event_interface_names::kPresentationConnectionCloseEvent;
}

void PresentationConnectionCloseEvent::Trace(Visitor* visitor) const {
  Event::Trace(visitor);
}

}  // namespace blink
