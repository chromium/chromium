// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/transport_connect_job.h"

#include <algorithm>
#include <utility>

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/trace_event.h"
#include "base/values.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/base/trace_constants.h"
#include "net/log/net_log.h"
#include "net/log/net_log_event_type.h"
#include "net/log/net_log_source_type.h"
#include "net/log/net_log_with_source.h"
#include "net/socket/client_socket_factory.h"
#include "net/socket/client_socket_handle.h"
#include "net/socket/socket_performance_watcher.h"
#include "net/socket/socket_performance_watcher_factory.h"
#include "net/socket/tcp_client_socket.h"
#include "net/socket/websocket_transport_connect_job.h"

namespace net {

namespace {

// Returns true iff all addresses in |list| are in the IPv6 family.
bool AddressListOnlyContainsIPv6(const AddressList& list) {
  DCHECK(!list.empty());
  for (auto iter = list.begin(); iter != list.end(); ++iter) {
    if (iter->GetFamily() != ADDRESS_FAMILY_IPV6)
      return false;
  }
  return true;
}

}  // namespace

TransportSocketParams::TransportSocketParams(
    const HostPortPair& host_port_pair,
    const NetworkIsolationKey& network_isolation_key,
    bool disable_secure_dns,
    const OnHostResolutionCallback& host_resolution_callback)
    : destination_(host_port_pair),
      network_isolation_key_(network_isolation_key),
      disable_secure_dns_(disable_secure_dns),
      host_resolution_callback_(host_resolution_callback) {}

TransportSocketParams::~TransportSocketParams() = default;

// TODO(eroman): The use of this constant needs to be re-evaluated. The time
// needed for TCPClientSocketXXX::Connect() can be arbitrarily long, since
// the address list may contain many alternatives, and most of those may
// timeout. Even worse, the per-connect timeout threshold varies greatly
// between systems (anywhere from 20 seconds to 190 seconds).
// See comment #12 at http://crbug.com/23364 for specifics.
const int TransportConnectJob::kTimeoutInSeconds = 240;  // 4 minutes.

// TODO(willchan): Base this off RTT instead of statically setting it. Note we
// choose a timeout that is different from the backup connect job timer so they
// don't synchronize.
const int TransportConnectJob::kIPv6FallbackTimerInMs = 300;

std::unique_ptr<ConnectJob> TransportConnectJob::CreateTransportConnectJob(
    scoped_refptr<TransportSocketParams> transport_client_params,
    RequestPriority priority,
    const SocketTag& socket_tag,
    const CommonConnectJobParams* common_connect_job_params,
    ConnectJob::Delegate* delegate,
    const NetLogWithSource* net_log) {
  if (!common_connect_job_params->websocket_endpoint_lock_manager) {
    return std::make_unique<TransportConnectJob>(
        priority, socket_tag, common_connect_job_params,
        transport_client_params, delegate, net_log);
  }

  return std::make_unique<WebSocketTransportConnectJob>(
      priority, socket_tag, common_connect_job_params, transport_client_params,
      delegate, net_log);
}

TransportConnectJob::TransportConnectJob(
    RequestPriority priority,
    const SocketTag& socket_tag,
    const CommonConnectJobParams* common_connect_job_params,
    const scoped_refptr<TransportSocketParams>& params,
    Delegate* delegate,
    const NetLogWithSource* net_log)
    : ConnectJob(priority,
                 socket_tag,
                 ConnectionTimeout(),
                 common_connect_job_params,
                 delegate,
                 net_log,
                 NetLogSourceType::TRANSPORT_CONNECT_JOB,
                 NetLogEventType::TRANSPORT_CONNECT_JOB_CONNECT),
      params_(params),
      next_state_(STATE_NONE),
      resolve_result_(OK) {
  // This is only set for WebSockets.
  DCHECK(!common_connect_job_params->websocket_endpoint_lock_manager);
}

TransportConnectJob::~TransportConnectJob() {
  // We don't worry about cancelling the host resolution and TCP connect, since
  // ~HostResolver::Request and ~StreamSocket will take care of it.
}

LoadState TransportConnectJob::GetLoadState() const {
  switch (next_state_) {
    case STATE_RESOLVE_HOST:
    case STATE_RESOLVE_HOST_COMPLETE:
      return LOAD_STATE_RESOLVING_HOST;
    case STATE_TRANSPORT_CONNECT:
    case STATE_TRANSPORT_CONNECT_COMPLETE:
      return LOAD_STATE_CONNECTING;
    case STATE_NONE:
      return LOAD_STATE_IDLE;
  }
  NOTREACHED();
  return LOAD_STATE_IDLE;
}

bool TransportConnectJob::HasEstablishedConnection() const {
  // No need to ever return true, since NotifyComplete() is called as soon as a
  // connection is established.
  return false;
}

ConnectionAttempts TransportConnectJob::GetConnectionAttempts() const {
  // If hostname resolution failed, record an empty endpoint and the result.
  // Also record any attempts made on either of the sockets.
  ConnectionAttempts attempts;
  if (resolve_result_ != OK) {
    DCHECK(!request_->GetAddressResults());
    attempts.push_back(ConnectionAttempt(IPEndPoint(), resolve_result_));
  }
  attempts.insert(attempts.begin(), connection_attempts_.begin(),
                  connection_attempts_.end());
  attempts.insert(attempts.begin(), fallback_connection_attempts_.begin(),
                  fallback_connection_attempts_.end());
  return attempts;
}

// static
void TransportConnectJob::MakeAddressListStartWithIPv4(AddressList* list) {
  for (auto i = list->begin(); i != list->end(); ++i) {
    if (i->GetFamily() == ADDRESS_FAMILY_IPV4) {
      std::rotate(list->begin(), i, list->end());
      break;
    }
  }
}

// static
void TransportConnectJob::HistogramDuration(
    const LoadTimingInfo::ConnectTiming& connect_timing,
    RaceResult race_result) {
  DCHECK(!connect_timing.connect_start.is_null());
  DCHECK(!connect_timing.dns_start.is_null());
  base::TimeTicks now = base::TimeTicks::Now();
  base::TimeDelta total_duration = now - connect_timing.dns_start;
  UMA_HISTOGRAM_CUSTOM_TIMES("Net.DNS_Resolution_And_TCP_Connection_Latency2",
                             total_duration,
                             base::TimeDelta::FromMilliseconds(1),
                             base::TimeDelta::FromMinutes(10), 100);

  base::TimeDelta connect_duration = now - connect_timing.connect_start;
  UMA_HISTOGRAM_CUSTOM_TIMES("Net.TCP_Connection_Latency", connect_duration,
                             base::TimeDelta::FromMilliseconds(1),
                             base::TimeDelta::FromMinutes(10), 100);

  switch (race_result) {
    case RACE_IPV4_WINS:
      UMA_HISTOGRAM_CUSTOM_TIMES("Net.TCP_Connection_Latency_IPv4_Wins_Race",
                                 connect_duration,
                                 base::TimeDelta::FromMilliseconds(1),
                                 base::TimeDelta::FromMinutes(10), 100);
      break;

    case RACE_IPV4_SOLO:
      UMA_HISTOGRAM_CUSTOM_TIMES("Net.TCP_Connection_Latency_IPv4_No_Race",
                                 connect_duration,
                                 base::TimeDelta::FromMilliseconds(1),
                                 base::TimeDelta::FromMinutes(10), 100);
      break;

    case RACE_IPV6_WINS:
      UMA_HISTOGRAM_CUSTOM_TIMES("Net.TCP_Connection_Latency_IPv6_Raceable",
                                 connect_duration,
                                 base::TimeDelta::FromMilliseconds(1),
                                 base::TimeDelta::FromMinutes(10), 100);
      break;

    case RACE_IPV6_SOLO:
      UMA_HISTOGRAM_CUSTOM_TIMES("Net.TCP_Connection_Latency_IPv6_Solo",
                                 connect_duration,
                                 base::TimeDelta::FromMilliseconds(1),
                                 base::TimeDelta::FromMinutes(10), 100);
      break;

    default:
      NOTREACHED();
      break;
  }
}

// static
base::TimeDelta TransportConnectJob::ConnectionTimeout() {
  return base::TimeDelta::FromSeconds(TransportConnectJob::kTimeoutInSeconds);
}

void TransportConnectJob::OnIOComplete(int result) {
  result = DoLoop(result);
  if (result != ERR_IO_PENDING)
    NotifyDelegateOfCompletion(result);  // Deletes |this|
}

int TransportConnectJob::DoLoop(int result) {
  DCHECK_NE(next_state_, STATE_NONE);

  int rv = result;
  do {
    State state = next_state_;
    next_state_ = STATE_NONE;
    switch (state) {
      case STATE_RESOLVE_HOST:
        DCHECK_EQ(OK, rv);
        rv = DoResolveHost();
        break;
      case STATE_RESOLVE_HOST_COMPLETE:
        rv = DoResolveHostComplete(rv);
        break;
      case STATE_TRANSPORT_CONNECT:
        DCHECK_EQ(OK, rv);
        rv = DoTransportConnect();
        break;
      case STATE_TRANSPORT_CONNECT_COMPLETE:
        rv = DoTransportConnectComplete(rv);
        break;
      default:
        NOTREACHED();
        rv = ERR_FAILED;
        break;
    }
  } while (rv != ERR_IO_PENDING && next_state_ != STATE_NONE);

  return rv;
}

int TransportConnectJob::DoResolveHost() {
  next_state_ = STATE_RESOLVE_HOST_COMPLETE;
  connect_timing_.dns_start = base::TimeTicks::Now();

  HostResolver::ResolveHostParameters parameters;
  parameters.initial_priority = priority();
  if (params_->disable_secure_dns())
    parameters.secure_dns_mode_override = DnsConfig::SecureDnsMode::OFF;
  request_ = host_resolver()->CreateRequest(params_->destination(),
                                            params_->network_isolation_key(),
                                            net_log(), parameters);

  return request_->Start(base::BindOnce(&TransportConnectJob::OnIOComplete,
                                        base::Unretained(this)));
}

int TransportConnectJob::DoResolveHostComplete(int result) {
  TRACE_EVENT0(NetTracingCategory(),
               "TransportConnectJob::DoResolveHostComplete");
  connect_timing_.dns_end = base::TimeTicks::Now();
  // Overwrite connection start time, since for connections that do not go
  // through proxies, |connect_start| should not include dns lookup time.
  connect_timing_.connect_start = connect_timing_.dns_end;
  resolve_result_ = result;

  if (result != OK)
    return result;
  DCHECK(request_->GetAddressResults());

  next_state_ = STATE_TRANSPORT_CONNECT;

  // Invoke callback.  If it indicates |this| may be slated for deletion, then
  // only continue after a PostTask.
  if (!params_->host_resolution_callback().is_null()) {
    OnHostResolutionCallbackResult callback_result =
        params_->host_resolution_callback().Run(
            params_->destination(), request_->GetAddressResults().value());
    if (callback_result == OnHostResolutionCallbackResult::kMayBeDeletedAsync) {
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE, base::BindOnce(&TransportConnectJob::OnIOComplete,
                                    weak_ptr_factory_.GetWeakPtr(), OK));
      return ERR_IO_PENDING;
    }
  }

  return result;
}

int TransportConnectJob::DoTransportConnect() {
  next_state_ = STATE_TRANSPORT_CONNECT_COMPLETE;
  // Create a |SocketPerformanceWatcher|, and pass the ownership.
  std::unique_ptr<SocketPerformanceWatcher> socket_performance_watcher;
  if (socket_performance_watcher_factory()) {
    socket_performance_watcher =
        socket_performance_watcher_factory()->CreateSocketPerformanceWatcher(
            SocketPerformanceWatcherFactory::PROTOCOL_TCP,
            request_->GetAddressResults().value());
  }
  transport_socket_ = client_socket_factory()->CreateTransportClientSocket(
      request_->GetAddressResults().value(),
      std::move(socket_performance_watcher), net_log().net_log(),
      net_log().source());

  // If the list contains IPv6 and IPv4 addresses, and the first address
  // is IPv6, the IPv4 addresses will be tried as fallback addresses, per
  // "Happy Eyeballs" (RFC 6555).
  bool try_ipv6_connect_with_ipv4_fallback =
      request_->GetAddressResults().value().front().GetFamily() ==
          ADDRESS_FAMILY_IPV6 &&
      !AddressListOnlyContainsIPv6(request_->GetAddressResults().value());

  transport_socket_->ApplySocketTag(socket_tag());

  int rv = transport_socket_->Connect(base::BindOnce(
      &TransportConnectJob::OnIOComplete, base::Unretained(this)));
  if (rv == ERR_IO_PENDING && try_ipv6_connect_with_ipv4_fallback) {
    fallback_timer_.Start(
        FROM_HERE, base::TimeDelta::FromMilliseconds(kIPv6FallbackTimerInMs),
        this, &TransportConnectJob::DoIPv6FallbackTransportConnect);
  }
  return rv;
}

int TransportConnectJob::DoTransportConnectComplete(int result) {
  if (result == OK) {
    // Success will be returned via the main socket, so also include connection
    // attempts made on the fallback socket up to this point. (Unfortunately,
    // the only simple way to return information in the success case is through
    // the successfully-connected socket.)
    if (fallback_transport_socket_) {
      ConnectionAttempts fallback_attempts;
      fallback_transport_socket_->GetConnectionAttempts(&fallback_attempts);
      transport_socket_->AddConnectionAttempts(fallback_attempts);
    }

    bool is_ipv4 = request_->GetAddressResults().value().front().GetFamily() ==
                   ADDRESS_FAMILY_IPV4;
    RaceResult race_result = RACE_UNKNOWN;
    if (is_ipv4)
      race_result = RACE_IPV4_SOLO;
    else if (AddressListOnlyContainsIPv6(request_->GetAddressResults().value()))
      race_result = RACE_IPV6_SOLO;
    else
      race_result = RACE_IPV6_WINS;
    HistogramDuration(connect_timing_, race_result);

    SetSocket(std::move(transport_socket_));
  } else {
    // Failure will be returned via |GetAdditionalErrorState|, so save
    // connection attempts from both sockets for use there.
    CopyConnectionAttemptsFromSockets();

    transport_socket_.reset();
  }

  fallback_timer_.Stop();
  fallback_transport_socket_.reset();
  fallback_addresses_.reset();

  return result;
}

void TransportConnectJob::DoIPv6FallbackTransportConnect() {
  // The timer should only fire while we're waiting for the main connect to
  // succeed.
  if (next_state_ != STATE_TRANSPORT_CONNECT_COMPLETE) {
    NOTREACHED();
    return;
  }

  DCHECK(!fallback_transport_socket_.get());
  DCHECK(!fallback_addresses_.get());

  fallback_addresses_.reset(
      new AddressList(request_->GetAddressResults().value()));
  MakeAddressListStartWithIPv4(fallback_addresses_.get());

  // Create a |SocketPerformanceWatcher|, and pass the ownership.
  std::unique_ptr<SocketPerformanceWatcher> socket_performance_watcher;
  if (socket_performance_watcher_factory()) {
    socket_performance_watcher =
        socket_performance_watcher_factory()->CreateSocketPerformanceWatcher(
            SocketPerformanceWatcherFactory::PROTOCOL_TCP,
            *fallback_addresses_);
  }

  fallback_transport_socket_ =
      client_socket_factory()->CreateTransportClientSocket(
          *fallback_addresses_, std::move(socket_performance_watcher),
          net_log().net_log(), net_log().source());
  fallback_connect_start_time_ = base::TimeTicks::Now();
  int rv = fallback_transport_socket_->Connect(base::BindOnce(
      &TransportConnectJob::DoIPv6FallbackTransportConnectComplete,
      base::Unretained(this)));
  if (rv != ERR_IO_PENDING)
    DoIPv6FallbackTransportConnectComplete(rv);
}

void TransportConnectJob::DoIPv6FallbackTransportConnectComplete(int result) {
  // This should only happen when we're waiting for the main connect to succeed.
  if (next_state_ != STATE_TRANSPORT_CONNECT_COMPLETE) {
    NOTREACHED();
    return;
  }

  DCHECK_NE(ERR_IO_PENDING, result);
  DCHECK(fallback_transport_socket_.get());
  DCHECK(fallback_addresses_.get());

  if (result == OK) {
    DCHECK(!fallback_connect_start_time_.is_null());

    // Success will be returned via the fallback socket, so also include
    // connection attempts made on the main socket up to this point.
    // (Unfortunately, the only simple way to return information in the success
    // case is through the successfully-connected socket.)
    if (transport_socket_) {
      ConnectionAttempts attempts;
      transport_socket_->GetConnectionAttempts(&attempts);
      fallback_transport_socket_->AddConnectionAttempts(attempts);
    }

    connect_timing_.connect_start = fallback_connect_start_time_;
    HistogramDuration(connect_timing_, RACE_IPV4_WINS);
    SetSocket(std::move(fallback_transport_socket_));
    next_state_ = STATE_NONE;
  } else {
    // Failure will be returned via |GetAdditionalErrorState|, so save
    // connection attempts from both sockets for use there.
    CopyConnectionAttemptsFromSockets();

    fallback_transport_socket_.reset();
    fallback_addresses_.reset();
  }

  transport_socket_.reset();

  NotifyDelegateOfCompletion(result);  // Deletes |this|
}

int TransportConnectJob::ConnectInternal() {
  next_state_ = STATE_RESOLVE_HOST;
  return DoLoop(OK);
}

void TransportConnectJob::ChangePriorityInternal(RequestPriority priority) {
  if (next_state_ == STATE_RESOLVE_HOST_COMPLETE) {
    DCHECK(request_);
    // Change the request priority in the host resolver.
    request_->ChangeRequestPriority(priority);
  }
}

void TransportConnectJob::CopyConnectionAttemptsFromSockets() {
  if (transport_socket_)
    transport_socket_->GetConnectionAttempts(&connection_attempts_);
  if (fallback_transport_socket_) {
    fallback_transport_socket_->GetConnectionAttempts(
        &fallback_connection_attempts_);
  }
}

}  // namespace net
