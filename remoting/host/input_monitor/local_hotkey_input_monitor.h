// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_INPUT_MONITOR_LOCAL_HOTKEY_INPUT_MONITOR_H_
#define REMOTING_HOST_INPUT_MONITOR_LOCAL_HOTKEY_INPUT_MONITOR_H_

#include <memory>

#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace remoting {

// Monitors the local input to notify about keyboard hotkeys. If implemented for
// the platform, catches the disconnection keyboard shortcut (Ctrl-Alt-Esc) and
// invokes |disconnect_callback| when this key combination is pressed.
class LocalHotkeyInputMonitor {
 public:
  virtual ~LocalHotkeyInputMonitor() = default;

  // Creates a platform-specific instance of LocalHotkeyInputMonitor.
  // |disconnect_callback| is called on the |caller_task_runner| thread.
  static std::unique_ptr<LocalHotkeyInputMonitor> Create(
      scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> input_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
      base::OnceClosure disconnect_callback);

 protected:
  LocalHotkeyInputMonitor() = default;
};

}  // namespace remoting

#endif  // REMOTING_HOST_INPUT_MONITOR_LOCAL_HOTKEY_INPUT_MONITOR_H_
