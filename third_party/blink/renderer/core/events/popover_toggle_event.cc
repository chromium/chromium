// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/events/popover_toggle_event.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_popover_toggle_event_init.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/event_interface_names.h"

namespace blink {

PopoverToggleEvent::PopoverToggleEvent() = default;

PopoverToggleEvent::PopoverToggleEvent(const AtomicString& type,
                                       Event::Cancelable cancelable,
                                       const String& current_state,
                                       const String& new_state)
    : Event(type, Bubbles::kNo, cancelable),
      current_state_(current_state),
      new_state_(new_state) {}

PopoverToggleEvent::PopoverToggleEvent(
    const AtomicString& type,
    const PopoverToggleEventInit* initializer)
    : Event(type, initializer) {
  if (initializer->hasCurrentState()) {
    current_state_ = initializer->currentState();
  }
  if (initializer->hasNewState()) {
    new_state_ = initializer->newState();
  }
}

PopoverToggleEvent::~PopoverToggleEvent() = default;

const String& PopoverToggleEvent::currentState() const {
  return current_state_;
}

const String& PopoverToggleEvent::newState() const {
  return new_state_;
}

const AtomicString& PopoverToggleEvent::InterfaceName() const {
  return event_interface_names::kPopoverToggleEvent;
}

void PopoverToggleEvent::Trace(Visitor* visitor) const {
  Event::Trace(visitor);
}

}  // namespace blink
