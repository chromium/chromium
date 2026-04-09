// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sys/file.h>
#include <sys/stat.h>

#include <functional>
#include <memory>
#include <utility>

#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/posix/eintr_wrapper.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_executor.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/time/time.h"
#include "net/base/backoff_entry.h"
#include "remoting/base/auto_thread.h"
#include "remoting/base/auto_thread_task_runner.h"
#include "remoting/base/logging.h"
#include "remoting/host/base/host_exit_codes.h"
#include "remoting/host/daemon_process.h"
#include "remoting/host/posix/signal_handler.h"

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

// The amount of time to wait for the daemon process to clean up before
// exiting.
constexpr base::TimeDelta kSigTermTimeout = base::Seconds(10);

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

  base::FilePath config_path = DaemonProcess::GetConfigPath();
  base::File config_file(config_path,
                         base::File::FLAG_OPEN | base::File::FLAG_READ);
  if (!config_file.IsValid()) {
    // The config file may be absent or unreadable. The daemon process should
    // not be run in this case.
    PLOG(ERROR) << "Failed to open the config file: "
                << base::File::ErrorToString(config_file.error_details());
    return kInvalidHostConfigurationExitCode;
  }
  // Apply an advisory lock to the config file to prevent multiple host
  // processes from using the same host config file. Only root processes can
  // apply locks to the config file, and the lock will be released once the file
  // descriptor is closed or the process is dead.
  if (HANDLE_EINTR(flock(config_file.GetPlatformFile(), LOCK_EX | LOCK_NB)) ==
      -1) {
    // Note: EWOULDBLOCK and EAGAIN are expanded to the same value.
    PLOG(ERROR)
        << ((errno == EWOULDBLOCK || errno == EAGAIN)
                ? "Failed to lock the config file. Another instance of the "
                  "host may be running"
                : "Failed to lock the config file");
    return kInitializationFailed;
  }
  struct stat file_stat;
  if (HANDLE_EINTR(fstat(config_file.GetPlatformFile(), &file_stat)) == -1) {
    PLOG(ERROR) << "Failed to stat the config file";
    return kInitializationFailed;
  }
  if (file_stat.st_mode & (S_IRGRP | S_IROTH | S_IWGRP | S_IWOTH)) {
    LOG(ERROR) << "Config file is readable or writable by other users.";
    return kInvalidHostConfigurationExitCode;
  }

  net::BackoffEntry backoff_entry(&kBackoffPolicy);

  std::unique_ptr<DaemonProcess> daemon_process;

  // Set up a dedicated IO thread for handling the SIGTERM signal.
  auto sigterm_handler_task_runner = AutoThread::CreateWithType(
      "SIGTERM handler thread", main_task_executor.task_runner(),
      base::MessagePumpType::IO);
  auto on_sigterm = base::BindRepeating(
      [](std::unique_ptr<DaemonProcess>& process, int signal) {
        HOST_LOG << "SIGTERM received.";
        if (!process) {
          HOST_LOG << "Nothing to cleanup. Exiting now.";
          exit(kSuccessExitCode);
        }
        base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
            FROM_HERE, base::BindOnce([]() {
              HOST_LOG << "Cleanup timed out. Exiting now.";
              exit(kSuccessExitCode);
            }),
            kSigTermTimeout);
        process->Cleanup(base::BindOnce([]() {
          HOST_LOG << "Cleanup completed. Exiting now.";
          exit(kSuccessExitCode);
        }));
      },
      std::ref(daemon_process));

  sigterm_handler_task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(
          base::IgnoreResult(&RegisterSignalHandler), SIGTERM,
          base::BindPostTask(main_task_executor.task_runner(), on_sigterm)));

  while (true) {
    base::RunLoop run_loop;

    // Note that this is just main_task_executor.task_runner() with a reference
    // counted wrapper. Code that uses SequencedTaskRunner::GetCurrentDefault()
    // will still get main_task_executor.task_runner() and won't increase the
    // reference count.
    auto main_auto_thread_task_runner =
        base::MakeRefCounted<AutoThreadTaskRunner>(
            main_task_executor.task_runner(), run_loop.QuitClosure());
    auto io_task_runner = AutoThread::CreateWithType(
        "I/O thread", main_auto_thread_task_runner, base::MessagePumpType::IO);
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
    // Use a run loop instead of the blocking PlatformThread::Sleep() to allow
    // the SIGTERM handler to be run.
    base::RunLoop backoff_run_loop;
    main_task_executor.task_runner()->PostDelayedTask(
        FROM_HERE, backoff_run_loop.QuitClosure(), backoff);
    backoff_run_loop.Run();
  }
}

}  // namespace remoting
