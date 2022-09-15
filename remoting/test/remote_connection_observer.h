// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_TEST_REMOTE_CONNECTION_OBSERVER_H_
#define REMOTING_TEST_REMOTE_CONNECTION_OBSERVER_H_

#include <string>

#include "remoting/proto/control.pb.h"
#include "remoting/protocol/connection_to_host.h"
#include "remoting/protocol/errors.h"
#include "remoting/protocol/transport.h"

namespace remoting {
namespace test {

// Interface for a remote connection observer which will be notified when
// certain connection status changes occur or events from the remote host
// are received. Observers must not tear-down the object they have registered
// while in a callback. The callbacks should be used for informational
// purposes only.
class RemoteConnectionObserver {
 public:
  RemoteConnectionObserver() {}

  RemoteConnectionObserver(const RemoteConnectionObserver&) = delete;
  RemoteConnectionObserver& operator=(const RemoteConnectionObserver&) = delete;

  virtual ~RemoteConnectionObserver() {}

  // Called when the connection state has changed.
  virtual void ConnectionStateChanged(protocol::ConnectionToHost::State state,
                                      protocol::ErrorCode error_code) {}

  // Called when the connection is ready to be used, |ready| will be true once
  // the video channel has been established.
  virtual void ConnectionReady(bool ready) {}

  // Called when a channel changes the type of route it is using.
  virtual void RouteChanged(const std::string& channel_name,
                            const protocol::TransportRoute& route) {}

  // Called when the host sends its list of capabilities to the client.
  virtual void CapabilitiesSet(const std::string& capabilities) {}

  // Called when a pairing response has been set.
  virtual void PairingResponseSet(
      const protocol::PairingResponse& pairing_response) {}

  // Called when we have received an ExtensionMessage from the host.
  virtual void HostMessageReceived(const protocol::ExtensionMessage& message) {}
};

}  // namespace test
}  // namespace remoting

#endif  // REMOTING_TEST_REMOTE_CONNECTION_OBSERVER_H_
