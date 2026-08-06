// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_PEER_SESSION_H_
#define REMOTING_HOST_PEER_SESSION_H_

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "build/build_config.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "remoting/base/source_location.h"
#include "remoting/host/mojom/chromoting_host_services.mojom.h"
#include "remoting/protocol/errors.h"

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX)
#include "remoting/host/mojom/peer_session.mojom.h"
#endif

namespace remoting {

namespace protocol {
class Transport;
struct TransportRoute;
}  // namespace protocol

class DesktopEnvironmentOptions;
class HostExtension;
struct SessionOptions;
struct SessionPolicies;

// A PeerSession keeps a reference to a connection to a client, and
// maintains per-client state.
class PeerSession {
 public:
  // LINT.IfChange(peer_session_event_handler)
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX)
  // On Windows and Linux, `PeerSession::EventHandler` is an alias to the
  // generated Mojo interface `mojom::PeerSessionEventHandler`.
  using EventHandler = mojom::PeerSessionEventHandler;
#else
  // Pure virtual fallback interface for platforms that do not support
  // multi-process peer connections (such as macOS). This definition must
  // exactly match the generated C++ interface of
  // `remoting::mojom::PeerSessionEventHandler`.
  class EventHandler {
   public:
    // Called after we've finished connecting all channels.
    virtual void OnSessionChannelsConnected() = 0;

    // Called after connection has failed or after the client closed it.
    // `error_details` is a human-readable diagnostic string intended for
    // logging/debugging only and must not be parsed programmatically.
    virtual void OnSessionClosed(protocol::ErrorCode error,
                                 const std::string& error_details,
                                 const SourceLocation& error_location) = 0;

    // Called on notification of a route change event, when a channel is
    // connected.
    virtual void OnSessionRouteChange(
        const std::string& channel_name,
        const protocol::TransportRoute& route) = 0;

   protected:
    virtual ~EventHandler() = default;
  };
#endif
  // LINT.ThenChange(//remoting/host/mojom/peer_session.mojom:peer_session_event_handler)

  virtual ~PeerSession() = default;

  // Starts the session with the specified `event_handler`, `client_jid`,
  // `desktop_environment_options`, `extensions`, `session_policies`, and
  // `session_options`.
  // `event_handler` and all elements of `extensions` must outlive this object.
  virtual void Start(
      EventHandler* event_handler,
      std::string_view client_jid,
      const DesktopEnvironmentOptions& desktop_environment_options,
      const std::vector<HostExtension*>& extensions,
      const SessionPolicies& session_policies,
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

// Factory interface for creating `PeerSession` instances.
class PeerSessionFactory {
 public:
  virtual ~PeerSessionFactory() = default;

  // Creates a new `PeerSession` instance.
  virtual std::unique_ptr<PeerSession> Create() = 0;
};

}  // namespace remoting

#endif  // REMOTING_HOST_PEER_SESSION_H_
