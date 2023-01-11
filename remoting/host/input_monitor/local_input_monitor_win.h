// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_INPUT_MONITOR_LOCAL_INPUT_MONITOR_WIN_H_
#define REMOTING_HOST_INPUT_MONITOR_LOCAL_INPUT_MONITOR_WIN_H_

#include <windows.h>

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace remoting {

// Monitors local input and notifies callers for any raw input events that have
// been registered.
class LocalInputMonitorWin {
 public:
  // This interface provides the per-input type logic for registering and
  // receiving raw input for the underlying input mode.
  class RawInputHandler {
   public:
    virtual ~RawInputHandler() = default;

    // Registers interest in receiving raw input events passed to |hwnd|.
    virtual bool Register(HWND hwnd) = 0;

    // Unregisters raw input listener.
    virtual void Unregister() = 0;

    // Called for each raw input event.
    virtual void OnInputEvent(const RAWINPUT* input) = 0;

    // Called when an non-recoverable error in raw input event listener occurs.
    virtual void OnError() = 0;
  };

  virtual ~LocalInputMonitorWin() = default;

  // Creates a platform-specific instance of LocalInputMonitorWin.
  // Methods on |raw_input_handler| are executed on |ui_task_runner_|.
  static std::unique_ptr<LocalInputMonitorWin> Create(
      scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
      std::unique_ptr<RawInputHandler> raw_input_handler);

 protected:
  LocalInputMonitorWin() = default;
};

}  // namespace remoting

#endif  // REMOTING_HOST_INPUT_MONITOR_LOCAL_INPUT_MONITOR_WIN_H_
