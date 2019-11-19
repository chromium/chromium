// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_LINUX_SERVICES_SCOPED_PROCESS_H_
#define SANDBOX_LINUX_SERVICES_SCOPED_PROCESS_H_

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/process/process_handle.h"
#include "sandbox/sandbox_export.h"

namespace sandbox {

// fork() a child process that will run a Closure.
// After the Closure has run, the child will pause forever. If this object
// is detroyed, the child will be destroyed, even if the closure did not
// finish running. It's ok to signal the child from outside of this class to
// destroy it.
// This class cannot be instanciated from a multi-threaded process, as it needs
// to fork().
class SANDBOX_EXPORT ScopedProcess {
 public:
  // A new process will be created and |child_callback| will run in the child
  // process. This callback is allowed to terminate the process or to simply
  // return. If the callback returns, the process will wait forever.
  explicit ScopedProcess(base::OnceClosure child_callback);
  ~ScopedProcess();

  // Wait for the process to exit.
  // |got_signaled| tells how to interpret the return value: either as an exit
  // code, or as a signal number.
  // When this returns, the process will still not have been reaped and will
  // survive as a zombie for the lifetime of this object. This method can be
  // called multiple times.
  int WaitForExit(bool* got_signaled);

  // Wait for the |child_callback| passed at construction to run. Return false
  // if |child_callback| did not finish running and we know it never will (for
  // instance the child crashed or used _exit()).
  bool WaitForClosureToRun();
  base::ProcessId GetPid() { return child_process_id_; }

 private:
  bool IsOriginalProcess();

  base::ProcessId child_process_id_;
  base::ProcessId process_id_;
  int pipe_fds_[2];
  DISALLOW_COPY_AND_ASSIGN(ScopedProcess);
};

}  // namespace sandbox

#endif  // SANDBOX_LINUX_SERVICES_SCOPED_PROCESS_H_
