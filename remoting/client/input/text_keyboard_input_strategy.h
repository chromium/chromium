// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_INPUT_TEXT_KEYBOARD_INPUT_STRATEGY_H_
#define REMOTING_CLIENT_INPUT_TEXT_KEYBOARD_INPUT_STRATEGY_H_

#include <queue>

#include "base/memory/raw_ptr.h"
#include "remoting/client/input/keyboard_input_strategy.h"

namespace remoting {

class ClientInputInjector;

class TextKeyboardInputStrategy : public KeyboardInputStrategy {
 public:
  explicit TextKeyboardInputStrategy(ClientInputInjector* input_injector);

  TextKeyboardInputStrategy(const TextKeyboardInputStrategy&) = delete;
  TextKeyboardInputStrategy& operator=(const TextKeyboardInputStrategy&) =
      delete;

  ~TextKeyboardInputStrategy() override;

  // KeyboardInputStrategy overrides.
  void HandleTextEvent(const std::string& text, uint8_t modifiers) override;
  void HandleKeysEvent(base::queue<KeyEvent> keys) override;

 private:
  base::queue<KeyEvent> ConvertDeleteEvent(uint8_t modifiers);

  raw_ptr<ClientInputInjector> input_injector_;
};

}  // namespace remoting
#endif  // REMOTING_CLIENT_INPUT_TEXT_KEYBOARD_INPUT_STRATEGY_H_
