// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_WIN_WTS_SESSION_PROCESS_DELEGATE_H_
#define REMOTING_HOST_WIN_WTS_SESSION_PROCESS_DELEGATE_H_

#include <stdint.h>

#include <memory>

#include "base/compiler_specific.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "remoting/host/win/worker_process_launcher.h"

namespace base {
class CommandLine;
class SingleThreadTaskRunner;
}  // namespace base

namespace remoting {

// Implements logic for launching and monitoring a worker process in a different
// session.
class WtsSessionProcessDelegate : public WorkerProcessLauncher::Delegate {
 public:
  WtsSessionProcessDelegate(
      scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
      std::unique_ptr<base::CommandLine> target,
      bool launch_elevated,
      const std::string& channel_security);

  WtsSessionProcessDelegate(const WtsSessionProcessDelegate&) = delete;
  WtsSessionProcessDelegate& operator=(const WtsSessionProcessDelegate&) =
      delete;

  ~WtsSessionProcessDelegate() override;

  // Initializes the object returning true on success.
  bool Initialize(uint32_t session_id);

  // WorkerProcessLauncher::Delegate implementation.
  void LaunchProcess(WorkerProcessLauncher* event_handler) override;
  void GetRemoteAssociatedInterface(
      mojo::GenericPendingAssociatedReceiver receiver) override;
  void CloseChannel() override;
  void CrashProcess(const base::Location& location) override;
  void KillProcess() override;

 private:
  // The actual implementation resides in WtsSessionProcessDelegate::Core class.
  class Core;
  scoped_refptr<Core> core_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace remoting

#endif  // REMOTING_HOST_WIN_WTS_SESSION_PROCESS_DELEGATE_H_
