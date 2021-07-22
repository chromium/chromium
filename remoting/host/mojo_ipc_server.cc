// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/mojo_ipc_server.h"

#include <memory>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/logging.h"
#include "base/process/process_handle.h"
#include "base/task/thread_pool.h"
#include "build/build_config.h"
#include "mojo/public/cpp/system/isolated_connection.h"

#if defined(OS_WIN)
#include "base/strings/stringprintf.h"
#include "base/win/win_util.h"
#endif  // defined(OS_WIN)

namespace remoting {

struct MojoIpcServerBase::PendingConnection {
  std::unique_ptr<mojo::IsolatedConnection> connection;
  mojo::ScopedMessagePipeHandle message_pipe;
};

namespace {

// Sends an invitation and returns the PendingReceiver. Must be called on an
// IO sequence.
// Note that this function won't wait for the other end to accept the
// invitation, even though it makes some blocking API calls.
std::unique_ptr<MojoIpcServerBase::PendingConnection>
SendInvitationOnIoSequence(
    const mojo::NamedPlatformChannel::ServerName& server_name) {
  mojo::NamedPlatformChannel::Options options;
  options.server_name = server_name;

#if defined(OS_WIN)
  options.enforce_uniqueness = false;
  // Create a named pipe owned by the current user (the LocalService account
  // (SID: S-1-5-19) when running in the network process) which is available to
  // all authenticated users.
  // presubmit: allow wstring
  std::wstring user_sid;
  if (!base::win::GetUserSidString(&user_sid)) {
    LOG(ERROR) << "Failed to get user SID string.";
    return nullptr;
  }
  options.security_descriptor = base::StringPrintf(
      L"O:%lsG:%lsD:(A;;GA;;;AU)", user_sid.c_str(), user_sid.c_str());
#endif  // defined(OS_WIN)

  mojo::NamedPlatformChannel channel(options);
  auto server_endpoint = channel.TakeServerEndpoint();
  if (!server_endpoint.is_valid()) {
    LOG(ERROR) << "Failed to send mojo invitation: Invalid server endpoint.";
    return nullptr;
  }

  auto pending_connection =
      std::make_unique<MojoIpcServerBase::PendingConnection>();
  pending_connection->connection = std::make_unique<mojo::IsolatedConnection>();
  pending_connection->message_pipe =
      pending_connection->connection->Connect(std::move(server_endpoint));
  if (!pending_connection->message_pipe.is_valid()) {
    LOG(ERROR) << "Message pipe is invalid.";
    return nullptr;
  }
  return pending_connection;
}

}  // namespace

MojoIpcServerBase::MojoIpcServerBase(
    const mojo::NamedPlatformChannel::ServerName& server_name)
    : server_name_(server_name),
      pending_message_pipe_watcher_(FROM_HERE,
                                    mojo::SimpleWatcher::ArmingPolicy::MANUAL) {
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
  server_started_ = true;
  SendInvitation();
}

void MojoIpcServerBase::StopServer() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!server_started_) {
    return;
  }
  server_started_ = false;
  UntrackAllMessagePipes();
  active_connections_.clear();
}

void MojoIpcServerBase::Close(mojo::ReceiverId id) {
  UntrackMessagePipe(id);
  active_connections_.erase(id);
}

void MojoIpcServerBase::SendInvitation() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!pending_message_pipe_watcher_.IsWatching());

  io_sequence_->PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(&SendInvitationOnIoSequence, server_name_),
      base::BindOnce(&MojoIpcServerBase::OnInvitationSent,
                     weak_factory_.GetWeakPtr()));
}

void MojoIpcServerBase::OnInvitationSent(
    std::unique_ptr<MojoIpcServerBase::PendingConnection> pending_connection) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!pending_message_pipe_watcher_.IsWatching());
  DCHECK(!pending_connection_);

  if (!pending_connection) {
    LOG(ERROR) << "Connection failed.";
    return;
  }

  pending_connection_ = std::move(pending_connection);
  pending_message_pipe_watcher_.Watch(
      pending_connection_->message_pipe.get(), MOJO_HANDLE_SIGNAL_READABLE,
      MOJO_TRIGGER_CONDITION_SIGNALS_SATISFIED,
      base::BindRepeating(&MojoIpcServerBase::OnMessagePipeReady,
                          weak_factory_.GetWeakPtr()));
  pending_message_pipe_watcher_.ArmOrNotify();
}

void MojoIpcServerBase::OnMessagePipeReady(
    MojoResult result,
    const mojo::HandleSignalsState& state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (result != MOJO_RESULT_OK) {
    LOG(ERROR) << "Message pipe is not ready. Result: " << result;
    pending_connection_.reset();
    return;
  }
  if (state.peer_closed()) {
    LOG(ERROR) << "Message pipe is closed.";
    pending_connection_.reset();
    return;
  }
  pending_message_pipe_watcher_.Cancel();

  DCHECK(pending_connection_->message_pipe.is_valid());
  auto receiver_id =
      TrackMessagePipe(std::move(pending_connection_->message_pipe));
  active_connections_[receiver_id] = std::move(pending_connection_->connection);
  pending_connection_.reset();
  SendInvitation();
}

}  // namespace remoting
