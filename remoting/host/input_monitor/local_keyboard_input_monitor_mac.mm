// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/input_monitor/local_keyboard_input_monitor.h"

#include <utility>

#include "base/functional/callback.h"
#import "base/task/single_thread_task_runner.h"
#include "base/task/single_thread_task_runner.h"

namespace remoting {

std::unique_ptr<LocalKeyboardInputMonitor> LocalKeyboardInputMonitor::Create(
    scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> input_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
    LocalInputMonitor::KeyPressedCallback on_key_event_callback,
    base::OnceClosure disconnect_callback) {
  return nullptr;
}

}  // namespace remoting
