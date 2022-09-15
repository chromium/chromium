// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_WORKER_PROCESS_IPC_DELEGATE_H_
#define REMOTING_HOST_WORKER_PROCESS_IPC_DELEGATE_H_

#include <string>

#include <stdint.h>

#include "base/compiler_specific.h"

namespace mojo {
class ScopedInterfaceEndpointHandle;
}  // namespace mojo

namespace remoting {

// An interface representing the object receiving IPC messages from a worker
// process.
class WorkerProcessIpcDelegate {
 public:
  virtual ~WorkerProcessIpcDelegate() {}

  // Notifies that a client has been connected to the channel.
  virtual void OnChannelConnected(int32_t peer_pid) = 0;

  // Notifies that a permanent error was encountered.
  virtual void OnPermanentError(int exit_code) = 0;

  // Notifies that the worker process stops for any reason.
  virtual void OnWorkerProcessStopped() = 0;

  // Handles associated interface requests sent by the client.
  virtual void OnAssociatedInterfaceRequest(
      const std::string& interface_name,
      mojo::ScopedInterfaceEndpointHandle handle) = 0;
};

}  // namespace remoting

#endif  // REMOTING_HOST_WORKER_PROCESS_IPC_DELEGATE_H_
