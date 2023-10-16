// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/nfc/ndef_reading_event.h"

#include "third_party/blink/renderer/bindings/modules/v8/v8_ndef_reading_event_init.h"
#include "third_party/blink/renderer/modules/nfc/ndef_message.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"

namespace blink {

// static
NDEFReadingEvent* NDEFReadingEvent::Create(const ScriptState* script_state,
                                           const AtomicString& event_type,
                                           const NDEFReadingEventInit* init,
                                           ExceptionState& exception_state) {
  NDEFMessage* message = NDEFMessage::Create(
      script_state, init->message(), exception_state, /*records_depth=*/0U);
  if (exception_state.HadException())
    return nullptr;
  DCHECK(message);
  return MakeGarbageCollected<NDEFReadingEvent>(event_type, init, message);
}

NDEFReadingEvent::NDEFReadingEvent(const AtomicString& event_type,
                                   const NDEFReadingEventInit* init,
                                   NDEFMessage* message)
    : Event(event_type, init),
      serial_number_(init->serialNumber()),
      message_(message) {}

NDEFReadingEvent::NDEFReadingEvent(const AtomicString& event_type,
                                   const String& serial_number,
                                   NDEFMessage* message)
    : Event(event_type, Bubbles::kNo, Cancelable::kNo),
      serial_number_(serial_number),
      message_(message) {}

NDEFReadingEvent::~NDEFReadingEvent() = default;

const AtomicString& NDEFReadingEvent::InterfaceName() const {
  return event_interface_names::kNDEFReadingEvent;
}

void NDEFReadingEvent::Trace(Visitor* visitor) const {
  visitor->Trace(message_);
  Event::Trace(visitor);
}

const String& NDEFReadingEvent::serialNumber() const {
  if (serial_number_.IsNull())
    return g_empty_string;
  return serial_number_;
}

NDEFMessage* NDEFReadingEvent::message() const {
  return message_.Get();
}

}  // namespace blink
