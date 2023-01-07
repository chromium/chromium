// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_HOST_STATUS_OBSERVER_H_
#define REMOTING_HOST_HOST_STATUS_OBSERVER_H_

#include <string>

#include "remoting/host/mojom/remoting_host.mojom.h"

namespace remoting {

namespace protocol {
struct TransportRoute;
}  // namespace protocol

// Interface for host status observer. All methods are invoked on the
// network thread. Observers must not tear-down ChromotingHost state
// on receipt of these callbacks; they are purely informational.
class HostStatusObserver : public mojom::HostStatusObserver {
 public:
  HostStatusObserver() = default;
  ~HostStatusObserver() override = default;

  // Called when an unauthorized user attempts to connect to the host.
  void OnClientAccessDenied(const std::string& signaling_id) override {}

  // Called when a new client is authenticated.
  void OnClientAuthenticated(const std::string& signaling_id) override {}

  // Called when all channels for an authenticated client are connected.
  void OnClientConnected(const std::string& signaling_id) override {}

  // Called when an authenticated client is disconnected.
  void OnClientDisconnected(const std::string& signaling_id) override {}

  // Called on notification of a route change event, when a channel is
  // connected.
  void OnClientRouteChange(const std::string& signaling_id,
                           const std::string& channel_name,
                           const protocol::TransportRoute& route) override {}

  // Called when the host is started for an account.
  void OnHostStarted(const std::string& owner_email) override {}

  // Called when the host shuts down.
  void OnHostShutdown() override {}
};

}  // namespace remoting

#endif  // REMOTING_HOST_HOST_STATUS_OBSERVER_H_
