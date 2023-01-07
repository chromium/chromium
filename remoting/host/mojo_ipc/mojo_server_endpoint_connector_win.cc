// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/mojo_ipc/mojo_server_endpoint_connector_win.h"

#include <string.h>
#include <windows.h>

#include <memory>

#include "base/bind.h"
#include "base/check.h"
#include "base/logging.h"
#include "base/process/process_handle.h"
#include "base/sequence_checker.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/current_thread.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/win/scoped_handle.h"
#include "base/win/windows_types.h"
#include "mojo/public/cpp/platform/platform_channel_endpoint.h"
#include "mojo/public/cpp/platform/platform_handle.h"
#include "mojo/public/cpp/system/isolated_connection.h"

namespace remoting {

MojoServerEndpointConnectorWin::MojoServerEndpointConnectorWin(
    Delegate* delegate)
    : delegate_(delegate),
      client_connected_event_(base::WaitableEvent::ResetPolicy::MANUAL,
                              base::WaitableEvent::InitialState::NOT_SIGNALED) {
  DCHECK(delegate_);
}

MojoServerEndpointConnectorWin::~MojoServerEndpointConnectorWin() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void MojoServerEndpointConnectorWin::Connect(
    mojo::PlatformChannelServerEndpoint server_endpoint) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(server_endpoint.is_valid());
  DCHECK(!pending_named_pipe_handle_.IsValid());

  pending_named_pipe_handle_ =
      server_endpoint.TakePlatformHandle().TakeHandle();
  // The |lpOverlapped| argument of ConnectNamedPipe() has the annotation of
  // [in, out, optional], so we reset the content before passing it in, just to
  // be safe.
  memset(&connect_overlapped_, 0, sizeof(connect_overlapped_));
  connect_overlapped_.hEvent = client_connected_event_.handle();
  BOOL ok =
      ConnectNamedPipe(pending_named_pipe_handle_.Get(), &connect_overlapped_);
  if (ok) {
    PLOG(ERROR) << "Unexpected success while waiting for pipe connection";
    OnError();
    return;
  }

  const DWORD err = GetLastError();
  switch (err) {
    case ERROR_PIPE_CONNECTED:
      // A client has connected before the server calls ConnectNamedPipe().
      OnReady();
      return;
    case ERROR_IO_PENDING:
      client_connection_watcher_.StartWatching(
          &client_connected_event_,
          base::BindOnce(
              &MojoServerEndpointConnectorWin::OnConnectedEventSignaled,
              base::Unretained(this)),
          base::SequencedTaskRunnerHandle::Get());
      return;
    default:
      PLOG(ERROR) << "Unexpected error: " << err;
      OnError();
      return;
  }
}

void MojoServerEndpointConnectorWin::OnConnectedEventSignaled(
    base::WaitableEvent* event) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(&client_connected_event_, event);

  OnReady();
}

void MojoServerEndpointConnectorWin::OnReady() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  base::ProcessId peer_pid;
  if (!GetNamedPipeClientProcessId(pending_named_pipe_handle_.Get(),
                                   &peer_pid)) {
    PLOG(ERROR) << "Failed to get peer PID";
    OnError();
    return;
  }
  mojo::PlatformChannelEndpoint endpoint(
      mojo::PlatformHandle(std::move(pending_named_pipe_handle_)));
  if (!endpoint.is_valid()) {
    LOG(ERROR) << "Endpoint is invalid.";
    OnError();
    return;
  }
  ResetConnectionObjects();
  auto connection = std::make_unique<mojo::IsolatedConnection>();
  auto message_pipe = connection->Connect(std::move(endpoint));
  delegate_->OnServerEndpointConnected(std::move(connection),
                                       std::move(message_pipe), peer_pid);
}

void MojoServerEndpointConnectorWin::OnError() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  ResetConnectionObjects();
  delegate_->OnServerEndpointConnectionFailed();
}

// static
std::unique_ptr<MojoServerEndpointConnector>
MojoServerEndpointConnector::Create(Delegate* delegate) {
  return std::make_unique<MojoServerEndpointConnectorWin>(delegate);
}

void MojoServerEndpointConnectorWin::ResetConnectionObjects() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  client_connection_watcher_.StopWatching();
  client_connected_event_.Reset();
  pending_named_pipe_handle_.Close();
}

}  // namespace remoting
