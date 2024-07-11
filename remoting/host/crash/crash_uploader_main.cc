// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/crash/crash_uploader_main.h"

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/message_loop/message_pump_type.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/single_thread_task_executor.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "mojo/core/embedder/embedder.h"
#include "remoting/base/logging.h"
#include "remoting/host/base/host_exit_codes.h"
#include "remoting/host/crash/minidump_handler.h"

namespace remoting {

namespace {
constexpr char kTimeoutSwitchName[] = "timeout";
}

int CrashUploaderMain(int argc, char** argv) {
  base::AtExitManager exit_manager;

  base::SingleThreadTaskExecutor task_executor(base::MessagePumpType::IO);
  base::ThreadPoolInstance::CreateAndStartWithDefaultParams(
      "RemotingCrashUploader");
  auto task_runner = base::SingleThreadTaskRunner::GetCurrentDefault();

  base::CommandLine::Init(argc, argv);

  remoting::InitHostLogging();

  base::TimeDelta timeout;
  const base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(kTimeoutSwitchName)) {
    auto switch_value = command_line->GetSwitchValueASCII(kTimeoutSwitchName);
    int parsed_value = 0;
    if (base::StringToInt(switch_value, &parsed_value)) {
      timeout = base::Seconds(std::max(0, parsed_value));
    } else {
      LOG(WARNING) << "Failed to parse timeout switch value: " << switch_value;
      return 1;
    }
  }

  mojo::core::Init();

  MinidumpHandler minidump_handler;

  base::RunLoop run_loop;

  if (!timeout.is_zero()) {
    LOG(INFO) << "Watching for crash dumps for " << timeout.InSeconds()
              << " seconds...";
    task_runner->PostDelayedTask(FROM_HERE, run_loop.QuitClosure(), timeout);
  }

  run_loop.Run();

  // Block until tasks blocking shutdown have completed their execution.
  base::ThreadPoolInstance::Get()->Shutdown();

  return kSuccessExitCode;
}

}  // namespace remoting
