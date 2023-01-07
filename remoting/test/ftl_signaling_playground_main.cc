// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/message_loop/message_pump_type.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_executor.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "mojo/core/embedder/embedder.h"
#include "remoting/test/ftl_signaling_playground.h"

int main(int argc, char const* argv[]) {
  base::AtExitManager exitManager;
  base::CommandLine::Init(argc, argv);

  base::SingleThreadTaskExecutor io_task_executor(base::MessagePumpType::IO);
  remoting::FtlSignalingPlayground playground;

  if (playground.ShouldPrintHelp()) {
    playground.PrintHelp();
    return 0;
  }

  base::ThreadPoolInstance::CreateAndStartWithDefaultParams(
      "FtlSignalingPlayground");
  mojo::core::Init();

  playground.StartLoop();

  return 0;
}
