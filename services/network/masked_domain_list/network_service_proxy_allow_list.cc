// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
#include "services/network/masked_domain_list/network_service_proxy_allow_list.h"

#include "base/metrics/histogram_functions.h"
#include "base/trace_event/memory_usage_estimator.h"
#include "components/privacy_sandbox/masked_domain_list/masked_domain_list.pb.h"
#include "net/base/features.h"
#include "net/base/schemeful_site.h"
#include "services/network/public/cpp/features.h"

namespace network {

NetworkServiceProxyAllowList::NetworkServiceProxyAllowList(
    network::mojom::IpProtectionProxyBypassPolicy policy)
    : proxy_bypass_policy_{policy} {}

NetworkServiceProxyAllowList::~NetworkServiceProxyAllowList() = default;

NetworkServiceProxyAllowList::NetworkServiceProxyAllowList(
    const NetworkServiceProxyAllowList&) {}

NetworkServiceProxyAllowList NetworkServiceProxyAllowList::CreateForTesting(
    std::map<std::string, std::set<std::string>> first_party_map) {
  auto allow_list = NetworkServiceProxyAllowList(
      network::mojom::IpProtectionProxyBypassPolicy::
          kFirstPartyToTopLevelFrame);

  for (auto const& [domain, properties] : first_party_map) {
    net::SchemeHostPortMatcher bypass_matcher;
    for (auto property : properties) {
      bypass_matcher.AddAsFirstRule(
          net::SchemeHostPortMatcherRule::FromUntrimmedRawString(property));
      bypass_matcher.AddAsFirstRule(
          net::SchemeHostPortMatcherRule::FromUntrimmedRawString("." +
                                                                 property));
    }

    allow_list.AddDomainWithBypass(domain, std::move(bypass_matcher));
  }

  return allow_list;
}

bool NetworkServiceProxyAllowList::IsEnabled() {
  return base::FeatureList::IsEnabled(network::features::kMaskedDomainList);
}

bool NetworkServiceProxyAllowList::IsPopulated() {
  return url_matcher_with_bypass_.IsPopulated();
}

// static
mojom::CustomProxyConfigPtr
NetworkServiceProxyAllowList::MakeIpProtectionCustomProxyConfig() {
  auto custom_proxy_config = network::mojom::CustomProxyConfig::New();
  // Indicate to the NetworkServiceProxyDelegate that this is for IP Protection
  // and it should use the allow list. In this situation, the delegate does not
  // use any other fields from the custom proxy config.
  custom_proxy_config->rules.restrict_to_network_service_proxy_allow_list =
      true;
  return custom_proxy_config;
}

void NetworkServiceProxyAllowList::AddDomainWithBypass(
    const std::string& domain,
    net::SchemeHostPortMatcher bypass_matcher) {
  url_matcher_with_bypass_.AddDomainWithBypass(
      domain, std::move(bypass_matcher), /*include_subdomains=*/true);
}

size_t NetworkServiceProxyAllowList::EstimateMemoryUsage() const {
  return base::trace_event::EstimateMemoryUsage(url_matcher_with_bypass_);
}

bool NetworkServiceProxyAllowList::Matches(
    const GURL& request_url,
    const net::NetworkAnonymizationKey& network_anonymization_key) {
  // TODO(https://crbug.com/1474932): Support proxying HTTP URLs by using
  // CONNECT requests (i.e. tunnelling) instead of using the old-style proxy GET
  // requests from the last proxy in the chain.
  if (request_url.SchemeIs(url::kHttpScheme)) {
    return false;
  }

  absl::optional<net::SchemefulSite> top_frame_site =
      network_anonymization_key.GetTopFrameSite();
  switch (proxy_bypass_policy_) {
    case network::mojom::IpProtectionProxyBypassPolicy::kNone: {
      return url_matcher_with_bypass_.Matches(request_url, top_frame_site, true)
          .matches;
    }
    case network::mojom::IpProtectionProxyBypassPolicy::
        kFirstPartyToTopLevelFrame: {
      if (!network_anonymization_key.GetTopFrameSite().has_value()) {
        DVLOG(3) << "NSPAL::Matches(" << request_url
                 << ", empty top_frame_site) - false";
        return false;
      }
      DVLOG(3) << "NSPAL::Matches(" << request_url << ", "
               << top_frame_site.value() << ")";

      // If the NAK is transient (has a nonce and/or top_frame_origin is
      // opaque), we should skip the first party check and match only on the
      // request_url.
      UrlMatcherWithBypass::MatchResult result =
          url_matcher_with_bypass_.Matches(
              request_url, top_frame_site,
              network_anonymization_key.IsTransient());
      return result.matches && result.is_third_party;
    }
  }
}

void NetworkServiceProxyAllowList::UseMaskedDomainList(
    const masked_domain_list::MaskedDomainList& mdl) {
  url_matcher_with_bypass_.Clear();
  for (auto owner : mdl.resource_owners()) {
    // Group domains by partition first so that only one set of the owner's
    // bypass rules are created per partition.
    std::map<std::string, std::vector<std::string>> owned_domains_by_partition;
    for (auto resource : owner.owned_resources()) {
      const std::string partition =
          UrlMatcherWithBypass::PartitionMapKey(resource.domain());
      owned_domains_by_partition[partition].emplace_back(resource.domain());
    }

    for (const auto& [partition, domains] : owned_domains_by_partition) {
      url_matcher_with_bypass_.AddMaskedDomainListRules(domains, partition,
                                                        owner);
    }
  }
  base::UmaHistogramMemoryKB(
      "NetworkService.MaskedDomainList.NetworkServiceProxyAllowList."
      "EstimatedMemoryUsageInKB",
      EstimateMemoryUsage() / 1024);
}

}  // namespace network
