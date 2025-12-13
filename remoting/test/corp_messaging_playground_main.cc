// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/task/single_thread_task_executor.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "mojo/core/embedder/embedder.h"
#include "remoting/test/corp_messaging_playground.h"

int main(int argc, char const* argv[]) {
  base::AtExitManager exitManager;
  base::CommandLine::Init(argc, argv);
  base::SingleThreadTaskExecutor io_task_executor(base::MessagePumpType::IO);
  base::ThreadPoolInstance::CreateAndStartWithDefaultParams(
      "CorpMessagingPlayground");
  mojo::core::Init();

  auto* command_line = base::CommandLine::ForCurrentProcess();
  if (!command_line->HasSwitch("username")) {
    LOG(ERROR) << "Username is required. Please run with --username=<value>";
    return 1;
  }

  remoting::CorpMessagingPlayground playground(
      command_line->GetSwitchValueASCII("username"));
  playground.Start();
  return 0;
}
