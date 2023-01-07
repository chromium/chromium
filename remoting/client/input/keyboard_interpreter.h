// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_INPUT_KEYBOARD_INTERPRETER_H_
#define REMOTING_CLIENT_INPUT_KEYBOARD_INTERPRETER_H_

#include <memory>
#include <string>

namespace remoting {

class KeyboardInputStrategy;
class ClientInputInjector;

struct KeypressInfo;

// This is a class for interpreting raw keyboard input, it will delegate
// handling of text events to the selected keyboard input strategy.
class KeyboardInterpreter {
 public:
  explicit KeyboardInterpreter();

  KeyboardInterpreter(const KeyboardInterpreter&) = delete;
  KeyboardInterpreter& operator=(const KeyboardInterpreter&) = delete;

  ~KeyboardInterpreter();

  // If |input_injector| is nullptr, all methods below will have no effect.
  void SetContext(ClientInputInjector* input_injector);

  // Assembles the key events and then delegates to |KeyboardInputStrategy| to
  // send the keys.
  void HandleKeypressEvent(const KeypressInfo& keypress);
  // Delegates to |KeyboardInputStrategy| to covert and send the input.
  void HandleTextEvent(const std::string& text, uint8_t modifiers);
  // Delegates to |KeyboardInputStrategy| to covert and send the delete.
  void HandleDeleteEvent(uint8_t modifiers);
  // Assembles CTRL+ALT+DEL key event and then delegates to
  // |KeyboardInputStrategy| send the keys.
  void HandleCtrlAltDeleteEvent();
  // Assembles PRINT_SCREEN key event and then delegates to
  // |KeyboardInputStrategy| send the keys.
  void HandlePrintScreenEvent();

 private:
  std::unique_ptr<KeyboardInputStrategy> input_strategy_;
};

}  // namespace remoting
#endif  // REMOTING_CLIENT_INPUT_KEYBOARD_INTERPRETER_H_
