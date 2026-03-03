// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/tcp_connect_job.h"

#include <memory>
#include <set>
#include <utility>
#include <variant>

#include "base/check_op.h"
#include "base/containers/flat_set.h"
#include "base/containers/to_vector.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "base/types/expected.h"
#include "net/base/features.h"
#include "net/base/host_port_pair.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/base/trace_constants.h"
#include "net/dns/public/host_resolver_results.h"
#include "net/dns/public/secure_dns_policy.h"
#include "net/log/net_log_event_type.h"
#include "net/socket/socket_tag.h"
#include "net/socket/tcp_connect_job_connector.h"
#include "net/socket/transport_connect_job.h"
#include "url/scheme_host_port.h"
#include "url/url_constants.h"

namespace net {

namespace {

// Converts `endpoint` to a HostPortPair.
HostPortPair ToLegacyDestinationEndpoint(
    const TransportSocketParams::Endpoint& endpoint) {
  if (std::holds_alternative<url::SchemeHostPort>(endpoint)) {
    return HostPortPair::FromSchemeHostPort(
        std::get<url::SchemeHostPort>(endpoint));
  }

  DCHECK(std::holds_alternative<HostPortPair>(endpoint));
  return std::get<HostPortPair>(endpoint);
}

}  // namespace

TcpConnectJob::ServiceEndpointOverride::ServiceEndpointOverride(
    ServiceEndpoint endpoint,
    std::set<std::string> dns_aliases)
    : endpoints({std::move(endpoint)}), dns_aliases(std::move(dns_aliases)) {}
TcpConnectJob::ServiceEndpointOverride::ServiceEndpointOverride(
    ServiceEndpointOverride&&) = default;
TcpConnectJob::ServiceEndpointOverride::ServiceEndpointOverride(
    const ServiceEndpointOverride&) = default;
TcpConnectJob::ServiceEndpointOverride::~ServiceEndpointOverride() = default;

TcpConnectJob::TcpConnectJob(
    RequestPriority priority,
    const SocketTag& socket_tag,
    const CommonConnectJobParams* common_connect_job_params,
    const scoped_refptr<TransportSocketParams>& params,
    ConnectJob::Delegate* delegate,
    const NetLogWithSource* net_log,
    std::optional<ServiceEndpointOverride> endpoint_result_override)
    : ConnectJob(priority,
                 socket_tag,
                 ConnectionTimeout(),
                 common_connect_job_params,
                 delegate,
                 net_log,
                 NetLogSourceType::TCP_CONNECT_JOB,
                 NetLogEventType::TCP_CONNECT_JOB_CONNECT),
      params_(params),
      endpoint_override_(std::move(endpoint_result_override)),
      primary_connector_(std::make_unique<Connector>(this, "first")) {
  DCHECK(base::FeatureList::IsEnabled(features::kHappyEyeballsV2));
  if (endpoint_override_) {
    UpdateSvcbOptional();
    DCHECK(!endpoint_override_->endpoints.front().ipv4_endpoints.empty() ||
           !endpoint_override_->endpoints.front().ipv6_endpoints.empty());
    DCHECK(IsEndpointResultUsable(endpoint_override_->endpoints.front()));
  }
}

TcpConnectJob::~TcpConnectJob() = default;

LoadState TcpConnectJob::GetLoadState() const {
  LoadState load_state = primary_connector_->GetLoadState();
  // This method should return LOAD_STATE_CONNECTING in preference to
  // LOAD_STATE_WAITING_FOR_AVAILABLE_SOCKET when possible because "waiting
  // for available socket" implies that nothing is happening.
  if (ipv4_connector_ && load_state != LOAD_STATE_CONNECTING) {
    load_state = ipv4_connector_->GetLoadState();
  }
  return load_state;
}

bool TcpConnectJob::HasEstablishedConnection() const {
  return has_established_connection_;
}

ConnectionAttempts TcpConnectJob::GetConnectionAttempts() const {
  return connection_attempts_;
}

ResolveErrorInfo TcpConnectJob::GetResolveErrorInfo() const {
  return resolve_error_info_;
}

std::optional<HostResolverEndpointResult>
TcpConnectJob::GetHostResolverEndpointResult() const {
  // Callers should call GetServiceEndpoint() instead, if they're using this
  // class.
  NOTREACHED();
}

ServiceEndpoint TcpConnectJob::PassServiceEndpoint() {
  CHECK(final_service_endpoint_);
  return std::move(final_service_endpoint_).value();
}

base::TimeDelta TcpConnectJob::ConnectionTimeout() {
  // TODO(eroman): The use of this constant needs to be re-evaluated. The time
  // needed for TCPClientSocketXXX::Connect() can be arbitrarily long, since
  // the address list may contain many alternatives, and most of those may
  // timeout. Even worse, the per-connect timeout threshold varies greatly
  // between systems (anywhere from 20 seconds to 190 seconds).
  // See comment #12 at http://crbug.com/23364 for specifics.
  return base::Minutes(4);
}

bool TcpConnectJob::has_two_connectors_for_testing() const {
  CHECK(!is_done_);
  // `primary_connector_` should never be nullptr.
  CHECK(primary_connector_);
  return ipv4_connector_.get() != nullptr;
}

int TcpConnectJob::ConnectInternal() {
  connect_timing_.domain_lookup_start = base::TimeTicks::Now();

  int rv = OK;
  if (!endpoint_override_) {
    HostResolver::ResolveHostParameters parameters;
    parameters.initial_priority = priority();
    parameters.secure_dns_policy = params_->secure_dns_policy();
    dns_request_ = host_resolver()->CreateServiceEndpointRequest(
        HostResolver::Host(params_->destination()),
        params_->network_anonymization_key(), net_log(), parameters);

    rv = dns_request_->Start(this);
    if (rv == ERR_IO_PENDING) {
      return rv;
    }
  }

  return DoServiceEndpointsUpdated(/*dns_request_final_result=*/rv);
}

void TcpConnectJob::ChangePriorityInternal(RequestPriority priority) {
  if (dns_request_) {
    // Only need to change the priority of the DNS request. The TCP connection
    // attempts doesn't have priorities.
    dns_request_->ChangeRequestPriority(priority);
  }
}

int TcpConnectJob::DoServiceEndpointsUpdated(
    std::optional<int> dns_request_final_result) {
  // SetDone() destroys the DNS request on completion, preventing this from
  // being reached once `is_done_` is set to true.
  CHECK(!is_done_);
  DCHECK(!dns_request_complete_);

  // Reset progress through endpoint results, as new ones may have been inserted
  // before the one that was currently under consideration.
  current_service_endpoint_index_ = 0;

  bool did_fail = false;
  if (dns_request_final_result) {
    DCHECK_NE(*dns_request_final_result, ERR_IO_PENDING);

    dns_request_complete_ = true;
    did_fail = dns_request_final_result.value() != OK;
  }

  // If the request has failed, or all live Connectors are waiting on the DNS
  // result, update `domain_lookup_end`, so it accurately reflects the time that
  // the request was blocked on DNS. This can hide fetch time, but for now, do
  // not return overlapping connect and DNS lookup times. See class not in
  // header for more details.
  if (did_fail ||
      (primary_connector_->is_waiting_on_dns() &&
       (!ipv4_connector_ || ipv4_connector_->is_waiting_on_dns()))) {
    connect_timing_.domain_lookup_end = base::TimeTicks::Now();
    // Even on failure, or when there are no IPs, update `connect_start`. This
    // matches legacy behavior. Unclear if it matters.
    connect_timing_.connect_start = connect_timing_.domain_lookup_end;
  }

  // Complete the TcpConnectJob on DNS error.
  if (did_fail) {
    resolve_error_info_ = dns_request_->GetResolveErrorInfo();

    // If hostname resolution failed, clear any recorded connection attempts
    // record. SetDone() will create a new entry containing
    // `dns_request_final_result` and no IP, as fatal DNS errors takes
    // precedence over any earlier connection failures.
    connection_attempts_.clear();
    return SetDone(*dns_request_final_result);
  }

  // Recompute `is_svcb_optional_`. There's no need to do it if
  // `endpoint_override_` is true, since it's already been set in that case, and
  // can't change.
  if (!endpoint_override_) {
    UpdateSvcbOptional();
  }

  if (!params_->host_resolution_callback().is_null()) {
    OnHostResolutionCallbackResult callback_result =
        params_->host_resolution_callback().Run(
            ToLegacyDestinationEndpoint(params_->destination()),
            GetEndpointResults(), GetDnsAliasResults());

    // Best effort to delay `this` to allow looking for H2 sessions that may
    // result in cancelling this job. Both the slow timer, and previous calls to
    // HandleServiceEndpointsUpdated() means that it's possible for work to
    // continue even when this is hit, and we're nominally waiting for
    // TryAdvanceWaitingConnectorsAsync() to be invoked.
    //
    // This is only intended to delay things long enough for a single PostTask,
    // invoked by the callback, to run. That's a short enough delay that it's
    // probably not worth trying to do better, though would could have
    // DoTryAdvanceWaitingConnectors() and GetNextIPEndPoint() return
    // ERR_IO_PENDING until the task posted here is run.
    //
    // This does rely on task scheduling order to work as expected - that is,
    // for a task posted by the host resolution callback to be run strictly
    // before this task ends up being executed, so need to be careful of
    // priority inversion and starvation if modifying task priority here.
    if (callback_result == OnHostResolutionCallbackResult::kMayBeDeletedAsync) {
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE,
          base::BindOnce(&TcpConnectJob::TryAdvanceWaitingConnectorsAsync,
                         weak_ptr_factory_.GetWeakPtr()));
      return ERR_IO_PENDING;
    }
  }

  return DoTryAdvanceWaitingConnectors();
}

void TcpConnectJob::TryAdvanceWaitingConnectorsAsync() {
  NotifyDelegateIfDone(DoTryAdvanceWaitingConnectors());
}

int TcpConnectJob::DoTryAdvanceWaitingConnectors() {
  // SetDone() should cancel all pending activity on completion, so this should
  // not be reachable after completion.
  CHECK(!is_done_);
  DCHECK(!primary_connector_->is_done() ||
         (ipv4_connector_ && !ipv4_connector_->is_done()));

  // Note that `primary_connector_` and `ipv4_connector_` are only deleted when
  // DoConnectorComplete() returns a value other than ERR_IO_PENDING, and
  // `ipv4_connector_` is only created from the slower timer callback, so this
  // is safe.
  for (Connector* connector :
       {primary_connector_.get(), ipv4_connector_.get()}) {
    if (connector && !connector->is_done()) {
      int rv = connector->TryAdvanceState();
      if (rv != ERR_IO_PENDING) {
        // The connection attempt completed synchronously. Call
        // DoConnectorComplete() to handle the result, and learn if the entire
        // ConnectJob is complete.
        rv = DoConnectorComplete(rv, *connector);
        if (rv != ERR_IO_PENDING) {
          return rv;
        }
      }
    }
  }

  // If we reach this point, There should still be work to do.
  CHECK(!is_done_);
  DCHECK(!primary_connector_->is_done() ||
         (ipv4_connector_ && !ipv4_connector_->is_done()));

  // If there is only a single connector, and we've started trying to connect,
  // and the slow timer isn't running, start the slow timer.
  //
  // This could result in starting the slow timer if we, e.g., we're already
  // trying to final IP and no more IPs are coming, but this keeps things
  // simple.
  if (!ipv4_connector_ && !slow_timer_.IsRunning() &&
      !attempted_addresses_.empty()) {
    slow_timer_.Start(
        FROM_HERE, kIPv6FallbackTime,
        base::BindOnce(&TcpConnectJob::OnSlow, base::Unretained(this)));
  }

  return ERR_IO_PENDING;
}

int TcpConnectJob::DoConnectorComplete(int result, Connector& connector) {
  DCHECK_NE(result, ERR_IO_PENDING);
  // Once one connector succeeds, no need to wait for the other. Also treat
  // ERR_NETWORK_IO_SUSPENDED as a failure for both connectors.
  if (result == OK || result == ERR_NETWORK_IO_SUSPENDED) {
    return SetDone(result, &connector);
  }

  // If both connectors have failed, we're also done.
  if (primary_connector_->is_done() &&
      (!ipv4_connector_ || ipv4_connector_->is_done())) {
    return SetDone(result, &connector);
  }

  return ERR_IO_PENDING;
}

void TcpConnectJob::OnServiceEndpointsUpdated() {
  NotifyDelegateIfDone(
      DoServiceEndpointsUpdated(/*dns_request_final_result=*/std::nullopt));
}

void TcpConnectJob::OnServiceEndpointRequestFinished(int rv) {
  NotifyDelegateIfDone(
      DoServiceEndpointsUpdated(/*dns_request_final_result=*/rv));
}

void TcpConnectJob::OnConnectorComplete(int result, Connector& connector) {
  int rv = DoConnectorComplete(result, connector);
  if (rv != ERR_IO_PENDING) {
    NotifyDelegateOfCompletion(rv);
  }
}

void TcpConnectJob::OnSlow() {
  CHECK(!is_done_);
  DCHECK(!ipv4_connector_);

  net_log().AddEvent(NetLogEventType::TCP_CONNECT_JOB_CREATE_SECOND_CONNECTOR);

  // Make a second connector, so have separate IPv4 and IPv6 connectors. The
  // `primary_connector_` may be waiting for an IP, or doing either a v4 or v6
  // lookup. If it's doing a v4 lookup, move it into `ipv4_connector_`.
  //
  // Since the connectors may be flipped here, the static names of the
  // connectors for logging purposes are "first" and "second", rather than
  // "primary" and "ipv4".
  ipv4_connector_ = std::make_unique<Connector>(this, "second");
  if (!primary_connector_->is_connecting_to_ipv6()) {
    std::swap(primary_connector_, ipv4_connector_);
  }

  TryAdvanceWaitingConnectorsAsync();
}

TcpConnectJob::IPEndPointInfo TcpConnectJob::GetNextIPEndPoint(
    const Connector& connector) {
  const auto& service_endpoints = GetEndpointResults();
  CHECK(!is_done_);
  DCHECK(!connector.is_done());

  // Other job, if any, for checking its state, and advancing it if necessary.
  const Connector* other_job =
      (&connector == primary_connector_.get() ? ipv4_connector_.get()
                                              : primary_connector_.get());

  // Note that this will make both jobs use IPv4/IPv6, once there are no more
  // IPs of the other type. Not clear if that's a concern. Not too difficult to
  // change behavior - only checking IPv4 or IPv6 when there are two jobs should
  // be sufficient. `current_service_endpoint_index_` logic will still work
  // correctly.
  bool prefer_ipv6 = prefer_ipv6_;
  if (ipv4_connector_) {
    prefer_ipv6 = (primary_connector_.get() == &connector);
  }

  bool posted_resume_task = false;

  while (current_service_endpoint_index_ < service_endpoints.size()) {
    const auto& service_endpoint =
        service_endpoints[current_service_endpoint_index_];
    if (IsEndpointResultUsable(service_endpoint)) {
      for (bool ip_v6 : {prefer_ipv6, !prefer_ipv6}) {
        const auto& ip_endpoints = ip_v6 ? service_endpoint.ipv6_endpoints
                                         : service_endpoint.ipv4_endpoints;
        for (const auto& ip_endpoint : ip_endpoints) {
          // If `ip_endpoint` hasn't been tried yet, add it to
          // `attempted_addresses_` and we will return it.
          auto [it, inserted] = attempted_addresses_.emplace(ip_endpoint);
          if (inserted) {
            return ip_endpoint;
          }
        }
      }
    }

    // Only move on to the next endpoint if either there's no other connector,
    // or the other connector is waiting to receive an endpoint. Since new
    // results may come in out of order, this isn't perfect - e.g., could
    // still be connecting to a AAAA record when HTTPS records come in. Then
    // the AAAA connection attempt could block connection attempts to the
    // second ServiceEndpoint entry (and could block the second job from
    // trying the next A/AAAA entry as well, if the HTTPS record connection
    // attempts fail quickly). Could throw away the AAAA attempt in that case,
    // or have logic to detect it, but this seems a reasonable balance of
    // accuracy, complexity, and performance.
    if (other_job && !other_job->is_waiting_for_endpoint()) {
      // Need to wait for other jobs to complete.
      return base::unexpected(ERR_IO_PENDING);
    }

    ++current_service_endpoint_index_;
    // May need to resume the other job after advancing to the next result.
    if (other_job && !posted_resume_task) {
      // Small optimization to avoid posting multiple tasks at once - probably
      // not really needed. Nothing would break without it.
      posted_resume_task = true;
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE,
          base::BindOnce(&TcpConnectJob::TryAdvanceWaitingConnectorsAsync,
                         weak_ptr_factory_.GetWeakPtr()));
    }
  }

  // If there are no more IPEndPoints to try, and the DNS request is done,
  // return ERR_NAME_NOT_RESOLVED. This will be passed to the Connector to mean
  // there are no more destinations to try, and it will return its last connect
  // error, if it has one, or ERR_NAME_NOT_RESOLVED, otherwise.
  //
  // TODO(https://crbug.com/484073410): This will currently result in returning
  // ERR_NAME_NOT_RESOLVED if there are endpoints, but we can't use any of them,
  // or if we established a connection, discovered we can't use it, and then
  // discovered there are no endpoints we can use. May want to improve returned
  // errors in those cases.
  if (dns_request_complete_) {
    return base::unexpected(ERR_NAME_NOT_RESOLVED);
  }

  // More DNS results may be received down the line. Wait for them.
  return base::unexpected(ERR_IO_PENDING);
}

bool TcpConnectJob::IsEndpointResultUsable(
    const ServiceEndpoint& service_endpoint) const {
  // We assume the authority endpoint (i.e. not from SVCB/HTTPS) is TCP-based,
  // so an authority endpoint.
  if (!service_endpoint.metadata.IsAlternative()) {
    // See RFC 9460, Section 3.
    return is_svcb_optional_;
  }

  // See RFC 9460, Section 7.1.2. Alternative endpoints are usable if there is
  // an overlap between the endpoint's ALPN protocols and the configured ones.
  // This ensures we do not, e.g., connect to a QUIC-only endpoint with TCP.
  // Note that, if `params_` did not specify any ALPN protocols, no
  // SVCB/HTTPS-based endpoints will match and we will effectively ignore all
  // but plain A/AAAA endpoints.
  for (const auto& alpn : service_endpoint.metadata.supported_protocol_alpns) {
    if (params_->supported_alpns().contains(alpn)) {
      return true;
    }
  }
  return false;
}

const ServiceEndpoint* TcpConnectJob::FindServiceEndpoint(
    const IPEndPoint& ip_endpoint) const {
  bool is_v6 = (ip_endpoint.GetFamily() == ADDRESS_FAMILY_IPV6);

  for (const auto& service_endpoint : GetEndpointResults()) {
    // Skip unusable endpoints.
    if (!IsEndpointResultUsable(service_endpoint)) {
      continue;
    }

    const auto& ip_endpoints = is_v6 ? service_endpoint.ipv6_endpoints
                                     : service_endpoint.ipv4_endpoints;
    if (std::find(ip_endpoints.begin(), ip_endpoints.end(), ip_endpoint) !=
        ip_endpoints.end()) {
      return &service_endpoint;
    }
  }

  // The service endpoint wasn't found. This can happen when called from
  // IsIPEndPointUsable() and the HTTPS records indicate that an endpoint that
  // we previously thought was usable actually is not.
  return nullptr;
}

void TcpConnectJob::UpdateSvcbOptional() {
  const auto* scheme_host_port =
      std::get_if<url::SchemeHostPort>(&params_->destination());
  if (!scheme_host_port || scheme_host_port->scheme() != url::kHttpsScheme) {
    // This is not a SVCB-capable request at all.
    is_svcb_optional_ = true;
  } else if (!common_connect_job_params()->ssl_client_context ||
             !common_connect_job_params()
                  ->ssl_client_context->config()
                  .ech_enabled) {
    // ECH is not supported for this request.
    is_svcb_optional_ = true;
  } else {
    is_svcb_optional_ =
        !HostResolver::AllAlternativeEndpointsHaveEch(GetEndpointResults());
  }
}

int TcpConnectJob::SetDone(int result, Connector* connector) {
  CHECK(!is_done_);
  DCHECK(!final_service_endpoint_);

  if (result == OK) {
    DCHECK(EndpointsCryptoReady());
    DCHECK(connector);

    SetSocket(connector->PassSocket(), GetDnsAliasResults());
    final_service_endpoint_ = connector->PassFinalServiceEndpoint();
    DCHECK(final_service_endpoint_);
    DCHECK(IsEndpointResultUsable(*final_service_endpoint_));
  } else {
    // If there were no attempts, there were no usable addresses. Use `result`
    // in that case.
    if (connection_attempts_.empty()) {
      connection_attempts_.emplace_back(IPEndPoint(), result);
    }

    // Pulling from `connection_attempts_` is the simplest way to get the most
    // recent error. If there are two Connectors, and they've both failed at
    // once when we learn there are no more IP addresses to try, it's difficult
    // to determine which one's error to use. Pulling the last error
    // `connection_attempts_`, conveniently, avoids that issue, since it's in
    // chronological order.
    result = connection_attempts_.back().result;
  }

  // Cancel all work, and any pending callbacks. Main methods all have
  // `CHECK(!is_done_)` to catch if they are incorrectly run after completion,
  // to help ensure this is comprehensive.
  slow_timer_.Stop();
  primary_connector_.reset();
  ipv4_connector_.reset();
  dns_request_.reset();
  // This will prevent any pending posted TryAdvanceWaitingConnectorsAsync tasks
  // from running.
  weak_ptr_factory_.InvalidateWeakPtrs();
  is_done_ = true;

  return result;
}

base::span<const ServiceEndpoint> TcpConnectJob::GetEndpointResults() const {
  if (dns_request_) {
    DCHECK(!endpoint_override_);
    return dns_request_->GetEndpointResults();
  }

  DCHECK(endpoint_override_);
  return endpoint_override_->endpoints;
}

bool TcpConnectJob::EndpointsCryptoReady() const {
  if (dns_request_) {
    DCHECK(!endpoint_override_);
    return dns_request_->EndpointsCryptoReady();
  }

  DCHECK(endpoint_override_);
  return true;
}

const std::set<std::string>& TcpConnectJob::GetDnsAliasResults() const {
  if (dns_request_) {
    DCHECK(!endpoint_override_);
    return dns_request_->GetDnsAliasResults();
  }

  DCHECK(endpoint_override_);
  return endpoint_override_->dns_aliases;
}

void TcpConnectJob::NotifyDelegateIfDone(int result) {
  if (result != ERR_IO_PENDING) {
    DCHECK(is_done_);
    NotifyDelegateOfCompletion(result);
  }
}

}  // namespace net
