// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GOOGLE_APIS_GCM_ENGINE_CONNECTION_FACTORY_H_
#define GOOGLE_APIS_GCM_ENGINE_CONNECTION_FACTORY_H_

#include <string>

#include "base/callback_forward.h"
#include "base/time/time.h"
#include "google_apis/gcm/base/gcm_export.h"
#include "google_apis/gcm/engine/connection_handler.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "services/network/public/mojom/proxy_resolving_socket.mojom.h"

class GURL;

namespace net {
class IPEndPoint;
}

namespace mcs_proto {
class LoginRequest;
}

namespace gcm {

using GetProxyResolvingFactoryCallback = base::RepeatingCallback<void(
    mojo::PendingReceiver<network::mojom::ProxyResolvingSocketFactory>)>;

// Factory for creating a ConnectionHandler and maintaining its connection.
// The factory retains ownership of the ConnectionHandler and will enforce
// backoff policies when attempting connections.
class GCM_EXPORT ConnectionFactory {
 public:
  using BuildLoginRequestCallback =
      base::RepeatingCallback<void(mcs_proto::LoginRequest* login_request)>;

  // Reasons for triggering a connection reset. Note that these enums are
  // consumed by a histogram, so ordering should not be modified.
  enum ConnectionResetReason {
    LOGIN_FAILURE,           // Login response included an error.
    CLOSE_COMMAND,           // Received a close command.
    HEARTBEAT_FAILURE,       // Heartbeat was not acknowledged in time.
    SOCKET_FAILURE,          // net::Socket error.
    NETWORK_CHANGE,          // NetworkChangeNotifier notified of a network
                             // change.
    NEW_HEARTBEAT_INTERVAL,  // New heartbeat interval was set.
    // Count of total number of connection reset reasons. All new reset reasons
    // should be added above this line.
    CONNECTION_RESET_COUNT,
  };

  // Listener interface to be notified of endpoint connection events.
  class GCM_EXPORT ConnectionListener {
   public:
    ConnectionListener();
    virtual ~ConnectionListener();

    // Notifies the listener that GCM has performed a handshake with and is now
    // actively connected to |current_server|. |ip_endpoint| is the resolved
    // ip address/port through which the connection is being made.
    virtual void OnConnected(const GURL& current_server,
                             const net::IPEndPoint& ip_endpoint) = 0;

    // Notifies the listener that the connection has been interrupted.
    virtual void OnDisconnected() = 0;
  };

  ConnectionFactory();
  virtual ~ConnectionFactory();

  // Initialize the factory, creating a connection handler with a disconnected
  // socket. Should only be called once.
  // Upon connection:
  // |read_callback| will be invoked with the contents of any received protobuf
  // message.
  // |write_callback| will be invoked anytime a message has been successfully
  // sent. Note: this just means the data was sent to the wire, not that the
  // other end received it.
  virtual void Initialize(
      const BuildLoginRequestCallback& request_builder,
      const ConnectionHandler::ProtoReceivedCallback& read_callback,
      const ConnectionHandler::ProtoSentCallback& write_callback) = 0;

  // Get the connection handler for this factory. Initialize(..) must have
  // been called.
  virtual ConnectionHandler* GetConnectionHandler() const = 0;

  // Opens a new connection and initiates login handshake. Upon completion of
  // the handshake, |read_callback| will be invoked with a valid
  // mcs_proto::LoginResponse.
  // Note: Initialize must have already been invoked.
  virtual void Connect() = 0;

  // Whether or not the MCS endpoint is currently reachable with an active
  // connection.
  virtual bool IsEndpointReachable() const = 0;

  // Returns a debug string describing the connection state.
  virtual std::string GetConnectionStateString() const = 0;

  // If in backoff, the time at which the next retry will be made. Otherwise,
  // a null time, indicating either no attempt to connect has been made or no
  // backoff is in progress.
  virtual base::TimeTicks NextRetryAttempt() const = 0;

  // Manually reset the connection. This can occur if an application specific
  // event forced a reset (e.g. server sends a close connection response).
  // If the last connection was made within kConnectionResetWindowSecs, the old
  // backoff is restored, else a new backoff kicks off.
  virtual void SignalConnectionReset(ConnectionResetReason reason) = 0;

  // Sets the current connection listener. Only one listener is supported at a
  // time, and the listener must either outlive the connection factory or
  // call SetConnectionListener(NULL) upon destruction.
  virtual void SetConnectionListener(ConnectionListener* listener) = 0;
};

}  // namespace gcm

#endif  // GOOGLE_APIS_GCM_ENGINE_CONNECTION_FACTORY_H_
