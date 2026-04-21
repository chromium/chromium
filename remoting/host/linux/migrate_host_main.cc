// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/linux/migrate_host_main.h"

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/message_loop/message_pump_type.h"
#include "base/task/single_thread_task_executor.h"
#include "remoting/base/logging.h"
#include "remoting/host/base/host_exit_codes.h"

namespace remoting {

int MigrateHostMain(int argc, char** argv) {
  base::AtExitManager exit_manager;
  base::SingleThreadTaskExecutor io_task_executor(base::MessagePumpType::IO);

  base::CommandLine::Init(argc, argv);
  InitHostLogging();

  HOST_LOG << "migrate_host binary started.";

  // TODO: crbug.com/502664397 - Implement host migration logic.

  return kSuccessExitCode;
}

}  // namespace remoting
