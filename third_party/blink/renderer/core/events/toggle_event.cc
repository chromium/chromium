// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/events/toggle_event.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_toggle_event_init.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/event_interface_names.h"

namespace blink {

ToggleEvent::ToggleEvent() = default;

ToggleEvent::ToggleEvent(const AtomicString& type,
                         Event::Cancelable cancelable,
                         const String& old_state,
                         const String& new_state)
    : Event(type, Bubbles::kNo, cancelable),
      old_state_(old_state),
      new_state_(new_state) {
  DCHECK(old_state == "closed" || old_state == "open")
      << " old_state should be \"closed\" or \"open\". Was: " << old_state;
  DCHECK(new_state == "closed" || new_state == "open")
      << " new_state should be \"closed\" or \"open\". Was: " << new_state;
}

ToggleEvent::ToggleEvent(const AtomicString& type,
                         const ToggleEventInit* initializer)
    : Event(type, initializer) {
  if (initializer->hasOldState()) {
    old_state_ = initializer->oldState();
  }
  if (initializer->hasNewState()) {
    new_state_ = initializer->newState();
  }
}

ToggleEvent::~ToggleEvent() = default;

const String& ToggleEvent::oldState() const {
  return old_state_;
}

const String& ToggleEvent::newState() const {
  return new_state_;
}

const AtomicString& ToggleEvent::InterfaceName() const {
  return event_interface_names::kToggleEvent;
}

void ToggleEvent::Trace(Visitor* visitor) const {
  Event::Trace(visitor);
}

}  // namespace blink
