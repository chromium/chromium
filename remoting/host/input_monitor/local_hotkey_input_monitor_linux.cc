// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/input_monitor/local_hotkey_input_monitor.h"

#include <memory>
#include <utility>

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/single_thread_task_runner.h"

#if defined(REMOTING_USE_X11)
#include "remoting/host/input_monitor/local_hotkey_input_monitor_x11.h"
#endif

namespace remoting {

std::unique_ptr<LocalHotkeyInputMonitor> LocalHotkeyInputMonitor::Create(
    scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> input_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
    base::OnceClosure disconnect_callback) {
#if defined(REMOTING_USE_X11)
  return std::make_unique<LocalHotkeyInputMonitorX11>(
      caller_task_runner, input_task_runner, std::move(disconnect_callback));
#else
#error "Can't currently build CRD without x11."
#endif
}

}  // namespace remoting
