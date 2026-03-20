// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <functional>
#include <memory>
#include <utility>

#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/task/single_thread_task_executor.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "net/base/backoff_entry.h"
#include "remoting/base/auto_thread.h"
#include "remoting/base/auto_thread_task_runner.h"
#include "remoting/base/logging.h"
#include "remoting/host/base/host_exit_codes.h"
#include "remoting/host/daemon_process.h"

namespace remoting {

namespace {

const net::BackoffEntry::Policy kBackoffPolicy = {
    // Do not ignore initial errors before applying exponential back-off rules.
    0,

    // Initial delay.
    5000,

    // Factor by which the waiting time will be multiplied.
    1.5,

    // Fuzzing percentage.
    0.2,

    // Maximum amount of time (in ms) we are willing to delay our request.
    60000,

    // Never discard an entry.
    -1,

    // Always use the initial delay, even before we've seen num_errors_to_ignore
    // errors.
    true,
};

// How long a process must run in order to reset the backoff and failure count.
constexpr base::TimeDelta kMinProcessLifetime = base::Seconds(60);

// How many transient failures are allowed before the daemon process exits
// permanently.
constexpr int kMaxTransientFailures = 15;

std::string ExitCodeToString(int exit_code) {
  const char* exit_code_string_ptr = ExitCodeToStringUnchecked(exit_code);
  return exit_code_string_ptr
             ? base::StringPrintf("%s (%d)", exit_code_string_ptr, exit_code)
             : base::NumberToString(exit_code);
}

}  // namespace

int DaemonProcessMain() {
  base::ThreadPoolInstance::CreateAndStartWithDefaultParams("Me2Me daemon");
  base::SingleThreadTaskExecutor main_task_executor(base::MessagePumpType::UI);

  net::BackoffEntry backoff_entry(&kBackoffPolicy);

  while (true) {
    base::RunLoop run_loop;
    auto main_auto_thread_task_runner =
        base::MakeRefCounted<AutoThreadTaskRunner>(
            main_task_executor.task_runner(), run_loop.QuitClosure());
    auto io_task_runner = AutoThread::CreateWithType(
        "I/O thread", main_auto_thread_task_runner, base::MessagePumpType::IO);
    std::unique_ptr<DaemonProcess> daemon_process;
    int daemon_exit_code = kSuccessExitCode;

    base::TimeTicks launch_time = base::TimeTicks::Now();

    daemon_process = DaemonProcess::Create(
        // Move task runners so that we don't hold a strong reference to
        // `main_auto_thread_task_runner`, which would prevent `run_loop` from
        // quitting.
        std::move(main_auto_thread_task_runner), std::move(io_task_runner),
        base::BindOnce(
            [](std::unique_ptr<DaemonProcess>& process, int* exit_code_ptr,
               int exit_code) {
              *exit_code_ptr = exit_code;
              process.reset();
            },
            std::ref(daemon_process), &daemon_exit_code));
    run_loop.Run();

    std::string exit_code_string = ExitCodeToString(daemon_exit_code);

    if (daemon_exit_code == kInvalidHostIdExitCode ||
        daemon_exit_code == kHostDeletedExitCode) {
      HOST_LOG << "Host no longer exists. Exit code: " << exit_code_string
               << ". Deleting host config file.";
      base::DeleteFile(DaemonProcess::GetConfigPath());
      return daemon_exit_code;
    }
    if (daemon_exit_code >= kMinPermanentErrorExitCode &&
        daemon_exit_code <= kMaxPermanentErrorExitCode) {
      LOG(ERROR) << "Host reported permanent error: " << exit_code_string
                 << ". Exiting.";
      return daemon_exit_code;
    }
    LOG(WARNING) << "Host reported error: " << exit_code_string;

    if (base::TimeTicks::Now() - launch_time >= kMinProcessLifetime) {
      backoff_entry.Reset();
    }

    backoff_entry.InformOfRequest(false);

    if (backoff_entry.failure_count() > kMaxTransientFailures) {
      LOG(ERROR) << "Too many launch failures ("
                 << backoff_entry.failure_count() << "), exiting.";
      return daemon_exit_code;
    }

    base::TimeDelta backoff = backoff_entry.GetTimeUntilRelease();
    HOST_LOG << "Waiting " << backoff << " before relaunching.";
    base::PlatformThread::Sleep(backoff);
  }
}

}  // namespace remoting
