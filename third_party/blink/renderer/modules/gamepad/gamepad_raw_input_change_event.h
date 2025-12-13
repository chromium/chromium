// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_GAMEPAD_GAMEPAD_RAW_INPUT_CHANGE_EVENT_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_GAMEPAD_GAMEPAD_RAW_INPUT_CHANGE_EVENT_H_

#include "third_party/blink/renderer/modules/event_modules.h"
#include "third_party/blink/renderer/modules/gamepad/gamepad.h"
#include "third_party/blink/renderer/modules/gamepad/gamepad_event.h"

namespace blink {

class Gamepad;
class GamepadRawInputChangeEventInit;

class GamepadRawInputChangeEvent : public GamepadEvent {
  DEFINE_WRAPPERTYPEINFO();

 public:
  // Create using explicit parameters (not init dictionary).
  static GamepadRawInputChangeEvent* Create(
      const AtomicString& type,
      Bubbles bubbles,
      Cancelable cancelable,
      Gamepad* gamepad,
      const Vector<int>& axesChanged,
      const Vector<int>& buttonsValueChanged,
      const Vector<int>& buttonsPressed,
      const Vector<int>& buttonsReleased,
      const Vector<int>& touchesChanged) {
    return MakeGarbageCollected<GamepadRawInputChangeEvent>(
        type, bubbles, cancelable, gamepad, axesChanged, buttonsValueChanged,
        buttonsPressed, buttonsReleased, touchesChanged);
  }

  static GamepadRawInputChangeEvent* Create(
      const AtomicString& type,
      const GamepadRawInputChangeEventInit* initializer) {
    return MakeGarbageCollected<GamepadRawInputChangeEvent>(type, initializer);
  }

  // Constructor with explicit parameters.
  GamepadRawInputChangeEvent(const AtomicString& type,
                             Bubbles bubbles,
                             Cancelable cancelable,
                             Gamepad* gamepad,
                             const Vector<int>& axesChanged,
                             const Vector<int>& buttonsValueChanged,
                             const Vector<int>& buttonsPressed,
                             const Vector<int>& buttonsReleased,
                             const Vector<int>& touchesChanged);

  GamepadRawInputChangeEvent(const AtomicString& type,
                             const GamepadRawInputChangeEventInit* initializer);

  ~GamepadRawInputChangeEvent() override;

  // Getters for the attributes.
  const Vector<int>& axesChanged() const { return axes_changed_; }
  const Vector<int>& buttonsValueChanged() const {
    return buttons_value_changed_;
  }
  const Vector<int>& buttonsPressed() const { return buttons_pressed_; }
  const Vector<int>& buttonsReleased() const { return buttons_released_; }
  const Vector<int>& touchesChanged() const { return touches_changed_; }

  // Returns the interface name for this event, used for runtime type
  // identification.
  const AtomicString& InterfaceName() const override;

  void Trace(Visitor*) const override;

 private:
  Vector<int> axes_changed_;
  Vector<int> buttons_value_changed_;
  Vector<int> buttons_pressed_;
  Vector<int> buttons_released_;
  Vector<int> touches_changed_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_GAMEPAD_GAMEPAD_RAW_INPUT_CHANGE_EVENT_H_
