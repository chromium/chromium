// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/transport_client_socket_pool.h"

#include <algorithm>
#include <utility>

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_util.h"
#include "base/synchronization/lock.h"
#include "base/time/time.h"
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
#include "net/socket/client_socket_pool_base.h"
#include "net/socket/socket_net_log_params.h"
#include "net/socket/socket_performance_watcher.h"
#include "net/socket/socket_performance_watcher_factory.h"
#include "net/socket/tcp_client_socket.h"

using base::TimeDelta;

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
    bool disable_resolver_cache,
    const OnHostResolutionCallback& host_resolution_callback,
    CombineConnectAndWritePolicy combine_connect_and_write_if_supported)
    : destination_(host_port_pair),
      host_resolution_callback_(host_resolution_callback),
      combine_connect_and_write_(combine_connect_and_write_if_supported) {
  if (disable_resolver_cache)
    destination_.set_allow_cached_response(false);
}

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

TransportConnectJob::TransportConnectJob(
    const std::string& group_name,
    RequestPriority priority,
    const SocketTag& socket_tag,
    ClientSocketPool::RespectLimits respect_limits,
    const scoped_refptr<TransportSocketParams>& params,
    base::TimeDelta timeout_duration,
    ClientSocketFactory* client_socket_factory,
    SocketPerformanceWatcherFactory* socket_performance_watcher_factory,
    HostResolver* host_resolver,
    Delegate* delegate,
    NetLog* net_log)
    : ConnectJob(
          group_name,
          timeout_duration,
          priority,
          socket_tag,
          respect_limits,
          delegate,
          NetLogWithSource::Make(net_log,
                                 NetLogSourceType::TRANSPORT_CONNECT_JOB)),
      params_(params),
      resolver_(host_resolver),
      client_socket_factory_(client_socket_factory),
      next_state_(STATE_NONE),
      socket_performance_watcher_factory_(socket_performance_watcher_factory),
      resolve_result_(OK) {}

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

void TransportConnectJob::GetAdditionalErrorState(ClientSocketHandle* handle) {
  // If hostname resolution failed, record an empty endpoint and the result.
  // Also record any attempts made on either of the sockets.
  ConnectionAttempts attempts;
  if (resolve_result_ != OK) {
    DCHECK_EQ(0u, addresses_.size());
    attempts.push_back(ConnectionAttempt(IPEndPoint(), resolve_result_));
  }
  attempts.insert(attempts.begin(), connection_attempts_.begin(),
                  connection_attempts_.end());
  attempts.insert(attempts.begin(), fallback_connection_attempts_.begin(),
                  fallback_connection_attempts_.end());
  handle->set_connection_attempts(attempts);
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
                             base::TimeDelta::FromMinutes(10),
                             100);

  base::TimeDelta connect_duration = now - connect_timing.connect_start;
  UMA_HISTOGRAM_CUSTOM_TIMES("Net.TCP_Connection_Latency",
                             connect_duration,
                             base::TimeDelta::FromMilliseconds(1),
                             base::TimeDelta::FromMinutes(10),
                             100);

  switch (race_result) {
    case RACE_IPV4_WINS:
      UMA_HISTOGRAM_CUSTOM_TIMES("Net.TCP_Connection_Latency_IPv4_Wins_Race",
                                 connect_duration,
                                 base::TimeDelta::FromMilliseconds(1),
                                 base::TimeDelta::FromMinutes(10),
                                 100);
      break;

    case RACE_IPV4_SOLO:
      UMA_HISTOGRAM_CUSTOM_TIMES("Net.TCP_Connection_Latency_IPv4_No_Race",
                                 connect_duration,
                                 base::TimeDelta::FromMilliseconds(1),
                                 base::TimeDelta::FromMinutes(10),
                                 100);
      break;

    case RACE_IPV6_WINS:
      UMA_HISTOGRAM_CUSTOM_TIMES("Net.TCP_Connection_Latency_IPv6_Raceable",
                                 connect_duration,
                                 base::TimeDelta::FromMilliseconds(1),
                                 base::TimeDelta::FromMinutes(10),
                                 100);
      break;

    case RACE_IPV6_SOLO:
      UMA_HISTOGRAM_CUSTOM_TIMES("Net.TCP_Connection_Latency_IPv6_Solo",
                                 connect_duration,
                                 base::TimeDelta::FromMilliseconds(1),
                                 base::TimeDelta::FromMinutes(10),
                                 100);
      break;

    default:
      NOTREACHED();
      break;
  }
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

  return resolver_->Resolve(
      params_->destination(), priority(), &addresses_,
      base::Bind(&TransportConnectJob::OnIOComplete, base::Unretained(this)),
      &request_, net_log());
}

int TransportConnectJob::DoResolveHostComplete(int result) {
  TRACE_EVENT0(kNetTracingCategory,
               "TransportConnectJob::DoResolveHostComplete");
  connect_timing_.dns_end = base::TimeTicks::Now();
  // Overwrite connection start time, since for connections that do not go
  // through proxies, |connect_start| should not include dns lookup time.
  connect_timing_.connect_start = connect_timing_.dns_end;
  resolve_result_ = result;

  if (result != OK)
    return result;

  // Invoke callback, and abort if it fails.
  if (!params_->host_resolution_callback().is_null()) {
    result = params_->host_resolution_callback().Run(addresses_, net_log());
    if (result != OK)
      return result;
  }

  next_state_ = STATE_TRANSPORT_CONNECT;
  return result;
}

int TransportConnectJob::DoTransportConnect() {
  next_state_ = STATE_TRANSPORT_CONNECT_COMPLETE;
  // Create a |SocketPerformanceWatcher|, and pass the ownership.
  std::unique_ptr<SocketPerformanceWatcher> socket_performance_watcher;
  if (socket_performance_watcher_factory_) {
    socket_performance_watcher =
        socket_performance_watcher_factory_->CreateSocketPerformanceWatcher(
            SocketPerformanceWatcherFactory::PROTOCOL_TCP, addresses_);
  }
  transport_socket_ = client_socket_factory_->CreateTransportClientSocket(
      addresses_, std::move(socket_performance_watcher), net_log().net_log(),
      net_log().source());

  // If the list contains IPv6 and IPv4 addresses, and the first address
  // is IPv6, the IPv4 addresses will be tried as fallback addresses, per
  // "Happy Eyeballs" (RFC 6555).
  bool try_ipv6_connect_with_ipv4_fallback =
      addresses_.front().GetFamily() == ADDRESS_FAMILY_IPV6 &&
      !AddressListOnlyContainsIPv6(addresses_);

  // Enable TCP FastOpen if indicated by transport socket params.
  // Note: We currently do not turn on TCP FastOpen for destinations where
  // we try a TCP connect over IPv6 with fallback to IPv4.
  if (!try_ipv6_connect_with_ipv4_fallback &&
      params_->combine_connect_and_write() ==
          TransportSocketParams::COMBINE_CONNECT_AND_WRITE_DESIRED) {
    transport_socket_->EnableTCPFastOpenIfSupported();
  }

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

    bool is_ipv4 = addresses_.front().GetFamily() == ADDRESS_FAMILY_IPV4;
    RaceResult race_result = RACE_UNKNOWN;
    if (is_ipv4)
      race_result = RACE_IPV4_SOLO;
    else if (AddressListOnlyContainsIPv6(addresses_))
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

  fallback_addresses_.reset(new AddressList(addresses_));
  MakeAddressListStartWithIPv4(fallback_addresses_.get());

  // Create a |SocketPerformanceWatcher|, and pass the ownership.
  std::unique_ptr<SocketPerformanceWatcher> socket_performance_watcher;
  if (socket_performance_watcher_factory_) {
    socket_performance_watcher =
        socket_performance_watcher_factory_->CreateSocketPerformanceWatcher(
            SocketPerformanceWatcherFactory::PROTOCOL_TCP,
            *fallback_addresses_);
  }

  fallback_transport_socket_ =
      client_socket_factory_->CreateTransportClientSocket(
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

void TransportConnectJob::CopyConnectionAttemptsFromSockets() {
  if (transport_socket_)
    transport_socket_->GetConnectionAttempts(&connection_attempts_);
  if (fallback_transport_socket_) {
    fallback_transport_socket_->GetConnectionAttempts(
        &fallback_connection_attempts_);
  }
}

std::unique_ptr<ConnectJob>
TransportClientSocketPool::TransportConnectJobFactory::NewConnectJob(
    const std::string& group_name,
    const PoolBase::Request& request,
    ConnectJob::Delegate* delegate) const {
  return std::unique_ptr<ConnectJob>(new TransportConnectJob(
      group_name, request.priority(), request.socket_tag(),
      request.respect_limits(), request.params(), ConnectionTimeout(),
      client_socket_factory_, socket_performance_watcher_factory_,
      host_resolver_, delegate, net_log_));
}

base::TimeDelta
    TransportClientSocketPool::TransportConnectJobFactory::ConnectionTimeout()
    const {
  return base::TimeDelta::FromSeconds(TransportConnectJob::kTimeoutInSeconds);
}

TransportClientSocketPool::TransportClientSocketPool(
    int max_sockets,
    int max_sockets_per_group,
    HostResolver* host_resolver,
    ClientSocketFactory* client_socket_factory,
    SocketPerformanceWatcherFactory* socket_performance_watcher_factory,
    NetLog* net_log)
    : base_(NULL,
            max_sockets,
            max_sockets_per_group,
            ClientSocketPool::unused_idle_socket_timeout(),
            ClientSocketPool::used_idle_socket_timeout(),
            new TransportConnectJobFactory(client_socket_factory,
                                           host_resolver,
                                           socket_performance_watcher_factory,
                                           net_log)),
      client_socket_factory_(client_socket_factory) {
  base_.EnableConnectBackupJobs();
}

TransportClientSocketPool::~TransportClientSocketPool() = default;

int TransportClientSocketPool::RequestSocket(const std::string& group_name,
                                             const void* params,
                                             RequestPriority priority,
                                             const SocketTag& socket_tag,
                                             RespectLimits respect_limits,
                                             ClientSocketHandle* handle,
                                             CompletionOnceCallback callback,
                                             const NetLogWithSource& net_log) {
  const scoped_refptr<TransportSocketParams>* casted_params =
      static_cast<const scoped_refptr<TransportSocketParams>*>(params);

  NetLogTcpClientSocketPoolRequestedSocket(net_log, casted_params);

  return base_.RequestSocket(group_name, *casted_params, priority, socket_tag,
                             respect_limits, handle, std::move(callback),
                             net_log);
}

void TransportClientSocketPool::NetLogTcpClientSocketPoolRequestedSocket(
    const NetLogWithSource& net_log,
    const scoped_refptr<TransportSocketParams>* casted_params) {
  if (net_log.IsCapturing()) {
    // TODO(eroman): Split out the host and port parameters.
    net_log.AddEvent(
        NetLogEventType::TCP_CLIENT_SOCKET_POOL_REQUESTED_SOCKET,
        CreateNetLogHostPortPairCallback(
            &casted_params->get()->destination().host_port_pair()));
  }
}

void TransportClientSocketPool::RequestSockets(
    const std::string& group_name,
    const void* params,
    int num_sockets,
    const NetLogWithSource& net_log) {
  const scoped_refptr<TransportSocketParams>* casted_params =
      static_cast<const scoped_refptr<TransportSocketParams>*>(params);

  if (net_log.IsCapturing()) {
    // TODO(eroman): Split out the host and port parameters.
    net_log.AddEvent(
        NetLogEventType::TCP_CLIENT_SOCKET_POOL_REQUESTED_SOCKETS,
        CreateNetLogHostPortPairCallback(
            &casted_params->get()->destination().host_port_pair()));
  }

  base_.RequestSockets(group_name, *casted_params, num_sockets, net_log);
}

void TransportClientSocketPool::SetPriority(const std::string& group_name,
                                            ClientSocketHandle* handle,
                                            RequestPriority priority) {
  base_.SetPriority(group_name, handle, priority);
}

void TransportClientSocketPool::CancelRequest(
    const std::string& group_name,
    ClientSocketHandle* handle) {
  base_.CancelRequest(group_name, handle);
}

void TransportClientSocketPool::ReleaseSocket(
    const std::string& group_name,
    std::unique_ptr<StreamSocket> socket,
    int id) {
  base_.ReleaseSocket(group_name, std::move(socket), id);
}

void TransportClientSocketPool::FlushWithError(int error) {
  base_.FlushWithError(error);
}

void TransportClientSocketPool::CloseIdleSockets() {
  base_.CloseIdleSockets();
}

void TransportClientSocketPool::CloseIdleSocketsInGroup(
    const std::string& group_name) {
  base_.CloseIdleSocketsInGroup(group_name);
}

int TransportClientSocketPool::IdleSocketCount() const {
  return base_.idle_socket_count();
}

int TransportClientSocketPool::IdleSocketCountInGroup(
    const std::string& group_name) const {
  return base_.IdleSocketCountInGroup(group_name);
}

LoadState TransportClientSocketPool::GetLoadState(
    const std::string& group_name, const ClientSocketHandle* handle) const {
  return base_.GetLoadState(group_name, handle);
}

std::unique_ptr<base::DictionaryValue>
TransportClientSocketPool::GetInfoAsValue(const std::string& name,
                                          const std::string& type,
                                          bool include_nested_pools) const {
  return base_.GetInfoAsValue(name, type);
}

base::TimeDelta TransportClientSocketPool::ConnectionTimeout() const {
  return base_.ConnectionTimeout();
}

bool TransportClientSocketPool::IsStalled() const {
  return base_.IsStalled();
}

void TransportClientSocketPool::AddHigherLayeredPool(
    HigherLayeredPool* higher_pool) {
  base_.AddHigherLayeredPool(higher_pool);
}

void TransportClientSocketPool::RemoveHigherLayeredPool(
    HigherLayeredPool* higher_pool) {
  base_.RemoveHigherLayeredPool(higher_pool);
}

}  // namespace net
