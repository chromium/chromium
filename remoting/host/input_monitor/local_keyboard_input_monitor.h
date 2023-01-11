// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_INPUT_MONITOR_LOCAL_KEYBOARD_INPUT_MONITOR_H_
#define REMOTING_HOST_INPUT_MONITOR_LOCAL_KEYBOARD_INPUT_MONITOR_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "remoting/host/input_monitor/local_input_monitor.h"

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace remoting {

// Monitors local input and notifies callers for any keyboard events.
class LocalKeyboardInputMonitor {
 public:
  virtual ~LocalKeyboardInputMonitor() = default;

  // Creates a platform-specific instance of LocalKeyboardInputMonitor.
  // Callbacks are called on the |caller_task_runner| thread.
  // |on_key_event_callback| is called for each keyboard event detected.
  // |disconnect_callback| is called if monitoring cannot be started.
  static std::unique_ptr<LocalKeyboardInputMonitor> Create(
      scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> input_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
      LocalInputMonitor::KeyPressedCallback on_key_event_callback,
      base::OnceClosure disconnect_callback);

 protected:
  LocalKeyboardInputMonitor() = default;
};

}  // namespace remoting

#endif  // REMOTING_HOST_INPUT_MONITOR_LOCAL_KEYBOARD_INPUT_MONITOR_H_
