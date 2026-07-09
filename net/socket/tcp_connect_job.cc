// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/tcp_connect_job.h"

#include <algorithm>
#include <memory>
#include <optional>
#include <set>
#include <utility>
#include <variant>
#include <vector>

#include "base/check_op.h"
#include "base/containers/adapters.h"
#include "base/containers/flat_set.h"
#include "base/containers/span.h"
#include "base/containers/to_vector.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
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
#include "net/http/http_server_properties.h"
#include "net/log/net_log_event_type.h"
#include "net/nqe/network_quality_estimator.h"
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

bool IsDualRaceOptimisticDnsEnabled() {
  return base::FeatureList::IsEnabled(features::kOptimisticDnsForTcp) &&
         features::kUseStaleConnectorsForOptimisticDns.Get();
}

// Returns true if the given `endpoint` is present in the provided `results`
// list. This is used to check if an actively connecting stale endpoint is
// still valid when fresh DNS results arrive, allowing the stale connector to
// be promoted to a fresh connector.
bool IsEndpointInFreshList(const IPEndPoint& endpoint,
                           base::span<const ServiceEndpoint> results) {
  for (const auto& result : results) {
    if (std::ranges::find(result.ipv4_endpoints, endpoint) !=
            result.ipv4_endpoints.end() ||
        std::ranges::find(result.ipv6_endpoints, endpoint) !=
            result.ipv6_endpoints.end()) {
      return true;
    }
  }
  return false;
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
    std::optional<ServiceEndpointOverride> endpoint_result_override,
    bool disable_stale_dns)
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
      disable_stale_dns_(disable_stale_dns) {
  fresh_state_.primary_connector = std::make_unique<Connector>(this, "first");
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
  if (!fresh_state_.primary_connector) {
    return LOAD_STATE_IDLE;
  }
  LoadState load_state = fresh_state_.primary_connector->GetLoadState();
  // This method should return LOAD_STATE_CONNECTING in preference to
  // LOAD_STATE_WAITING_FOR_AVAILABLE_SOCKET when possible because "waiting
  // for available socket" implies that nothing is happening.
  if (fresh_state_.ipv4_connector && load_state != LOAD_STATE_CONNECTING) {
    load_state = fresh_state_.ipv4_connector->GetLoadState();
  }
  // If stale state is doing something and fresh state is just waiting for DNS,
  // we could report that.
  if (stale_state_.primary_connector &&
      load_state == LOAD_STATE_RESOLVING_HOST) {
    LoadState stale_load_state = stale_state_.primary_connector->GetLoadState();
    if (stale_load_state == LOAD_STATE_CONNECTING) {
      load_state = stale_load_state;
    }
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

std::optional<ResolutionDetails> TcpConnectJob::GetResolutionDetails() const {
  return resolution_details_;
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

size_t TcpConnectJob::GetFreshConnectorCountForTesting() const {
  CHECK(!is_done_);
  size_t count = 0;
  if (fresh_state_.primary_connector) {
    count++;
  }
  if (fresh_state_.ipv4_connector) {
    count++;
  }
  return count;
}

size_t TcpConnectJob::GetStaleConnectorCountForTesting() const {
  CHECK(!is_done_);
  size_t count = 0;
  if (stale_state_.primary_connector) {
    count++;
  }
  if (stale_state_.ipv4_connector) {
    count++;
  }
  return count;
}

int TcpConnectJob::ConnectInternal() {
  connect_timing_.domain_lookup_start = base::TimeTicks::Now();

  int rv = OK;
  if (!endpoint_override_) {
    HostResolver::ResolveHostParameters parameters;
    parameters.initial_priority = priority();
    parameters.secure_dns_policy = params_->secure_dns_policy();
    if (base::FeatureList::IsEnabled(features::kOptimisticDnsForTcp) &&
        !disable_stale_dns_) {
      parameters.cache_usage = HostResolver::ResolveHostParameters::CacheUsage::
          STALE_ALLOWED_WHILE_REFRESHING;
    }
    dns_request_ = host_resolver()->CreateServiceEndpointRequest(
        HostResolver::Host(params_->destination()),
        params_->network_anonymization_key(), params_->target_network(),
        net_log(), parameters);

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

  // If we have fresh endpoints, try to promote any active stale connectors to
  // the fresh state, and clear unstarted stale attempts.
  MaybePromoteStaleConnectors();

  // Reset progress through endpoint results, as new ones may have been inserted
  // before the one that was currently under consideration.
  fresh_state_.current_endpoint_index = 0;
  stale_state_.current_endpoint_index = 0;
  if (IsDualRaceOptimisticDnsEnabled() &&
      !dns_request_->IsStaleWhileRefreshing()) {
    // If fresh endpoints are now available, the stale state should no longer
    // start any new connection attempts. Point its index to the end.
    stale_state_.current_endpoint_index = GetEndpointResults().size();
  }

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
  if (did_fail || (fresh_state_.primary_connector->is_waiting_on_dns() &&
                   (!fresh_state_.ipv4_connector ||
                    fresh_state_.ipv4_connector->is_waiting_on_dns()))) {
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
      ++waiting_on_possible_async_deletion_count_;
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE,
          base::BindOnce(
              &TcpConnectJob::TryAdvanceWaitingConnectorsAsync,
              weak_ptr_factory_.GetWeakPtr(),
              /*decrement_waiting_on_possible_async_deletion_count=*/true));
      return ERR_IO_PENDING;
    }
  }

  return DoTryAdvanceWaitingConnectors();
}

void TcpConnectJob::TryAdvanceWaitingConnectorsAsync(
    bool decrement_waiting_on_possible_async_deletion_count) {
  if (decrement_waiting_on_possible_async_deletion_count) {
    DCHECK_GT(waiting_on_possible_async_deletion_count_, 0u);
    --waiting_on_possible_async_deletion_count_;
  }
  NotifyDelegateIfDone(DoTryAdvanceWaitingConnectors());
}

int TcpConnectJob::DoTryAdvanceWaitingConnectors() {
  if (waiting_on_possible_async_deletion_count_) {
    // We're currently waiting on either `this` to be deleted, or a callback to
    // TryAdvanceWaitingConnectorsAsync() to be invoked indicating that we're
    // not going to be deleted. That callback will call this method again, so
    // can safely defer further work until it's invoked.
    return ERR_IO_PENDING;
  }

  // SetDone() should cancel all pending activity on completion, so this should
  // not be reachable after completion.
  CHECK(!is_done_);

  if (IsDualRaceOptimisticDnsEnabled()) {
    if (dns_request_ && dns_request_->IsStaleWhileRefreshing()) {
      if (!stale_state_.primary_connector) {
        stale_state_.primary_connector =
            std::make_unique<Connector>(this, "stale_primary");
      }
    }

    if (!IsStateDone(stale_state_)) {
      int rv = AdvanceConnectionState(stale_state_, /*is_stale=*/true);
      if (rv != ERR_IO_PENDING) {
        return rv;
      }
    }
  }

  if (!IsStateDone(fresh_state_)) {
    return AdvanceConnectionState(fresh_state_, /*is_stale=*/false);
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

  // If all connectors have failed, we're also done.
  // Note: `fresh_state_` is only considered "done" due to a null primary
  // connector if the DNS request is complete. Before DNS completes, the
  // connector is null because we are still waiting for endpoints.
  // `stale_state_` does not wait for DNS. If its primary connector is null,
  // it means stale endpoints were disabled, unavailable, or promoted, so it is
  // genuinely done.
  const bool fresh_done =
      IsStateDone(fresh_state_) &&
      (fresh_state_.primary_connector || dns_request_complete_);
  const bool stale_done = IsStateDone(stale_state_);

  if (fresh_done && stale_done) {
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

void TcpConnectJob::OnSlow(bool is_stale) {
  CHECK(!is_done_);
  ConnectionState& state = is_stale ? stale_state_ : fresh_state_;
  DCHECK(!state.ipv4_connector);

  net_log().AddEvent(NetLogEventType::TCP_CONNECT_JOB_CREATE_SECOND_CONNECTOR);

  // Make a second connector, so have separate IPv4 and IPv6 connectors. The
  // `state.primary_connector` may be waiting for an IP, or doing either
  // a v4 or v6 lookup. If it's doing a v4 lookup, move it into
  // `state.ipv4_connector`.
  //
  // Since the connectors may be flipped here, the static names of the
  // connectors for logging purposes are "first" and "second", rather than
  // "primary" and "ipv4".
  state.ipv4_connector = std::make_unique<Connector>(this, "second");
  if (!state.primary_connector->is_connecting_to_ipv6()) {
    std::swap(state.primary_connector, state.ipv4_connector);
  }

  TryAdvanceWaitingConnectorsAsync(
      /*decrement_waiting_on_possible_async_deletion_count=*/false);
}

// static
base::TimeDelta TcpConnectJob::GetIPv6FallbackTime(
    const CommonConnectJobParams* common_connect_job_params,
    const TransportSocketParams* params) {
  base::TimeDelta fallback_time = kIPv6FallbackTime;

  if (base::FeatureList::IsEnabled(features::kAdjustIPv6FallbackTime)) {
    fallback_time = features::kIPv6FallbackTime.Get();
  }

  if (base::FeatureList::IsEnabled(features::kIPv6FallbackBasedOnRTT)) {
    std::optional<base::TimeDelta> rtt =
        [&]() -> std::optional<base::TimeDelta> {
      // 1. Try to get destination-specific RTT from HttpServerProperties.
      if (common_connect_job_params->http_server_properties) {
        url::SchemeHostPort scheme_host_port;
        if (std::holds_alternative<url::SchemeHostPort>(
                params->destination())) {
          scheme_host_port =
              std::get<url::SchemeHostPort>(params->destination());
        } else {
          const auto& host_port_pair =
              std::get<HostPortPair>(params->destination());
          scheme_host_port = url::SchemeHostPort("https", host_port_pair.host(),
                                                 host_port_pair.port());
        }

        const ServerNetworkStats* stats =
            common_connect_job_params->http_server_properties
                ->GetServerNetworkStats(scheme_host_port,
                                        params->network_anonymization_key());
        if (stats && !stats->srtt.is_zero()) {
          return stats->srtt;
        }
      }

      // 2. Fallback to global network RTT from NetworkQualityEstimator.
      if (common_connect_job_params->network_quality_estimator) {
        std::optional<base::TimeDelta> nqe_rtt =
            common_connect_job_params->network_quality_estimator
                ->GetTransportRTT();
        if (nqe_rtt.has_value()) {
          return nqe_rtt;
        }
      }

      return std::nullopt;
    }();

    // 3. Calculate and clamp fallback time.
    if (rtt.has_value()) {
      base::TimeDelta rtt_based_fallback =
          rtt.value() * features::kIPv6FallbackRTTMultiplier.Get();

      rtt_based_fallback =
          std::clamp(rtt_based_fallback, features::kIPv6FallbackMin.Get(),
                     features::kIPv6FallbackMax.Get());
      return rtt_based_fallback;
    }
  }

  return fallback_time;
}

int TcpConnectJob::AdvanceConnectionState(ConnectionState& state,
                                          bool is_stale) {
  CHECK(!IsStateDone(state));

  if (state.primary_connector && !state.primary_connector->is_done()) {
    int rv = state.primary_connector->TryAdvanceState();
    if (rv != ERR_IO_PENDING) {
      // The connection attempt completed synchronously. Call
      // DoConnectorComplete() to handle the result, and learn if the entire
      // ConnectJob is complete.
      rv = DoConnectorComplete(rv, *state.primary_connector);
      if (rv != ERR_IO_PENDING) {
        return rv;
      }
    }
  }

  // The primary connector's TryAdvanceState() may have synchronously completed
  // the entire job, triggering cleanup that resets both connectors. Evaluate
  // `state.ipv4_connector` here after the primary connector runs to avoid a
  // potential use-after-free.
  if (state.ipv4_connector && !state.ipv4_connector->is_done()) {
    int rv = state.ipv4_connector->TryAdvanceState();
    if (rv != ERR_IO_PENDING) {
      // The connection attempt completed synchronously. Call
      // DoConnectorComplete() to handle the result, and learn if the entire
      // ConnectJob is complete.
      rv = DoConnectorComplete(rv, *state.ipv4_connector);
      if (rv != ERR_IO_PENDING) {
        return rv;
      }
    }
  }

  // If we reach this point, there should still be work to do.
  CHECK(!is_done_);
  // If both connectors within this state are already done, there is no work to
  // advance. Return ERR_IO_PENDING to wait for other states or DNS completion.
  if (IsStateDone(state)) {
    return ERR_IO_PENDING;
  }

  // Start the slow timer to try an IPv4 fallback if the primary connector is
  // taking too long.
  //
  // This could result in starting the slow timer if, for example, we're
  // already trying the final IP and no more IPs are coming, but this keeps
  // things simple.
  //
  // We only start the timer if all of the following conditions are met:
  // 1. We are using fresh results (not stale), or if we are using stale
  // results,
  //    the DNS request must still be refreshing. (If we are using stale results
  //    and the DNS request is complete, we don't start the fallback timer for
  //    stale IPs).
  // 2. We haven't already started the secondary (IPv4) connector.
  // 3. The timer isn't already running.
  // 4. The primary connector has actually started trying to connect to an IP
  //    address.
  if ((!is_stale || !dns_request_ || dns_request_->IsStaleWhileRefreshing()) &&
      !state.ipv4_connector && !state.slow_timer.IsRunning() &&
      state.primary_connector &&
      state.primary_connector->current_address().has_value()) {
    base::TimeDelta fallback_time =
        GetIPv6FallbackTime(common_connect_job_params(), params_.get());

    // If the connector was promoted from stale_state_, it may have already
    // been running for some time. We reduce the fallback timer by the time
    // already elapsed since the connector started its attempt.
    base::TimeTicks start_time = state.primary_connector->start_time();
    if (!start_time.is_null()) {
      base::TimeDelta elapsed = base::TimeTicks::Now() - start_time;
      fallback_time = std::max(base::TimeDelta(), fallback_time - elapsed);
    }

    state.slow_timer.Start(FROM_HERE, fallback_time,
                           base::BindOnce(&TcpConnectJob::OnSlow,
                                          base::Unretained(this), is_stale));
  }

  return ERR_IO_PENDING;
}

void TcpConnectJob::MaybePromoteStaleConnectors() {
  if (!IsDualRaceOptimisticDnsEnabled() || !dns_request_ ||
      dns_request_->IsStaleWhileRefreshing()) {
    return;
  }

  stale_state_.slow_timer.Stop();

  const auto& results = GetEndpointResults();

  auto try_promote = [&](std::unique_ptr<Connector>& stale_connector,
                         std::unique_ptr<Connector>& fresh_connector) {
    if (stale_connector && !stale_connector->is_done()) {
      if (!fresh_connector || fresh_connector->is_waiting_for_endpoint() ||
          fresh_connector->is_waiting_on_dns() || fresh_connector->is_done() ||
          !fresh_connector->current_address()) {
        if (stale_connector->current_address() &&
            IsEndpointInFreshList(*stale_connector->current_address(),
                                  results)) {
          fresh_connector = std::move(stale_connector);
        }
      }
    }
  };

  try_promote(stale_state_.primary_connector, fresh_state_.primary_connector);
  try_promote(stale_state_.ipv4_connector, fresh_state_.ipv4_connector);
}

TcpConnectJob::IPEndPointInfo TcpConnectJob::GetNextIPEndPoint(
    const Connector& connector) {
  const bool is_stale = IsStaleConnector(connector);

  // Stale connectors must be using stale endpoints, which are only available
  // when the DNS request is stale while refreshing.
  if (is_stale && (!dns_request_ || !dns_request_->IsStaleWhileRefreshing())) {
    return base::unexpected(ERR_NAME_NOT_RESOLVED);
  }

  // If we are using dedicated stale connectors, fresh connectors must wait
  // for fresh endpoints to arrive if the current endpoints are stale.
  if (IsDualRaceOptimisticDnsEnabled()) {
    if (!is_stale && dns_request_ && dns_request_->IsStaleWhileRefreshing()) {
      return base::unexpected(ERR_IO_PENDING);
    }
  }

  ConnectionState& state = is_stale ? stale_state_ : fresh_state_;
  if (waiting_on_possible_async_deletion_count_) {
    // We're currently waiting on either `this` to be deleted, or a callback to
    // be invoked indicating that we're not going to be deleted. That callback
    // will wake up the Connectors, so can safely defer further work until it's
    // invoked.
    return base::unexpected(ERR_IO_PENDING);
  }

  const auto& service_endpoints = GetEndpointResults();
  CHECK(!is_done_);
  DCHECK(!connector.is_done());

  // Other job, if any, for checking its state, and advancing it if necessary.
  const Connector* other_job = (&connector == state.primary_connector.get()
                                    ? state.ipv4_connector.get()
                                    : state.primary_connector.get());

  // Note that this will make both jobs use IPv4/IPv6, once there are no more
  // IPs of the other type. Not clear if that's a concern. Not too difficult to
  // change behavior - only checking IPv4 or IPv6 when there are two jobs should
  // be sufficient. `state.current_endpoint_index` logic will still work
  // correctly.
  bool prefer_ipv6 = state.prefer_ipv6;
  if (state.ipv4_connector) {
    prefer_ipv6 = (state.primary_connector.get() == &connector);
  }

  // If there are two jobs and DNS has not completed yet, only try to connect to
  // IPv4 destinations with `ipv4_connector_` and IPv6 destinations with
  // `primary_connector_`. Otherwise, only prefer preferred IP types.
  const bool only_preferred = other_job && !dns_request_complete_;

  bool posted_resume_task = false;

  while (state.current_endpoint_index < service_endpoints.size()) {
    const auto& service_endpoint =
        service_endpoints[state.current_endpoint_index];
    if (IsEndpointResultUsable(service_endpoint)) {
      base::span<const IPEndPoint> preferred_endpoints(
          prefer_ipv6 ? service_endpoint.ipv6_endpoints
                      : service_endpoint.ipv4_endpoints);
      // The endpoints that are not preferred. The connector may or may not be
      // allowed to use them, depending on `only_preferred`.
      base::span<const IPEndPoint> other_endpoints(
          prefer_ipv6 ? service_endpoint.ipv4_endpoints
                      : service_endpoint.ipv6_endpoints);

      // This will contain endpoints that connector may connect to. It will
      // always have the preferred endpoints, and depending on `only_preferred`
      // may or may not contain the others as well.
      std::vector<base::span<const IPEndPoint>> allowed_endpoints{
          preferred_endpoints};

      // These are endpoints that we may not connect to. It's only populated if
      // `only_preferred` is true. It's still needed so that we can check if the
      // other connector still has endpoints to try, even if it's currently
      // idle.
      std::optional<base::span<const IPEndPoint>> disallowed_endpoints;

      // Add `other_endpoints` where appropriate.
      if (only_preferred) {
        disallowed_endpoints.emplace(other_endpoints);
      } else {
        allowed_endpoints.emplace_back(other_endpoints);
      }

      // Walk through allowed endpoints, returning first one that hasn't yet
      // been tried if there is one, and adding it to `attempted_addresses_`.
      for (const auto& ip_endpoints : allowed_endpoints) {
        for (const auto& ip_endpoint : ip_endpoints) {
          // If `ip_endpoint` hasn't been tried yet, add it to
          // `attempted_addresses_` and we will return it.
          auto [it, inserted] = attempted_addresses_.emplace(ip_endpoint);
          if (inserted) {
            return ip_endpoint;
          }
        }
      }

      if (other_job) {
        // Only move on to the next endpoint if either there's no other
        // connector, or the other connector is waiting to receive an endpoint.
        // Since new results may come in out of order, this isn't perfect -
        // e.g., could still be connecting to a AAAA record when HTTPS records
        // come in. Then the AAAA connection attempt could block connection
        // attempts to the second ServiceEndpoint entry (and could block the
        // second job from trying the next A/AAAA entry as well, if the HTTPS
        // record connection attempts fail quickly). Could throw away the AAAA
        // attempt in that case, or have logic to detect it, but this seems a
        // reasonable balance of accuracy, complexity, and performance.
        if (!other_job->is_waiting_for_endpoint()) {
          // Need to wait for other jobs to complete.
          return base::unexpected(ERR_IO_PENDING);
        }

        // If there are disallowed endpoints, then it's possible that there's no
        // available endpoint for this connector, and the other connector is
        // waiting for an endpoint, but we have yet to inform it that there are
        // new endpoints available. To detect that case, look for any disallowed
        // endpoints
        if (disallowed_endpoints) {
          // Walk through the endpoints in reverse order looking for one that
          // hasn't been tried yet, which should tend to find any untried
          // addresses sooner.
          for (const auto& disallowed_endpoint :
               base::Reversed(*disallowed_endpoints)) {
            if (!attempted_addresses_.contains(disallowed_endpoint)) {
              return base::unexpected(ERR_IO_PENDING);
            }
          }
        }
      }
    }

    ++state.current_endpoint_index;
    // May need to resume the other job after advancing to the next result.
    if (other_job && !posted_resume_task) {
      // Small optimization to avoid posting multiple tasks at once - probably
      // not really needed. Nothing would break without it.
      posted_resume_task = true;
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE,
          base::BindOnce(
              &TcpConnectJob::TryAdvanceWaitingConnectorsAsync,
              weak_ptr_factory_.GetWeakPtr(),
              /*decrement_waiting_on_possible_async_deletion_count=*/false));
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
  const bool is_v6 = (ip_endpoint.GetFamily() == ADDRESS_FAMILY_IPV6);

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

void TcpConnectJob::ResetConnectionState(ConnectionState& state) {
  state.slow_timer.Stop();
  state.primary_connector.reset();
  state.ipv4_connector.reset();
  state.current_endpoint_index = 0;
  state.prefer_ipv6 = true;
}

// static
bool TcpConnectJob::IsStateDone(const ConnectionState& state) {
  const bool primary_done =
      !state.primary_connector || state.primary_connector->is_done();
  const bool ipv4_done =
      !state.ipv4_connector || state.ipv4_connector->is_done();
  return primary_done && ipv4_done;
}

int TcpConnectJob::SetDone(int result, Connector* connector) {
  CHECK(!is_done_);
  DCHECK(!final_service_endpoint_);

  if (dns_request_) {
    resolution_details_ = dns_request_->GetResolutionDetails();
  }

  if (result == OK) {
    DCHECK(EndpointsCryptoReady());
    DCHECK(connector);

    SetSocket(connector->PassSocket(), GetDnsAliasResults());
    final_service_endpoint_ = connector->PassFinalServiceEndpoint();
    DCHECK(final_service_endpoint_);
    DCHECK(IsEndpointResultUsable(*final_service_endpoint_));

    auto calculate_is_connected_via_stale_dns = [&]() -> bool {
      // If `disable_stale_dns_` is true, we explicitly requested a fresh
      // connection, so it cannot be stale.
      if (disable_stale_dns_) {
        return false;
      }
      if (features::kUseStaleConnectorsForOptimisticDns.Get()) {
        // If the feature is enabled, stale connectors are completely isolated
        // in `stale_state_`. If the winning connector is from `stale_state_`,
        // it is guaranteed to be a connection to a stale endpoint.
        return IsStaleConnector(*connector);
      }
      if (dns_request_) {
        // If the request is actively returning stale results, the connection is
        // currently considered stale.
        if (dns_request_->IsStaleWhileRefreshing()) {
          return true;
        }
        if (connector->current_address() &&
            !IsEndpointInFreshList(*connector->current_address(),
                                   dns_request_->GetEndpointResults())) {
          // If fresh results have arrived but the connected endpoint is not in
          // the fresh list, we must have raced and completed a connection to an
          // IP that was present in the stale results but removed in the fresh
          // results. This connection is stale.
          return true;
        }
      }
      return false;
    };

    CHECK(!is_connected_via_stale_dns_.has_value());
    is_connected_via_stale_dns_ = calculate_is_connected_via_stale_dns();
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
  ResetConnectionState(fresh_state_);
  ResetConnectionState(stale_state_);
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

bool TcpConnectJob::IsConnectedViaStaleDns() const {
  return is_connected_via_stale_dns_.value_or(false);
}

}  // namespace net
