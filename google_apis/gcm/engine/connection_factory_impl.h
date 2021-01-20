// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GOOGLE_APIS_GCM_ENGINE_CONNECTION_FACTORY_IMPL_H_
#define GOOGLE_APIS_GCM_ENGINE_CONNECTION_FACTORY_IMPL_H_

#include "google_apis/gcm/engine/connection_factory.h"

#include <stddef.h>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "google_apis/gcm/engine/connection_event_tracker.h"
#include "google_apis/gcm/engine/connection_handler.h"
#include "google_apis/gcm/protocol/mcs.pb.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "net/base/backoff_entry.h"
#include "net/log/net_log_with_source.h"
#include "services/network/public/cpp/network_connection_tracker.h"
#include "services/network/public/mojom/proxy_resolving_socket.mojom.h"
#include "url/gurl.h"

namespace gcm {

class GCMStatsRecorder;

class GCM_EXPORT ConnectionFactoryImpl
    : public ConnectionFactory,
      public network::NetworkConnectionTracker::NetworkConnectionObserver {
 public:
  ConnectionFactoryImpl(
      const std::vector<GURL>& mcs_endpoints,
      const net::BackoffEntry::Policy& backoff_policy,
      GetProxyResolvingFactoryCallback get_socket_factory_callback,
      scoped_refptr<base::SequencedTaskRunner> io_task_runner,
      GCMStatsRecorder* recorder,
      network::NetworkConnectionTracker* network_connection_tracker);
  ~ConnectionFactoryImpl() override;

  // ConnectionFactory implementation.
  void Initialize(
      const BuildLoginRequestCallback& request_builder,
      const ConnectionHandler::ProtoReceivedCallback& read_callback,
      const ConnectionHandler::ProtoSentCallback& write_callback) override;
  ConnectionHandler* GetConnectionHandler() const override;
  void Connect() override;
  bool IsEndpointReachable() const override;
  std::string GetConnectionStateString() const override;
  base::TimeTicks NextRetryAttempt() const override;
  void SignalConnectionReset(ConnectionResetReason reason) override;
  void SetConnectionListener(ConnectionListener* listener) override;

  // NetworkConnectionTracker implementation.
  void OnConnectionChanged(network::mojom::ConnectionType type) override;

  // Returns the server to which the factory is currently connected, or if
  // a connection is currently pending, the server to which the next connection
  // attempt will be made.
  GURL GetCurrentEndpoint() const;

 protected:
  // Initiate the connection to the GCM server.
  // Virtual for testing.
  virtual void StartConnection();

  // Helper method for initalizing the connection hander.
  // Virtual for testing.
  virtual void InitHandler(mojo::ScopedDataPipeConsumerHandle receive_stream,
                           mojo::ScopedDataPipeProducerHandle send_stream);

  // Helper method for creating a backoff entry.
  // Virtual for testing.
  virtual std::unique_ptr<net::BackoffEntry> CreateBackoffEntry(
      const net::BackoffEntry::Policy* const policy);

  // Helper method for creating the connection handler.
  // Virtual for testing.
  virtual std::unique_ptr<ConnectionHandler> CreateConnectionHandler(
      base::TimeDelta read_timeout,
      const ConnectionHandler::ProtoReceivedCallback& read_callback,
      const ConnectionHandler::ProtoSentCallback& write_callback,
      const ConnectionHandler::ConnectionChangedCallback& connection_callback);

  // Returns the current time in Ticks.
  // Virtual for testing.
  virtual base::TimeTicks NowTicks();

  // Callback for Socket connection completion. This is public for testing.
  void OnConnectDone(int result,
                     const base::Optional<net::IPEndPoint>& local_addr,
                     const base::Optional<net::IPEndPoint>& peer_addr,
                     mojo::ScopedDataPipeConsumerHandle receive_stream,
                     mojo::ScopedDataPipeProducerHandle send_stream);

  // ConnectionHandler callback for connection issues.
  void ConnectionHandlerCallback(int result);

 private:
  friend class ConnectionFactoryImplTest;

  ConnectionEventTracker* GetEventTrackerForTesting();

  // Helper method for checking backoff and triggering a connection as
  // necessary.
  void ConnectWithBackoff();

  // Implementation of Connect(..). If not in backoff attempts a connection and
  // handshake. On connection/handshake failure, goes into backoff.
  void ConnectImpl();

  // Closes the local socket if one is present, and resets connection handler.
  void CloseSocket();

  // Updates the GCM Network Session's with current data from HTTP Network
  // Session's, if available.
  // Specifically, HttpAuthCache and IsQuicEnabled are updated.
  void UpdateFromHttpNetworkSession();

  // The tracker will maintain a list of all connection attempts with GCM,
  // whether they succeeded, and their duration.
  ConnectionEventTracker event_tracker_;

  // The MCS endpoints to make connections to, sorted in order of priority.
  const std::vector<GURL> mcs_endpoints_;
  // Index to the endpoint for which a connection should be attempted next.
  size_t next_endpoint_;
  // Index to the endpoint that was last successfully connected.
  size_t last_successful_endpoint_;

  // The backoff policy to use.
  const net::BackoffEntry::Policy backoff_policy_;

  // ---- network:: components for establishing connections. ----
  // Socket factory for creating new GCM connections.
  GetProxyResolvingFactoryCallback get_socket_factory_callback_;
  mojo::Remote<network::mojom::ProxyResolvingSocketFactory> socket_factory_;
  // The handle to the socket for the current connection, if one exists.
  mojo::Remote<network::mojom::ProxyResolvingSocket> socket_;
  // Peer address of |socket_|.
  net::IPEndPoint peer_addr_;

  // Current backoff entry.
  std::unique_ptr<net::BackoffEntry> backoff_entry_;
  // Backoff entry from previous connection attempt. Updated on each login
  // completion.
  std::unique_ptr<net::BackoffEntry> previous_backoff_;

  // Whether a connection attempt is currently actively in progress.
  bool connecting_;

  // Whether the client is waiting for backoff to finish before attempting to
  // connect. Canary jobs are able to preempt connections pending backoff
  // expiration.
  bool waiting_for_backoff_;

  // Whether the NetworkConnectionTracker has informed the client that there is
  // no current connection. No connection attempts will be made until the
  // client is informed of a valid connection type.
  bool waiting_for_network_online_;

  // Whether handshake is in progress after the connection was established. If
  // a connection reset happens while attempting to complete the handshake, the
  // current backoff entry is reused (after incrementing with a new failure).
  bool handshake_in_progress_;

  // The time of the last login completion. Used for calculating whether to
  // restore a previous backoff entry and for measuring uptime.
  base::TimeTicks last_login_time_;

  // Cached callbacks. Set at |Initialize| time, consumed at first |Connect|
  // time.
  ConnectionHandler::ProtoReceivedCallback read_callback_;
  ConnectionHandler::ProtoSentCallback write_callback_;

  // The current connection handler, if one exists.
  std::unique_ptr<ConnectionHandler> connection_handler_;

  // Builder for generating new login requests.
  BuildLoginRequestCallback request_builder_;

  // Task runner.
  const scoped_refptr<base::SequencedTaskRunner> io_task_runner_;

  // Recorder that records GCM activities for debugging purpose. Not owned.
  GCMStatsRecorder* recorder_;

  // Notifies this class of network connection changes.
  // Must outlive the ConnectionFactoryImpl.
  network::NetworkConnectionTracker* network_connection_tracker_;

  // The currently registered listener to notify of connection changes.
  ConnectionListener* listener_;

  base::WeakPtrFactory<ConnectionFactoryImpl> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ConnectionFactoryImpl);
};

}  // namespace gcm

#endif  // GOOGLE_APIS_GCM_ENGINE_CONNECTION_FACTORY_IMPL_H_
