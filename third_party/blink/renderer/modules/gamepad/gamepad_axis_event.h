// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_GAMEPAD_GAMEPAD_AXIS_EVENT_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_GAMEPAD_GAMEPAD_AXIS_EVENT_H_

#include "third_party/blink/renderer/bindings/modules/v8/v8_gamepad_axis_event_init.h"
#include "third_party/blink/renderer/modules/gamepad/gamepad.h"
#include "third_party/blink/renderer/modules/gamepad/gamepad_event.h"

namespace blink {

class GamepadAxisEvent final : public GamepadEvent {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static GamepadAxisEvent* Create(const AtomicString& type,
                                  Bubbles bubbles,
                                  Cancelable cancelable,
                                  Gamepad* gamepad,
                                  uint32_t axis,
                                  double value) {
    return MakeGarbageCollected<GamepadAxisEvent>(type, bubbles, cancelable,
                                                  gamepad, axis, value);
  }
  static GamepadAxisEvent* Create(const AtomicString& type,
                                  const GamepadAxisEventInit* initializer) {
    return MakeGarbageCollected<GamepadAxisEvent>(type, initializer);
  }

  GamepadAxisEvent(const AtomicString& type,
                   Bubbles,
                   Cancelable,
                   Gamepad*,
                   uint32_t axis,
                   double value);
  GamepadAxisEvent(const AtomicString&, const GamepadAxisEventInit*);
  ~GamepadAxisEvent() override;

  uint32_t getAxis() const { return axis_; }
  double getValue() const { return value_; }

  const AtomicString& InterfaceName() const override;

 private:
  uint32_t axis_ = 0;
  double value_ = 0.0;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_GAMEPAD_GAMEPAD_AXIS_EVENT_H_
