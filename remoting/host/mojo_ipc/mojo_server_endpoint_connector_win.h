// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_MOJO_IPC_MOJO_SERVER_ENDPOINT_CONNECTOR_WIN_H_
#define REMOTING_HOST_MOJO_IPC_MOJO_SERVER_ENDPOINT_CONNECTOR_WIN_H_

#include <windows.h>

#include "base/memory/raw_ptr.h"
#include "base/sequence_checker.h"
#include "base/synchronization/waitable_event.h"
#include "base/synchronization/waitable_event_watcher.h"
#include "base/thread_annotations.h"
#include "base/win/scoped_handle.h"
#include "base/win/windows_types.h"
#include "remoting/host/mojo_ipc/mojo_server_endpoint_connector.h"

namespace remoting {

// Windows implementation for MojoServerEndpointConnector.
class MojoServerEndpointConnectorWin final
    : public MojoServerEndpointConnector {
 public:
  explicit MojoServerEndpointConnectorWin(Delegate* delegate);
  MojoServerEndpointConnectorWin(const MojoServerEndpointConnectorWin&) =
      delete;
  MojoServerEndpointConnectorWin& operator=(
      const MojoServerEndpointConnectorWin&) = delete;
  ~MojoServerEndpointConnectorWin() override;

  void Connect(mojo::PlatformChannelServerEndpoint server_endpoint) override;

 private:
  void OnConnectedEventSignaled(base::WaitableEvent* event);

  void OnReady();
  void OnError();

  void ResetConnectionObjects();

  SEQUENCE_CHECKER(sequence_checker_);

  raw_ptr<Delegate> delegate_ GUARDED_BY_CONTEXT(sequence_checker_);
  base::WaitableEventWatcher client_connection_watcher_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Non-null when there is a pending connection.
  base::win::ScopedHandle pending_named_pipe_handle_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Signaled by ConnectNamedPipe() once |pending_named_pipe_handle_| is
  // connected to a client.
  base::WaitableEvent client_connected_event_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Object to allow ConnectNamedPipe() to run asynchronously.
  OVERLAPPED connect_overlapped_ GUARDED_BY_CONTEXT(sequence_checker_);
};

}  // namespace remoting

#endif  // REMOTING_HOST_MOJO_IPC_MOJO_SERVER_ENDPOINT_CONNECTOR_WIN_H_
