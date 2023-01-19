// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/input_monitor/local_pointer_input_monitor.h"

#include <utility>

#include "base/functional/callback.h"
#include "base/notreached.h"
#include "base/task/single_thread_task_runner.h"
#include "remoting/host/input_monitor/local_mouse_input_monitor_x11.h"
#include "remoting/host/linux/wayland_utils.h"

namespace remoting {

std::unique_ptr<LocalPointerInputMonitor> LocalPointerInputMonitor::Create(
    scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> input_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
    LocalInputMonitor::PointerMoveCallback on_mouse_move,
    base::OnceClosure disconnect_callback) {
  if (IsRunningWayland()) {
    NOTIMPLEMENTED();
    return nullptr;
  }
  return std::make_unique<LocalMouseInputMonitorX11>(
      caller_task_runner, input_task_runner, std::move(on_mouse_move));
}

}  // namespace remoting
