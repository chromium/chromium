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
                 NetLogSourceType::TRANSPORT_CONNECT_JOB,
                 NetLogEventType::TRANSPORT_CONNECT_JOB_CONNECT),
      params_(params),
      endpoint_override_(std::move(endpoint_result_override)),
      connector_(std::make_unique<Connector>(this)) {
  if (endpoint_override_) {
    UpdateSvcbOptional();
    DCHECK(!endpoint_override_->endpoints.front().ipv4_endpoints.empty() ||
           !endpoint_override_->endpoints.front().ipv6_endpoints.empty());
    DCHECK(IsEndpointResultUsable(endpoint_override_->endpoints.front()));
  }
}

TcpConnectJob::~TcpConnectJob() = default;

LoadState TcpConnectJob::GetLoadState() const {
  // TODO(https://crbug.com/484073410): Test this.
  return connector_->GetLoadState();
}

bool TcpConnectJob::HasEstablishedConnection() const {
  return has_established_connection_;
}

ConnectionAttempts TcpConnectJob::GetConnectionAttempts() const {
  // TODO(https://crbug.com/484073410): Implement this.
  return ConnectionAttempts();
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

ServiceEndpoint TcpConnectJob::GetServiceEndpoint() const {
  DCHECK(final_address_);
  // TODO(https://crbug.com/484073410): This search has already been done. Could
  // consider caching the result. If we cached a copy, we could also destroy the
  // request on cancellation, though then we'd want to make this std::move() out
  // the return value, to avoid the extra copy.
  const ServiceEndpoint* service_endpoint =
      FindServiceEndpoint(*final_address_);
  DCHECK(service_endpoint);
  return *service_endpoint;
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

int TcpConnectJob::ConnectInternal() {
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
  // TODO(https://crbug.com/484073410): Test this method.
  if (dns_request_) {
    DCHECK(dns_request_);
    // Only need to change the priority of the DNS request. The TCP connection
    // attempts doesn't have priorities.
    dns_request_->ChangeRequestPriority(priority);
  }
}

int TcpConnectJob::DoServiceEndpointsUpdated(
    std::optional<int> dns_request_final_result) {
  DCHECK(!is_done_);
  DCHECK(!dns_request_complete_);

  bool did_fail = false;
  if (dns_request_final_result) {
    DCHECK_NE(*dns_request_final_result, ERR_IO_PENDING);

    dns_request_complete_ = true;
    did_fail = dns_request_final_result.value() != OK;
  }

  // TODO(https://crbug.com/484073410): Update `connect_timing_` here.

  // Complete the TcpConnectJob on DNS error.
  if (did_fail) {
    resolve_error_info_ = dns_request_->GetResolveErrorInfo();

    return SetDone(*dns_request_final_result);
  }

  // Recompute `is_svcb_optional_`. There's no need to do it if
  // `endpoint_override_` is true, since it's already been set in that case, and
  // can't change.
  if (!endpoint_override_) {
    UpdateSvcbOptional();
  }

  // TODO(https://crbug.com/484073410): Call `
  // params_->host_resolution_callback()` here, if non-null, and delay next step
  // if needed.

  return DoTryAdvanceWaitingConnectors();
}

int TcpConnectJob::DoTryAdvanceWaitingConnectors() {
  DCHECK(!is_done_);
  DCHECK(!connector_->is_done());

  if (!connector_->is_done()) {
    int rv = connector_->OnEndpointDataAvailable();
    if (rv != ERR_IO_PENDING) {
      return SetDone(rv);
    }
  }

  // TODO(https://crbug.com/484073410) Add a second Connector after some delay.
  return ERR_IO_PENDING;
}

void TcpConnectJob::OnServiceEndpointsUpdated() {
  // These `is_done_` checks my be be hit if the consumer doesn't delete `this`
  // immediately on completion.
  if (is_done_) {
    return;
  }

  NotifyDelegateIfDone(
      DoServiceEndpointsUpdated(/*dns_request_final_result=*/std::nullopt));
}

void TcpConnectJob::OnServiceEndpointRequestFinished(int rv) {
  // These `is_done_` checks my be be hit if the consumer doesn't delete `this`
  // immediately on completion.
  if (is_done_) {
    return;
  }

  NotifyDelegateIfDone(
      DoServiceEndpointsUpdated(/*dns_request_final_result=*/rv));
}

void TcpConnectJob::OnConnectorComplete(int result) {
  NotifyDelegateOfCompletion(SetDone(result));
}

TcpConnectJob::IPEndPointInfo TcpConnectJob::GetNextIPEndPoint() {
  DCHECK(!is_done_);
  DCHECK(!connector_->is_done());

  for (const auto& service_endpoint : GetEndpointResults()) {
    if (!IsEndpointResultUsable(service_endpoint)) {
      continue;
    }

    for (bool ip_v6 : {prefer_ipv6_, !prefer_ipv6_}) {
      const auto& ip_endpoints = ip_v6 ? service_endpoint.ipv6_endpoints
                                       : service_endpoint.ipv4_endpoints;
      for (const auto& ip_endpoint : ip_endpoints) {
        if (attempted_addresses_.contains(ip_endpoint)) {
          continue;
        }
        attempted_addresses_.emplace(ip_endpoint);
        return ip_endpoint;
      }
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

int TcpConnectJob::SetDone(int result) {
  DCHECK(!is_done_);
  DCHECK(!final_address_);

  if (result == OK) {
    DCHECK(EndpointsCryptoReady());
    DCHECK(IsIPEndPointUsable(connector_->CurrentAddress()));

    SetSocket(connector_->PassSocket(), GetDnsAliasResults());
    final_address_ = connector_->CurrentAddress();
  } else {
    // On success, may still need the DNS result, but don't need it on failure.
    dns_request_.reset();
  }

  connector_.reset();
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
