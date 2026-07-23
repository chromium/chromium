// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_PEER_SESSION_H_
#define REMOTING_HOST_PEER_SESSION_H_

#include <memory>
#include <string>
#include <string_view>

#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "remoting/base/source_location.h"
#include "remoting/host/mojom/chromoting_host_services.mojom.h"
#include "remoting/protocol/connection_to_client.h"
#include "remoting/protocol/errors.h"
#include "remoting/protocol/host_stub.h"

namespace remoting {

class SessionOptions;
struct SessionPolicies;

// A PeerSession keeps a reference to a connection to a client, and
// maintains per-client state.
class PeerSession : public protocol::HostStub,
                    public protocol::ConnectionToClient::EventHandler {
 public:
  // Callback interface for passing events to the ClientSession.
  class EventHandler {
   public:
    // Called after we've finished connecting all channels.
    virtual void OnSessionChannelsConnected() = 0;

    // Called after connection has failed or after the client closed it.
    virtual void OnSessionClosed(protocol::ErrorCode error,
                                 std::string_view error_details,
                                 const SourceLocation& error_location) = 0;

    // Called on notification of a route change event, when a channel is
    // connected.
    virtual void OnSessionRouteChange(
        const std::string& channel_name,
        const protocol::TransportRoute& route) = 0;

   protected:
    virtual ~EventHandler() = default;
  };

  ~PeerSession() override = default;

  // Starts the session with the specified policies and options.
  virtual void Start(const SessionPolicies& session_policies,
                     const SessionOptions& session_options) = 0;

  // Disconnects the peer session and tears down transport and desktop
  // resources.
  virtual void DisconnectSession(protocol::ErrorCode error,
                                 std::string_view error_details,
                                 const SourceLocation& error_location) = 0;

  // Connects a ChromotingSessionServices client.
  virtual void OnSessionServicesClientConnected(
      mojo::PendingReceiver<mojom::ChromotingSessionServices> receiver) = 0;

  virtual protocol::Transport* transport() const = 0;
};

}  // namespace remoting

#endif  // REMOTING_HOST_PEER_SESSION_H_
