// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/files/file.h"
#include "base/message_loop/message_pump_type.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_executor.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "mojo/core/embedder/embedder.h"
#include "mojo/core/embedder/scoped_ipc_support.h"
#include "remoting/host/logging.h"
#include "remoting/host/native_messaging/native_messaging_pipe.h"
#include "remoting/host/native_messaging/pipe_messaging_channel.h"
#include "remoting/host/remote_auth_native_messaging_host.h"

int main(int argc, char** argv) {
  base::AtExitManager exit_manager;
  base::SingleThreadTaskExecutor task_executor(base::MessagePumpType::IO);
  auto task_runner = base::ThreadTaskRunnerHandle::Get();

  base::CommandLine::Init(argc, argv);
  remoting::InitHostLogging();

  mojo::core::Init();
  mojo::core::ScopedIPCSupport ipc_support(
      task_runner, mojo::core::ScopedIPCSupport::ShutdownPolicy::FAST);

  base::File read_file;
  base::File write_file;

#if defined(OS_POSIX)
  read_file = base::File(STDIN_FILENO);
  write_file = base::File(STDOUT_FILENO);
#else
  NOTIMPLEMENTED();
#endif

  base::RunLoop run_loop;

  remoting::NativeMessagingPipe native_messaging_pipe;
  auto channel = std::make_unique<remoting::PipeMessagingChannel>(
      std::move(read_file), std::move(write_file));

#if defined(OS_POSIX)
  remoting::PipeMessagingChannel::ReopenStdinStdout();
#endif  // defined(OS_POSIX)

  auto native_messaging_host =
      std::make_unique<remoting::RemoteAuthNativeMessagingHost>(task_runner);
  native_messaging_host->Start(&native_messaging_pipe);
  native_messaging_pipe.Start(std::move(native_messaging_host),
                              std::move(channel));

  run_loop.Run();

  return 0;
}
