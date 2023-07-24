// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/message_loop/message_pump_type.h"
#include "base/task/single_thread_task_executor.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "build/build_config.h"
#include "mojo/core/embedder/embedder.h"
#include "remoting/host/resources.h"
#include "remoting/test/it2me_cli_host.h"

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#include "base/linux_util.h"
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

int main(int argc, char const* argv[]) {
  base::AtExitManager exitManager;
  base::CommandLine::Init(argc, argv);

  if (remoting::It2MeCliHost::ShouldPrintHelp()) {
    remoting::It2MeCliHost::PrintHelp();
    return 0;
  }

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  // Need to prime the host OS version value for linux to prevent IO on the
  // network thread. base::GetLinuxDistro() caches the result.
  base::GetLinuxDistro();
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

  base::SingleThreadTaskExecutor io_task_executor(base::MessagePumpType::IO);
  remoting::It2MeCliHost cli_host;

  base::ThreadPoolInstance::CreateAndStartWithDefaultParams("It2MeCliHost");
  mojo::core::Init();
  remoting::LoadResources("");

  cli_host.Start();

  return 0;
}
