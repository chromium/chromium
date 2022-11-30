// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_TEST_FAKE_ICE_CONNECTION_H_
#define REMOTING_TEST_FAKE_ICE_CONNECTION_H_

#include "remoting/protocol/channel_dispatcher_base.h"
#include "remoting/protocol/ice_transport.h"

namespace remoting {
namespace test {

class FakeIceConnection final
    : public protocol::IceTransport::EventHandler,
      public protocol::ChannelDispatcherBase::EventHandler {
 public:
  FakeIceConnection(scoped_refptr<protocol::TransportContext> transport_context,
                    base::OnceClosure on_closed);

  FakeIceConnection(const FakeIceConnection&) = delete;
  FakeIceConnection& operator=(const FakeIceConnection&) = delete;

  ~FakeIceConnection() override;

  void OnAuthenticated();

  protocol::Transport* transport() { return transport_.get(); }

 private:
  // protocol::IceTransport::EventHandler implementations.
  void OnIceTransportRouteChange(
      const std::string& channel_name,
      const protocol::TransportRoute& route) override;
  void OnIceTransportError(protocol::ErrorCode error) override;

  // ChannelDispatcherBase::EventHandler implementations.
  void OnChannelInitialized(
      protocol::ChannelDispatcherBase* channel_dispatcher) override;
  void OnChannelClosed(
      protocol::ChannelDispatcherBase* channel_dispatcher) override;

  // |transport_| must outlive |control_dispatcher_|.
  std::unique_ptr<protocol::IceTransport> transport_;
  std::unique_ptr<protocol::ChannelDispatcherBase> control_dispatcher_;
  base::OnceClosure on_closed_;
};

}  // namespace test
}  // namespace remoting

#endif  // REMOTING_TEST_FAKE_ICE_CONNECTION_H_
