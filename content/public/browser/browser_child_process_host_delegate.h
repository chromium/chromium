// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_BROWSER_CHILD_PROCESS_HOST_DELEGATE_H_
#define CONTENT_PUBLIC_BROWSER_BROWSER_CHILD_PROCESS_HOST_DELEGATE_H_

#include <optional>
#include <string>

#include "content/common/content_export.h"
#include "ipc/ipc_listener.h"
#include "mojo/public/cpp/bindings/generic_pending_receiver.h"

namespace content {

// Interface that all users of BrowserChildProcessHost need to provide.
class CONTENT_EXPORT BrowserChildProcessHostDelegate : public IPC::Listener {
 public:
  ~BrowserChildProcessHostDelegate() override {}

  // Called when the process has been started.
  virtual void OnProcessLaunched() {}

  // Called if the process failed to launch.  In this case the process never
  // started so the code here is a platform specific error code.
  virtual void OnProcessLaunchFailed(int error_code) {}

  // Called if the process crashed. |exit_code| is the status returned when the
  // process crashed (for posix, as returned from waitpid(), for Windows, as
  // returned from GetExitCodeProcess()).
  virtual void OnProcessCrashed(int exit_code) {}

  // Returns a string identifying the primary service running in the child
  // process, if any.
  virtual std::optional<std::string> GetServiceName();

  // Binds an interface receiver in the host process, as requested by the child
  // process.
  virtual void BindHostReceiver(mojo::GenericPendingReceiver receiver) {}

  // Default no-op handler for incoming legacy IPCs, for processes that don't
  // use legacy IPC.
  bool OnMessageReceived(const IPC::Message& message) override;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_BROWSER_CHILD_PROCESS_HOST_DELEGATE_H_
