// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/input/text_keyboard_input_strategy.h"

#include "remoting/client/input/client_input_injector.h"
#include "remoting/client/input/native_device_keymap.h"
#include "ui/events/keycodes/dom/dom_code.h"

namespace remoting {

TextKeyboardInputStrategy::TextKeyboardInputStrategy(
    ClientInputInjector* input_injector)
    : input_injector_(input_injector) {}

TextKeyboardInputStrategy::~TextKeyboardInputStrategy() = default;

// KeyboardInputStrategy

void TextKeyboardInputStrategy::HandleTextEvent(const std::string& text,
                                                uint8_t modifiers) {
  // TODO(nicholss): Handle modifiers.
  input_injector_->SendTextEvent(text);
}

void TextKeyboardInputStrategy::HandleKeysEvent(base::queue<KeyEvent> keys) {
  while (!keys.empty()) {
    KeyEvent key = keys.front();
    input_injector_->SendKeyEvent(0, key.keycode, key.keydown);
    keys.pop();
  }
}

}  // namespace remoting
