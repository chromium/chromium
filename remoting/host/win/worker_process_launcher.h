// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_WIN_WORKER_PROCESS_LAUNCHER_H_
#define REMOTING_HOST_WIN_WORKER_PROCESS_LAUNCHER_H_

#include <stdint.h>

#include <memory>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/sequence_checker.h"
#include "base/timer/timer.h"
#include "base/win/object_watcher.h"
#include "base/win/scoped_handle.h"
#include "net/base/backoff_entry.h"

namespace base {
class Location;
class TimeDelta;
} // namespace base

namespace IPC {
class Message;
} // namespace IPC

namespace remoting {

class WorkerProcessIpcDelegate;

// Launches a worker process that is controlled via an IPC channel. All
// interaction with the spawned process is through WorkerProcessIpcDelegate and
// Send() method. In case of error the channel is closed and the worker process
// is terminated.
class WorkerProcessLauncher : public base::win::ObjectWatcher::Delegate {
 public:
  class Delegate {
   public:
    virtual ~Delegate();

    // Asynchronously starts the worker process and creates an IPC channel it
    // can connect to. |event_handler| must remain valid until KillProcess() has
    // been called.
    virtual void LaunchProcess(WorkerProcessLauncher* event_handler) = 0;

    // Sends an IPC message to the worker process. The message will be silently
    // dropped if the channel is closed.
    virtual void Send(IPC::Message* message) = 0;

    // Closes the IPC channel.
    virtual void CloseChannel() = 0;

    // Terminates the worker process and closes the IPC channel.
    virtual void KillProcess() = 0;
  };

  // Creates the launcher that will use |launcher_delegate| to manage the worker
  // process and |ipc_handler| to handle IPCs. The caller must ensure that
  // |ipc_handler| must outlive this object.
  WorkerProcessLauncher(std::unique_ptr<Delegate> launcher_delegate,
                        WorkerProcessIpcDelegate* ipc_handler);
  ~WorkerProcessLauncher() override;

  // Asks the worker process to crash and generate a dump, and closes the IPC
  // channel. |location| is passed to the worker so that it is on the stack in
  // the dump. Restarts the worker process forcefully, if it does
  // not exit on its own.
  void Crash(const base::Location& location);

  // Sends an IPC message to the worker process. The message will be silently
  // dropped if Send() is called before Start() or after stutdown has been
  // initiated.
  void Send(IPC::Message* message);

  // Notification methods invoked by |Delegate|.

  // Invoked to pass a handle of the launched process back to the caller of
  // Delegate::LaunchProcess(). The delegate has to make sure that this method
  // is called before OnChannelConnected().
  void OnProcessLaunched(base::win::ScopedHandle worker_process);

  // Called when a fatal error occurs (i.e. a failed process launch).
  // The delegate must guarantee that no other notifications are delivered once
  // OnFatalError() has been called.
  void OnFatalError();

  // Mirrors methods of IPC::Listener to be invoked by |Delegate|. |Delegate|
  // has to validate |peer_pid| if necessary.
  bool OnMessageReceived(const IPC::Message& message);
  void OnChannelConnected(int32_t peer_pid);
  void OnChannelError();

 private:
  friend class WorkerProcessLauncherTest;

  // base::win::ObjectWatcher::Delegate implementation used to watch for
  // the worker process exiting.
  void OnObjectSignaled(HANDLE object) override;

  // Returns true when the object is being destroyed.
  bool stopping() const { return ipc_handler_ == nullptr; }

  // Attempts to launch the worker process. Schedules next launch attempt if
  // creation of the process fails.
  void LaunchWorker();

  // Called to record outcome of a launch attempt: success or failure.
  void RecordLaunchResult();

  // Called by the test to record a successful launch attempt.
  void RecordSuccessfulLaunchForTest();

  // Set the desired timeout for |kill_process_timer_|.
  void SetKillProcessTimeoutForTest(const base::TimeDelta& timeout);

  // Stops the worker process and schedules next launch attempt unless the
  // object is being destroyed already.
  void StopWorker();

  // Handles IPC messages sent by the worker process.
  WorkerProcessIpcDelegate* ipc_handler_;

  // Implements specifics of launching a worker process.
  std::unique_ptr<WorkerProcessLauncher::Delegate> launcher_delegate_;

  // Keeps the exit code of the worker process after it was closed. The exit
  // code is used to determine whether the process has to be restarted.
  DWORD exit_code_;

  // True if IPC messages should be passed to |ipc_handler_|.
  bool ipc_enabled_;

  // The timer used to delay termination of the worker process when an IPC error
  // occured or when Crash() request is pending
  base::OneShotTimer kill_process_timer_;

  // The default timeout for |kill_process_timer_|.
  base::TimeDelta kill_process_timeout_;

  // State used to backoff worker launch attempts on failure.
  net::BackoffEntry launch_backoff_;

  // Timer used to schedule the next attempt to launch the process.
  base::OneShotTimer launch_timer_;

  // Monitors |worker_process_| to detect when the launched process
  // terminates.
  base::win::ObjectWatcher process_watcher_;

  // Timer used to detect whether a launch attempt was successful or not, and to
  // cancel the launch attempt if it is taking too long.
  base::OneShotTimer launch_result_timer_;

  // The handle of the worker process, if launched.
  base::win::ScopedHandle worker_process_;

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(WorkerProcessLauncher);
};

}  // namespace remoting

#endif  // REMOTING_HOST_WIN_WORKER_PROCESS_LAUNCHER_H_
