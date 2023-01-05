// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_CHILD_PROCESS_HOST_DELEGATE_H_
#define CONTENT_PUBLIC_BROWSER_CHILD_PROCESS_HOST_DELEGATE_H_

#include "base/process/process.h"
#include "ipc/ipc_listener.h"
#include "mojo/public/cpp/bindings/generic_pending_receiver.h"
#include "mojo/public/cpp/system/message_pipe.h"

namespace IPC {
class Channel;
}

namespace content {

// Interface that all users of ChildProcessHost need to provide.
class ChildProcessHostDelegate : public IPC::Listener {
 public:
  ~ChildProcessHostDelegate() override {}

  // Called when the IPC channel for the child process is initialized.
  virtual void OnChannelInitialized(IPC::Channel* channel) {}

  // Called when the child process unexpected closes the IPC channel. Delegates
  // would normally delete the object in this case.
  virtual void OnChildDisconnected() {}

  // Returns a reference to the child process. This can be called only after
  // OnProcessLaunched is called or it will be invalid and may crash.
  virtual const base::Process& GetProcess() = 0;

  // Binds an interface receiver in the host process, as requested by the child
  // process.
  virtual void BindHostReceiver(mojo::GenericPendingReceiver receiver) {}
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_CHILD_PROCESS_HOST_DELEGATE_H_
