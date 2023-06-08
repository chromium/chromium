// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/active_display_monitor.h"

#include "base/notreached.h"
#include "base/task/single_thread_task_runner.h"
#include "remoting/host/linux/active_display_monitor_x11.h"
#include "remoting/host/linux/wayland_utils.h"

namespace remoting {

std::unique_ptr<ActiveDisplayMonitor> ActiveDisplayMonitor::Create(
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
    Callback active_display_callback) {
  if (IsRunningWayland()) {
    NOTIMPLEMENTED();
    return nullptr;
  }
  return std::make_unique<ActiveDisplayMonitorX11>(
      ui_task_runner, std::move(active_display_callback));
}

}  // namespace remoting
