// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/mojo_ipc_server.h"

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/logging.h"
#include "base/process/process_handle.h"
#include "base/task/thread_pool.h"
#include "build/build_config.h"
#include "mojo/public/cpp/system/invitation.h"

#if defined(OS_LINUX)
#include "base/files/file_util.h"
#endif  // defined(OS_LINUX)

#if defined(OS_WIN)
#include "base/strings/stringprintf.h"
#include "base/win/win_util.h"
#endif  // defined(OS_WIN)

namespace remoting {

namespace {

// Sends an invitation and returns the PendingReceiver. Must be called on an
// IO sequence.
// Note that this function won't wait for the other end to accept the
// invitation, even though it makes some blocking API calls.
mojo::ScopedMessagePipeHandle SendInvitationOnIoSequence(
    const mojo::NamedPlatformChannel::ServerName& server_name,
    uint64_t message_pipe_id) {
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
    return mojo::ScopedMessagePipeHandle();
  }
  options.security_descriptor = base::StringPrintf(
      L"O:%lsG:%lsD:(A;;GA;;;AU)", user_sid.c_str(), user_sid.c_str());
#endif  // defined(OS_WIN)

  mojo::NamedPlatformChannel channel(options);
  auto server_endpoint = channel.TakeServerEndpoint();
  if (!server_endpoint.is_valid()) {
    LOG(ERROR) << "Failed to send mojo invitation: Invalid server endpoint.";
    return mojo::ScopedMessagePipeHandle();
  }

  mojo::OutgoingInvitation invitation;
  auto message_pipe = invitation.AttachMessagePipe(message_pipe_id);
  mojo::OutgoingInvitation::Send(std::move(invitation),
                                 base::kNullProcessHandle,
                                 std::move(server_endpoint));
  return message_pipe;
}

}  // namespace

MojoIpcServerBase::MojoIpcServerBase(
    const mojo::NamedPlatformChannel::ServerName& server_name,
    uint64_t message_pipe_id)
    : server_name_(server_name),
      message_pipe_id_(message_pipe_id),
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
  CloseAllConnections();

#if defined(OS_LINUX)
  // Any pending invitations will become orphaned, and a client that accepts an
  // orphaned invitation may incorrectly believe that the server is still alive,
  // so we just simply delete the socket file to prevent clients from
  // connecting.
  io_sequence_->PostTask(FROM_HERE,
                         base::BindOnce(base::IgnoreResult(&base::DeleteFile),
                                        base::FilePath(server_name_)));
#endif  // defined(OS_LINUX)
}

void MojoIpcServerBase::SendInvitation() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!pending_message_pipe_watcher_.IsWatching());

  io_sequence_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&SendInvitationOnIoSequence, server_name_,
                     message_pipe_id_),
      base::BindOnce(&MojoIpcServerBase::OnInvitationSent,
                     weak_factory_.GetWeakPtr()));
}

void MojoIpcServerBase::OnInvitationSent(
    mojo::ScopedMessagePipeHandle message_pipe) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!pending_message_pipe_watcher_.IsWatching());
  DCHECK(!pending_message_pipe_.is_valid());

  if (!message_pipe.is_valid()) {
    LOG(ERROR) << "Message pipe is invalid";
    return;
  }

  pending_message_pipe_ = std::move(message_pipe);
  pending_message_pipe_watcher_.Watch(
      pending_message_pipe_.get(), MOJO_HANDLE_SIGNAL_READABLE,
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
    pending_message_pipe_.reset();
    return;
  }
  if (state.peer_closed()) {
    LOG(ERROR) << "Message pipe is closed.";
    pending_message_pipe_.reset();
    return;
  }
  pending_message_pipe_watcher_.Cancel();

  DCHECK(pending_message_pipe_.is_valid());
  OnInvitationAccepted(std::move(pending_message_pipe_));
}

}  // namespace remoting
