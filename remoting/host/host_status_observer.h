// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_HOST_STATUS_OBSERVER_H_
#define REMOTING_HOST_HOST_STATUS_OBSERVER_H_

#include <string>

namespace remoting {

namespace protocol {
struct TransportRoute;
}  // namespace protocol

// Interface for host status observer. All methods are invoked on the
// network thread. Observers must not tear-down ChromotingHost state
// on receipt of these callbacks; they are purely informational.
class HostStatusObserver {
 public:
  HostStatusObserver() {}
  virtual ~HostStatusObserver() {}

  // Called when an unauthorized user attempts to connect to the host.
  virtual void OnAccessDenied(const std::string& jid) {}

  // A new client is authenticated.
  virtual void OnClientAuthenticated(const std::string& jid) {}

  // All channels for an authenticated client are connected.
  virtual void OnClientConnected(const std::string& jid) {}

  // An authenticated client is disconnected.
  virtual void OnClientDisconnected(const std::string& jid) {}

  // Called on notification of a route change event, when a channel is
  // connected.
  virtual void OnClientRouteChange(const std::string& jid,
                                   const std::string& channel_name,
                                   const protocol::TransportRoute& route) {}

  // Called when hosting is started for an account.
  virtual void OnStart(const std::string& host_owner_email) {}

  // Called when the host shuts down.
  virtual void OnShutdown() {}
};

}  // namespace remoting

#endif  // REMOTING_HOST_HOST_STATUS_OBSERVER_H_
