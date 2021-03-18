// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/input_monitor/local_input_monitor.h"

#include <memory>

#include "base/memory/ref_counted.h"
#include "base/notreached.h"
#include "base/single_thread_task_runner.h"

namespace remoting {

std::unique_ptr<LocalInputMonitor> LocalInputMonitor::Create(
    scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> input_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner) {
  NOTIMPLEMENTED();
  return nullptr;
}

}  // namespace remoting
