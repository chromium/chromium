// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
#include "services/network/masked_domain_list/network_service_proxy_allow_list.h"

#include <vector>

#include "base/containers/contains.h"
#include "base/metrics/histogram_functions.h"
#include "base/trace_event/memory_usage_estimator.h"
#include "components/privacy_sandbox/masked_domain_list/masked_domain_list.pb.h"
#include "net/base/features.h"
#include "net/base/schemeful_site.h"
#include "net/base/url_util.h"
#include "services/network/public/cpp/features.h"
#include "url/url_constants.h"

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

  allow_list.UseMaskedDomainList(mdl,
                                 /*exclusion_list=*/std::vector<std::string>());
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
  std::optional<net::SchemefulSite> top_frame_site =
      network_anonymization_key.GetTopFrameSite();
  switch (proxy_bypass_policy_) {
    case network::mojom::IpProtectionProxyBypassPolicy::kNone:
    case network::mojom::IpProtectionProxyBypassPolicy::kExclusionList: {
      return url_matcher_with_bypass_.Matches(request_url, top_frame_site,
                                              /*skip_bypass_check=*/true);
    }
    case network::mojom::IpProtectionProxyBypassPolicy::
        kFirstPartyToTopLevelFrame: {
      if (!top_frame_site.has_value()) {
        DVLOG(3) << "NSPAL::Matches(" << request_url
                 << ", empty top_frame_site) - false";
        return false;
      }
      DVLOG(3) << "NSPAL::Matches(" << request_url << ", "
               << top_frame_site.value() << ")";

      // Only proxy traffic where the top-level site is an HTTP/HTTPS page or
      // where the NAK corresponds to a fenced frame.
      if (net::features::kIpPrivacyRestrictTopLevelSiteSchemes.Get() &&
          !network_anonymization_key.GetNonce().has_value() &&
          !top_frame_site.value().GetURL().SchemeIsHTTPOrHTTPS()) {
        // Note: It's possible that the top-level site could be a file: URL in
        // the case where an HTML file was downloaded and then opened. We don't
        // proxy in this case in favor of better compatibility. It's also
        // possible that the top-level site could be a blob URL, data URL, or
        // filesystem URL (the latter two with restrictions on how they could
        // have been navigated to), but we'll assume these aren't used
        // pervasively as the top-level site for pages that make the types of
        // requests that IP Protection will apply to.
        return false;
      }

      // If the NAK is transient (has a nonce and/or top_frame_origin is
      // opaque), we should skip the first party check and match only on the
      // request_url.
      return url_matcher_with_bypass_.Matches(
          request_url, top_frame_site, network_anonymization_key.IsTransient());
    }
  }
}

std::set<std::string> NetworkServiceProxyAllowList::ExcludeDomainsFromMDL(
    const std::set<std::string>& mdl_domains,
    const std::set<std::string>& excluded_domains) {
  if (excluded_domains.empty()) {
    return mdl_domains;
  }

  std::set<std::string> mdl_domains_after_exclusions;
  for (const auto& mdl_domain : mdl_domains) {
    std::string mdl_superdomain(mdl_domain);
    bool shouldInclude = true;

    // Check if any super domains exist in the exclusion set
    // For example, mdl_domain W.X.Y.Z should be excluded if exclusion set
    // contains super domains W.X.Y.Z or X.Y.Z or Y.Z or Z

    // TODO(crbug/326399905): Add logic for excluding a domain X if any other
    // domain owned by X's resource owner is on the exclusion list.

    while (!mdl_superdomain.empty()) {
      if (base::Contains(excluded_domains, mdl_superdomain)) {
        shouldInclude = false;
        break;
      }
      mdl_superdomain = net::GetSuperdomain(mdl_superdomain);
    }

    if (shouldInclude) {
      mdl_domains_after_exclusions.insert(mdl_domain);
    }
  }

  return mdl_domains_after_exclusions;
}

void NetworkServiceProxyAllowList::UseMaskedDomainList(
    const masked_domain_list::MaskedDomainList& mdl,
    const std::vector<std::string> exclusion_list) {
  const int experiment_group_id =
      network::features::kMaskedDomainListExperimentGroup.Get();
  const std::set<std::string> exclusion_set(exclusion_list.begin(),
                                            exclusion_list.end());

  // Clients are in the default group if the experiment_group_id is the
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

    std::set<std::string> eligible_domains;
    for (auto resource : owner.owned_resources()) {
      if (is_eligible(resource)) {
        eligible_domains.insert(resource.domain());
      }
    }

    switch (proxy_bypass_policy_) {
      case network::mojom::IpProtectionProxyBypassPolicy::kNone: {
        url_matcher_with_bypass_.AddRulesWithoutBypass(eligible_domains);
        break;
      }
      case network::mojom::IpProtectionProxyBypassPolicy::
          kFirstPartyToTopLevelFrame: {
        url_matcher_with_bypass_.AddMaskedDomainListRules(eligible_domains,
                                                          owner);
        break;
      }
      case network::mojom::IpProtectionProxyBypassPolicy::kExclusionList: {
        url_matcher_with_bypass_.AddRulesWithoutBypass(
            ExcludeDomainsFromMDL(eligible_domains, exclusion_set));
        break;
      }
    }
  }
  base::UmaHistogramMemoryKB(
      "NetworkService.MaskedDomainList.NetworkServiceProxyAllowList."
      "EstimatedMemoryUsageInKB",
      EstimateMemoryUsage() / 1024);
}

}  // namespace network
