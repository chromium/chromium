// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/input/keyboard_interpreter.h"

#include "base/check.h"
#include "remoting/client/input/keycode_map.h"
#include "remoting/client/input/text_keyboard_input_strategy.h"
#include "ui/events/keycodes/dom/dom_code.h"

namespace remoting {

KeyboardInterpreter::KeyboardInterpreter() = default;

KeyboardInterpreter::~KeyboardInterpreter() = default;

void KeyboardInterpreter::SetContext(ClientInputInjector* input_injector) {
  // TODO(nicholss): This should be configurable.
  if (input_injector) {
    input_strategy_ =
        std::make_unique<TextKeyboardInputStrategy>(input_injector);
  } else {
    input_strategy_.reset();
  }
}

void KeyboardInterpreter::HandleKeypressEvent(const KeypressInfo& keypress) {
  if (!input_strategy_) {
    return;
  }

  DCHECK(keypress.dom_code != ui::DomCode::NONE);
  base::queue<KeyEvent> keys;
  if (keypress.modifiers & KeypressInfo::Modifier::SHIFT) {
    keys.push({static_cast<uint32_t>(ui::DomCode::SHIFT_LEFT), true});
  }
  keys.push({static_cast<uint32_t>(keypress.dom_code), true});
  keys.push({static_cast<uint32_t>(keypress.dom_code), false});
  if (keypress.modifiers & KeypressInfo::Modifier::SHIFT) {
    keys.push({static_cast<uint32_t>(ui::DomCode::SHIFT_LEFT), false});
  }
  input_strategy_->HandleKeysEvent(keys);
}

void KeyboardInterpreter::HandleTextEvent(const std::string& text,
                                          uint8_t modifiers) {
  if (!input_strategy_) {
    return;
  }

  input_strategy_->HandleTextEvent(text, modifiers);
}

void KeyboardInterpreter::HandleDeleteEvent(uint8_t modifiers) {
  if (!input_strategy_) {
    return;
  }

  base::queue<KeyEvent> keys;
  // TODO(nicholss): Handle modifers.
  // Key press.
  keys.push({static_cast<uint32_t>(ui::DomCode::BACKSPACE), true});

  // Key release.
  keys.push({static_cast<uint32_t>(ui::DomCode::BACKSPACE), false});

  input_strategy_->HandleKeysEvent(keys);
}

void KeyboardInterpreter::HandleCtrlAltDeleteEvent() {
  if (!input_strategy_) {
    return;
  }

  base::queue<KeyEvent> keys;

  // Key press.
  keys.push({static_cast<uint32_t>(ui::DomCode::CONTROL_LEFT), true});
  keys.push({static_cast<uint32_t>(ui::DomCode::ALT_LEFT), true});
  keys.push({static_cast<uint32_t>(ui::DomCode::DEL), true});

  // Key release.
  keys.push({static_cast<uint32_t>(ui::DomCode::DEL), false});
  keys.push({static_cast<uint32_t>(ui::DomCode::ALT_LEFT), false});
  keys.push({static_cast<uint32_t>(ui::DomCode::CONTROL_LEFT), false});

  input_strategy_->HandleKeysEvent(keys);
}

void KeyboardInterpreter::HandlePrintScreenEvent() {
  if (!input_strategy_) {
    return;
  }

  base::queue<KeyEvent> keys;

  // Key press.
  keys.push({static_cast<uint32_t>(ui::DomCode::PRINT_SCREEN), true});

  // Key release.
  keys.push({static_cast<uint32_t>(ui::DomCode::PRINT_SCREEN), false});

  input_strategy_->HandleKeysEvent(keys);
}

}  // namespace remoting
