// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/dns_task_results_manager.h"

#include <algorithm>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "net/base/connection_endpoint_metadata.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/dns/host_resolver.h"
#include "net/dns/host_resolver_dns_task.h"
#include "net/dns/host_resolver_internal_result.h"
#include "net/dns/https_record_rdata.h"
#include "net/dns/public/dns_query_type.h"
#include "net/dns/public/host_resolver_results.h"
#include "net/log/net_log_event_type.h"
#include "net/log/net_log_with_source.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "url/scheme_host_port.h"

namespace net {

namespace {

// Prioritize with-ipv6 over ipv4-only.
bool CompareServiceEndpointAddresses(const ServiceEndpoint& a,
                                     const ServiceEndpoint& b) {
  const bool a_has_ipv6 = !a.ipv6_endpoints.empty();
  const bool b_has_ipv6 = !b.ipv6_endpoints.empty();
  if ((a_has_ipv6 && b_has_ipv6) || (!a_has_ipv6 && !b_has_ipv6)) {
    return false;
  }

  if (b_has_ipv6) {
    return false;
  }

  return true;
}

// Prioritize with-metadata, with-ipv6 over ipv4-only.
// TODO(crbug.com/41493696): Consider which fields should be prioritized. We
// may want to have different sorting algorithms and choose one via config.
bool CompareServiceEndpoint(const ServiceEndpoint& a,
                            const ServiceEndpoint& b) {
  const bool a_has_metadata = a.metadata != ConnectionEndpointMetadata();
  const bool b_has_metadata = b.metadata != ConnectionEndpointMetadata();
  if (a_has_metadata && b_has_metadata) {
    return CompareServiceEndpointAddresses(a, b);
  }

  if (a_has_metadata) {
    return true;
  }

  if (b_has_metadata) {
    return false;
  }

  return CompareServiceEndpointAddresses(a, b);
}

}  // namespace

// Holds service endpoint results per domain name.
struct DnsTaskResultsManager::PerDomainResult {
  PerDomainResult() = default;
  ~PerDomainResult() = default;

  PerDomainResult(PerDomainResult&&) = default;
  PerDomainResult& operator=(PerDomainResult&&) = default;
  PerDomainResult(const PerDomainResult&) = delete;
  PerDomainResult& operator=(const PerDomainResult&) = delete;

  std::vector<IPEndPoint> ipv4_endpoints;
  std::vector<IPEndPoint> ipv6_endpoints;

  std::multimap<HttpsRecordPriority, ConnectionEndpointMetadata> metadatas;
};

DnsTaskResultsManager::DnsTaskResultsManager(Delegate* delegate,
                                             HostResolver::Host host,
                                             DnsQueryTypeSet query_types,
                                             const NetLogWithSource& net_log)
    : delegate_(delegate),
      host_(std::move(host)),
      query_types_(query_types),
      net_log_(net_log) {
  CHECK(delegate_);
}

DnsTaskResultsManager::~DnsTaskResultsManager() = default;

void DnsTaskResultsManager::ProcessDnsTransactionResults(
    DnsQueryType query_type,
    const std::set<std::unique_ptr<HostResolverInternalResult>>& results) {
  CHECK(query_types_.Has(query_type));

  bool should_update_endpoints = false;
  bool should_notify = false;

  if (query_type == DnsQueryType::HTTPS) {
    // Chrome does not yet support HTTPS follow-up queries so metadata is
    // considered ready when the HTTPS response is received.
    CHECK(!is_metadata_ready_);
    is_metadata_ready_ = true;
    should_notify = true;
  }

  if (query_type == DnsQueryType::AAAA) {
    aaaa_response_received_ = true;
    if (resolution_delay_timer_.IsRunning()) {
      resolution_delay_timer_.Stop();
      RecordResolutionDelayResult(/*timedout=*/false);
      // Need to update endpoints when there are IPv4 addresses.
      if (HasIpv4Addresses()) {
        should_update_endpoints = true;
      }
    }
  }

  for (auto& result : results) {
    aliases_.insert(result->domain_name());

    switch (result->type()) {
      case HostResolverInternalResult::Type::kData: {
        PerDomainResult& per_domain_result =
            GetOrCreatePerDomainResult(result->domain_name());
        for (const auto& ip_endpoint : result->AsData().endpoints()) {
          CHECK_EQ(ip_endpoint.port(), 0);
          // TODO(crbug.com/41493696): This will eventually need to handle
          // DnsQueryType::HTTPS to support getting ipv{4,6}hints.
          if (ip_endpoint.address().IsIPv4()) {
            per_domain_result.ipv4_endpoints.emplace_back(ip_endpoint.address(),
                                                          host_.GetPort());
          } else {
            CHECK(ip_endpoint.address().IsIPv6());
            per_domain_result.ipv6_endpoints.emplace_back(ip_endpoint.address(),
                                                          host_.GetPort());
          }
        }

        should_update_endpoints |= !result->AsData().endpoints().empty();

        break;
      }
      case HostResolverInternalResult::Type::kMetadata: {
        CHECK_EQ(query_type, DnsQueryType::HTTPS);
        for (auto [priority, metadata] : result->AsMetadata().metadatas()) {
          // Associate the metadata with the target name instead of the domain
          // name since the metadata is for the target name.
          PerDomainResult& per_domain_result =
              GetOrCreatePerDomainResult(metadata.target_name);
          per_domain_result.metadatas.emplace(priority, metadata);
        }

        should_update_endpoints |= !result->AsMetadata().metadatas().empty();

        break;
      }
      case net::HostResolverInternalResult::Type::kAlias:
        aliases_.insert(result->AsAlias().alias_target());

        break;
      case net::HostResolverInternalResult::Type::kError:
        // Need to update endpoints when AAAA response is NODATA but A response
        // has at least one valid address.
        // TODO(crbug.com/41493696): Revisit how to handle errors other than
        // NODATA. Currently we just ignore errors here and defer
        // HostResolverManager::Job to create an error result and notify the
        // error to the corresponding requests. This means that if the
        // connection layer has already attempted a connection using an
        // intermediate endpoint, the error might not be treated as fatal. We
        // may want to have a different semantics.
        PerDomainResult& per_domain_result =
            GetOrCreatePerDomainResult(result->domain_name());
        if (query_type == DnsQueryType::AAAA &&
            result->AsError().error() == ERR_NAME_NOT_RESOLVED &&
            !per_domain_result.ipv4_endpoints.empty()) {
          CHECK(per_domain_result.ipv6_endpoints.empty());
          should_update_endpoints = true;
        }

        break;
    }
  }

  const bool waiting_for_aaaa_response =
      query_types_.Has(DnsQueryType::AAAA) && !aaaa_response_received_;
  if (waiting_for_aaaa_response) {
    if (query_type == DnsQueryType::A && should_update_endpoints) {
      // A is responded, start the resolution delay timer.
      CHECK(!resolution_delay_timer_.IsRunning());
      resolution_delay_start_time_ = base::TimeTicks::Now();
      net_log_.BeginEvent(
          NetLogEventType::HOST_RESOLVER_SERVICE_ENDPOINTS_RESOLUTION_DELAY);
      // Safe to unretain since `this` owns the timer.
      resolution_delay_timer_.Start(
          FROM_HERE, kResolutionDelay,
          base::BindOnce(&DnsTaskResultsManager::OnAaaaResolutionTimedout,
                         base::Unretained(this)));
    }

    return;
  }

  if (should_update_endpoints) {
    UpdateEndpoints();
    return;
  }

  if (should_notify && !current_endpoints_.empty()) {
    delegate_->OnServiceEndpointsUpdated();
  }
}

const std::vector<ServiceEndpoint>& DnsTaskResultsManager::GetCurrentEndpoints()
    const {
  return current_endpoints_;
}

const std::set<std::string>& DnsTaskResultsManager::GetAliases() const {
  return aliases_;
}

bool DnsTaskResultsManager::IsMetadataReady() const {
  return !query_types_.Has(DnsQueryType::HTTPS) || is_metadata_ready_;
}

DnsTaskResultsManager::PerDomainResult&
DnsTaskResultsManager::GetOrCreatePerDomainResult(
    const std::string& domain_name) {
  auto it = per_domain_results_.find(domain_name);
  if (it == per_domain_results_.end()) {
    it = per_domain_results_.try_emplace(it, domain_name,
                                         std::make_unique<PerDomainResult>());
  }
  return *it->second;
}

void DnsTaskResultsManager::OnAaaaResolutionTimedout() {
  CHECK(!aaaa_response_received_);
  RecordResolutionDelayResult(/*timedout=*/true);
  UpdateEndpoints();
}

void DnsTaskResultsManager::UpdateEndpoints() {
  std::vector<ServiceEndpoint> new_endpoints;

  for (const auto& [domain_name, per_domain_result] : per_domain_results_) {
    if (per_domain_result->ipv4_endpoints.empty() &&
        per_domain_result->ipv6_endpoints.empty()) {
      continue;
    }

    if (per_domain_result->metadatas.empty()) {
      ServiceEndpoint endpoint;
      endpoint.ipv4_endpoints = per_domain_result->ipv4_endpoints;
      endpoint.ipv6_endpoints = per_domain_result->ipv6_endpoints;
      new_endpoints.emplace_back(std::move(endpoint));
    } else {
      for (const auto& [_, metadata] : per_domain_result->metadatas) {
        ServiceEndpoint endpoint;
        endpoint.ipv4_endpoints = per_domain_result->ipv4_endpoints;
        endpoint.ipv6_endpoints = per_domain_result->ipv6_endpoints;
        // TODO(crbug.com/41493696): Just adding per-domain metadata does not
        // work properly when the target name of HTTPS is an alias, e.g:
        //   example.com.     60 IN CNAME svc.example.com.
        //   svc.example.com. 60 IN AAAA  2001:db8::1
        //   svc.example.com. 60 IN HTTPS 1 example.com alpn="h2"
        // In this case, svc.example.com should have metadata with alpn="h2" but
        // the current logic doesn't do that. To handle it correctly we need to
        // go though an alias tree for the domain name.
        endpoint.metadata = metadata;
        new_endpoints.emplace_back(std::move(endpoint));
      }
    }
  }

  // TODO(crbug.com/41493696): Determine how to handle non-SVCB connection
  // fallback. See https://datatracker.ietf.org/doc/html/rfc9460#section-3-8
  // HostCache::Entry::GetEndpoints() appends a final non-alternative endpoint
  // at the end to ensure that the connection layer can fall back to non-SVCB
  // connection. For ServiceEndpoint request API, the current plan is to handle
  // non-SVCB connection fallback in the connection layer. The approach might
  // not work when Chrome tries to support HTTPS follow-up queries and aliases.

  // Stable sort preserves metadata priorities.
  std::stable_sort(new_endpoints.begin(), new_endpoints.end(),
                   CompareServiceEndpoint);
  current_endpoints_ = std::move(new_endpoints);

  if (current_endpoints_.empty()) {
    return;
  }

  net_log_.AddEvent(NetLogEventType::HOST_RESOLVER_SERVICE_ENDPOINTS_UPDATED,
                    [&] {
                      base::Value::Dict dict;
                      base::Value::List endpoints;
                      for (const auto& endpoint : current_endpoints_) {
                        endpoints.Append(endpoint.ToValue());
                      }
                      dict.Set("endpoints", std::move(endpoints));
                      return dict;
                    });

  delegate_->OnServiceEndpointsUpdated();
}

bool DnsTaskResultsManager::HasIpv4Addresses() {
  for (const auto& [_, per_domain_result] : per_domain_results_) {
    if (!per_domain_result->ipv4_endpoints.empty()) {
      return true;
    }
  }
  return false;
}

void DnsTaskResultsManager::RecordResolutionDelayResult(bool timedout) {
  net_log_.EndEvent(
      NetLogEventType::HOST_RESOLVER_SERVICE_ENDPOINTS_RESOLUTION_DELAY, [&]() {
        base::TimeDelta elapsed =
            base::TimeTicks::Now() - resolution_delay_start_time_;
        base::Value::Dict dict;
        dict.Set("timedout", timedout);
        dict.Set("elapsed", base::NumberToString(elapsed.InMilliseconds()));
        return dict;
      });
}

}  // namespace net
