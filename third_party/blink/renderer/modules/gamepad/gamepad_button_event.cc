// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/gamepad/gamepad_button_event.h"

namespace blink {

GamepadButtonEvent::GamepadButtonEvent(const AtomicString& type,
                                       Bubbles bubbles,
                                       Cancelable cancelable,
                                       Gamepad* gamepad,
                                       uint32_t button,
                                       double value)
    : GamepadEvent(type, bubbles, cancelable, gamepad),
      button_(button),
      value_(value) {}

GamepadButtonEvent::GamepadButtonEvent(
    const AtomicString& type,
    const GamepadButtonEventInit* initializer)
    : GamepadEvent(type, initializer) {
  if (initializer->hasButton())
    button_ = initializer->button();
  if (initializer->hasValue())
    value_ = initializer->value();
}

GamepadButtonEvent::~GamepadButtonEvent() = default;

const AtomicString& GamepadButtonEvent::InterfaceName() const {
  return event_interface_names::kGamepadButtonEvent;
}

}  // namespace blink
