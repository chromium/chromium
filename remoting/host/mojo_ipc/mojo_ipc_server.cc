// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/mojo_ipc/mojo_ipc_server.h"

#include <memory>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "remoting/host/mojo_ipc/mojo_caller_security_checker.h"
#include "remoting/host/mojo_ipc/mojo_server_endpoint_connector.h"

#if BUILDFLAG(IS_WIN)
#include "base/strings/stringprintf.h"
#include "base/win/win_util.h"
#endif  // BUILDFLAG(IS_WIN)

namespace remoting {

namespace {

// Delay to throttle resending invitations when there is a recurring error.
// TODO(yuweih): Implement backoff.
base::TimeDelta kResentInvitationOnErrorDelay = base::Seconds(5);

mojo::PlatformChannelServerEndpoint CreateServerEndpointOnIoSequence(
    const mojo::NamedPlatformChannel::ServerName& server_name) {
  mojo::NamedPlatformChannel::Options options;
  options.server_name = server_name;

#if BUILDFLAG(IS_WIN)
  options.enforce_uniqueness = false;
  // Create a named pipe owned by the current user (the LocalService account
  // (SID: S-1-5-19) when running in the network process) which is available to
  // all authenticated users.
  // presubmit: allow wstring
  std::wstring user_sid;
  if (!base::win::GetUserSidString(&user_sid)) {
    LOG(ERROR) << "Failed to get user SID string.";
    return mojo::PlatformChannelServerEndpoint();
  }
  options.security_descriptor = base::StringPrintf(
      L"O:%lsG:%lsD:(A;;GA;;;AU)", user_sid.c_str(), user_sid.c_str());
#endif  // BUILDFLAG(IS_WIN)

  mojo::NamedPlatformChannel channel(options);
  return channel.TakeServerEndpoint();
}

}  // namespace

MojoIpcServerBase::MojoIpcServerBase(
    const mojo::NamedPlatformChannel::ServerName& server_name)
    : server_name_(server_name) {
  io_sequence_ =
      base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()});
}

MojoIpcServerBase::~MojoIpcServerBase() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void MojoIpcServerBase::StartServer() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (server_started_) {
    return;
  }
  endpoint_connector_ = MojoServerEndpointConnector::Create(this);
  server_started_ = true;
  SendInvitation();
}

void MojoIpcServerBase::StopServer() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!server_started_) {
    return;
  }
  server_started_ = false;
  endpoint_connector_.reset();
  UntrackAllMessagePipes();
  active_connections_.clear();
}

void MojoIpcServerBase::Close(mojo::ReceiverId id) {
  UntrackMessagePipe(id);
  auto it = active_connections_.find(id);
  if (it != active_connections_.end()) {
    active_connections_.erase(it);
  }
}

void MojoIpcServerBase::SendInvitation() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  io_sequence_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&CreateServerEndpointOnIoSequence, server_name_),
      base::BindOnce(&MojoIpcServerBase::OnServerEndpointCreated,
                     weak_factory_.GetWeakPtr()));
}

void MojoIpcServerBase::OnIpcDisconnected() {
  if (disconnect_handler_) {
    disconnect_handler_.Run();
  }
  Close(current_receiver());
}

void MojoIpcServerBase::OnServerEndpointCreated(
    mojo::PlatformChannelServerEndpoint endpoint) {
  if (!server_started_) {
    // A server endpoint might be created on |io_sequence_| after StopServer()
    // is called, which should be ignored.
    return;
  }
  if (on_invitation_sent_callback_for_testing_) {
    on_invitation_sent_callback_for_testing_.Run();
  }
  if (!endpoint.is_valid()) {
    OnServerEndpointConnectionFailed();
    return;
  }
  endpoint_connector_->Connect(std::move(endpoint));
}

void MojoIpcServerBase::OnServerEndpointConnected(
    std::unique_ptr<mojo::IsolatedConnection> connection,
    mojo::ScopedMessagePipeHandle message_pipe,
    base::ProcessId peer_pid) {
  if (IsTrustedMojoEndpoint(peer_pid)) {
    auto receiver_id = TrackMessagePipe(std::move(message_pipe), peer_pid);
    active_connections_[receiver_id] = std::move(connection);
  } else {
    LOG(ERROR) << "Process " << peer_pid
               << " is not a trusted mojo endpoint. Connection refused.";
  }

  SendInvitation();
}

void MojoIpcServerBase::OnServerEndpointConnectionFailed() {
  resent_invitation_on_error_timer_.Start(FROM_HERE,
                                          kResentInvitationOnErrorDelay, this,
                                          &MojoIpcServerBase::SendInvitation);
}

}  // namespace remoting
