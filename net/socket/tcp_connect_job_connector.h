// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SOCKET_TCP_CONNECT_JOB_CONNECTOR_H_
#define NET_SOCKET_TCP_CONNECT_JOB_CONNECTOR_H_

#include <memory>
#include <optional>
#include <string_view>

#include "base/memory/raw_ref.h"
#include "base/values.h"
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
//
// `name` is a string identifying the connector, and is used for logging. It
// must point at a statically allocated memory, as the Connector will not create
// a copy of its value, but will use it in all logged events.
class TcpConnectJob::Connector {
 public:
  Connector(TcpConnectJob* parent, std::string_view name);

  Connector(const Connector&) = delete;
  Connector& operator=(const Connector&) = delete;

  ~Connector();

  // If `next_state_` is one of the two waiting states, updates `next_state_`
  // and runs DoLoop() to attempt to pull needed data from parent class's host
  // resolution. Returns ERR_IO_PENDING or final net::Error code. If not in a
  // waiting state, always returns ERR_IO_PENDING. Must not be called if already
  // done. On error, may return ERR_NAME_NOT_RESOLVED, if all IPs have been
  // exhausted, rather than the error from the most recent connection attempt.
  //
  // Needs to be called by the parent job whenever the host resolution may have
  // data available the Connector hasn't observed yet.
  int TryAdvanceState();

  LoadState GetLoadState() const;

  // Returns the connected socket. May only be called once the Connector has
  // completed successfully.
  std::unique_ptr<StreamSocket> PassSocket();

  // Returns the ServiceEndpoint that was ultimately used. May only be called
  // once the Connector has completed successfully, and may only be called once,
  // since it moves out the cached value.
  ServiceEndpoint PassFinalServiceEndpoint();

  bool is_waiting_for_endpoint() const {
    return next_state_ == State::kWaitForIPEndPoint;
  }

  // True if the job is waiting on DNS data before it can advance - either
  // waiting for more IPs, or waiting on crypto ready.
  bool is_waiting_on_dns() const {
    return next_state_ == State::kWaitForIPEndPoint ||
           next_state_ == State::kWaitForCryptoReady;
  }

  bool is_done() const { return next_state_ == State::kDone; }

  // Whether `this` is currently connecting to an IPv6 IP, or is connected to
  // one and waiting for DNS to make progress.
  bool is_connecting_to_ipv6() const {
    return current_address_ &&
           current_address_->GetFamily() == ADDRESS_FAMILY_IPV6;
  }

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

  // Updates state_ to kDone and adds TCP_CONNECT_JOB_CONNECTOR_DONE event to
  // the NetLog. Doesn't perform any other action.
  void OnDone(int result);

  // Creates a dictionary for use with NetLog containing `name_`.
  base::DictValue NetLogDict() const;

  const raw_ref<TcpConnectJob> parent_;

  const std::string_view name_;

  std::optional<IPEndPoint> current_address_;

  State next_state_ = State::kWaitForIPEndPoint;

  std::unique_ptr<StreamSocket> transport_socket_;
  std::optional<ServiceEndpoint> final_service_endpoint_;
};

}  // namespace net

#endif  // NET_SOCKET_TCP_CONNECT_JOB_CONNECTOR_H_
