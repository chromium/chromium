// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/system/isolated_connection.h"

#include "mojo/public/cpp/system/invitation.h"

namespace mojo {

IsolatedConnection::IsolatedConnection()
    : token_(base::UnguessableToken::Create()) {}

// Isolated connections are not transient and can outlive this object.
IsolatedConnection::~IsolatedConnection() = default;

ScopedMessagePipeHandle IsolatedConnection::Connect(
    PlatformChannelEndpoint endpoint) {
  return Connect(std::move(endpoint), base::Process{});
}

ScopedMessagePipeHandle IsolatedConnection::Connect(
    PlatformChannelEndpoint endpoint,
    base::Process process) {
  return Connect(std::move(endpoint), std::move(process),
                 MOJO_SEND_INVITATION_FLAG_NONE);
}

ScopedMessagePipeHandle IsolatedConnection::Connect(
    PlatformChannelEndpoint endpoint,
    base::Process process,
    MojoSendInvitationFlags invitation_flags) {
  return OutgoingInvitation::SendIsolated(std::move(endpoint),
                                          token_.ToString(), process.Handle(),
                                          invitation_flags);
}

ScopedMessagePipeHandle IsolatedConnection::Connect(
    PlatformChannelServerEndpoint endpoint) {
  return OutgoingInvitation::SendIsolated(std::move(endpoint),
                                          token_.ToString());
}

}  // namespace mojo
