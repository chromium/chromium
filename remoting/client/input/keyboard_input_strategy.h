// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_INPUT_KEYBOARD_INPUT_STRATEGY_H_
#define REMOTING_CLIENT_INPUT_KEYBOARD_INPUT_STRATEGY_H_

#include <stdint.h>
#include <string>

#include "base/containers/queue.h"

namespace remoting {

struct KeyEvent {
  uint32_t keycode;
  bool keydown;
};

// This is an interface used by |KeyboardInterpreter| to customize how keyboard
// input is handled.
class KeyboardInputStrategy {
 public:
  virtual ~KeyboardInputStrategy() {}

  // Handle a text event.
  virtual void HandleTextEvent(const std::string& text, uint8_t modifiers) = 0;
  // Handle keys event as keycodes.
  virtual void HandleKeysEvent(base::queue<KeyEvent> keys) = 0;
};

}  // namespace remoting
#endif  // REMOTING_CLIENT_INPUT_KEYBOARD_INPUT_STRATEGY_H_
