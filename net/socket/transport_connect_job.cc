// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/transport_connect_job.h"

#include <memory>
#include <utility>

#include "base/check_op.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "net/base/features.h"
#include "net/base/host_port_pair.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/base/trace_constants.h"
#include "net/base/tracing.h"
#include "net/dns/public/host_resolver_results.h"
#include "net/dns/public/secure_dns_policy.h"
#include "net/log/net_log_event_type.h"
#include "net/socket/socket_tag.h"
#include "net/socket/transport_connect_sub_job.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "url/scheme_host_port.h"
#include "url/url_constants.h"

namespace net {

namespace {

// TODO(crbug.com/40181080): Delete once endpoint usage is converted to using
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
    NetworkAnonymizationKey network_anonymization_key,
    SecureDnsPolicy secure_dns_policy,
    OnHostResolutionCallback host_resolution_callback,
    base::flat_set<std::string> supported_alpns)
    : destination_(std::move(destination)),
      network_anonymization_key_(std::move(network_anonymization_key)),
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

TransportConnectJob::TransportConnectJob(
    RequestPriority priority,
    const SocketTag& socket_tag,
    const CommonConnectJobParams* common_connect_job_params,
    const scoped_refptr<TransportSocketParams>& params,
    Delegate* delegate,
    const NetLogWithSource* net_log,
    std::optional<EndpointResultOverride> endpoint_result_override)
    : ConnectJob(priority,
                 socket_tag,
                 ConnectionTimeout(),
                 common_connect_job_params,
                 delegate,
                 net_log,
                 NetLogSourceType::TRANSPORT_CONNECT_JOB,
                 NetLogEventType::TRANSPORT_CONNECT_JOB_CONNECT),
      params_(params) {
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
// ~HostResolver::Request and ~TransportConnectSubJob will take care of it.
TransportConnectJob::~TransportConnectJob() = default;

LoadState TransportConnectJob::GetLoadState() const {
  switch (next_state_) {
    case STATE_RESOLVE_HOST:
    case STATE_RESOLVE_HOST_COMPLETE:
    case STATE_RESOLVE_HOST_CALLBACK_COMPLETE:
      return LOAD_STATE_RESOLVING_HOST;
    case STATE_TRANSPORT_CONNECT:
    case STATE_TRANSPORT_CONNECT_COMPLETE: {
      LoadState load_state = LOAD_STATE_IDLE;
      if (ipv6_job_ && ipv6_job_->started()) {
        load_state = ipv6_job_->GetLoadState();
      }
      // This method should return LOAD_STATE_CONNECTING in preference to
      // LOAD_STATE_WAITING_FOR_AVAILABLE_SOCKET when possible because "waiting
      // for available socket" implies that nothing is happening.
      if (ipv4_job_ && ipv4_job_->started() &&
          load_state != LOAD_STATE_CONNECTING) {
        load_state = ipv4_job_->GetLoadState();
      }
      return load_state;
    }
    case STATE_NONE:
      return LOAD_STATE_IDLE;
  }
}

bool TransportConnectJob::HasEstablishedConnection() const {
  // No need to ever return true, since NotifyComplete() is called as soon as a
  // connection is established.
  return false;
}

ConnectionAttempts TransportConnectJob::GetConnectionAttempts() const {
  return connection_attempts_;
}

ResolveErrorInfo TransportConnectJob::GetResolveErrorInfo() const {
  return resolve_error_info_;
}

std::optional<HostResolverEndpointResult>
TransportConnectJob::GetHostResolverEndpointResult() const {
  CHECK_LT(current_endpoint_result_, endpoint_results_.size());
  return endpoint_results_[current_endpoint_result_];
}

base::TimeDelta TransportConnectJob::ConnectionTimeout() {
  // TODO(eroman): The use of this constant needs to be re-evaluated. The time
  // needed for TCPClientSocketXXX::Connect() can be arbitrarily long, since
  // the address list may contain many alternatives, and most of those may
  // timeout. Even worse, the per-connect timeout threshold varies greatly
  // between systems (anywhere from 20 seconds to 190 seconds).
  // See comment #12 at http://crbug.com/23364 for specifics.
  return base::Minutes(4);
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
        rv = DoTransportConnectComplete(rv);
        break;
      default:
        NOTREACHED_IN_MIGRATION();
        rv = ERR_FAILED;
        break;
    }
  } while (rv != ERR_IO_PENDING && next_state_ != STATE_NONE);

  return rv;
}

int TransportConnectJob::DoResolveHost() {
  connect_timing_.domain_lookup_start = base::TimeTicks::Now();

  if (has_dns_override_) {
    DCHECK_EQ(1u, endpoint_results_.size());
    connect_timing_.domain_lookup_end = connect_timing_.domain_lookup_start;
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
        params_->network_anonymization_key(), net_log(), parameters);
  } else {
    request_ = host_resolver()->CreateRequest(
        absl::get<HostPortPair>(params_->destination()),
        params_->network_anonymization_key(), net_log(), parameters);
  }

  return request_->Start(base::BindOnce(&TransportConnectJob::OnIOComplete,
                                        base::Unretained(this)));
}

int TransportConnectJob::DoResolveHostComplete(int result) {
  TRACE_EVENT0(NetTracingCategory(),
               "TransportConnectJob::DoResolveHostComplete");
  connect_timing_.domain_lookup_end = base::TimeTicks::Now();
  // Overwrite connection start time, since for connections that do not go
  // through proxies, |connect_start| should not include dns lookup time.
  connect_timing_.connect_start = connect_timing_.domain_lookup_end;
  resolve_error_info_ = request_->GetResolveErrorInfo();

  if (result != OK) {
    // If hostname resolution failed, record an empty endpoint and the result.
    connection_attempts_.push_back(ConnectionAttempt(IPEndPoint(), result));
    return result;
  }

  DCHECK(request_->GetAddressResults());
  DCHECK(request_->GetDnsAliasResults());
  DCHECK(request_->GetEndpointResults());

  // Invoke callback.  If it indicates |this| may be slated for deletion, then
  // only continue after a PostTask.
  next_state_ = STATE_RESOLVE_HOST_CALLBACK_COMPLETE;
  if (!params_->host_resolution_callback().is_null()) {
    OnHostResolutionCallbackResult callback_result =
        params_->host_resolution_callback().Run(
            ToLegacyDestinationEndpoint(params_->destination()),
            *request_->GetEndpointResults(), *request_->GetDnsAliasResults());
    if (callback_result == OnHostResolutionCallbackResult::kMayBeDeletedAsync) {
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
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
  std::set<IPEndPoint> ip_endpoints_seen;
  for (const auto& result : unfiltered_results) {
    if (!IsEndpointResultUsable(result, svcb_optional)) {
      continue;
    }
    // The TCP connect itself does not depend on any metadata, so we can dedup
    // by IP endpoint. In particular, the fallback A/AAAA route will often use
    // the same IP endpoints as the HTTPS route. If they do not work for one
    // route, there is no use in trying a second time.
    std::vector<IPEndPoint> ip_endpoints;
    for (const auto& ip_endpoint : result.ip_endpoints) {
      auto [iter, inserted] = ip_endpoints_seen.insert(ip_endpoint);
      if (inserted) {
        ip_endpoints.push_back(ip_endpoint);
      }
    }
    if (!ip_endpoints.empty()) {
      HostResolverEndpointResult new_result;
      new_result.ip_endpoints = std::move(ip_endpoints);
      new_result.metadata = result.metadata;
      endpoint_results_.push_back(std::move(new_result));
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

  const HostResolverEndpointResult& endpoint =
      GetEndpointResultForCurrentSubJobs();
  std::vector<IPEndPoint> ipv4_addresses, ipv6_addresses;
  for (const auto& ip_endpoint : endpoint.ip_endpoints) {
    switch (ip_endpoint.GetFamily()) {
      case ADDRESS_FAMILY_IPV4:
        ipv4_addresses.push_back(ip_endpoint);
        break;

      case ADDRESS_FAMILY_IPV6:
        ipv6_addresses.push_back(ip_endpoint);
        break;

      default:
        DVLOG(1) << "Unexpected ADDRESS_FAMILY: " << ip_endpoint.GetFamily();
        break;
    }
  }

  if (!ipv4_addresses.empty()) {
    ipv4_job_ = std::make_unique<TransportConnectSubJob>(
        std::move(ipv4_addresses), this, SUB_JOB_IPV4);
  }

  if (!ipv6_addresses.empty()) {
    ipv6_job_ = std::make_unique<TransportConnectSubJob>(
        std::move(ipv6_addresses), this, SUB_JOB_IPV6);
    int result = ipv6_job_->Start();
    if (result != ERR_IO_PENDING)
      return HandleSubJobComplete(result, ipv6_job_.get());
    if (ipv4_job_) {
      // This use of base::Unretained is safe because |fallback_timer_| is
      // owned by this object.
      fallback_timer_.Start(
          FROM_HERE, kIPv6FallbackTime,
          base::BindOnce(&TransportConnectJob::StartIPv4JobAsync,
                         base::Unretained(this)));
    }
    return ERR_IO_PENDING;
  }

  DCHECK(!ipv6_job_);
  DCHECK(ipv4_job_);
  int result = ipv4_job_->Start();
  if (result != ERR_IO_PENDING)
    return HandleSubJobComplete(result, ipv4_job_.get());
  return ERR_IO_PENDING;
}

int TransportConnectJob::DoTransportConnectComplete(int result) {
  // Make sure nothing else calls back into this object.
  ipv4_job_.reset();
  ipv6_job_.reset();
  fallback_timer_.Stop();

  if (result == OK) {
    DCHECK(!connect_timing_.connect_start.is_null());
    DCHECK(!connect_timing_.domain_lookup_start.is_null());
    // `HandleSubJobComplete` should have called `SetSocket`.
    DCHECK(socket());
    base::TimeTicks now = base::TimeTicks::Now();
    base::TimeDelta total_duration = now - connect_timing_.domain_lookup_start;
    UMA_HISTOGRAM_CUSTOM_TIMES("Net.DNS_Resolution_And_TCP_Connection_Latency2",
                               total_duration, base::Milliseconds(1),
                               base::Minutes(10), 100);

    base::TimeDelta connect_duration = now - connect_timing_.connect_start;
    UMA_HISTOGRAM_CUSTOM_TIMES("Net.TCP_Connection_Latency", connect_duration,
                               base::Milliseconds(1), base::Minutes(10), 100);
  } else {
    // Don't try the next route if entering suspend mode.
    if (result != ERR_NETWORK_IO_SUSPENDED) {
      // If there is another endpoint available, try it.
      current_endpoint_result_++;
      if (current_endpoint_result_ < endpoint_results_.size()) {
        next_state_ = STATE_TRANSPORT_CONNECT;
        result = OK;
      }
    }
  }

  return result;
}

int TransportConnectJob::HandleSubJobComplete(int result,
                                              TransportConnectSubJob* job) {
  DCHECK_NE(result, ERR_IO_PENDING);
  if (result == OK) {
    SetSocket(job->PassSocket(), dns_aliases_);
    return result;
  }

  if (result == ERR_NETWORK_IO_SUSPENDED) {
    // Don't try other jobs if entering suspend mode.
    return result;
  }

  switch (job->type()) {
    case SUB_JOB_IPV4:
      ipv4_job_.reset();
      break;

    case SUB_JOB_IPV6:
      ipv6_job_.reset();
      // Start the other job, rather than wait for the fallback timer.
      if (ipv4_job_ && !ipv4_job_->started()) {
        fallback_timer_.Stop();
        result = ipv4_job_->Start();
        if (result != ERR_IO_PENDING) {
          return HandleSubJobComplete(result, ipv4_job_.get());
        }
      }
      break;
  }

  if (ipv4_job_ || ipv6_job_) {
    // Wait for the other job to complete, rather than reporting |result|.
    return ERR_IO_PENDING;
  }

  return result;
}

void TransportConnectJob::OnSubJobComplete(int result,
                                           TransportConnectSubJob* job) {
  result = HandleSubJobComplete(result, job);
  if (result != ERR_IO_PENDING) {
    OnIOComplete(result);
  }
}

void TransportConnectJob::StartIPv4JobAsync() {
  DCHECK(ipv4_job_);
  net_log().AddEvent(NetLogEventType::TRANSPORT_CONNECT_JOB_IPV6_FALLBACK);
  int result = ipv4_job_->Start();
  if (result != ERR_IO_PENDING)
    OnSubJobComplete(result, ipv4_job_.get());
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
  if (!scheme_host_port || scheme_host_port->scheme() != url::kHttpsScheme) {
    return true;  // This is not a SVCB-capable request at all.
  }

  if (!common_connect_job_params()->ssl_client_context ||
      !common_connect_job_params()->ssl_client_context->config().ech_enabled) {
    return true;  // ECH is not supported for this request.
  }

  return !HostResolver::AllProtocolEndpointsHaveEch(results);
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

const HostResolverEndpointResult&
TransportConnectJob::GetEndpointResultForCurrentSubJobs() const {
  CHECK_LT(current_endpoint_result_, endpoint_results_.size());
  return endpoint_results_[current_endpoint_result_];
}

}  // namespace net
