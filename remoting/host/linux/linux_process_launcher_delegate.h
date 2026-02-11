// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_LINUX_LINUX_PROCESS_LAUNCHER_DELEGATE_H_
#define REMOTING_HOST_LINUX_LINUX_PROCESS_LAUNCHER_DELEGATE_H_

#include <memory>

#include "base/command_line.h"
#include "base/environment.h"
#include "base/files/file_path.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/process/process.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/thread_annotations.h"
#include "base/threading/sequence_bound.h"
#include "ipc/ipc_channel_proxy.h"
#include "ipc/ipc_listener.h"
#include "remoting/host/mojom/desktop_session.mojom.h"
#include "remoting/host/worker_process_launcher.h"

namespace remoting {

// Implements logic for launching and monitoring a worker process, which may be
// run as a different user.
class LinuxWorkerProcessLauncherDelegate
    : public WorkerProcessLauncher::Delegate,
      public IPC::Listener {
 public:
  struct LaunchOptions {
    explicit LaunchOptions(base::CommandLine command_line);
    LaunchOptions(LaunchOptions&&);
    LaunchOptions(const LaunchOptions&);
    ~LaunchOptions();

    // The command line to launch.
    base::CommandLine command_line;

    // If true, creates a session and sets the process group ID for the process
    // to be launched, i.e. calling setsid().
    bool new_session = false;

    // The effective user ID of the process to be launched. A negative value
    // indicates no change of the effective user.
    int uid = -1;

    // The effective  group ID of the process to be launched. A negative value
    // indicates no change of the effective group ID.
    int gid = -1;

    // The working directory of the process to be launched. An empty value
    // indicates no change of the working directory.
    base::FilePath working_dir;

    // The environment variables to be set on the process to be launched. Note
    // that none of the parent process' environment variables will be inherited.
    base::EnvironmentMap environment_variables;
  };

  LinuxWorkerProcessLauncherDelegate(
      LaunchOptions options,
      scoped_refptr<base::SingleThreadTaskRunner> io_task_runner);
  ~LinuxWorkerProcessLauncherDelegate() override;

  LinuxWorkerProcessLauncherDelegate(
      const LinuxWorkerProcessLauncherDelegate&) = delete;
  LinuxWorkerProcessLauncherDelegate& operator=(
      const LinuxWorkerProcessLauncherDelegate&) = delete;

  // WorkerProcessLauncher::Delegate implementation.
  void LaunchProcess(WorkerProcessLauncher* event_handler) override;
  void GetRemoteAssociatedInterface(
      mojo::GenericPendingAssociatedReceiver receiver) override;
  void CloseChannel() override;
  void CrashProcess(const base::Location& location) override;
  void KillProcess() override;

 private:
  class ProcessExitWatcher;

  // IPC::Listener implementation.
  void OnChannelConnected(int32_t peer_pid) override;
  void OnChannelError() override;
  void OnAssociatedInterfaceRequest(
      const std::string& interface_name,
      mojo::ScopedInterfaceEndpointHandle handle) override;

  void WatchForProcessExit();
  void OnProcessExited(int exit_code);

  void ReportFatalError();

  SEQUENCE_CHECKER(sequence_checker_);

  const LaunchOptions options_ GUARDED_BY_CONTEXT(sequence_checker_);

  // The server end of the IPC channel used to communicate to the worker
  // process.
  std::unique_ptr<IPC::ChannelProxy> channel_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // The worker process, if launched.
  base::Process worker_process_;

  base::SequenceBound<ProcessExitWatcher> process_exit_watcher_
      GUARDED_BY_CONTEXT(sequence_checker_);

  mojo::AssociatedRemote<mojom::WorkerProcessControl> worker_process_control_;

  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_
      GUARDED_BY_CONTEXT(sequence_checker_);
  raw_ptr<WorkerProcessLauncher> event_handler_
      GUARDED_BY_CONTEXT(sequence_checker_) = nullptr;

  base::WeakPtrFactory<LinuxWorkerProcessLauncherDelegate> weak_ptr_factory_{
      this};
};

}  // namespace remoting

#endif  // REMOTING_HOST_LINUX_LINUX_PROCESS_LAUNCHER_DELEGATE_H_
