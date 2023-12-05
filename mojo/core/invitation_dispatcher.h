// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_CORE_INVITATION_DISPATCHER_H_
#define MOJO_CORE_INVITATION_DISPATCHER_H_

#include <stdint.h>

#include <string_view>

#include "base/containers/flat_map.h"
#include "base/synchronization/lock.h"
#include "mojo/core/dispatcher.h"
#include "mojo/core/ports/port_ref.h"
#include "mojo/core/system_impl_export.h"

namespace mojo {
namespace core {

class MOJO_SYSTEM_IMPL_EXPORT InvitationDispatcher : public Dispatcher {
 public:
  InvitationDispatcher();

  InvitationDispatcher(const InvitationDispatcher&) = delete;
  InvitationDispatcher& operator=(const InvitationDispatcher&) = delete;

  // Dispatcher:
  Type GetType() const override;
  MojoResult Close() override;
  MojoResult AttachMessagePipe(std::string_view name,
                               ports::PortRef remote_peer_port) override;
  MojoResult ExtractMessagePipe(std::string_view name,
                                MojoHandle* message_pipe_handle) override;

  using PortMapping = base::flat_map<std::string, ports::PortRef>;
  PortMapping TakeAttachedPorts();

 private:
  ~InvitationDispatcher() override;

  base::Lock lock_;
  bool is_closed_ = false;
  PortMapping attached_ports_;
};

}  // namespace core
}  // namespace mojo

#endif  // MOJO_CORE_INVITATION_DISPATCHER_H_
