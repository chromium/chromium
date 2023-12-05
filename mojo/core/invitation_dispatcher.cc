// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/core/invitation_dispatcher.h"

#include "mojo/core/core.h"

namespace mojo {
namespace core {

InvitationDispatcher::InvitationDispatcher() = default;

Dispatcher::Type InvitationDispatcher::GetType() const {
  return Type::INVITATION;
}

MojoResult InvitationDispatcher::Close() {
  PortMapping attached_ports;
  {
    base::AutoLock lock(lock_);
    if (is_closed_)
      return MOJO_RESULT_INVALID_ARGUMENT;
    is_closed_ = true;
    std::swap(attached_ports, attached_ports_);
  }
  for (auto& entry : attached_ports)
    Core::Get()->GetNodeController()->ClosePort(entry.second);
  return MOJO_RESULT_OK;
}

MojoResult InvitationDispatcher::AttachMessagePipe(
    std::string_view name,
    ports::PortRef remote_peer_port) {
  base::AutoLock lock(lock_);
  auto result = attached_ports_.emplace(std::string(name), remote_peer_port);
  if (!result.second) {
    Core::Get()->GetNodeController()->ClosePort(remote_peer_port);
    return MOJO_RESULT_ALREADY_EXISTS;
  }
  return MOJO_RESULT_OK;
}

MojoResult InvitationDispatcher::ExtractMessagePipe(
    std::string_view name,
    MojoHandle* message_pipe_handle) {
  ports::PortRef remote_peer_port;
  {
    base::AutoLock lock(lock_);
    auto it = attached_ports_.find(std::string(name));
    if (it == attached_ports_.end())
      return MOJO_RESULT_NOT_FOUND;
    remote_peer_port = std::move(it->second);
    attached_ports_.erase(it);
  }

  *message_pipe_handle =
      Core::Get()->CreatePartialMessagePipe(remote_peer_port);
  if (*message_pipe_handle == MOJO_HANDLE_INVALID)
    return MOJO_RESULT_RESOURCE_EXHAUSTED;
  return MOJO_RESULT_OK;
}

InvitationDispatcher::PortMapping InvitationDispatcher::TakeAttachedPorts() {
  PortMapping attached_ports;
  {
    base::AutoLock lock(lock_);
    std::swap(attached_ports, attached_ports_);
  }
  return attached_ports;
}

InvitationDispatcher::~InvitationDispatcher() {
  DCHECK(is_closed_);
}

}  // namespace core
}  // namespace mojo
