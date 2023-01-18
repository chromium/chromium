// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/input_monitor/local_pointer_input_monitor.h"

#include <utility>

#include "base/functional/callback.h"
#include "base/notreached.h"
#include "base/task/single_thread_task_runner.h"
#include "remoting/host/linux/wayland_utils.h"

#if defined(REMOTING_USE_X11)
#include "remoting/host/input_monitor/local_mouse_input_monitor_x11.h"
#endif

namespace remoting {

std::unique_ptr<LocalPointerInputMonitor> LocalPointerInputMonitor::Create(
    scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> input_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
    LocalInputMonitor::PointerMoveCallback on_mouse_move,
    base::OnceClosure disconnect_callback) {
#if defined(REMOTING_USE_WAYLAND)
  NOTIMPLEMENTED();
  return nullptr;
#elif defined(REMOTING_USE_X11)
  return std::make_unique<LocalMouseInputMonitorX11>(
      caller_task_runner, input_task_runner, std::move(on_mouse_move));
#else
#error "Should use either wayland or X11."
#endif
}

}  // namespace remoting
