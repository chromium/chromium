// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/gamepad/gamepad_raw_input_change_event.h"

#include "third_party/blink/renderer/bindings/modules/v8/v8_gamepad_raw_input_change_event_init.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

namespace blink {

GamepadRawInputChangeEvent::GamepadRawInputChangeEvent(
    const AtomicString& type,
    Bubbles bubbles,
    Cancelable cancelable,
    Gamepad* gamepad,
    const Vector<int>& axesChanged,
    const Vector<int>& buttonsValueChanged,
    const Vector<int>& buttonsPressed,
    const Vector<int>& buttonsReleased,
    const Vector<int>& touchesChanged)
    : GamepadEvent(type, bubbles, cancelable, gamepad),
      axes_changed_(axesChanged),
      buttons_value_changed_(buttonsValueChanged),
      buttons_pressed_(buttonsPressed),
      buttons_released_(buttonsReleased),
      touches_changed_(touchesChanged) {}

GamepadRawInputChangeEvent::GamepadRawInputChangeEvent(
    const AtomicString& type,
    const GamepadRawInputChangeEventInit* initializer)
    : GamepadEvent(type, initializer) {
  if (initializer) {
    axes_changed_ = initializer->axesChanged();
    buttons_value_changed_ = initializer->buttonsValueChanged();
    buttons_pressed_ = initializer->buttonsPressed();
    buttons_released_ = initializer->buttonsReleased();
    touches_changed_ = initializer->touchesChanged();
  }
}

GamepadRawInputChangeEvent::~GamepadRawInputChangeEvent() = default;

const AtomicString& GamepadRawInputChangeEvent::InterfaceName() const {
  return event_interface_names::kGamepadRawInputChangeEvent;
}

void GamepadRawInputChangeEvent::Trace(Visitor* visitor) const {
  GamepadEvent::Trace(visitor);
}

}  // namespace blink
