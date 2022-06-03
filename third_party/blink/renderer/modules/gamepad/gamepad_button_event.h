// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_GAMEPAD_GAMEPAD_BUTTON_EVENT_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_GAMEPAD_GAMEPAD_BUTTON_EVENT_H_

#include "third_party/blink/renderer/bindings/modules/v8/v8_gamepad_button_event_init.h"
#include "third_party/blink/renderer/modules/gamepad/gamepad.h"
#include "third_party/blink/renderer/modules/gamepad/gamepad_button.h"
#include "third_party/blink/renderer/modules/gamepad/gamepad_event.h"

namespace blink {

class GamepadButtonEvent final : public GamepadEvent {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static GamepadButtonEvent* Create(const AtomicString& type,
                                    Bubbles bubbles,
                                    Cancelable cancelable,
                                    Gamepad* gamepad,
                                    uint32_t button,
                                    double value) {
    return MakeGarbageCollected<GamepadButtonEvent>(type, bubbles, cancelable,
                                                    gamepad, button, value);
  }
  static GamepadButtonEvent* Create(const AtomicString& type,
                                    const GamepadButtonEventInit* initializer) {
    return MakeGarbageCollected<GamepadButtonEvent>(type, initializer);
  }

  GamepadButtonEvent(const AtomicString& type,
                     Bubbles,
                     Cancelable,
                     Gamepad*,
                     uint32_t button,
                     double value);
  GamepadButtonEvent(const AtomicString&, const GamepadButtonEventInit*);
  ~GamepadButtonEvent() override;

  uint32_t getButton() const { return button_; }
  double getValue() const { return value_; }

  const AtomicString& InterfaceName() const override;

 private:
  uint32_t button_ = 0;
  double value_ = 0.0;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_GAMEPAD_GAMEPAD_BUTTON_EVENT_H_
