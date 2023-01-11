// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_INPUT_MONITOR_LOCAL_POINTER_INPUT_MONITOR_H_
#define REMOTING_HOST_INPUT_MONITOR_LOCAL_POINTER_INPUT_MONITOR_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "remoting/host/input_monitor/local_input_monitor.h"

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace remoting {

// Monitors the local input and sends a notification via the callback passed in
// for every mouse or touch move event received.
class LocalPointerInputMonitor {
 public:
  virtual ~LocalPointerInputMonitor() = default;

  // Creates a platform-specific instance of LocalPointerInputMonitor.
  // Callbacks are called on the |caller_task_runner| thread.
  // |pointer_event_callback| is called for each pointer event detected.
  // |disconnect_callback| is called if monitoring cannot be started.
  static std::unique_ptr<LocalPointerInputMonitor> Create(
      scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> input_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
      LocalInputMonitor::PointerMoveCallback pointer_move_callback,
      base::OnceClosure disconnect_callback);

 protected:
  LocalPointerInputMonitor() = default;
};

}  // namespace remoting

#endif  // REMOTING_HOST_INPUT_MONITOR_LOCAL_POINTER_INPUT_MONITOR_H_
