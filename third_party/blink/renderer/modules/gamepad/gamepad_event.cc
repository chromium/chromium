// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/gamepad/gamepad_event.h"

namespace blink {

GamepadEvent::GamepadEvent(const AtomicString& type,
                           Bubbles bubbles,
                           Cancelable cancelable,
                           Gamepad* gamepad)
    : Event(type, bubbles, cancelable), gamepad_(gamepad) {}

GamepadEvent::GamepadEvent(const AtomicString& type,
                           const GamepadEventInit* initializer)
    : Event(type, initializer) {
  if (initializer->hasGamepad())
    gamepad_ = initializer->gamepad();
}

GamepadEvent::~GamepadEvent() = default;

const AtomicString& GamepadEvent::InterfaceName() const {
  return event_interface_names::kGamepadEvent;
}

void GamepadEvent::Trace(Visitor* visitor) const {
  visitor->Trace(gamepad_);
  Event::Trace(visitor);
}

}  // namespace blink
