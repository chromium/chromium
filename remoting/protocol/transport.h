// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_TRANSPORT_H_
#define REMOTING_PROTOCOL_TRANSPORT_H_

#include <memory>
#include <string>

#include "base/functional/callback_forward.h"
#include "net/base/ip_endpoint.h"
#include "remoting/protocol/errors.h"

namespace jingle_xmpp {
class XmlElement;
}  // namespace jingle_xmpp

namespace remoting::protocol {

class Authenticator;

enum class TransportRole {
  SERVER,
  CLIENT,
};

struct TransportRoute {
  enum RouteType {
    DIRECT,
    STUN,
    RELAY,
    ROUTE_TYPE_MAX = RELAY,
  };

  // Helper method to get string representation of the type.
  static std::string GetTypeString(RouteType type);

  TransportRoute();
  ~TransportRoute();

  RouteType type;
  net::IPEndPoint remote_address;
  net::IPEndPoint local_address;
};

// Transport represents a P2P connection that consists of one or more channels.
// This interface is used just to send and receive transport-info messages.
// Implementations should provide other methods to send and receive data.
class Transport {
 public:
  typedef base::RepeatingCallback<void(
      std::unique_ptr<jingle_xmpp::XmlElement> transport_info)>
      SendTransportInfoCallback;

  virtual ~Transport() {}

  // Sets the object responsible for delivering outgoing transport-info messages
  // to the peer.
  virtual void Start(
      Authenticator* authenticator,
      SendTransportInfoCallback send_transport_info_callback) = 0;
  virtual bool ProcessTransportInfo(
      jingle_xmpp::XmlElement* transport_info) = 0;
};

}  // namespace remoting::protocol

#endif  // REMOTING_PROTOCOL_TRANSPORT_H_
