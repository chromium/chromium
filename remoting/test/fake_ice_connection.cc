// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/test/fake_ice_connection.h"

#include "remoting/base/logging.h"
#include "remoting/protocol/client_control_dispatcher.h"
#include "remoting/protocol/host_control_dispatcher.h"
#include "remoting/protocol/transport_context.h"

namespace remoting {
namespace test {

FakeIceConnection::FakeIceConnection(
    scoped_refptr<protocol::TransportContext> transport_context,
    base::OnceClosure on_closed) {
  transport_ =
      std::make_unique<protocol::IceTransport>(transport_context, this);
  on_closed_ = std::move(on_closed);
  if (transport_context->role() == protocol::TransportRole::CLIENT) {
    control_dispatcher_ = std::make_unique<protocol::ClientControlDispatcher>();
  } else {
    control_dispatcher_ = std::make_unique<protocol::HostControlDispatcher>();
  }
}

FakeIceConnection::~FakeIceConnection() = default;

void FakeIceConnection::OnAuthenticated() {
  control_dispatcher_->Init(transport_->GetMultiplexedChannelFactory(), this);
}

void FakeIceConnection::OnIceTransportRouteChange(
    const std::string& channel_name,
    const protocol::TransportRoute& route) {
  HOST_LOG << "Channel: " << channel_name << " changed route:\n"
           << "Route type: " << route.type
           << ", remote address: " << route.remote_address.ToString()
           << ", local address: " << route.local_address.ToString();
}

void FakeIceConnection::OnIceTransportError(protocol::ErrorCode error) {
  LOG(ERROR) << "ICE transport error: " << error;
  std::move(on_closed_).Run();
}

void FakeIceConnection::OnChannelInitialized(
    protocol::ChannelDispatcherBase* channel_dispatcher) {
  HOST_LOG << "Channel initialized!";
  std::move(on_closed_).Run();
}

void FakeIceConnection::OnChannelClosed(
    protocol::ChannelDispatcherBase* channel_dispatcher) {
  HOST_LOG << "Channel closed!";
  std::move(on_closed_).Run();
}

}  // namespace test
}  // namespace remoting
