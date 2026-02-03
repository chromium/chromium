// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_WIN_WINDOWS_PROCESS_DELEGATE_H_
#define REMOTING_HOST_WIN_WINDOWS_PROCESS_DELEGATE_H_

#include "base/win/object_watcher.h"
#include "base/win/scoped_handle.h"
#include "remoting/host/worker_process_launcher.h"

namespace remoting {

// Serves as a base class for Windows implementations of
// WorkerProcessLauncher::Delegate. It holds the process watcher and handles
// process exit events.
class WindowsProcessDelegate : public WorkerProcessLauncher::Delegate,
                               public base::win::ObjectWatcher::Delegate {
 public:
  WindowsProcessDelegate();
  ~WindowsProcessDelegate() override;

 protected:
  // Called by the subclass to start watching the given process handle. The
  // subclass has to make sure that this method is called before
  // OnChannelConnected(). `event_handler_` must be set when this method is
  // called.
  void WatchProcess(base::win::ScopedHandle worker_process);

  // Subclass should set `event_handler_` in LaunchProcess().
  raw_ptr<WorkerProcessLauncher> event_handler_;

  // The handle of the worker process, if launched.
  base::win::ScopedHandle worker_process_;

  // Monitors |worker_process_| to detect when the launched process
  // terminates.
  base::win::ObjectWatcher process_watcher_;

 private:
  // base::win::ObjectWatcher::Delegate implementation used to watch for
  // the worker process exiting.
  void OnObjectSignaled(HANDLE object) override;
};

}  // namespace remoting

#endif  // REMOTING_HOST_WIN_WINDOWS_PROCESS_DELEGATE_H_
