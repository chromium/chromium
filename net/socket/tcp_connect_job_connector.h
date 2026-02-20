// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SOCKET_TCP_CONNECT_JOB_CONNECTOR_H_
#define NET_SOCKET_TCP_CONNECT_JOB_CONNECTOR_H_

#include <memory>
#include <optional>

#include "base/memory/raw_ref.h"
#include "net/base/address_list.h"
#include "net/base/ip_endpoint.h"
#include "net/base/load_states.h"
#include "net/base/net_errors.h"
#include "net/socket/tcp_connect_job.h"

namespace net {

class IPEndPoint;
class StreamSocket;

// Attempts to connect to a set of addresses pulled one-by-one from a parent
// TcpConnectJob. The TcpConnectJob is responsible for informing waiting
// Connectors when more DNS information is available. Connectors call
// TcpConnectJob::OnConnectorComplete() once connections to all addresses have
// failed and the DNS resolution is complete, or when a connection succeeds and
// the socket can definitively be returned to the caller. This requires
// Connectors wait for the DNS resolution to be crypto ready before they can
// successfully complete.
//
// Note that there is no start method. Instead, Connectors are started with a
// `next_state` of State::kWaitForIPEndPoint, and then wait to have
// TryResumeIfWaiting() invoked. This allows for a simpler API from the
// standpoint of the parent TcpConnectJob class, with sync and async host
// resolutions needing to call the exact same connector method.
class TcpConnectJob::Connector {
 public:
  explicit Connector(TcpConnectJob* parent);

  Connector(const Connector&) = delete;
  Connector& operator=(const Connector&) = delete;

  ~Connector();

  // Called when there may be ServiceEndpoint data available for the connector
  // to advance. If `next_state_` is one of the two waiting state, updates
  // `next_state_` and runs DoLoop(). Otherwise, returns ERR_IO_PENDING, since
  // busy with something else. Must not be called if already done.
  int OnEndpointDataAvailable();

  LoadState GetLoadState() const;

  // Returns the connected socket. May only be called once the Connector has
  // completed successfully.
  std::unique_ptr<StreamSocket> PassSocket();

  // Returns the current address. May only be called by the parent TcpConnectJob
  // once the connector has successfully completed.
  const IPEndPoint& CurrentAddress() const;

  bool is_done() const { return next_state_ == State::kDone; }

 private:
  // Note that while in either of the "Wait" states, this is waiting on more
  // ServiceEndpoint data. The parent ConnectJob signals that data may be
  // available by calling OnEndpointDataAvailable(). If the data is not one of
  // those two when OnEndpointDataAvailable() is invoked, nothing happens.
  enum class State {
    // Temporary state while processing DoLoop(), before the real next state is
    // determined. Should never be the state at any other point in time.
    kNone,

    // Waiting for TryResumeIfWaiting(). Once invoked, advances
    // State::kWaitForIPEndPoint. Initial state.
    kWaitForIPEndPoint,

    // Tries to pull an IPEndPoint from parent class.
    kObtainIPEndPoint,

    kTcpConnect,
    kTcpConnectComplete,

    // Connection establishment is complete, but need to wait until the
    // ServiceEndpoint request is crypto ready, so can verify the IPEndPoint may
    // be used. Switches to State::kCheckForCryptoReady once
    // TryResumeIfWaiting() is invoked. Only State::kCheckForCryptoReady may
    // enter this state.
    kWaitForCryptoReady,

    kVerifyIPEndPointUsable,

    kDone,
  };

  void OnIOComplete(int result);
  int DoLoop(int result);
  int DoObtainIPEndPoint();
  int DoTcpConnect();
  int DoTcpConnectComplete(int result);
  int DoVerifyIPEndPointUsable();

  // Called when an endpoint failed. Either returns OK and prepares to retry, or
  // sets state to complete. Must not be passed OK or ERR_IO_PENDING.
  int OnEndpointFailed(int error);

  const raw_ref<TcpConnectJob> parent_;

  std::optional<IPEndPoint> current_address_;

  State next_state_ = State::kWaitForIPEndPoint;

  // Error from the last attempted connection, if any. Cached in case we learn
  // late that there are no more IPs to try.
  std::optional<int> last_error_;

  std::unique_ptr<StreamSocket> transport_socket_;
};

}  // namespace net

#endif  // NET_SOCKET_TCP_CONNECT_JOB_CONNECTOR_H_
