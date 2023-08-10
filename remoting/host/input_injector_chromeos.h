// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_INPUT_INJECTOR_CHROMEOS_H_
#define REMOTING_HOST_INPUT_INJECTOR_CHROMEOS_H_

#include <memory>

#include "base/task/single_thread_task_runner.h"
#include "remoting/host/input_injector.h"

namespace ui {
class SystemInputInjector;
}  // namespace ui

namespace remoting {

// InputInjector implementation that translates input to ui::Events and passes
// them to a supplied delegate for injection into ChromeOS.
class InputInjectorChromeos : public InputInjector {
 public:
  explicit InputInjectorChromeos(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner);

  InputInjectorChromeos(const InputInjectorChromeos&) = delete;
  InputInjectorChromeos& operator=(const InputInjectorChromeos&) = delete;

  ~InputInjectorChromeos() override;

  // Clipboard stub interface.
  void InjectClipboardEvent(const protocol::ClipboardEvent& event) override;

  // InputStub interface.
  void InjectKeyEvent(const protocol::KeyEvent& event) override;
  void InjectTextEvent(const protocol::TextEvent& event) override;
  void InjectMouseEvent(const protocol::MouseEvent& event) override;
  void InjectTouchEvent(const protocol::TouchEvent& event) override;

  // InputInjector interface.
  void Start(
      std::unique_ptr<protocol::ClipboardStub> client_clipboard) override;

  // Overload for testing that allows injecting our own system input injector.
  void StartForTesting(
      std::unique_ptr<ui::SystemInputInjector> input_injector,
      std::unique_ptr<protocol::ClipboardStub> client_clipboard);

 private:
  class Core;

  // Task runner for input injection.
  scoped_refptr<base::SingleThreadTaskRunner> input_task_runner_;
  std::unique_ptr<Core> core_;
};

}  // namespace remoting

#endif  // REMOTING_HOST_INPUT_INJECTOR_CHROMEOS_H_
