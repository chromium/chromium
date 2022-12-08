// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/events/before_toggle_event.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_before_toggle_event_init.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/event_interface_names.h"

namespace blink {

BeforeToggleEvent::BeforeToggleEvent() = default;

BeforeToggleEvent::BeforeToggleEvent(const AtomicString& type,
                                     Event::Cancelable cancelable,
                                     const String& current_state,
                                     const String& new_state)
    : Event(type, Bubbles::kNo, cancelable),
      current_state_(current_state),
      new_state_(new_state) {}

BeforeToggleEvent::BeforeToggleEvent(const AtomicString& type,
                                     const BeforeToggleEventInit* initializer)
    : Event(type, initializer) {
  if (initializer->hasCurrentState()) {
    current_state_ = initializer->currentState();
  }
  if (initializer->hasNewState()) {
    new_state_ = initializer->newState();
  }
}

BeforeToggleEvent::~BeforeToggleEvent() = default;

const String& BeforeToggleEvent::currentState() const {
  return current_state_;
}

const String& BeforeToggleEvent::newState() const {
  return new_state_;
}

const AtomicString& BeforeToggleEvent::InterfaceName() const {
  return event_interface_names::kBeforeToggleEvent;
}

void BeforeToggleEvent::Trace(Visitor* visitor) const {
  Event::Trace(visitor);
}

}  // namespace blink
