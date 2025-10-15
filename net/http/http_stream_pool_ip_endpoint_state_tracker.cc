// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_stream_pool_ip_endpoint_state_tracker.h"

#include <map>
#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/values.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_export.h"
#include "net/dns/host_resolver.h"
#include "net/dns/public/host_resolver_results.h"
#include "net/http/http_stream_pool.h"

namespace net {

HttpStreamPool::IPEndPointStateTracker::IPEndPointStateTracker(
    Delegate* delegate)
    : delegate_(delegate) {
  CHECK(delegate_);
}

HttpStreamPool::IPEndPointStateTracker::~IPEndPointStateTracker() = default;

std::optional<HttpStreamPool::IPEndPointStateTracker::IPEndPointState>
HttpStreamPool::IPEndPointStateTracker::GetState(
    const IPEndPoint& ip_endpoint) const {
  auto it = ip_endpoint_states_.find(ip_endpoint);
  if (it == ip_endpoint_states_.end()) {
    return std::nullopt;
  } else {
    return it->second;
  }
}

void HttpStreamPool::IPEndPointStateTracker::OnEndpointSlow(
    const IPEndPoint& ip_endpoint) {
  // This will not overwrite the previous value, if it's already tagged as
  // kSlowSucceeded (Nor will it overwrite other values).
  ip_endpoint_states_.emplace(ip_endpoint, IPEndPointState::kSlowAttempting);
  prefer_ipv6_ = !ip_endpoint.address().IsIPv6();
}

void HttpStreamPool::IPEndPointStateTracker::OnEndpointSlowSucceeded(
    const IPEndPoint& ip_endpoint) {
  auto it = ip_endpoint_states_.find(ip_endpoint);
  CHECK(it != ip_endpoint_states_.end());
  it->second = IPEndPointState::kSlowSucceeded;
}

void HttpStreamPool::IPEndPointStateTracker::OnEndpointFailed(
    const IPEndPoint& ip_endpoint) {
  ip_endpoint_states_.insert_or_assign(ip_endpoint, IPEndPointState::kFailed);
}

void HttpStreamPool::IPEndPointStateTracker::RemoveSlowAttemptingEndpoint() {
  std::erase_if(ip_endpoint_states_, [](const auto& it) {
    return it.second == IPEndPointState::kSlowAttempting;
  });
}

std::optional<IPEndPoint>
HttpStreamPool::IPEndPointStateTracker::GetIPEndPointToAttemptTcpBased() {
  // TODO(crbug.com/383824591): Add a trace event to see if this method is
  // time consuming.

  HostResolver::ServiceEndpointRequest* service_endpoint_request =
      delegate_->GetServiceEndpointRequest();
  if (!service_endpoint_request ||
      service_endpoint_request->GetEndpointResults().empty()) {
    return std::nullopt;
  }

  const bool svcb_optional = delegate_->IsSvcbOptional();
  std::optional<IPEndPoint> current_endpoint;
  std::optional<IPEndPointState> current_state;

  for (bool ip_v6 : {prefer_ipv6_, !prefer_ipv6_}) {
    for (const auto& service_endpoint :
         service_endpoint_request->GetEndpointResults()) {
      if (!delegate_->IsEndpointUsableForTcpBasedAttempt(service_endpoint,
                                                         svcb_optional)) {
        continue;
      }

      const std::vector<IPEndPoint>& ip_endpoints =
          ip_v6 ? service_endpoint.ipv6_endpoints
                : service_endpoint.ipv4_endpoints;
      FindBetterIPEndPoint(ip_endpoints, current_state, current_endpoint);
      if (current_endpoint.has_value() && !current_state.has_value()) {
        // This endpoint is fast or no connection attempt has been made to
        // it yet.
        return current_endpoint;
      }
    }
  }

  // No available IP endpoint, or `current_endpoint` is slow.
  return current_endpoint;
}

void HttpStreamPool::IPEndPointStateTracker::FindBetterIPEndPoint(
    const std::vector<IPEndPoint>& ip_endpoints,
    std::optional<IPEndPointState>& current_state,
    std::optional<IPEndPoint>& current_endpoint) {
  for (const auto& ip_endpoint : ip_endpoints) {
    auto it = ip_endpoint_states_.find(ip_endpoint);
    if (it == ip_endpoint_states_.end()) {
      // If there is no state for the IP endpoint it means that we haven't tried
      // the endpoint yet or previous attempt to the endpoint was fast. Just use
      // it.
      current_endpoint = ip_endpoint;
      current_state = std::nullopt;
      return;
    }

    switch (it->second) {
      case IPEndPointState::kFailed:
        continue;
      case IPEndPointState::kSlowAttempting:
        if (!current_endpoint.has_value() &&
            !delegate_->HasEnoughTcpBasedAttemptsForSlowIPEndPoint(
                ip_endpoint)) {
          current_endpoint = ip_endpoint;
          current_state = it->second;
        }
        continue;
      case IPEndPointState::kSlowSucceeded:
        const bool prefer_slow_succeeded =
            !current_state.has_value() ||
            *current_state == IPEndPointState::kSlowAttempting;
        if (prefer_slow_succeeded &&
            !delegate_->HasEnoughTcpBasedAttemptsForSlowIPEndPoint(
                ip_endpoint)) {
          current_endpoint = ip_endpoint;
          current_state = it->second;
        }
        continue;
    }
  }
}

base::Value::List HttpStreamPool::IPEndPointStateTracker::GetInfoAsValue()
    const {
  base::Value::List list;
  for (const auto& [ip_endpoint, state] : ip_endpoint_states_) {
    list.Append(base::Value::Dict()
                    .Set("ip_endpoint", ip_endpoint.ToString())
                    .Set("state", static_cast<int>(state)));
  }
  return list;
}

}  // namespace net
