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

NetworkServiceProxyAllowList::NetworkServiceProxyAllowList() = default;
NetworkServiceProxyAllowList::~NetworkServiceProxyAllowList() = default;

NetworkServiceProxyAllowList::NetworkServiceProxyAllowList(
    const NetworkServiceProxyAllowList&) {}

NetworkServiceProxyAllowList NetworkServiceProxyAllowList::CreateForTesting(
    std::map<std::string, std::set<std::string>> first_party_map) {
  auto allow_list = NetworkServiceProxyAllowList();

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
  return base::FeatureList::IsEnabled(
             net::features::kEnableIpProtectionProxy) &&
         base::FeatureList::IsEnabled(network::features::kMaskedDomainList);
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
  url_matcher_with_bypass_.AddDomainWithBypass(domain,
                                               std::move(bypass_matcher));
}

size_t NetworkServiceProxyAllowList::EstimateMemoryUsage() const {
  return base::trace_event::EstimateMemoryUsage(url_matcher_with_bypass_);
}

bool NetworkServiceProxyAllowList::Matches(const GURL& request_url,
                                           const GURL& top_frame_url) {
  VLOG(3) << "NSPAL::Matches(" << request_url << ", " << top_frame_url << ")";
  return url_matcher_with_bypass_.Matches(request_url, top_frame_url);
}

void NetworkServiceProxyAllowList::UseMaskedDomainList(
    const masked_domain_list::MaskedDomainList& mdl) {
  url_matcher_with_bypass_.Clear();
  for (auto owner : mdl.resource_owners()) {
    for (auto resource : owner.owned_resources()) {
      url_matcher_with_bypass_.AddMaskedDomainListRules(resource.domain(),
                                                        owner);
    }
  }
  base::UmaHistogramMemoryKB(
      "NetworkService.MaskedDomainList.NetworkServiceProxyAllowList."
      "EstimatedMemoryUsageInKB",
      EstimateMemoryUsage() / 1024);
}

}  // namespace network
