// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_executor.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "mojo/core/embedder/embedder.h"
#include "mojo/core/embedder/scoped_ipc_support.h"
#include "remoting/base/logging.h"
#include "remoting/host/mac/agent_process_broker.h"

int main(int argc, char const* argv[]) {
  base::AtExitManager exitManager;
  base::CommandLine::Init(argc, argv);
  remoting::InitHostLogging();
  base::ThreadPoolInstance::CreateAndStartWithDefaultParams(
      "AgentProcessBroker");
  base::SingleThreadTaskExecutor main_task_executor(base::MessagePumpType::IO);
  auto task_runner = base::SingleThreadTaskRunner::GetCurrentDefault();
  mojo::core::Init({
      .is_broker_process = true,
  });
  mojo::core::ScopedIPCSupport ipc_support(
      task_runner, mojo::core::ScopedIPCSupport::ShutdownPolicy::FAST);

  remoting::AgentProcessBroker agent_process_broker;
  base::RunLoop run_loop;
  agent_process_broker.Start();
  run_loop.Run();
  return 0;
}
