// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_WIN_WTS_SESSION_PROCESS_DELEGATE_H_
#define REMOTING_HOST_WIN_WTS_SESSION_PROCESS_DELEGATE_H_

#include <stdint.h>

#include <memory>

#include "base/compiler_specific.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/process/process_handle.h"
#include "base/sequence_checker.h"
#include "remoting/host/win/windows_process_delegate.h"

namespace base {
class CommandLine;
class SingleThreadTaskRunner;
}  // namespace base

namespace mojo {
class PlatformChannelServerEndpoint;
}  // namespace mojo

namespace remoting {

// Implements logic for launching and monitoring a worker process in a different
// session.
class WtsSessionProcessDelegate : public WindowsProcessDelegate {
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

  class TestApi {
   public:
    explicit TestApi(WtsSessionProcessDelegate* delegate);

    // Assigns |process| to the job object so that tests can exercise the
    // job-object shutdown path without launching a process in another session.
    // Returns true on success. The delegate must have been created with
    // |launch_elevated| and the job must have been initialized.
    bool AssignProcessToJob(base::ProcessHandle process);

    // Registers |callback| to be run when the internal Core object is
    // destroyed.
    void SetCoreDeletedCallback(base::OnceClosure callback);

    // Simulates an I/O completion event from the job object on the I/O thread.
    void SimulateJobNotification(DWORD event, base::ProcessId pid);

    // Returns the currently stored worker process ID.
    base::ProcessId GetWorkerProcessPid() const;

    // Sets the elevated launcher PID.
    void SetElevatedLauncherPid(base::ProcessId pid);

    // Sets the worker process PID.
    void SetWorkerProcessPid(base::ProcessId pid);

    // Invokes OnProcessLaunchDetected directly.
    void OnProcessLaunchDetected(base::ProcessId pid);

    // Sets the elevated server endpoint.
    void SetElevatedServerEndpoint(
        mojo::PlatformChannelServerEndpoint endpoint);

    // Registers callbacks for fatal error and process launched events.
    void SetFatalErrorCallback(base::OnceClosure callback);
    void SetProcessLaunchedCallback(base::OnceClosure callback);

   private:
    raw_ptr<WtsSessionProcessDelegate> delegate_;
  };

 private:
  // The actual implementation resides in WtsSessionProcessDelegate::Core class.
  class Core;
  scoped_refptr<Core> core_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<WtsSessionProcessDelegate> weak_ptr_factory_{this};
};

}  // namespace remoting

#endif  // REMOTING_HOST_WIN_WTS_SESSION_PROCESS_DELEGATE_H_
