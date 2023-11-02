// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/gamepad/gamepad_axis_event.h"

namespace blink {

GamepadAxisEvent::GamepadAxisEvent(const AtomicString& type,
                                   Bubbles bubbles,
                                   Cancelable cancelable,
                                   Gamepad* gamepad,
                                   uint32_t axis,
                                   double value)
    : GamepadEvent(type, bubbles, cancelable, gamepad),
      axis_(axis),
      value_(value) {}

GamepadAxisEvent::GamepadAxisEvent(const AtomicString& type,
                                   const GamepadAxisEventInit* initializer)
    : GamepadEvent(type, initializer) {
  if (initializer->hasAxis())
    axis_ = initializer->axis();
  if (initializer->hasValue())
    value_ = initializer->value();
}

GamepadAxisEvent::~GamepadAxisEvent() = default;

const AtomicString& GamepadAxisEvent::InterfaceName() const {
  return event_interface_names::kGamepadAxisEvent;
}

}  // namespace blink
