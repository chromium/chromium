// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
#include "services/network/masked_domain_list/network_service_proxy_allow_list.h"

#include "base/containers/contains.h"
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

  auto mdl = masked_domain_list::MaskedDomainList();

  for (auto const& [domain, properties] : first_party_map) {
    auto* resourceOwner = mdl.add_resource_owners();
    for (auto property : properties) {
      resourceOwner->add_owned_properties(property);
    }
    auto* resource = resourceOwner->add_owned_resources();
    resource->set_domain(domain);
  }

  allow_list.UseMaskedDomainList(mdl);
  return allow_list;
}

bool NetworkServiceProxyAllowList::IsEnabled() {
  return base::FeatureList::IsEnabled(network::features::kMaskedDomainList);
}

bool NetworkServiceProxyAllowList::IsPopulated() {
  return url_matcher_with_bypass_.IsPopulated();
}

size_t NetworkServiceProxyAllowList::EstimateMemoryUsage() const {
  return base::trace_event::EstimateMemoryUsage(url_matcher_with_bypass_);
}

bool NetworkServiceProxyAllowList::Matches(
    const GURL& request_url,
    const net::NetworkAnonymizationKey& network_anonymization_key) {
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
  const int experiment_group_id =
      network::features::kMaskedDomainListExperimentGroup.Get();

  // Clients are in the default group if the experiment_group_id is the feature
  // default value of 0.
  const bool in_default_group = experiment_group_id == 0;

  // All Resources are used by the default group unless they are explicitly
  // excluded. For a client in the experiment group to use a Resource, the
  // Resource must explicitly list the experiment group.
  auto is_eligible = [&](masked_domain_list::Resource resource) {
    if (in_default_group) {
      return !resource.exclude_default_group();
    }
    return base::Contains(resource.experiment_group_ids(), experiment_group_id);
  };

  url_matcher_with_bypass_.Clear();
  for (auto owner : mdl.resource_owners()) {
    // Group domains by partition first so that only one set of the owner's
    // bypass rules are created per partition.
    std::map<std::string, std::set<std::string>> owned_domains_by_partition;
    for (auto resource : owner.owned_resources()) {
      if (is_eligible(resource)) {
        const std::string partition =
            UrlMatcherWithBypass::PartitionMapKey(resource.domain());
        owned_domains_by_partition[partition].insert(resource.domain());
      }
    }

    for (const auto& [partition, domains] : owned_domains_by_partition) {
      switch (proxy_bypass_policy_) {
        case network::mojom::IpProtectionProxyBypassPolicy::kNone: {
          url_matcher_with_bypass_.AddRulesWithoutBypass(domains, partition);
          break;
        }
        case network::mojom::IpProtectionProxyBypassPolicy::
            kFirstPartyToTopLevelFrame: {
          url_matcher_with_bypass_.AddMaskedDomainListRules(domains, partition,
                                                            owner);
          break;
        }
      }
    }
  }
  base::UmaHistogramMemoryKB(
      "NetworkService.MaskedDomainList.NetworkServiceProxyAllowList."
      "EstimatedMemoryUsageInKB",
      EstimateMemoryUsage() / 1024);
}

}  // namespace network
