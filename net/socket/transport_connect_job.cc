// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/transport_connect_job.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/check_op.h"
#include "base/compiler_specific.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/trace_event.h"
#include "base/values.h"
#include "net/base/features.h"
#include "net/base/host_port_pair.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/base/trace_constants.h"
#include "net/dns/host_resolver_results.h"
#include "net/dns/public/secure_dns_policy.h"
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
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "url/scheme_host_port.h"
#include "url/url_constants.h"

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

// TODO(crbug.com/1206799): Delete once endpoint usage is converted to using
// url::SchemeHostPort when available.
HostPortPair ToLegacyDestinationEndpoint(
    const TransportSocketParams::Endpoint& endpoint) {
  if (absl::holds_alternative<url::SchemeHostPort>(endpoint)) {
    return HostPortPair::FromSchemeHostPort(
        absl::get<url::SchemeHostPort>(endpoint));
  }

  DCHECK(absl::holds_alternative<HostPortPair>(endpoint));
  return absl::get<HostPortPair>(endpoint);
}

}  // namespace

TransportSocketParams::TransportSocketParams(
    Endpoint destination,
    NetworkIsolationKey network_isolation_key,
    SecureDnsPolicy secure_dns_policy,
    OnHostResolutionCallback host_resolution_callback,
    base::flat_set<std::string> supported_alpns)
    : destination_(std::move(destination)),
      network_isolation_key_(std::move(network_isolation_key)),
      secure_dns_policy_(secure_dns_policy),
      host_resolution_callback_(std::move(host_resolution_callback)),
      supported_alpns_(std::move(supported_alpns)) {
#if DCHECK_IS_ON()
  auto* scheme_host_port = absl::get_if<url::SchemeHostPort>(&destination_);
  if (scheme_host_port) {
    if (scheme_host_port->scheme() == url::kHttpsScheme) {
      // HTTPS destinations will, when passed to the DNS resolver, return
      // SVCB/HTTPS-based routes. Those routes require ALPN protocols to
      // evaluate. If there are none, `IsEndpointResultUsable` will correctly
      // skip each route, but it doesn't make sense to make a DNS query if we
      // can't handle the result.
      DCHECK(!supported_alpns_.empty());
    } else if (scheme_host_port->scheme() == url::kHttpScheme) {
      // HTTP (not HTTPS) does not currently define ALPN protocols, so the list
      // should be empty. This means `IsEndpointResultUsable` will skip any
      // SVCB-based routes. HTTP also has no SVCB mapping, so `HostResolver`
      // will never return them anyway.
      //
      // `HostResolver` will still query SVCB (rather, HTTPS) records for the
      // corresponding HTTPS URL to implement an upgrade flow (section 9.5 of
      // draft-ietf-dnsop-svcb-https-08), but this will result in DNS resolution
      // failing with `ERR_DNS_NAME_HTTPS_ONLY`, not SVCB-based routes.
      DCHECK(supported_alpns_.empty());
    }
  }
#endif
}

TransportSocketParams::~TransportSocketParams() = default;

std::unique_ptr<TransportConnectJob> TransportConnectJob::Factory::Create(
    RequestPriority priority,
    const SocketTag& socket_tag,
    const CommonConnectJobParams* common_connect_job_params,
    const scoped_refptr<TransportSocketParams>& params,
    Delegate* delegate,
    const NetLogWithSource* net_log) {
  return std::make_unique<TransportConnectJob>(priority, socket_tag,
                                               common_connect_job_params,
                                               params, delegate, net_log);
}

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

TransportConnectJob::EndpointResultOverride::EndpointResultOverride(
    HostResolverEndpointResult result,
    std::set<std::string> dns_aliases)
    : result(std::move(result)), dns_aliases(std::move(dns_aliases)) {}
TransportConnectJob::EndpointResultOverride::EndpointResultOverride(
    EndpointResultOverride&&) = default;
TransportConnectJob::EndpointResultOverride::EndpointResultOverride(
    const EndpointResultOverride&) = default;
TransportConnectJob::EndpointResultOverride::~EndpointResultOverride() =
    default;
TransportConnectJob::EndpointResultOverride&
TransportConnectJob::EndpointResultOverride::operator=(
    EndpointResultOverride&&) = default;
TransportConnectJob::EndpointResultOverride&
TransportConnectJob::EndpointResultOverride::operator=(
    const EndpointResultOverride&) = default;

TransportConnectJob::TransportConnectJob(
    RequestPriority priority,
    const SocketTag& socket_tag,
    const CommonConnectJobParams* common_connect_job_params,
    const scoped_refptr<TransportSocketParams>& params,
    Delegate* delegate,
    const NetLogWithSource* net_log,
    absl::optional<EndpointResultOverride> endpoint_result_override)
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

  if (endpoint_result_override) {
    has_dns_override_ = true;
    endpoint_results_ = {std::move(endpoint_result_override->result)};
    dns_aliases_ = std::move(endpoint_result_override->dns_aliases);
    DCHECK(!endpoint_results_.front().ip_endpoints.empty());
    DCHECK(IsEndpointResultUsable(endpoint_results_.front(),
                                  IsSvcbOptional(endpoint_results_)));
  }
}

// We don't worry about cancelling the host resolution and TCP connect, since
// ~HostResolver::Request and ~StreamSocket will take care of it.
TransportConnectJob::~TransportConnectJob() = default;

LoadState TransportConnectJob::GetLoadState() const {
  switch (next_state_) {
    case STATE_RESOLVE_HOST:
    case STATE_RESOLVE_HOST_COMPLETE:
      return LOAD_STATE_RESOLVING_HOST;
    case STATE_RESOLVE_HOST_CALLBACK_COMPLETE:
    case STATE_TRANSPORT_CONNECT:
    case STATE_TRANSPORT_CONNECT_COMPLETE:
    case STATE_FALLBACK_CONNECT_COMPLETE:
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
  // Also record any attempts made on any failed sockets.
  ConnectionAttempts attempts = connection_attempts_;
  if (resolve_result_ != OK) {
    DCHECK(endpoint_results_.empty());
    attempts.push_back(ConnectionAttempt(IPEndPoint(), resolve_result_));
  }
  return attempts;
}

ResolveErrorInfo TransportConnectJob::GetResolveErrorInfo() const {
  return resolve_error_info_;
}

absl::optional<HostResolverEndpointResult>
TransportConnectJob::GetHostResolverEndpointResult() const {
  CHECK_LT(current_endpoint_result_, endpoint_results_.size());
  return endpoint_results_[current_endpoint_result_];
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
    const LoadTimingInfo::ConnectTiming& connect_timing) {
  DCHECK(!connect_timing.connect_start.is_null());
  DCHECK(!connect_timing.dns_start.is_null());
  base::TimeTicks now = base::TimeTicks::Now();
  base::TimeDelta total_duration = now - connect_timing.dns_start;
  UMA_HISTOGRAM_CUSTOM_TIMES("Net.DNS_Resolution_And_TCP_Connection_Latency2",
                             total_duration, base::Milliseconds(1),
                             base::Minutes(10), 100);

  base::TimeDelta connect_duration = now - connect_timing.connect_start;
  UMA_HISTOGRAM_CUSTOM_TIMES("Net.TCP_Connection_Latency", connect_duration,
                             base::Milliseconds(1), base::Minutes(10), 100);
}

// static
base::TimeDelta TransportConnectJob::ConnectionTimeout() {
  return base::Seconds(TransportConnectJob::kTimeoutInSeconds);
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
      case STATE_RESOLVE_HOST_CALLBACK_COMPLETE:
        DCHECK_EQ(OK, rv);
        rv = DoResolveHostCallbackComplete();
        break;
      case STATE_TRANSPORT_CONNECT:
        DCHECK_EQ(OK, rv);
        rv = DoTransportConnect();
        break;
      case STATE_TRANSPORT_CONNECT_COMPLETE:
        rv = DoTransportConnectComplete(/*is_fallback=*/false, rv);
        break;
      case STATE_FALLBACK_CONNECT_COMPLETE:
        rv = DoTransportConnectComplete(/*is_fallback=*/true, rv);
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
  connect_timing_.dns_start = base::TimeTicks::Now();

  if (has_dns_override_) {
    DCHECK_EQ(1u, endpoint_results_.size());
    connect_timing_.dns_end = connect_timing_.dns_start;
    next_state_ = STATE_TRANSPORT_CONNECT;
    return OK;
  }

  next_state_ = STATE_RESOLVE_HOST_COMPLETE;

  HostResolver::ResolveHostParameters parameters;
  parameters.initial_priority = priority();
  parameters.secure_dns_policy = params_->secure_dns_policy();
  if (absl::holds_alternative<url::SchemeHostPort>(params_->destination())) {
    request_ = host_resolver()->CreateRequest(
        absl::get<url::SchemeHostPort>(params_->destination()),
        params_->network_isolation_key(), net_log(), parameters);
  } else {
    request_ = host_resolver()->CreateRequest(
        absl::get<HostPortPair>(params_->destination()),
        params_->network_isolation_key(), net_log(), parameters);
  }

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
  resolve_error_info_ = request_->GetResolveErrorInfo();

  if (result != OK)
    return result;
  DCHECK(request_->GetAddressResults());
  DCHECK(request_->GetDnsAliasResults());
  DCHECK(request_->GetEndpointResults());

  // Invoke callback.  If it indicates |this| may be slated for deletion, then
  // only continue after a PostTask.
  next_state_ = STATE_RESOLVE_HOST_CALLBACK_COMPLETE;
  if (!params_->host_resolution_callback().is_null()) {
    // TODO(https://crbug.com/1287240): Switch `OnHostResolutionCallbackResult`
    // to `request_->GetEndpointResults()` and `request_->GetDnsAliasResults()`.
    OnHostResolutionCallbackResult callback_result =
        params_->host_resolution_callback().Run(
            ToLegacyDestinationEndpoint(params_->destination()),
            *request_->GetAddressResults());
    if (callback_result == OnHostResolutionCallbackResult::kMayBeDeletedAsync) {
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE, base::BindOnce(&TransportConnectJob::OnIOComplete,
                                    weak_ptr_factory_.GetWeakPtr(), OK));
      return ERR_IO_PENDING;
    }
  }

  return result;
}

int TransportConnectJob::DoResolveHostCallbackComplete() {
  const auto& unfiltered_results = *request_->GetEndpointResults();
  bool svcb_optional = IsSvcbOptional(unfiltered_results);
  for (const auto& result : unfiltered_results) {
    if (IsEndpointResultUsable(result, svcb_optional)) {
      endpoint_results_.push_back(result);
    }
  }
  dns_aliases_ = *request_->GetDnsAliasResults();

  // No need to retain `request_` beyond this point.
  request_.reset();

  if (endpoint_results_.empty()) {
    // In the general case, DNS may successfully return routes, but none are
    // compatible with this `ConnectJob`. This should not happen for HTTPS
    // because `HostResolver` will reject SVCB/HTTPS sets that do not cover the
    // default "http/1.1" ALPN.
    return ERR_NAME_NOT_RESOLVED;
  }

  next_state_ = STATE_TRANSPORT_CONNECT;
  return OK;
}

int TransportConnectJob::DoTransportConnect() {
  next_state_ = STATE_TRANSPORT_CONNECT_COMPLETE;
  AddressList addresses = GetCurrentAddressList();

  // Create a |SocketPerformanceWatcher|, and pass the ownership.
  std::unique_ptr<SocketPerformanceWatcher> socket_performance_watcher;
  if (socket_performance_watcher_factory()) {
    socket_performance_watcher =
        socket_performance_watcher_factory()->CreateSocketPerformanceWatcher(
            SocketPerformanceWatcherFactory::PROTOCOL_TCP, addresses);
  }
  transport_socket_ = client_socket_factory()->CreateTransportClientSocket(
      addresses, std::move(socket_performance_watcher),
      network_quality_estimator(), net_log().net_log(), net_log().source());

  // If the list contains IPv6 and IPv4 addresses, and the first address
  // is IPv6, the IPv4 addresses will be tried as fallback addresses, per
  // "Happy Eyeballs" (RFC 6555).
  bool try_ipv6_connect_with_ipv4_fallback =
      addresses.front().GetFamily() == ADDRESS_FAMILY_IPV6 &&
      !AddressListOnlyContainsIPv6(addresses);

  transport_socket_->ApplySocketTag(socket_tag());

  int rv = transport_socket_->Connect(base::BindOnce(
      &TransportConnectJob::OnIOComplete, base::Unretained(this)));
  if (rv == ERR_IO_PENDING && try_ipv6_connect_with_ipv4_fallback) {
    fallback_timer_.Start(FROM_HERE, base::Milliseconds(kIPv6FallbackTimerInMs),
                          this,
                          &TransportConnectJob::OnIPv6FallbackTimerComplete);
  }
  return rv;
}

int TransportConnectJob::DoTransportConnectComplete(bool is_fallback,
                                                    int result) {
  // Either the main socket or the fallback one completed.
  std::unique_ptr<StreamSocket>& completed_socket =
      is_fallback ? fallback_transport_socket_ : transport_socket_;
  std::unique_ptr<StreamSocket>& other_socket =
      is_fallback ? transport_socket_ : fallback_transport_socket_;
  DCHECK(completed_socket);
  if (other_socket) {
    // Save the connection attempts from the other socket. (Unfortunately, the
    // only simple way to return information in the success case is through the
    // successfully-connected socket.)
    SaveConnectionAttempts(*other_socket);
  }
  if (is_fallback) {
    connect_timing_.connect_start = fallback_connect_start_time_;
  }

  // Cancel any completion events from the callback timer and other socket.
  fallback_timer_.Stop();
  other_socket.reset();

  if (result == OK) {
    HistogramDuration(connect_timing_);

    // Add connection attempts from previous routes.
    completed_socket->AddConnectionAttempts(connection_attempts_);
    SetSocket(std::move(completed_socket), dns_aliases_);
  } else {
    // Failure will be returned via |GetAdditionalErrorState|, so save
    // connection attempts from the socket for use there.
    SaveConnectionAttempts(*completed_socket);
    completed_socket.reset();

    // If there is another endpoint available, try it.
    current_endpoint_result_++;
    if (current_endpoint_result_ < endpoint_results_.size()) {
      next_state_ = STATE_TRANSPORT_CONNECT;
      result = OK;
    }
  }

  return result;
}

void TransportConnectJob::OnIPv6FallbackTimerComplete() {
  // The timer should only fire while we're waiting for the main connect to
  // succeed.
  if (next_state_ != STATE_TRANSPORT_CONNECT_COMPLETE) {
    NOTREACHED();
    return;
  }

  DCHECK(!fallback_transport_socket_.get());

  AddressList fallback_addresses = GetCurrentAddressList();
  MakeAddressListStartWithIPv4(&fallback_addresses);

  // Create a |SocketPerformanceWatcher|, and pass the ownership.
  std::unique_ptr<SocketPerformanceWatcher> socket_performance_watcher;
  if (socket_performance_watcher_factory()) {
    socket_performance_watcher =
        socket_performance_watcher_factory()->CreateSocketPerformanceWatcher(
            SocketPerformanceWatcherFactory::PROTOCOL_TCP, fallback_addresses);
  }

  fallback_transport_socket_ =
      client_socket_factory()->CreateTransportClientSocket(
          fallback_addresses, std::move(socket_performance_watcher),
          network_quality_estimator(), net_log().net_log(), net_log().source());
  fallback_connect_start_time_ = base::TimeTicks::Now();
  fallback_transport_socket_->ApplySocketTag(socket_tag());
  int rv = fallback_transport_socket_->Connect(base::BindOnce(
      base::BindOnce(&TransportConnectJob::OnIPv6FallbackConnectComplete,
                     base::Unretained(this))));
  if (rv != ERR_IO_PENDING)
    OnIPv6FallbackConnectComplete(rv);
}

void TransportConnectJob::OnIPv6FallbackConnectComplete(int result) {
  // This should only happen when we're waiting for the main connect to succeed.
  if (next_state_ != STATE_TRANSPORT_CONNECT_COMPLETE) {
    NOTREACHED();
    return;
  }

  next_state_ = STATE_FALLBACK_CONNECT_COMPLETE;
  OnIOComplete(result);
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

bool TransportConnectJob::IsSvcbOptional(
    base::span<const HostResolverEndpointResult> results) const {
  // If SVCB/HTTPS resolution succeeded, the client supports ECH, and all routes
  // support ECH, disable the A/AAAA fallback. See Section 10.1 of
  // draft-ietf-dnsop-svcb-https-08.
  auto* scheme_host_port =
      absl::get_if<url::SchemeHostPort>(&params_->destination());
  if (!base::FeatureList::IsEnabled(features::kEncryptedClientHello) ||
      !scheme_host_port || scheme_host_port->scheme() != url::kHttpsScheme) {
    return true;  // ECH is not supported for this request.
  }

  bool has_svcb = false;
  for (const auto& result : results) {
    if (!result.metadata.supported_protocol_alpns.empty()) {
      has_svcb = true;
      if (result.metadata.ech_config_list.empty()) {
        return true;  // There is a non-ECH SVCB/HTTPS route.
      }
    }
  }
  // Either there were no SVCB/HTTPS records (should be SVCB-optional), or there
  // were and all supported ECH (should be SVCB-reliant).
  return !has_svcb;
}

bool TransportConnectJob::IsEndpointResultUsable(
    const HostResolverEndpointResult& result,
    bool svcb_optional) const {
  // A `HostResolverEndpointResult` with no ALPN protocols is the fallback
  // A/AAAA route. This is always compatible. We assume the ALPN-less option is
  // TCP-based.
  if (result.metadata.supported_protocol_alpns.empty()) {
    // See draft-ietf-dnsop-svcb-https-08, Section 3.
    return svcb_optional;
  }

  // See draft-ietf-dnsop-svcb-https-08, Section 7.1.2. Routes are usable if
  // there is an overlap between the route's ALPN protocols and the configured
  // ones. This ensures we do not, e.g., connect to a QUIC-only route with TCP.
  // Note that, if `params_` did not specify any ALPN protocols, no
  // SVCB/HTTPS-based routes will match and we will effectively ignore all but
  // plain A/AAAA routes.
  for (const auto& alpn : result.metadata.supported_protocol_alpns) {
    if (params_->supported_alpns().contains(alpn)) {
      return true;
    }
  }
  return false;
}

AddressList TransportConnectJob::GetCurrentAddressList() const {
  CHECK_LT(current_endpoint_result_, endpoint_results_.size());
  const HostResolverEndpointResult& endpoint_result =
      endpoint_results_[current_endpoint_result_];
  // TODO(crbug.com/126134): `HostResolverEndpointResult` has a
  // `vector<IPEndPoint>`, while all these classes expect an `AddressList`.
  // Align these after DNS aliases are fully moved out of `AddressList`.
  // https://crbug.com/1291352 will also likely move the `AddressList` iteration
  // out of `TCPClientSocket`, which will also avoid the conversion.
  return AddressList(endpoint_result.ip_endpoints);
}

void TransportConnectJob::SaveConnectionAttempts(const StreamSocket& socket) {
  ConnectionAttempts attempts;
  socket.GetConnectionAttempts(&attempts);
  connection_attempts_.insert(connection_attempts_.end(), attempts.begin(),
                              attempts.end());
}

}  // namespace net
