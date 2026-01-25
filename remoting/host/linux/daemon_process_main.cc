// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <functional>

#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_executor.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "remoting/base/auto_thread.h"
#include "remoting/base/auto_thread_task_runner.h"
#include "remoting/host/base/host_exit_codes.h"
#include "remoting/host/daemon_process.h"

namespace remoting {

int DaemonProcessMain() {
  base::ThreadPoolInstance::CreateAndStartWithDefaultParams("Me2Me daemon");
  base::SingleThreadTaskExecutor main_task_executor(base::MessagePumpType::UI);
  base::RunLoop run_loop;
  auto main_auto_thread_task_runner =
      base::MakeRefCounted<AutoThreadTaskRunner>(
          main_task_executor.task_runner(), run_loop.QuitClosure());
  auto io_task_runner = AutoThread::CreateWithType(
      "I/O thread", main_auto_thread_task_runner, base::MessagePumpType::IO);
  std::unique_ptr<DaemonProcess> daemon_process;
  daemon_process = DaemonProcess::Create(
      main_auto_thread_task_runner, io_task_runner,
      base::BindOnce(
          [](std::unique_ptr<DaemonProcess>& process) { process.reset(); },
          std::ref(daemon_process)));
  run_loop.Run();
  return kSuccessExitCode;
}

}  // namespace remoting
