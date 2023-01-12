// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_WIN_SESSION_INPUT_INJECTOR_H_
#define REMOTING_HOST_WIN_SESSION_INPUT_INJECTOR_H_

#include <memory>

#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "remoting/host/input_injector.h"

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace remoting {

// Monitors and passes key/mouse events to a nested event executor. Also handles
// Ctrl+Alt+Del and Win+L key combinations by invoking the given callbacks.
class SessionInputInjectorWin : public InputInjector {
 public:
  // |inject_sas| and |lock_workstation| are invoked on
  // |execute_action_task_runner|.
  SessionInputInjectorWin(
      scoped_refptr<base::SingleThreadTaskRunner> input_task_runner,
      std::unique_ptr<InputInjector> nested_executor,
      scoped_refptr<base::SingleThreadTaskRunner> execute_action_task_runner,
      const base::RepeatingClosure& inject_sas,
      const base::RepeatingClosure& lock_workstation);

  SessionInputInjectorWin(const SessionInputInjectorWin&) = delete;
  SessionInputInjectorWin& operator=(const SessionInputInjectorWin&) = delete;

  ~SessionInputInjectorWin() override;

  // InputInjector implementation.
  void Start(
      std::unique_ptr<protocol::ClipboardStub> client_clipboard) override;

  // protocol::ClipboardStub implementation.
  void InjectClipboardEvent(const protocol::ClipboardEvent& event) override;

  // protocol::InputStub implementation.
  void InjectKeyEvent(const protocol::KeyEvent& event) override;
  void InjectTextEvent(const protocol::TextEvent& event) override;
  void InjectMouseEvent(const protocol::MouseEvent& event) override;
  void InjectTouchEvent(const protocol::TouchEvent& event) override;

 private:
  // The actual implementation resides in SessionInputInjectorWin::Core class.
  class Core;
  scoped_refptr<Core> core_;
};

}  // namespace remoting

#endif  // REMOTING_HOST_WIN_SESSION_INPUT_INJECTOR_H_
