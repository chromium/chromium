// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_WIN_UNPRIVILEGED_PROCESS_DELEGATE_H_
#define REMOTING_HOST_WIN_UNPRIVILEGED_PROCESS_DELEGATE_H_

#include <stdint.h>

#include <memory>

#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/sequence_checker.h"
#include "base/win/scoped_handle.h"
#include "ipc/ipc_listener.h"
#include "remoting/host/win/worker_process_launcher.h"

namespace base {
class CommandLine;
class SingleThreadTaskRunner;
} // namespace base

namespace IPC {
class ChannelProxy;
class Message;
} // namespace IPC

namespace remoting {

// Implements logic for launching and monitoring a worker process under a less
// privileged user account.
class UnprivilegedProcessDelegate : public IPC::Listener,
                                    public WorkerProcessLauncher::Delegate {
 public:
  UnprivilegedProcessDelegate(
      scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
      std::unique_ptr<base::CommandLine> target_command);
  ~UnprivilegedProcessDelegate() override;

  // WorkerProcessLauncher::Delegate implementation.
  void LaunchProcess(WorkerProcessLauncher* event_handler) override;
  void Send(IPC::Message* message) override;
  void CloseChannel() override;
  void KillProcess() override;

 private:
  // IPC::Listener implementation.
  bool OnMessageReceived(const IPC::Message& message) override;
  void OnChannelConnected(int32_t peer_pid) override;
  void OnChannelError() override;

  void ReportFatalError();
  void ReportProcessLaunched(base::win::ScopedHandle worker_process);

  // The task runner serving job object notifications.
  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;

  // Command line of the launched process.
  std::unique_ptr<base::CommandLine> target_command_;

  // The server end of the IPC channel used to communicate to the worker
  // process.
  std::unique_ptr<IPC::ChannelProxy> channel_;

  WorkerProcessLauncher* event_handler_;

  // The handle of the worker process, if launched.
  base::win::ScopedHandle worker_process_;

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(UnprivilegedProcessDelegate);
};

}  // namespace remoting

#endif  // REMOTING_HOST_WIN_UNPRIVILEGED_PROCESS_DELEGATE_H_
